# 🏭 FabX SmartDesk — Bộ Công Cụ Nạp Xưởng, Ghi NFC (RC522) & Quản Lý Local

Thư mục này chứa trọn bộ công cụ sản xuất & quản lý máy thương mại:

| File | Công dụng |
| :--- | :--- |
| `smartdesk_manager.py` | **App quản lý Local** (giao diện web cục bộ `http://127.0.0.1:8765`, SQLite offline, nạp firmware 1 chạm, ghi thẻ NFC, in tem đáy, xuất Excel CSV, đồng bộ 2 chiều với `smartdesk.fabx.vn/admin`, upload Cloud OTA). |
| `provision_cli.py` | **Script dòng lệnh nạp nhanh** (flash ESP32 + đọc ID + ghi thẻ RC522 + lưu DB). |
| `arduino_rc522_nfc_writer/arduino_rc522_nfc_writer.ino` | **Code nạp cho Arduino Uno/Nano + MFRC522** để ghi thẻ NTAG213/215/216 tự động qua USB. |

---

## 1. Cài đặt thư viện Python (chỉ làm 1 lần)
```bash
pip install pyserial esptool
```

## 2. Nạp bộ ghi NFC (Arduino Uno/Nano + RC522)
Đấu dây SPI chuẩn (nhớ dùng **3.3V**, không dùng 5V):
- `SDA (SS)` → `D10`
- `SCK` → `D13`
- `MOSI` → `D11`
- `MISO` → `D12`
- `RST` → `D9`
- `3.3V` → `3.3V`, `GND` → `GND`

Mở `arduino_rc522_nfc_writer/arduino_rc522_nfc_writer.ino` trong Arduino IDE, cài thư viện **MFRC522** (GithubCommunity), rồi nạp lên Arduino.

## 3. Chạy App Quản Lý Local
```bash
python tools/factory/smartdesk_manager.py
```
Trình duyệt sẽ tự mở tại `http://127.0.0.1:8765`.
- Vào tab **⚙️ Kết nối Cloud Admin**, nhập `https://smartdesk.fabx.vn` và `ADMIN_KEY` của bạn.
- Cắm ESP32 + Arduino RC522 vào máy tính, vào tab **🏭 Trạm Nạp Xưởng & NFC** → bấm **⚡ Nạp Firmware + Đọc ID** → bấm **📡 Ghi thẻ NFC** → chạm thẻ NTAG21x là xong 1 máy!

## 4. Hoặc dùng CLI siêu tốc
```bash
python tools/factory/provision_cli.py --esp-port COM5 --nfc-port COM6 --customer "Anh Minh" --order "FX-001"
```
