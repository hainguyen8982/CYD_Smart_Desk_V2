"""
FabX SmartDesk — Local Factory & Fleet Manager App
--------------------------------------------------
Chạy:
  python tools/factory/smartdesk_manager.py

Tính năng:
  - Giao diện quản lý chạy local trên http://127.0.0.1:8765 (tự mở trình duyệt)
  - Lưu trữ offline bằng SQLite (smartdesk_local.db) + Xuất CSV Excel
  - Nạp xưởng 1 chạm: Auto-flash ESP32 + Đọc MAC/ID + Ghi thẻ NFC qua Arduino RC522 + In tem đáy
  - Đồng bộ 2 chiều với trang Cloud Admin (https://smartdesk.fabx.vn/admin)
  - Upload & phát hành Firmware Cloud OTA trực tiếp từ máy tính lên smartdesk.fabx.vn
"""

import csv
import datetime
import io
import json
import os
import sqlite3
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import provision_cli as prov

HOST = "127.0.0.1"
PORT = 8765

HTML_APP = r"""<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FabX SmartDesk — Local Factory & Fleet Manager</title>
<style>
  :root{--bg:#060a10;--card:#0e1520;--border:#1e2f42;--accent:#64d2ff;--ok:#22c55e;--warn:#f59e0b;--err:#ef4444;--text:#eef2f7;--dim:#8b9bb0}
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;padding:20px}
  .wrap{max-width:1180px;margin:0 auto}
  header{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px;margin-bottom:18px;padding-bottom:14px;border-bottom:1px solid var(--border)}
  .brand{font-size:20px;font-weight:800}.brand span{color:var(--accent)}
  .badge{font-size:11px;background:rgba(34,197,94,.15);color:#4ade80;border:1px solid rgba(34,197,94,.3);padding:3px 9px;border-radius:99px;font-weight:700}
  .tabs{display:flex;gap:8px;margin-bottom:16px;flex-wrap:wrap}
  .tab{padding:10px 16px;border-radius:10px;background:var(--card);border:1px solid var(--border);color:var(--dim);font-weight:700;font-size:13px;cursor:pointer}
  .tab.active{background:rgba(100,210,255,.14);border-color:var(--accent);color:var(--accent)}
  .card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:18px;margin-bottom:16px}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
  .grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px}
  @media(max-width:780px){.grid2,.grid3{grid-template-columns:1fr}}
  .field{margin-bottom:10px}
  .field label{display:block;font-size:12px;color:var(--dim);margin-bottom:4px}
  input,select,textarea{width:100%;background:#070c14;border:1px solid var(--border);color:var(--text);border-radius:9px;padding:9px 11px;font-size:13px;font-family:inherit}
  .btn{background:var(--accent);color:#000;border:none;border-radius:10px;padding:9px 14px;font-weight:700;font-size:13px;cursor:pointer;display:inline-flex;align-items:center;gap:6px}
  .btn:disabled{opacity:.5;cursor:not-allowed}
  .btn-ok{background:#22c55e;color:#000}
  .btn-outline{background:transparent;color:var(--accent);border:1px solid rgba(100,210,255,.4)}
  .btn-danger{background:rgba(239,68,68,.15);color:#f87171;border:1px solid rgba(239,68,68,.35)}
  .btn-sm{padding:5px 10px;font-size:12px;border-radius:8px}
  .log{background:#05080d;border:1px solid var(--border);border-radius:10px;padding:12px;font-family:Consolas,monospace;font-size:12px;color:#a7f3d0;min-height:110px;max-height:220px;overflow:auto;white-space:pre-wrap}
  .result-box{background:rgba(100,210,255,.08);border:1px solid rgba(100,210,255,.35);border-radius:12px;padding:14px;display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:12px}
  table{width:100%;border-collapse:collapse;font-size:13px}
  th,td{text-align:left;padding:9px 8px;border-bottom:1px solid rgba(255,255,255,.06);vertical-align:middle}
  th{color:var(--dim);font-size:11px;text-transform:uppercase}
  .pill{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11px;font-weight:700}
  .pill-ok{background:rgba(34,197,94,.16);color:#4ade80}
  .pill-off{background:rgba(139,155,176,.14);color:var(--dim)}
  .pill-beta{background:rgba(245,158,11,.16);color:#fbbf24}
  .toast{position:fixed;bottom:20px;right:20px;background:#132030;border:1px solid var(--accent);color:#fff;padding:11px 16px;border-radius:10px;font-size:13px;display:none;z-index:99}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div style="display:flex;align-items:center;gap:10px">
      <div class="brand">FabX <span>SmartDesk</span> Local Manager</div>
      <span class="badge">● LOCAL OFFLINE + CLOUD SYNC</span>
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="btn btn-outline btn-sm" onclick="syncCloud()">☁️ Đồng bộ Cloud</button>
      <a class="btn btn-outline btn-sm" href="/api/export.csv" style="text-decoration:none">📥 Xuất Excel CSV</a>
    </div>
  </header>

  <div class="tabs">
    <button class="tab active" id="tb1" onclick="goTab(1)">🏭 Trạm Nạp Xưởng & NFC</button>
    <button class="tab" id="tb2" onclick="goTab(2)">📦 Kho máy Local (<span id="cntDev">0</span>)</button>
    <button class="tab" id="tb3" onclick="goTab(3)">🚀 Phát hành Cloud OTA</button>
    <button class="tab" id="tb4" onclick="goTab(4)">⚙️ Kết nối Cloud Admin</button>
  </div>

  <!-- TAB 1: FACTORY STATION -->
  <div id="tab1">
    <div class="card">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px">
        <h3>⚡ Nạp Firmware ESP32 + Ghi thẻ NFC (Arduino RC522)</h3>
        <button class="btn btn-outline btn-sm" onclick="refreshPorts()">🔄 Quét lại cổng COM</button>
      </div>
      <div class="grid3">
        <div class="field"><label>Cổng COM ESP32 (SmartDesk)</label><select id="espPort"></select></div>
        <div class="field"><label>Cổng COM Arduino + RC522</label><select id="nfcPort"></select></div>
        <div class="field"><label>Mã phần cứng (hw)</label>
          <select id="facHw">
            <option value="nicemcu_2.8ips">nicemcu_2.8ips</option>
            <option value="cyd_2.8_resistive">cyd_2.8_resistive</option>
          </select>
        </div>
      </div>
      <div class="grid3">
        <div class="field"><label>Tên khách / Lô sản xuất (tùy chọn)</label><input id="facCustomer" placeholder="VD: Lô T11 / Anh Minh"></div>
        <div class="field"><label>Mã đơn hàng (tùy chọn)</label><input id="facOrder" placeholder="VD: FX-2026-001"></div>
        <div class="field"><label>Ghi chú</label><input id="facNote" placeholder="VD: Vỏ xám Fuzzy, combo EMO"></div>
      </div>
      <div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:6px">
        <button class="btn" id="btnFlash" onclick="runFactory(false)">⚡ Nạp Firmware + Đọc ID</button>
        <button class="btn btn-outline" id="btnReadOnly" onclick="runFactory(true)">🔍 Chỉ đọc ID từ ESP32</button>
        <button class="btn btn-ok" id="btnWriteNfc" onclick="writeNfcOnly()" disabled>📡 Ghi thẻ NFC (RC522)</button>
        <button class="btn btn-outline" id="btnPrintLabel" onclick="printLabel()" disabled>🖨️ In tem đáy máy</button>
      </div>

      <div id="lastResult" class="result-box" style="display:none;margin-top:14px">
        <div>
          <div style="font-size:12px;color:var(--dim)">MÁY VỪA NẠP</div>
          <div style="font-size:22px;font-weight:800;color:var(--accent)" id="resId">—</div>
          <div style="font-size:12px;color:var(--dim)" id="resUrl">—</div>
        </div>
        <div style="text-align:right;font-size:12px;color:var(--dim)" id="resMeta"></div>
      </div>

      <div style="margin-top:12px">
        <div style="font-size:12px;color:var(--dim);margin-bottom:4px">Nhật ký thao tác:</div>
        <div class="log" id="facLog">Sẵn sàng. Cắm ESP32 & Arduino RC522 rồi bấm "Quét lại cổng COM".</div>
      </div>
    </div>
  </div>

  <!-- TAB 2: LOCAL INVENTORY -->
  <div id="tab2" style="display:none">
    <div class="card">
      <div style="display:flex;gap:10px;margin-bottom:12px;flex-wrap:wrap">
        <input id="qDev" placeholder="Tìm ID, khách hàng, mã đơn, SĐT, ghi chú..." style="flex:1;min-width:240px" oninput="renderDevices()">
        <button class="btn btn-outline btn-sm" onclick="loadDevices()">🔄 Tải lại</button>
      </div>
      <div style="overflow-x:auto">
        <table>
          <thead>
            <tr><th>Device ID / NFC</th><th>Cloud / FW</th><th>Khách hàng & Đơn hàng</th><th>Bảo hành / Ghi chú</th><th>Thao tác</th></tr>
          </thead>
          <tbody id="devBody"></tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- TAB 3: CLOUD OTA PUBLISH -->
  <div id="tab3" style="display:none">
    <div class="card">
      <h3 style="margin-bottom:12px">🚀 Đẩy bản Firmware mới lên smartdesk.fabx.vn (Cloud OTA)</h3>
      <p style="font-size:12px;color:var(--dim);margin-bottom:14px">Tự động lấy file <code>.pio/build/nicemcu_32s_2_8ips/firmware.bin</code> vừa build (hoặc chọn file bất kỳ) và phát hành cho khách hàng.</p>
      <div class="grid2">
        <div class="field"><label>Phiên bản (vd 2.5.0)</label><input id="otaVer" value="2.5.0"></div>
        <div class="field"><label>Mã phần cứng (hw)</label>
          <select id="otaHw">
            <option value="nicemcu_2.8ips">nicemcu_2.8ips</option>
            <option value="cyd_2.8_resistive">cyd_2.8_resistive</option>
          </select>
        </div>
      </div>
      <div class="field"><label>Ghi chú cập nhật (Changelog)</label><input id="otaLog" placeholder="VD: Thêm Cloud OTA 1 chạm, cải thiện lịch âm..."></div>
      <div class="field">
        <label style="display:flex;align-items:center;gap:8px;color:#fbbf24;cursor:pointer">
          <input type="checkbox" id="otaBeta" style="width:auto"> Phát hành kênh Beta trước (chỉ máy test thấy)
        </label>
      </div>
      <button class="btn" id="btnPushOta" onclick="pushBuiltFirmware()">⬆️ Upload firmware.bin vừa build lên Cloud</button>
    </div>
  </div>

  <!-- TAB 4: SETTINGS -->
  <div id="tab4" style="display:none">
    <div class="card" style="max-width:540px">
      <h3 style="margin-bottom:12px">⚙️ Cấu hình kết nối Cloud Admin</h3>
      <div class="field"><label>Cloud Server URL</label><input id="cfgUrl" placeholder="https://smartdesk.fabx.vn"></div>
      <div class="field"><label>Admin Key (ADMIN_KEY trên server)</label><input id="cfgKey" type="password" placeholder="Nhập ADMIN_KEY..."></div>
      <button class="btn" onclick="saveSettings()">💾 Lưu cấu hình</button>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<script>
let devices = [];
let currentUnit = null;

function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.display = 'block';
  clearTimeout(el._t);
  el._t = setTimeout(() => el.style.display = 'none', 3200);
}
function log(msg) {
  const el = document.getElementById('facLog');
  el.textContent += '\n' + msg;
  el.scrollTop = el.scrollHeight;
}
function esc(s) {
  return String(s || '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function goTab(n) {
  for (let i = 1; i <= 4; i++) {
    document.getElementById('tab' + i).style.display = i === n ? 'block' : 'none';
    document.getElementById('tb' + i).classList.toggle('active', i === n);
  }
  if (n === 2) loadDevices();
}

async function refreshPorts() {
  const r = await fetch('/api/ports');
  const d = await r.json();
  const opts = '<option value="">-- Chọn cổng COM --</option>' +
    d.ports.map(p => `<option value="${esc(p.device)}">${esc(p.device)} — ${esc(p.description)}</option>`).join('');
  document.getElementById('espPort').innerHTML = opts;
  document.getElementById('nfcPort').innerHTML = opts;
  toast(`Tìm thấy ${d.ports.length} cổng COM`);
}

async function runFactory(readOnly) {
  const espPort = document.getElementById('espPort').value;
  if (!espPort) return toast('Hãy chọn cổng COM của ESP32!');
  document.getElementById('btnFlash').disabled = true;
  document.getElementById('btnReadOnly').disabled = true;
  document.getElementById('facLog').textContent = readOnly ? `Đang đọc MAC từ ${espPort}...` : `Đang nạp Firmware lên ${espPort} (~10-15 giây)...`;
  try {
    const r = await fetch('/api/factory/provision', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({
        esp_port: espPort,
        read_only: readOnly,
        hw: document.getElementById('facHw').value,
        customer: document.getElementById('facCustomer').value.trim(),
        order_id: document.getElementById('facOrder').value.trim(),
        note: document.getElementById('facNote').value.trim(),
      })
    });
    const d = await r.json();
    if (!d.ok) throw new Error(d.error || 'Thất bại');
    currentUnit = d;
    document.getElementById('lastResult').style.display = 'flex';
    document.getElementById('resId').textContent = d.id;
    document.getElementById('resUrl').textContent = d.nfc_url;
    document.getElementById('resMeta').textContent = `MAC: ${d.mac}`;
    document.getElementById('btnWriteNfc').disabled = false;
    document.getElementById('btnPrintLabel').disabled = false;
    log(`✅ Thành công! ID=${d.id} | MAC=${d.mac}\n   Link NFC: ${d.nfc_url}`);
    toast('Đã lưu máy ' + d.id + ' vào kho local!');
    loadDevices();
  } catch (e) {
    log('❌ Lỗi: ' + e.message);
    toast('Lỗi: ' + e.message);
  } finally {
    document.getElementById('btnFlash').disabled = false;
    document.getElementById('btnReadOnly').disabled = false;
  }
}

async function writeNfcOnly() {
  if (!currentUnit) return;
  const nfcPort = document.getElementById('nfcPort').value;
  if (!nfcPort) return toast('Hãy chọn cổng COM của Arduino + RC522!');
  document.getElementById('btnWriteNfc').disabled = true;
  log(`📡 Đang gửi link ${currentUnit.nfc_url} xuống ${nfcPort}... Hãy chạm thẻ NTAG21x vào đầu đọc!`);
  try {
    const r = await fetch('/api/factory/write-nfc', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ nfc_port: nfcPort, id: currentUnit.id, url: currentUnit.nfc_url })
    });
    const d = await r.json();
    if (!d.ok) throw new Error(d.error || 'Lỗi ghi NFC');
    log(`✅ Ghi & kiểm tra thẻ NFC thành công! UID=${d.nfc_uid}`);
    toast('Ghi thẻ NFC thành công!');
    loadDevices();
  } catch (e) {
    log('❌ Lỗi NFC: ' + e.message);
    toast(e.message);
  } finally {
    document.getElementById('btnWriteNfc').disabled = false;
  }
}

function printLabel() {
  if (!currentUnit) return;
  const w = window.open('', '_blank', 'width=360,height=260');
  w.document.write(`<!DOCTYPE html><html><body style="font-family:sans-serif;text-align:center;padding:12px;margin:0">
    <div style="border:2px solid #000;border-radius:8px;padding:10px;display:inline-block;width:240px">
      <div style="font-size:11px;font-weight:700;letter-spacing:1px">FABX SMARTDESK V2</div>
      <div style="font-size:24px;font-weight:900;margin:4px 0;letter-spacing:2px">${esc(currentUnit.id)}</div>
      <div style="font-size:10px;color:#333">${esc(currentUnit.nfc_url)}</div>
      <div style="font-size:9px;color:#666;margin-top:4px">MAC: ${esc(currentUnit.mac)}</div>
    </div>
    <script>window.onload=function(){window.print();}<\/script>
  </body></html>`);
  w.document.close();
}

async function loadDevices() {
  const r = await fetch('/api/devices');
  const d = await r.json();
  devices = d.devices || [];
  document.getElementById('cntDev').textContent = devices.length;
  renderDevices();
}

function renderDevices() {
  const q = (document.getElementById('qDev').value || '').toLowerCase();
  const rows = devices.filter(d => !q || `${d.id} ${d.customer} ${d.order_id} ${d.phone} ${d.note}`.toLowerCase().includes(q));
  document.getElementById('devBody').innerHTML = rows.map(d => {
    const online = d.cloud_updated_at && (Date.now() - d.cloud_updated_at < 86400000);
    return `<tr>
      <td><b>${esc(d.id)}</b> ${d.beta ? '<span class="pill pill-beta">BETA</span>' : ''}
        <div style="font-size:11px;color:var(--dim)">${esc(d.nfc_url || '')}</div>
        ${d.nfc_uid ? `<div style="font-size:10px;color:#4ade80">NFC UID: ${esc(d.nfc_uid)}</div>` : ''}
      </td>
      <td><span class="pill ${online ? 'pill-ok' : 'pill-off'}">${online ? '● Online' : '○ Offline'}</span>
        <div style="font-size:11px;color:var(--dim)">v${esc(d.fw || '?')} • ${esc(d.ip || '—')}</div>
      </td>
      <td><b>${esc(d.customer || '—')}</b><div style="font-size:11px;color:var(--dim)">${esc(d.order_id || '')} ${d.sold_at ? '• ' + esc(d.sold_at) : ''}</div></td>
      <td style="font-size:12px;color:var(--dim)">BH ${d.warranty_months || 6} tháng<br>${esc(d.note || '')}</td>
      <td style="white-space:nowrap">
        <button class="btn btn-outline btn-sm" onclick="editDevicePrompt('${esc(d.id)}')">Sửa</button>
        <button class="btn btn-danger btn-sm" onclick="deleteLocal('${esc(d.id)}')">✕</button>
      </td>
    </tr>`;
  }).join('') || '<tr><td colspan="5" style="text-align:center;color:var(--dim);padding:20px">Chưa có thiết bị nào</td></tr>';
}

async function editDevicePrompt(id) {
  const d = devices.find(x => x.id === id);
  if (!d) return;
  const customer = prompt('Tên khách hàng:', d.customer || '');
  if (customer === null) return;
  const order_id = prompt('Mã đơn hàng:', d.order_id || '') ?? d.order_id;
  const note = prompt('Ghi chú:', d.note || '') ?? d.note;
  const beta = confirm('Bật kênh Beta cho máy ' + id + '?\n(OK = Beta, Cancel = Stable)');
  await fetch('/api/devices/' + encodeURIComponent(id), {
    method: 'PATCH',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ customer, order_id, note, beta: beta ? 1 : 0 })
  });
  toast('Đã cập nhật ' + id);
  loadDevices();
}

async function deleteLocal(id) {
  if (!confirm('Xóa máy ' + id + ' khỏi kho local?')) return;
  await fetch('/api/devices/' + encodeURIComponent(id), { method: 'DELETE' });
  loadDevices();
}

async function syncCloud() {
  toast('Đang đồng bộ 2 chiều với Cloud...');
  const r = await fetch('/api/sync-cloud', { method: 'POST' });
  const d = await r.json();
  if (d.ok) {
    toast(`✅ Đồng bộ xong! Kéo ${d.pulled} máy, đẩy ${d.pushed} máy.`);
    loadDevices();
  } else {
    toast('❌ ' + (d.error || 'Chưa cấu hình Admin Key'));
  }
}

async function pushBuiltFirmware() {
  const btn = document.getElementById('btnPushOta');
  btn.disabled = true;
  btn.textContent = '⏳ Đang upload lên smartdesk.fabx.vn...';
  try {
    const r = await fetch('/api/cloud/upload-firmware', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({
        version: document.getElementById('otaVer').value.trim(),
        hw: document.getElementById('otaHw').value,
        changelog: document.getElementById('otaLog').value.trim(),
        beta: document.getElementById('otaBeta').checked,
      })
    });
    const d = await r.json();
    if (!d.ok) throw new Error(d.error || 'Upload thất bại');
    toast('✅ Đã phát hành v' + d.release.version + ' lên Cloud OTA!');
  } catch (e) {
    toast('❌ ' + e.message);
  } finally {
    btn.disabled = false;
    btn.textContent = '⬆️ Upload firmware.bin vừa build lên Cloud';
  }
}

async function loadSettings() {
  const r = await fetch('/api/settings');
  const d = await r.json();
  document.getElementById('cfgUrl').value = d.cloud_url || 'https://smartdesk.fabx.vn';
  document.getElementById('cfgKey').value = d.admin_key || '';
}

async function saveSettings() {
  await fetch('/api/settings', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({
      cloud_url: document.getElementById('cfgUrl').value.trim(),
      admin_key: document.getElementById('cfgKey').value.trim(),
    })
  });
  toast('Đã lưu cấu hình Cloud Admin!');
}

refreshPorts();
loadDevices();
loadSettings();
</script>
</body>
</html>
"""


