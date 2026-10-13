// ═══════════════════════════════════════════════════════════════════════
//  WebDashboard.cpp — Web server implementation for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "WebDashboard.h"
#include "Display.h"
#include "HardwareManager.h"
#include "Theme.h"
#include "WeatherService.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

WebDashboard webDashboard;

static File s_wpUploadFile;

// HTML/CSS/JS Dashboard page stored in PROGMEM
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>CYD SmartDesk V2 Dashboard</title>
<style>
:root {
  --bg: #070B14;
  --card: rgba(16, 23, 38, 0.85);
  --card-border: rgba(56, 94, 144, 0.4);
  --accent: #00E5FF;
  --accent-glow: rgba(0, 229, 255, 0.25);
  --text: #F8FAFC;
  --text-dim: #8899B0;
  --gold: #FFD60A;
  --danger: #FF3B30;
  --success: #00E676;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
body { background: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; display: flex; flex-direction: column; align-items: center; }
.container { width: 100%; max-width: 480px; display: flex; flex-direction: column; gap: 16px; }

/* Header */
.header { display: flex; justify-content: space-between; align-items: center; padding: 12px 16px; background: var(--card); border: 1px solid var(--card-border); border-radius: 16px; backdrop-filter: blur(12px); }
.brand { display: flex; align-items: center; gap: 10px; }
.brand-icon { width: 34px; height: 34px; border-radius: 10px; background: linear-gradient(135deg, #00E5FF, #7000FF); display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 18px; color: #fff; box-shadow: 0 0 12px var(--accent-glow); }
.brand-title { font-size: 16px; font-weight: 700; color: #fff; letter-spacing: 0.5px; }
.brand-sub { font-size: 11px; color: var(--accent); }
.badge-online { background: rgba(0, 230, 118, 0.15); color: var(--success); border: 1px solid rgba(0, 230, 118, 0.3); font-size: 11px; padding: 4px 10px; border-radius: 20px; font-weight: 600; display: flex; align-items: center; gap: 6px; }
.badge-online::before { content: ""; width: 6px; height: 6px; background: var(--success); border-radius: 50%; box-shadow: 0 0 6px var(--success); }

/* Card */
.card { background: var(--card); border: 1px solid var(--card-border); border-radius: 18px; padding: 18px; backdrop-filter: blur(12px); box-shadow: 0 8px 24px rgba(0,0,0,0.3); }
.card-title { font-size: 14px; font-weight: 700; color: var(--accent); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 14px; display: flex; align-items: center; gap: 8px; }

/* CYD Screen Mockup & Preview */
.screen-frame { width: 100%; aspect-ratio: 4 / 3; max-height: 220px; background: #000; border-radius: 12px; border: 2px solid #2A364F; position: relative; overflow: hidden; display: flex; align-items: center; justify-content: center; margin-bottom: 12px; box-shadow: inset 0 0 20px rgba(0,0,0,0.8); }
.screen-preview { width: 100%; height: 100%; object-fit: cover; display: block; }
.screen-overlay { position: absolute; inset: 0; pointer-events: none; border-radius: 10px; border: 1px solid rgba(255,255,255,0.08); background: linear-gradient(180deg, rgba(255,255,255,0.06) 0%, transparent 40%); display: flex; flex-direction: column; justify-content: space-between; padding: 8px; }
.overlay-clock { font-size: 26px; font-weight: 800; color: #fff; text-shadow: 0 0 10px #00E5FF; text-align: center; }
.overlay-dock { align-self: center; background: rgba(12, 16, 26, 0.85); border: 1px solid rgba(56, 94, 144, 0.4); border-radius: 12px; padding: 3px 12px; font-size: 10px; color: var(--accent); }

/* File Picker & Buttons */
.upload-btn-wrap { position: relative; overflow: hidden; display: inline-block; width: 100%; }
.file-input { position: absolute; left: 0; top: 0; opacity: 0; width: 100%; height: 100%; cursor: pointer; }
.btn { width: 100%; padding: 13px; border-radius: 12px; border: none; font-size: 14px; font-weight: 600; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px; transition: all 0.2s; }
.btn-primary { background: linear-gradient(135deg, #00B4D8, #0077B6); color: #fff; box-shadow: 0 4px 14px rgba(0, 180, 216, 0.3); }
.btn-primary:active { transform: scale(0.98); opacity: 0.9; }
.btn-accent { background: linear-gradient(135deg, #00E5FF, #00A3FF); color: #070B14; font-weight: 700; box-shadow: 0 4px 14px var(--accent-glow); }
.btn-accent:disabled { background: #1C2638; color: #475569; box-shadow: none; cursor: not-allowed; }
.btn-danger { background: rgba(255, 59, 48, 0.15); color: var(--danger); border: 1px solid rgba(255, 59, 48, 0.3); margin-top: 8px; }
.btn-danger:active { background: rgba(255, 59, 48, 0.25); }

/* Action Grid */
.grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.btn-chip { background: #131C2D; border: 1px solid var(--card-border); color: var(--text); padding: 10px 6px; border-radius: 10px; font-size: 12px; font-weight: 500; cursor: pointer; text-align: center; }
.btn-chip.active { background: var(--accent); color: #070B14; font-weight: 700; border-color: var(--accent); box-shadow: 0 0 10px var(--accent-glow); }

/* Slider */
.slider-wrap { margin-top: 10px; display: flex; flex-direction: column; gap: 6px; }
.slider-hdr { display: flex; justify-content: space-between; font-size: 12px; color: var(--text-dim); }
input[type=range] { width: 100%; accent-color: var(--accent); height: 6px; border-radius: 3px; background: #1C2638; outline: none; }

/* Toast Notification */
#toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%) translateY(100px); background: #162238; border: 1px solid var(--accent); color: #fff; padding: 12px 22px; border-radius: 30px; font-size: 13px; font-weight: 600; box-shadow: 0 8px 24px rgba(0,0,0,0.6); transition: transform 0.3s cubic-bezier(0.18, 0.89, 0.32, 1.28); z-index: 999; pointer-events: none; }
#toast.show { transform: translateX(-50%) translateY(0); }
</style>
</head>
<body>
<div class="container">

  <!-- Header -->
  <div class="header">
    <div class="brand">
      <div class="brand-icon">V2</div>
      <div>
        <div class="brand-title">CYD SmartDesk</div>
        <div class="brand-sub">Bàn làm việc thông minh 2.8"</div>
      </div>
    </div>
    <div class="badge-online" id="statusBadge">Trực tuyến</div>
  </div>

  <!-- Wallpaper Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><path d="M21 15l-5-5L5 21"/></svg>
      Tùy chỉnh hình nền (Wallpaper)
    </div>

    <!-- Live Preview Frame -->
    <div class="screen-frame">
      <img id="previewImg" class="screen-preview" src="/current-wallpaper" onerror="this.src='data:image/svg+xml;utf8,<svg xmlns=\'http://www.w3.org/2000/svg\' width=\'320\' height=\'240\'><rect fill=\'%23070B14\' width=\'320\' height=\'240\'/><circle cx=\'280\' cy=\'30\' r=\'50\' fill=\'%232A4C7A\'/><circle cx=\'45\' cy=\'20\' r=\'30\' fill=\'%23E879F9\'/><text fill=\'%23475569\' x=\'160\' y=\'130\' font-size=\'14\' text-anchor=\'middle\' font-family=\'sans-serif\'>Mặc định (Chưa tải ảnh)</text></svg>'">
      <div class="screen-overlay">
        <div class="overlay-clock">08:30</div>
        <div class="overlay-dock">⊞ Smart Dock</div>
      </div>
    </div>

    <div id="fileInfo" style="font-size: 11px; color: var(--text-dim); text-align: center; margin-bottom: 12px;">
      Chọn bất kỳ ảnh nào trên điện thoại — Trình duyệt sẽ tự động cắt & tối ưu chuẩn 320×240.
    </div>

    <div style="display: flex; flex-direction: column; gap: 8px;">
      <div class="upload-btn-wrap">
        <button class="btn btn-primary">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
          Chọn ảnh từ điện thoại / máy tính
        </button>
        <input type="file" id="wpInput" class="file-input" accept="image/*">
      </div>

      <button id="applyBtn" class="btn btn-accent" disabled onclick="uploadWallpaper()">
        Áp dụng hình nền cho màn hình
      </button>

      <button class="btn btn-danger" onclick="deleteWallpaper()">
        Khôi phục hình nền mặc định
      </button>
    </div>
  </div>

  <!-- Remote Control Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>
      Điều khiển màn hình từ xa
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;">Chuyển trang hiển thị:</div>
    <div class="grid-4" id="pageButtons">
      <button class="btn-chip" onclick="switchPage(0)">Đồng hồ</button>
      <button class="btn-chip" onclick="switchPage(1)">Lịch âm</button>
      <button class="btn-chip" onclick="switchPage(2)">Tài chính</button>
      <button class="btn-chip" onclick="switchPage(3)">Pomodoro</button>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin: 14px 0 8px;">Giao diện (Theme):</div>
    <div class="grid-2" id="themeButtons">
      <button class="btn-chip" onclick="switchTheme(0)">Cyber Neon</button>
      <button class="btn-chip" onclick="switchTheme(1)">Minimal Obsidian</button>
      <button class="btn-chip" onclick="switchTheme(2)">Sunset Amber</button>
      <button class="btn-chip" onclick="switchTheme(3)">Cyberpunk</button>
    </div>

    <div class="slider-wrap">
      <div class="slider-hdr">
        <span>Độ sáng màn hình</span>
        <span id="brightVal">80%</span>
      </div>
      <input type="range" id="brightSlider" min="10" max="100" value="80" oninput="changeBrightness(this.value)">
    </div>
  </div>

</div>

<div id="toast">Thông báo</div>

<script>
let currentBlob = null;

function showToast(msg, isSuccess = true) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.borderColor = isSuccess ? 'var(--accent)' : 'var(--danger)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

// Client-side auto-crop & resize to 320x240 JPEG
document.getElementById('wpInput').addEventListener('change', function(e) {
  const file = e.target.files[0];
  if (!file) return;

  const reader = new FileReader();
  reader.onload = function(evt) {
    const img = new Image();
    img.onload = function() {
      const canvas = document.createElement('canvas');
      canvas.width = 320;
      canvas.height = 240;
      const ctx = canvas.getContext('2d');

      // Cover aspect ratio
      const scale = Math.max(320 / img.width, 240 / img.height);
      const w = img.width * scale;
      const h = img.height * scale;
      const x = (320 - w) / 2;
      const y = (240 - h) / 2;

      ctx.drawImage(img, x, y, w, h);

      canvas.toBlob(function(blob) {
        currentBlob = blob;
        document.getElementById('previewImg').src = canvas.toDataURL('image/jpeg', 0.85);
        document.getElementById('fileInfo').innerHTML = `Đã tối ưu: <b>${(blob.size / 1024).toFixed(1)} KB</b> (320×240) • Sẵn sàng tải lên!`;
        document.getElementById('fileInfo').style.color = 'var(--accent)';
        document.getElementById('applyBtn').disabled = false;
        showToast('Đã chuẩn bị ảnh 320×240 thành công!');
      }, 'image/jpeg', 0.85);
    };
    img.src = evt.target.result;
  };
  reader.readAsDataURL(file);
});

async function uploadWallpaper() {
  if (!currentBlob) return;
  const btn = document.getElementById('applyBtn');
  btn.disabled = true;
  btn.textContent = 'Đang tải lên...';
  showToast('Đang truyền ảnh tới CYD qua WiFi...');

  const formData = new FormData();
  formData.append('wallpaper', currentBlob, 'wallpaper.jpg');

  try {
    const res = await fetch('/upload-wallpaper', { method: 'POST', body: formData });
    if (res.ok) {
      showToast('Đã áp dụng hình nền thành công!');
      btn.textContent = 'Áp dụng hình nền cho màn hình';
      btn.disabled = false;
    } else {
      showToast('Lỗi khi tải ảnh lên!', false);
      btn.disabled = false;
    }
  } catch (err) {
    showToast('Lỗi kết nối WiFi!', false);
    btn.disabled = false;
  }
}

async function deleteWallpaper() {
  if (!confirm('Bạn có chắc muốn xóa ảnh nền và khôi phục hiệu ứng mặc định?')) return;
  try {
    const res = await fetch('/delete-wallpaper', { method: 'POST' });
    if (res.ok) {
      showToast('Đã khôi phục hình nền mặc định!');
      document.getElementById('previewImg').src = '';
      document.getElementById('applyBtn').disabled = true;
      document.getElementById('fileInfo').textContent = 'Đang sử dụng hiệu ứng mặc định (Obsidian + Neon Blobs).';
    }
  } catch (err) {
    showToast('Lỗi khi xóa ảnh!', false);
  }
}

async function switchPage(pageId) {
  await fetch(`/api/page?id=${pageId}`);
  highlightPage(pageId);
  showToast(`Đã chuyển sang trang ${pageId}`);
}

async function switchTheme(themeId) {
  await fetch(`/api/theme?id=${themeId}`);
  highlightTheme(themeId);
  showToast(`Đã chuyển Theme ${themeId}`);
}

let brightTimeout = null;
function changeBrightness(val) {
  document.getElementById('brightVal').textContent = val + '%';
  clearTimeout(brightTimeout);
  brightTimeout = setTimeout(() => {
    fetch(`/api/brightness?val=${val}`);
  }, 200);
}

function highlightPage(id) {
  const btns = document.querySelectorAll('#pageButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === id));
}

function highlightTheme(id) {
  const btns = document.querySelectorAll('#themeButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === id));
}

async function fetchStatus() {
  try {
    const res = await fetch('/status');
    const data = await res.json();
    highlightPage(data.page);
    highlightTheme(data.theme);
    document.getElementById('brightSlider').value = data.brightness;
    document.getElementById('brightVal').textContent = data.brightness + '%';
  } catch (e) {}
}

fetchStatus();
</script>
</body>
</html>
)rawliteral";

WebDashboard::WebDashboard() : _server(80), _running(false) {}

void WebDashboard::begin() {
    if (_running) return;
    _setupRoutes();
    _server.begin();
    _running = true;
    Serial.printf("[WebDashboard] HTTP Server started at http://%s/\n", WiFi.localIP().toString().c_str());
}

void WebDashboard::update() {
    // ESPAsyncWebServer is asynchronous, no loop polling needed
}

void WebDashboard::_setupRoutes() {
    // ── GET /: Serve mobile dashboard ──────────────────────────────
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // ── GET /current-wallpaper: Serve current image from LittleFS ──
    _server.on("/current-wallpaper", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/wallpaper.jpg")) {
            request->send(LittleFS, "/wallpaper.jpg", "image/jpeg");
        } else {
            request->send(404, "text/plain", "No wallpaper set");
        }
    });

    // ── POST /upload-wallpaper: Receive & save 320x240 JPEG ────────
    _server.on("/upload-wallpaper", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"success\":true}");
        },
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                Serial.printf("[WebDashboard] Start receiving wallpaper upload: %s\n", filename.c_str());
                if (LittleFS.exists("/wallpaper.jpg")) {
                    LittleFS.remove("/wallpaper.jpg");
                }
                s_wpUploadFile = LittleFS.open("/wallpaper.jpg", "w");
            }
            if (s_wpUploadFile) {
                s_wpUploadFile.write(data, len);
            }
            if (final) {
                if (s_wpUploadFile) {
                    s_wpUploadFile.close();
                    Serial.printf("[WebDashboard] Wallpaper saved! Total bytes: %u\n", index + len);
                }
                display.notifyWallpaperUpdated();
            }
        }
    );

    // ── POST /delete-wallpaper: Remove custom wallpaper ────────────
    _server.on("/delete-wallpaper", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/wallpaper.jpg")) {
            LittleFS.remove("/wallpaper.jpg");
            display.notifyWallpaperUpdated();
            Serial.println("[WebDashboard] Wallpaper deleted, reverted to default");
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Wallpaper deleted\"}");
        } else {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"No wallpaper was active\"}");
        }
    });

    // ── GET /status: System telemetry ─────────────────────────────
    _server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["ip"]           = WiFi.localIP().toString();
        doc["rssi"]         = WiFi.RSSI();
        doc["freeHeap"]     = ESP.getFreeHeap();
        doc["page"]         = display.getPage();
        doc["theme"]        = getThemeIndex();
        doc["themeName"]    = getThemeName();
        doc["brightness"]   = hardware.getBacklight();
        doc["hasWallpaper"] = LittleFS.exists("/wallpaper.jpg");
        const WeatherData& w = weather.getData();
        doc["city"]         = w.city;
        doc["temp"]         = w.temp;

        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    // ── GET /api/page?id=X ─────────────────────────────────────────
    _server.on("/api/page", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (request->hasParam("id")) {
            uint8_t p = (uint8_t)request->getParam("id")->value().toInt();
            display.setPage(p);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing id\"}");
        }
    });

    // ── GET /api/theme?id=X ────────────────────────────────────────
    _server.on("/api/theme", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (request->hasParam("id")) {
            uint8_t id = (uint8_t)request->getParam("id")->value().toInt();
            applyThemePreset(id);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing id\"}");
        }
    });

    // ── GET /api/brightness?val=X ──────────────────────────────────
    _server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (request->hasParam("val")) {
            uint8_t val = (uint8_t)request->getParam("val")->value().toInt();
            hardware.setAutoBrightness(false);
            hardware.setBacklight(val);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing val\"}");
        }
    });
}
