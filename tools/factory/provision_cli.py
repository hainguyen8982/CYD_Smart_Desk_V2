"""
FabX SmartDesk — Factory Auto-Flash & NFC Provisioning CLI
-----------------------------------------------------------
Tự động:
  1. Nạp firmware (bootloader + partitions + boot_app0 + firmware.bin) vào ESP32
  2. Đọc eFuse MAC -> sinh Device ID 6 ký tự (khớp 100% EcosystemService::getDeviceId)
  3. Sinh link NFC: https://smartdesk.fabx.vn/d/<ID>
  4. (Tùy chọn) Gửi lệnh ghi thẻ NTAG21x qua Arduino + RC522
  5. Lưu vào kho local SQLite (smartdesk_local.db) + đồng bộ lên Cloud Admin (nếu cấu hình)

Ví dụ chạy:
  python tools/factory/provision_cli.py --esp-port COM5 --nfc-port COM6 --customer "Anh Minh" --order "FX-001"
  python tools/factory/provision_cli.py --esp-port COM5 --read-only
"""

import argparse
import datetime
import json
import os
import re
import sqlite3
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FACTORY_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(FACTORY_DIR, "smartdesk_local.db")
DEFAULT_BUILD_DIR = os.path.join(ROOT, ".pio", "build", "nicemcu_32s_2_8ips")
DEFAULT_CLOUD_URL = "https://smartdesk.fabx.vn"