def get_settings(conn):
    rows = dict(conn.execute("SELECT key, value FROM settings").fetchall())
    return {
        "cloud_url": rows.get("cloud_url", prov.DEFAULT_CLOUD_URL),
        "admin_key": rows.get("admin_key", ""),
    }


def list_com_ports():
    try:
        from serial.tools import list_ports
        return [{"device": p.device, "description": p.description} for p in list_ports.comports()]
    except ImportError:
        return []


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send_json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n) if n > 0 else b"{}"
        return json.loads(raw.decode("utf-8"))

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/":
            body = HTML_APP.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if path == "/api/ports":
            return self._send_json(200, {"ok": True, "ports": list_com_ports()})

        if path == "/api/settings":
            conn = prov.init_db()
            s = get_settings(conn)
            conn.close()
            return self._send_json(200, s)

        if path == "/api/devices":
            conn = prov.init_db()
            conn.row_factory = sqlite3.Row
            rows = [dict(r) for r in conn.execute("SELECT * FROM devices ORDER BY created_at DESC").fetchall()]
            conn.close()
            return self._send_json(200, {"ok": True, "devices": rows})

        if path == "/api/export.csv":
            conn = prov.init_db()
            conn.row_factory = sqlite3.Row
            rows = [dict(r) for r in conn.execute("SELECT * FROM devices ORDER BY created_at DESC").fetchall()]
            conn.close()
            buf = io.StringIO()
            cols = ["id", "mac", "nfc_url", "nfc_uid", "hw", "fw", "ip", "customer", "phone", "order_id", "sold_at", "warranty_months", "note", "created_at"]
            w = csv.DictWriter(buf, fieldnames=cols)
            w.writeheader()
            for r in rows:
                w.writerow({c: r.get(c, "") for c in cols})
            data = ("\ufeff" + buf.getvalue()).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/csv; charset=utf-8")
            self.send_header("Content-Disposition", 'attachment; filename="smartdesk_devices.csv"')
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return

        self.send_error(404)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/settings":
            body = self._read_json()
            conn = prov.init_db()
            for k in ("cloud_url", "admin_key"):
                if k in body:
                    conn.execute("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)", (k, str(body[k])))
            conn.commit()
            conn.close()
            return self._send_json(200, {"ok": True})

        if path == "/api/factory/provision":
            body = self._read_json()
            try:
                esp_port = body["esp_port"]
                read_only = bool(body.get("read_only"))
                hw = body.get("hw", "nicemcu_2.8ips")
                if not read_only:
                    prov.flash_esp32(esp_port, prov.DEFAULT_BUILD_DIR)
                mac, dev_id = prov.read_esp32_mac_and_id(esp_port)
                conn = prov.init_db()
                s = get_settings(conn)
                nfc_url = f"{s['cloud_url'].rstrip('/')}/d/{dev_id}"
                now_iso = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                sold_at = datetime.date.today().isoformat() if body.get("customer") or body.get("order_id") else ""
                conn.execute(
                    """
                    INSERT INTO devices (id, mac, nfc_url, hw, customer, order_id, sold_at, note, created_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ON CONFLICT(id) DO UPDATE SET mac=excluded.mac, nfc_url=excluded.nfc_url, hw=excluded.hw,
                      customer=CASE WHEN excluded.customer!='' THEN excluded.customer ELSE devices.customer END,
                      order_id=CASE WHEN excluded.order_id!='' THEN excluded.order_id ELSE devices.order_id END,
                      note=CASE WHEN excluded.note!='' THEN excluded.note ELSE devices.note END
                    """,
                    (dev_id, mac, nfc_url, hw, body.get("customer", ""), body.get("order_id", ""), sold_at, body.get("note", ""), now_iso),
                )
                conn.commit()
                conn.close()
                return self._send_json(200, {"ok": True, "id": dev_id, "mac": mac, "nfc_url": nfc_url})
            except Exception as e:
                return self._send_json(500, {"ok": False, "error": str(e)})

        if path == "/api/factory/write-nfc":
            body = self._read_json()
            try:
                uid = prov.write_nfc_rc522(body["nfc_port"], body["url"])
                conn = prov.init_db()
                conn.execute("UPDATE devices SET nfc_uid=? WHERE id=?", (uid, body["id"]))
                conn.commit()
                conn.close()
                return self._send_json(200, {"ok": True, "nfc_uid": uid})
            except Exception as e:
                return self._send_json(500, {"ok": False, "error": str(e)})

        if path == "/api/sync-cloud":
            conn = prov.init_db()
            s = get_settings(conn)
            if not s["admin_key"]:
                conn.close()
                return self._send_json(400, {"ok": False, "error": "Chưa nhập Admin Key trong tab Cài đặt!"})
            try:
                req = urllib.request.Request(
                    f"{s['cloud_url'].rstrip('/')}/api/admin/overview",
                    headers={"X-Admin-Key": s["admin_key"]},
                )
                with urllib.request.urlopen(req, timeout=10) as resp:
                    cloud = json.loads(resp.read().decode("utf-8"))
                c_devs = cloud.get("devices", {})
                pulled = 0
                for dev_id, cd in c_devs.items():
                    nfc_url = f"{s['cloud_url'].rstrip('/')}/d/{dev_id}"
                    now_iso = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                    conn.execute(
                        """
                        INSERT INTO devices (id, nfc_url, hw, fw, ip, rssi, customer, phone, order_id, sold_at, warranty_months, beta, note, created_at, cloud_updated_at)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT(id) DO UPDATE SET
                            fw=COALESCE(excluded.fw, devices.fw),
                            hw=COALESCE(excluded.hw, devices.hw),
                            ip=COALESCE(excluded.ip, devices.ip),
                            rssi=COALESCE(excluded.rssi, devices.rssi),
                            cloud_updated_at=excluded.cloud_updated_at
                        """,
                        (
                            dev_id, nfc_url, cd.get("hw", "nicemcu_2.8ips"), cd.get("fw", ""), cd.get("ip", ""),
                            cd.get("rssi"), cd.get("customer", ""), cd.get("phone", ""), cd.get("orderId", ""),
                            cd.get("soldAt", ""), cd.get("warrantyMonths", 6), 1 if cd.get("beta") else 0,
                            cd.get("note", ""), now_iso, cd.get("updatedAt", 0),
                        ),
                    )
                    pulled += 1
                conn.commit()

                # Push local metadata up to cloud
                conn.row_factory = sqlite3.Row
                pushed = 0
                for row in conn.execute("SELECT * FROM devices").fetchall():
                    r = dict(row)
                    prov.sync_device_to_cloud(
                        s["cloud_url"], s["admin_key"], r["id"],
                        {
                            "hw": r["hw"] or "nicemcu_2.8ips",
                            "customer": r["customer"] or "",
                            "phone": r["phone"] or "",
                            "orderId": r["order_id"] or "",
                            "soldAt": r["sold_at"] or "",
                            "warrantyMonths": r["warranty_months"] or 6,
                            "beta": bool(r["beta"]),
                            "nfcUid": r["nfc_uid"] or "",
                            "note": r["note"] or "",
                        },
                    )
                    pushed += 1
                conn.close()
                return self._send_json(200, {"ok": True, "pulled": pulled, "pushed": pushed})
            except Exception as e:
                conn.close()
                return self._send_json(500, {"ok": False, "error": str(e)})

        if path == "/api/cloud/upload-firmware":
            body = self._read_json()
            conn = prov.init_db()
            s = get_settings(conn)
            conn.close()
            if not s["admin_key"]:
                return self._send_json(400, {"ok": False, "error": "Chưa nhập Admin Key trong tab Cài đặt!"})
            fw_path = os.path.join(prov.DEFAULT_BUILD_DIR, "firmware.bin")
            if not os.path.exists(fw_path):
                return self._send_json(404, {"ok": False, "error": f"Không tìm thấy {fw_path}"})
            try:
                with open(fw_path, "rb") as f:
                    raw = f.read()
                qs = urllib.parse.urlencode({
                    "version": body.get("version", ""),
                    "hw": body.get("hw", "nicemcu_2.8ips"),
                    "beta": "1" if body.get("beta") else "0",
                    "changelog": body.get("changelog", ""),
                })
                req = urllib.request.Request(
                    f"{s['cloud_url'].rstrip('/')}/api/admin/firmware?{qs}",
                    data=raw,
                    method="POST",
                    headers={
                        "Content-Type": "application/octet-stream",
                        "X-Admin-Key": s["admin_key"],
                    },
                )
                with urllib.request.urlopen(req, timeout=30) as resp:
                    res = json.loads(resp.read().decode("utf-8"))
                return self._send_json(200, res)
            except Exception as e:
                return self._send_json(500, {"ok": False, "error": str(e)})

        self.send_error(404)

    def do_PATCH(self):
        if self.path.startswith("/api/devices/"):
            dev_id = urllib.parse.unquote(self.path[13:]).upper()
            body = self._read_json()
            conn = prov.init_db()
            conn.execute(
                "UPDATE devices SET customer=?, order_id=?, note=?, beta=? WHERE id=?",
                (body.get("customer", ""), body.get("order_id", ""), body.get("note", ""), int(body.get("beta", 0)), dev_id),
            )
            conn.commit()
            conn.close()
            return self._send_json(200, {"ok": True})
        self.send_error(404)

    def do_DELETE(self):
        if self.path.startswith("/api/devices/"):
            dev_id = urllib.parse.unquote(self.path[13:]).upper()
            conn = prov.init_db()
            conn.execute("DELETE FROM devices WHERE id=?", (dev_id,))
            conn.commit()
            conn.close()
            return self._send_json(200, {"ok": True})
        self.send_error(404)


def main():
    prov.init_db().close()
    url = f"http://{HOST}:{PORT}"
    print(f"[FabX SmartDesk Local Manager] Đang chạy tại {url}")
    print("Nhấn Ctrl+C để thoát.")
    threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    srv = ThreadingHTTPServer((HOST, PORT), Handler)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nĐã dừng Local Manager.")


if __name__ == "__main__":
    main()