def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS devices (
            id TEXT PRIMARY KEY,
            mac TEXT,
            nfc_url TEXT,
            nfc_uid TEXT,
            hw TEXT DEFAULT 'nicemcu_2.8ips',
            fw TEXT,
            ip TEXT,
            rssi INTEGER,
            customer TEXT DEFAULT '',
            phone TEXT DEFAULT '',
            order_id TEXT DEFAULT '',
            sold_at TEXT DEFAULT '',
            warranty_months INTEGER DEFAULT 6,
            beta INTEGER DEFAULT 0,
            note TEXT DEFAULT '',
            created_at TEXT,
            cloud_updated_at INTEGER DEFAULT 0
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT
        )
        """
    )
    conn.commit()
    return conn


def get_esptool_cmd():
    # Ưu tiên esptool đi kèm Python hoặc PlatformIO
    for mod in ("esptool",):
        try:
            __import__(mod)
            return [sys.executable, "-m", "esptool"]
        except ImportError:
            pass
    pio_esptool = os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")
    if os.path.exists(pio_esptool):
        return [sys.executable, pio_esptool]
    return ["esptool"]


def find_boot_app0():
    candidates = [
        os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


def flash_esp32(port, build_dir, baud=921600):
    fw = os.path.join(build_dir, "firmware.bin")
    bl = os.path.join(build_dir, "bootloader.bin")
    pt = os.path.join(build_dir, "partitions.bin")
    ba = find_boot_app0()

    if not os.path.exists(fw):
        raise FileNotFoundError(f"Không tìm thấy {fw}. Hãy build firmware trước!")

    cmd = get_esptool_cmd() + [
        "--chip", "esp32",
        "--port", port,
        "--baud", str(baud),
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", "-z",
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", "4MB",
    ]
    if os.path.exists(bl):
        cmd += ["0x1000", bl]
    if os.path.exists(pt):
        cmd += ["0x8000", pt]
    if ba:
        cmd += ["0xe000", ba]
    cmd += ["0x10000", fw]

    print(f"[Flash] Đang nạp firmware lên {port}...")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr or proc.stdout or "esptool flash failed")
    return proc.stdout


def read_esp32_mac_and_id(port):
    cmd = get_esptool_cmd() + ["--chip", "esp32", "--port", port, "read_mac"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = (proc.stdout or "") + "\n" + (proc.stderr or "")
    m = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", out)
    if not m:
        raise RuntimeError(f"Không đọc được MAC từ {port}:\n{out}")
    mac = m.group(1).upper()
    parts = mac.split(":")
    # Khớp với EcosystemService::getDeviceId() (3 byte cuối của base MAC viết hoa)
    dev_id = "".join(parts[3:6])
    return mac, dev_id


def write_nfc_rc522(nfc_port, url, timeout=20):
    import serial  # pyserial

    with serial.Serial(nfc_port, 115200, timeout=1) as ser:
        time.sleep(1.8)  # Arduino Uno/Nano auto-reset on open
        ser.reset_input_buffer()
        ser.write(f"WRITE:{url}\n".encode("utf-8"))
        print(f"[NFC] Đã gửi lệnh ghi xuống {nfc_port}. Hãy chạm thẻ NTAG21x vào đầu đọc RC522...")
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            print(f"  [RC522] {line}")
            if line.startswith("OK:"):
                m = re.search(r"UID=([0-9A-F]+)", line)
                return m.group(1) if m else ""
            if line.startswith("ERR:"):
                raise RuntimeError(f"RC522 báo lỗi: {line}")
        raise TimeoutError("Hết thời gian chờ chạm thẻ NFC")


def sync_device_to_cloud(cloud_url, admin_key, dev_id, payload):
    url = f"{cloud_url.rstrip('/')}/api/admin/devices/{dev_id}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="PATCH",
        headers={
            "Content-Type": "application/json",
            "X-Admin-Key": admin_key,
        },
    )
    with urllib.request.urlopen(req, timeout=8) as resp:
        return json.loads(resp.read().decode("utf-8"))


def main():
    ap = argparse.ArgumentParser(description="FabX SmartDesk Factory Provisioning CLI")
    ap.add_argument("--esp-port", required=True, help="Cổng COM của ESP32 (vd COM5)")
    ap.add_argument("--nfc-port", default="", help="Cổng COM của Arduino + RC522 (vd COM6)")
    ap.add_argument("--build-dir", default=DEFAULT_BUILD_DIR, help="Thư mục build PlatformIO")
    ap.add_argument("--hw", default="nicemcu_2.8ips", help="Mã phần cứng")
    ap.add_argument("--fw", default="2.5.0", help="Phiên bản firmware")
    ap.add_argument("--read-only", action="store_true", help="Chỉ đọc MAC/ID, không flash lại")
    ap.add_argument("--customer", default="", help="Tên khách hàng")
    ap.add_argument("--order", default="", help="Mã đơn hàng")
    ap.add_argument("--note", default="", help="Ghi chú")
    ap.add_argument("--cloud-url", default=DEFAULT_CLOUD_URL)
    ap.add_argument("--admin-key", default="", help="Admin Key để đăng ký trước lên Cloud")
    args = ap.parse_args()

    if not args.read_only:
        flash_esp32(args.esp_port, args.build_dir)
        print("[Flash] Nạp Firmware thành công!")

    mac, dev_id = read_esp32_mac_and_id(args.esp_port)
    nfc_url = f"{args.cloud_url.rstrip('/')}/d/{dev_id}"
    print("=" * 56)
    print(f"  DEVICE ID : {dev_id}")
    print(f"  BASE MAC  : {mac}")
    print(f"  NFC URL   : {nfc_url}")
    print("=" * 56)

    nfc_uid = ""
    if args.nfc_port:
        nfc_uid = write_nfc_rc522(args.nfc_port, nfc_url)
        print(f"[NFC] Ghi & kiểm tra thẻ thành công! UID={nfc_uid}")

    conn = init_db()
    now_iso = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    sold_at = datetime.date.today().isoformat() if args.customer or args.order else ""
    conn.execute(
        """
        INSERT INTO devices (id, mac, nfc_url, nfc_uid, hw, fw, customer, order_id, sold_at, note, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(id) DO UPDATE SET
            mac=excluded.mac,
            nfc_url=excluded.nfc_url,
            nfc_uid=CASE WHEN excluded.nfc_uid != '' THEN excluded.nfc_uid ELSE devices.nfc_uid END,
            hw=excluded.hw,
            fw=excluded.fw,
            customer=CASE WHEN excluded.customer != '' THEN excluded.customer ELSE devices.customer END,
            order_id=CASE WHEN excluded.order_id != '' THEN excluded.order_id ELSE devices.order_id END,
            note=CASE WHEN excluded.note != '' THEN excluded.note ELSE devices.note END
        """,
        (dev_id, mac, nfc_url, nfc_uid, args.hw, args.fw, args.customer, args.order, sold_at, args.note, now_iso),
    )
    conn.commit()
    conn.close()
    print(f"[DB] Đã lưu máy {dev_id} vào {DB_PATH}")

    if args.admin_key:
        try:
            sync_device_to_cloud(
                args.cloud_url,
                args.admin_key,
                dev_id,
                {
                    "hw": args.hw,
                    "customer": args.customer,
                    "orderId": args.order,
                    "soldAt": sold_at,
                    "nfcUid": nfc_uid,
                    "note": args.note,
                },
            )
            print(f"[Cloud] Đã đồng bộ {dev_id} lên {args.cloud_url}/admin")
        except Exception as e:
            print(f"[Cloud] Cảnh báo: chưa đồng bộ được lên Cloud ({e})")


if __name__ == "__main__":
    main()
