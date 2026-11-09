// ═══════════════════════════════════════════════════════════════════════
//  WebDashboard.cpp — Web server implementation for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "WebDashboard.h"
#include "Language.h"
#include "Display.h"
#include "FinanceService.h"
#include "HardwareManager.h"
#include "Theme.h"
#include "WeatherService.h"
#include "DeskPet.h"
#include "DeskMood.h"
#include "EcosystemService.h"
#include "AppIcon.h"
#include "WiFiProfileManager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

WebDashboard webDashboard;

// PWA Web App Manifest JSON
static const char MANIFEST_JSON[] PROGMEM = R"rawliteral({
  "name": "CYD SmartDesk V2",
  "short_name": "SmartDesk",
  "description": "Bảng điều khiển CYD SmartDesk để bàn thông minh",
  "start_url": "/",
  "scope": "/",
  "display": "standalone",
  "orientation": "portrait",
  "background_color": "#070B14",
  "theme_color": "#070B14",
  "icons": [
    {
      "src": "/app-icon.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/app-icon.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any maskable"
    }
  ]
})rawliteral";

// PWA Service Worker script
static const char SW_JS[] PROGMEM = R"rawliteral(
const CACHE_NAME = 'cyd-smartdesk-v2';
self.addEventListener('install', e => {
  self.skipWaiting();
});
self.addEventListener('activate', e => {
  e.waitUntil(self.clients.claim());
});
self.addEventListener('fetch', e => {
  if (e.request.url.includes('/api/') || e.request.url.includes('/status')) {
    return;
  }
  e.respondWith(
    fetch(e.request).catch(() => caches.match(e.request))
  );
});
)rawliteral";

// HTML/CSS/JS Dashboard page stored in PROGMEM
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>CYD SmartDesk V2</title>
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#070B14">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="SmartDesk">
<link rel="apple-touch-icon" href="/app-icon.png">
<link rel="icon" type="image/png" href="/app-icon.png">
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

/* Action Buttons */
.btn { width: 100%; min-height: 44px; padding: 13px 16px; border-radius: 12px; border: none; font-size: 14px; font-weight: 600; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px; transition: all 0.2s; }
.btn-primary { background: linear-gradient(135deg, #00B4D8, #0077B6); color: #fff; box-shadow: 0 4px 14px rgba(0, 180, 216, 0.3); }
.btn-primary:active { transform: scale(0.98); opacity: 0.9; }
.btn-accent { background: linear-gradient(135deg, #00E5FF, #00A3FF); color: #070B14; font-weight: 700; box-shadow: 0 4px 14px var(--accent-glow); }
.btn-accent:disabled { background: #1C2638; color: #475569; box-shadow: none; cursor: not-allowed; }
.btn-outline { background: transparent; border: 1px solid var(--card-border); color: var(--text-dim); }
.btn-outline:active { background: rgba(255, 255, 255, 0.05); }

/* Action Grid */
.grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
.grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.btn-chip { background: #131C2D; border: 1px solid var(--card-border); color: var(--text); padding: 10px 6px; border-radius: 10px; font-size: 12px; font-weight: 500; cursor: pointer; text-align: center; }
.btn-chip.active { background: var(--accent); color: #070B14; font-weight: 700; border-color: var(--accent); box-shadow: 0 0 10px var(--accent-glow); }

/* Select Control */
.select-control { width: 100%; padding: 10px 12px; border-radius: 10px; background: #131C2D; border: 1px solid var(--card-border); color: #fff; font-size: 13px; font-weight: 500; outline: none; }
.select-control:focus { border-color: var(--accent); }

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
        <div class="brand-sub" data-i18n="brand_sub">Màn hình để bàn thông minh</div>
      </div>
    </div>
    <div style="display: flex; gap: 6px; align-items: center; flex-wrap: wrap; justify-content: flex-end;">
      <button id="langBtn" class="btn-chip" style="font-size: 11px; padding: 4px 10px; font-weight: 700; color: var(--accent); border-color: var(--accent);" onclick="toggleWebLang()">🇻🇳 Tiếng Việt</button>
      <div class="badge-online" id="ecoBadge" style="background: rgba(255,255,255,0.06); color: var(--text-dim); border-color: rgba(255,255,255,0.12);" title="Trạng thái kết nối ESP-NOW với EMO Companion">🤖 Độc lập</div>
      <div class="badge-online" id="statusBadge" data-i18n="status_online">Trực tuyến</div>
    </div>
  </div>

  <!-- PWA Install Banner -->
  <div id="pwaBanner" class="card" style="display:none; padding:12px 14px; background:linear-gradient(135deg, rgba(0,229,255,0.1), rgba(112,0,255,0.12)); border:1px solid var(--accent); flex-direction:row; align-items:center; justify-content:space-between; gap:10px;">
    <div style="display:flex; align-items:center; gap:10px;">
      <img src="/app-icon.png" width="38" height="38" style="border-radius:10px; border:1px solid rgba(0,229,255,0.4);" alt="App Icon">
      <div>
        <div style="font-size:13px; font-weight:700; color:#fff;" data-i18n="pwa_title">Cài đặt SmartDesk App</div>
        <div style="font-size:11px; color:var(--text-dim);" data-i18n="pwa_desc">Trải nghiệm toàn màn hình như App xịn</div>
      </div>
    </div>
    <div style="display:flex; gap:6px; align-items:center;">
      <button type="button" class="btn btn-accent" style="width:auto; padding:6px 12px; font-size:11px; border-radius:8px;" onclick="installPWA()" data-i18n="pwa_btn">Cài đặt</button>
      <button type="button" class="btn-chip" style="padding:6px 8px; font-size:11px; border-radius:8px;" onclick="dismissPWA()">✕</button>
    </div>
  </div>

  <!-- 1. Desk Mood & Signboard Section -->
  <div class="card">
    <div class="card-title">
      <span style="font-size: 18px;">🎭</span>
      <span data-i18n="sec_mood_title">Tâm trạng & Biển hiệu Bàn làm việc (Desk Mood)</span>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="sec_pack_sub">Bộ Meme Nhân Vật (Meme Pack):</div>
    <div class="grid-3" id="packButtons" style="margin-bottom: 14px;">
      <button class="btn-chip" onclick="switchPack(0)">🥟 Mochi Guy</button>
      <button class="btn-chip" onclick="switchPack(1)">⭐ Sao Vàng</button>
      <button class="btn-chip" onclick="switchPack(2)">🐰 Thỏ Trắng</button>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="sec_mood_sub">Tâm trạng bàn làm việc (Desk Mood):</div>
    <div class="grid-2" id="moodButtons" style="margin-bottom: 16px;">
      <button class="btn-chip" onclick="switchMood(0)">🟢 Vui vẻ</button>
      <button class="btn-chip" onclick="switchMood(1)">🔴 Tập trung cao</button>
      <button class="btn-chip" onclick="switchMood(2)">⚡ Đang bực mình</button>
      <button class="btn-chip" onclick="switchMood(3)">🟡 Sập nguồn 0%</button>
      <button class="btn-chip" onclick="switchMood(4)">🟣 Đang chán</button>
      <button class="btn-chip" onclick="switchMood(5)">🎉 Cuối tuần / Quẩy</button>
    </div>

    <!-- Custom Status Toggle & Inputs (Tùy chỉnh riêng cho từng trạng thái) -->
    <div style="background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; padding: 14px;">
      <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
        <div style="font-size: 13px; font-weight: 600; color: #fff;">
          <span data-i18n="mood_custom_prefix">Tùy chỉnh riêng cho: </span>
          <span id="curEditMoodName" style="color: var(--accent); font-weight: 700;">🟢 Vui vẻ</span>
        </div>
        <input type="checkbox" id="customMoodToggle" onchange="toggleCustomMood(this.checked)" style="width: 18px; height: 18px; accent-color: var(--accent); cursor: pointer;">
      </div>
      <div style="font-size: 11px; color: var(--text-dim); line-height: 1.5; margin-bottom: 12px;" data-i18n="mood_custom_hint">
        • Bật để đặt Tiêu đề & Thông điệp riêng cho trạng thái này.<br>
        • Các trạng thái khác vẫn giữ nguyên câu riêng hoặc câu mặc định song ngữ.
      </div>

      <div id="customMoodInputs" style="display: none;">
        <div style="margin-bottom: 12px;">
          <div style="display: flex; justify-content: space-between; font-size: 11px; color: var(--text-dim); margin-bottom: 4px;">
            <span data-i18n="mood_tag_label">Tiêu đề bảng Neon (Tối đa 20 ký tự, in HOA):</span>
            <span id="customTagCount" style="color: var(--accent); font-weight: 600;">0/20</span>
          </div>
          <input type="text" id="customTagInput" class="select-control" maxlength="20" placeholder="VD: ĐANG ĐI HỌP, THÈM CÀ PHÊ..." oninput="updateMoodCounters()" style="width: 100%; text-transform: uppercase;">
        </div>

        <div style="margin-bottom: 12px;">
          <div style="display: flex; justify-content: space-between; font-size: 11px; color: var(--text-dim); margin-bottom: 4px;">
            <span data-i18n="mood_msg_label">Câu nhắn phía dưới (Tối đa 36 ký tự):</span>
            <span id="customMsgCount" style="color: var(--accent); font-weight: 600;">0/36</span>
          </div>
          <input type="text" id="customMsgInput" class="select-control" maxlength="36" placeholder="VD: Về sau 30p • Gấp thì gọi điện nhé!" oninput="updateMoodCounters()" style="width: 100%;">
        </div>

        <!-- Quick suggestions container -->
        <div style="font-size: 11px; color: var(--text-dim); margin-bottom: 6px;" data-i18n="mood_quick_label">Gợi ý nhanh cho trạng thái này:</div>
        <div id="quickMoodContainer" style="display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 14px;"></div>

        <div style="display: flex; gap: 8px;">
          <button type="button" class="btn btn-accent" style="flex: 2;" onclick="saveCustomMood()">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>
            <span id="saveMoodBtnText" data-i18n="btn_save_mood">Lưu Trạng Thái</span>
          </button>
          <button type="button" class="btn btn-outline" style="flex: 1;" onclick="resetDefaultMood()" data-i18n="btn_reset_mood">Mặc định</button>
        </div>
      </div>
    </div>
  </div>

  <!-- 2. Remote Screen Control Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>
      <span data-i18n="sec_ctrl_title">Điều khiển màn hình từ xa</span>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="page_sub">Chuyển trang hiển thị:</div>
    <div class="grid-3" id="pageButtons">
      <button class="btn-chip" onclick="switchPage(0)">🕒 Đồng hồ</button>
      <button class="btn-chip" onclick="switchPage(1)">📅 Lịch âm</button>
      <button class="btn-chip" onclick="switchPage(2)">📈 Tài chính</button>
      <button class="btn-chip" onclick="switchPage(3)">🍅 Pomodoro</button>
      <button class="btn-chip" onclick="switchPage(4)">🎭 Tâm trạng</button>
      <button class="btn-chip" onclick="switchPage(5)">⚙️ Cài đặt</button>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin: 14px 0 8px;" data-i18n="theme_sub">Bộ sưu tập Giao diện (Theme):</div>
    <div class="grid-2" id="themeButtons">
      <button class="btn-chip" onclick="switchTheme(0)">Cyber Neon</button>
      <button class="btn-chip" onclick="switchTheme(1)">Minimal Obsidian</button>
      <button class="btn-chip" onclick="switchTheme(2)">Sunset Amber</button>
      <button class="btn-chip" onclick="switchTheme(3)">Cyberpunk</button>
      <button class="btn-chip" onclick="switchTheme(4)">💜 Pastel Lavender</button>
      <button class="btn-chip" onclick="switchTheme(5)">❄️ Nordic Frost</button>
      <button class="btn-chip" onclick="switchTheme(6)">🌸 Sakura Blossom</button>
      <button class="btn-chip" onclick="switchTheme(7)">🌿 Emerald Forest</button>
    </div>

    <div class="slider-wrap">
      <div class="slider-hdr">
        <span data-i18n="bright_label">Độ sáng màn hình</span>
        <span id="brightVal">80%</span>
      </div>
      <input type="range" id="brightSlider" min="10" max="100" value="80" oninput="changeBrightness(this.value)">
    </div>
  </div>

  <!-- Office Mode & Power Management Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/></svg>
      <span data-i18n="sec_power_title">🏢 Chế độ Văn phòng & Quản lý Nguồn</span>
    </div>

    <!-- Quick 1-tap Screen Toggle -->
    <div style="display: flex; justify-content: space-between; align-items: center; background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; padding: 12px 14px; margin-bottom: 14px;">
      <div>
        <div style="font-size: 13px; font-weight: 700; color: #fff;" data-i18n="screen_pwr_label">Nguồn màn hình CYD:</div>
        <div id="screenStateBadge" style="font-size: 11px; color: var(--success); font-weight: 600; margin-top: 2px;">🟢 Màn hình đang BẬT</div>
      </div>
      <button id="screenPowerBtn" type="button" class="btn btn-outline" style="width: auto; padding: 8px 14px; font-size: 12px; border-radius: 10px; border-color: rgba(255,59,48,0.5); color: #ff6b6b;" onclick="toggleScreenPower()">
        <span style="font-size: 14px;">⏻</span> <span id="screenPowerBtnText" data-i18n="btn_screen_off">Tắt màn hình ngay</span>
      </button>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="power_mode_label">Không gian sử dụng (Profile):</div>
    <div class="grid-2" id="powerModeButtons" style="margin-bottom: 10px;">
      <button type="button" class="btn-chip active" onclick="switchPowerMode(0)">
        <div style="font-weight: 700;" data-i18n="mode_office_title">🏢 Văn phòng (Mặc định)</div>
        <div style="font-size: 10px; opacity: 0.8; margin-top: 2px;" data-i18n="mode_office_sub">Tắt đen 0% ngoài giờ</div>
      </button>
      <button type="button" class="btn-chip" onclick="switchPowerMode(1)">
        <div style="font-weight: 700;" data-i18n="mode_home_title">🏠 Ở nhà (Đồng hồ đêm)</div>
        <div style="font-size: 10px; opacity: 0.8; margin-top: 2px;" data-i18n="mode_home_sub">Giảm mờ 5% xem giờ</div>
      </button>
    </div>

    <!-- Power Schedule Form -->
    <div style="background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 12px; padding: 14px; margin-top: 10px;">
      <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
        <label for="sleepScheduleEn" id="sleepScheduleEnLabel" style="font-size: 13px; font-weight: 600; color: #fff; cursor: pointer;" data-i18n="sleep_sched_en">Hẹn giờ nghỉ tự động:</label>
        <input type="checkbox" id="sleepScheduleEn" style="width: 18px; height: 18px; accent-color: var(--accent); cursor: pointer;">
      </div>

      <div class="grid-2" style="margin-bottom: 12px;">
        <div>
          <div id="sleepStartLabel" style="font-size: 11px; color: var(--text-dim); margin-bottom: 4px;" data-i18n="sleep_start_lbl">Tan ca (Tắt màn hình):</div>
          <input type="time" id="sleepStartTime" class="select-control" value="18:30">
        </div>
        <div>
          <div id="sleepEndLabel" style="font-size: 11px; color: var(--text-dim); margin-bottom: 4px;" data-i18n="sleep_end_lbl">Vào ca (Bật lại):</div>
          <input type="time" id="sleepEndTime" class="select-control" value="07:45">
        </div>
      </div>

      <div id="powerHintText" style="font-size: 11px; color: var(--text-dim); line-height: 1.5; margin-bottom: 12px; border-left: 2px solid var(--accent); padding-left: 8px;" data-i18n="power_hint">
        • <b>Tối ưu cho văn phòng:</b> Sau giờ làm việc, khi phòng tắt đèn ra về thiết bị sẽ tự động tắt đen hoàn toàn để tiết kiệm điện & bảo vệ màn hình.<br>
        • <b>Cảm biến LDR thông minh:</b> Dù đã đến giờ hẹn, nếu phòng còn bật đèn làm thêm (OT), màn hình vẫn giữ sáng để phục vụ bạn.<br>
        • <b>Chạm để bật:</b> Bất cứ lúc nào bạn chạm tay vào màn hình SmartDesk sẽ lập tức bật sáng lại!
      </div>

      <button type="button" class="btn btn-accent" onclick="savePowerSchedule()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>
        <span data-i18n="btn_save_power">Lưu Cài Đặt Hẹn Giờ</span>
      </button>
    </div>
  </div>

  <!-- 3. Desk Pet & Pomodoro Station Section -->
  <div class="card">
    <div class="card-title" style="justify-content: space-between;">
      <div style="display:flex; align-items:center; gap:8px;">
        <span style="font-size: 18px;">🍅</span>
        <span data-i18n="sec_pet_title">Trạm Tập Trung Pomodoro & Desk Pet</span>
      </div>
      <span id="webPomoTimer" style="font-size: 15px; font-weight: 800; color: var(--accent); font-family: monospace;">25:00</span>
    </div>

    <div class="grid-3" id="pomoModeButtons" style="margin-bottom: 10px;">
      <button type="button" class="btn-chip active" onclick="setWebPomoMode(0)">🔥 Focus 25p</button>
      <button type="button" class="btn-chip" onclick="setWebPomoMode(1)">☕ Nghỉ 5p</button>
      <button type="button" class="btn-chip" onclick="setWebPomoMode(2)">🛋️ Nghỉ 15p</button>
    </div>

    <div style="display: flex; gap: 8px; margin-bottom: 14px;">
      <button type="button" id="btnWebPomoStart" class="btn btn-accent" style="flex: 2;" onclick="toggleWebPomodoro()">
        ▶️ Bắt đầu Pomodoro (Đồng bộ EMO)
      </button>
      <button type="button" class="btn btn-outline" style="flex: 1;" onclick="resetWebPomodoro()">
        🔄 Đặt lại
      </button>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="sec_pet_sub">
      Chọn bạn đồng hành hiển thị ở tâm vòng tròn Pomodoro:
    </div>
    <div class="grid-4" id="petButtons">
      <button class="btn-chip" onclick="switchPet(0)">🍅 Cà chua</button>
      <button class="btn-chip" onclick="switchPet(1)">🐱 Mèo Pixel</button>
      <button class="btn-chip" onclick="switchPet(2)">🐕 Shiba Inu</button>
      <button class="btn-chip" onclick="switchPet(3)">🦫 Capybara</button>
    </div>
  </div>

  <!-- 4. Alarm & Smart Reminder Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/></svg>
      <span data-i18n="sec_alarm_title">Báo thức & Lời nhắc SmartDesk</span>
    </div>
    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 12px;" data-i18n="sec_alarm_sub">
      Thiết lập giờ báo thức và lời nhắn hiển thị trên màn hình để bàn:
    </div>

    <div style="display: flex; gap: 12px; align-items: center; margin-bottom: 12px;">
      <input type="time" id="alarmTimeInput" class="select-control" style="font-size: 17px; font-weight: 600; padding: 6px 10px; width: 130px; text-align: center;">
      <label style="display: flex; align-items: center; gap: 8px; font-size: 13px; cursor: pointer; user-select: none;">
        <input type="checkbox" id="alarmEnInput" style="width: 18px; height: 18px; accent-color: var(--accent);">
        <span data-i18n="alarm_en_label">Kích hoạt báo thức</span>
      </label>
    </div>

    <div style="margin-bottom: 10px;">
      <div style="font-size: 11px; color: var(--text-dim); margin-bottom: 4px;" data-i18n="alarm_msg_label">Nội dung lời nhắc / thông báo khi chuông reo:</div>
      <input type="text" id="alarmMsgInput" class="select-control" placeholder="VD: Uống nước, Vận động, Họp online..." data-i18n-ph="alarm_msg_ph" maxlength="36" style="width: 100%;">
    </div>

    <div style="display: flex; gap: 6px; flex-wrap: wrap; margin-bottom: 12px;">
      <button class="btn-chip" style="font-size: 11px; padding: 3px 8px;" onclick="setQuickMsg('drink')" data-i18n="quick_drink">💧 Uống nước</button>
      <button class="btn-chip" style="font-size: 11px; padding: 3px 8px;" onclick="setQuickMsg('sport')" data-i18n="quick_sport">🏃 Vận động</button>
      <button class="btn-chip" style="font-size: 11px; padding: 3px 8px;" onclick="setQuickMsg('rest')" data-i18n="quick_rest">☕ Nghỉ ngơi</button>
      <button class="btn-chip" style="font-size: 11px; padding: 3px 8px;" onclick="setQuickMsg('work')" data-i18n="quick_work">💼 Tan ca</button>
    </div>

    <button class="btn btn-accent" onclick="saveAlarmSettings()" data-i18n="btn_save_alarm">
      Lưu cài đặt báo thức
    </button>
  </div>

  <!-- Sound & Audio Settings Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 5L6 9H2v6h4l5 4V5z"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"/></svg>
      <span data-i18n="sec_snd_title">Âm thanh & Điệu chuông (Loa GPIO 26)</span>
    </div>
    <div class="grid-2">
      <div>
        <label style="font-size: 12px; color: var(--text-dim); display: block; margin-bottom: 6px;" data-i18n="snd_startup_label">Âm báo khởi động:</label>
        <select class="select-control" id="startupSndSelect" onchange="changeSound()">
          <option value="0">🎶 Marimba Acoustic</option>
          <option value="1">💎 Pha lê (Crystal Chime)</option>
          <option value="2">⚡ Điện tử (Cyber Synth)</option>
          <option value="3">🕹️ Cổ điển (Retro 8-Bit)</option>
          <option value="4">🔇 Tắt âm (Mute)</option>
        </select>
      </div>
      <div>
        <label style="font-size: 12px; color: var(--text-dim); display: block; margin-bottom: 6px;" data-i18n="snd_touch_label">Âm chạm cảm ứng:</label>
        <select class="select-control" id="touchSndSelect" onchange="changeSound()">
          <option value="0">Tắt âm (Mute)</option>
          <option value="1">Cơ bản (Click)</option>
          <option value="2">Giọt nước (Water)</option>
          <option value="3">Tiếng bíp (Beep)</option>
          <option value="4">Tiếng chim (Chirp)</option>
        </select>
      </div>
    </div>
    <div style="margin-top: 10px;">
      <label style="font-size: 12px; color: var(--text-dim); display: block; margin-bottom: 6px;" data-i18n="snd_melody_label">Điệu chuông báo thức:</label>
      <select class="select-control" id="alarmMelodySelect" onchange="changeSound()">
        <option value="0">Classic Alarm</option>
        <option value="1">Melodic Chime</option>
        <option value="2">Cyber Rhythm</option>
        <option value="3">Gentle Morning</option>
      </select>
    </div>
    <div class="grid-2" style="margin-top: 12px;">
      <button class="btn btn-outline" style="border-color:var(--accent); color:var(--accent);" onclick="testStartupSound()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg>
        <span data-i18n="btn_test_startup">Thử âm khởi động</span>
      </button>
      <button class="btn btn-primary" onclick="testAlarmMelody()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg>
        <span data-i18n="btn_test_melody">Thử chuông báo</span>
      </button>
    </div>
  </div>

  <!-- Currency Settings Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M12 6v12M17 10H9.5a2.5 2.5 0 0 0 0 5H14a2.5 2.5 0 0 1 0 5H7"/></svg>
      <span data-i18n="sec_cur_title">Cấu hình Ngoại tệ</span>
    </div>
    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 12px;" data-i18n="sec_cur_sub">
      Tùy chọn 2 đồng ngoại tệ bạn muốn theo dõi trên màn hình để bàn:
    </div>
    <div class="grid-2" style="margin-bottom: 12px;">
      <div>
        <div style="font-size: 11px; color: var(--text-dim); margin-bottom: 6px;" data-i18n="cur1_label">Ngoại tệ 1 (Mục trên):</div>
        <select id="cur1Select" class="select-control">
          <option value="USD">USD — Đô la Mỹ</option>
          <option value="EUR">EUR — Euro</option>
          <option value="CAD">CAD — Đô la Canada</option>
          <option value="JPY">JPY — Yên Nhật</option>
          <option value="GBP">GBP — Bảng Anh</option>
          <option value="AUD">AUD — Đô la Úc</option>
          <option value="SGD">SGD — Đô la Singapore</option>
          <option value="CNY">CNY — Nhân dân tệ</option>
          <option value="KRW">KRW — Won Hàn Quốc</option>
          <option value="CHF">CHF — Franc Thụy Sĩ</option>
          <option value="THB">THB — Baht Thái</option>
        </select>
      </div>
      <div>
        <div style="font-size: 11px; color: var(--text-dim); margin-bottom: 6px;" data-i18n="cur2_label">Ngoại tệ 2 (Mục dưới):</div>
        <select id="cur2Select" class="select-control">
          <option value="CAD">CAD — Đô la Canada</option>
          <option value="EUR">EUR — Euro</option>
          <option value="USD">USD — Đô la Mỹ</option>
          <option value="JPY">JPY — Yên Nhật</option>
          <option value="GBP">GBP — Bảng Anh</option>
          <option value="AUD">AUD — Đô la Úc</option>
          <option value="SGD">SGD — Đô la Singapore</option>
          <option value="CNY">CNY — Nhân dân tệ</option>
          <option value="KRW">KRW — Won Hàn Quốc</option>
          <option value="CHF">CHF — Franc Thụy Sĩ</option>
          <option value="THB">THB — Baht Thái</option>
        </select>
      </div>
    </div>
    <button class="btn btn-accent" onclick="saveCurrencies()" data-i18n="btn_save_cur">
      Lưu cấu hình ngoại tệ
    </button>
  </div>

  <!-- Network & WiFi Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/></svg>
      <span data-i18n="sec_wifi_title">Cài đặt Mạng & WiFi</span>
    </div>
    <div style="font-size: 12px; color: var(--text-dim); line-height: 1.7; margin-bottom: 12px;">
      <div><span data-i18n="wifi_mdns">• Tên miền mDNS: </span><b style="color:var(--accent);">http://smartdesk.local</b></div>
      <div><span data-i18n="wifi_ip_prefix">• Địa chỉ IP hiện tại: </span><span id="wifiIp" style="color:#fff;">...</span><span data-i18n="wifi_rssi_prefix"> (Sóng: </span><span id="wifiRssi">...</span> dBm)</div>
    </div>

    <!-- Multi-WiFi Profiles List -->
    <div style="margin-bottom: 14px;">
      <div style="font-size: 12px; font-weight: 600; color: var(--accent); margin-bottom: 8px; display:flex; justify-content:space-between; align-items:center;">
        <span data-i18n="wifi_saved_title">Mạng đã ghi nhớ (Tối đa 3 mạng):</span>
        <span id="wifiProfileCount" style="color:var(--text-dim); font-size:11px;">0/3</span>
      </div>
      <div id="wifiProfilesList" style="display:flex; flex-direction:column; gap:6px;">
        <div style="font-size:11px; color:var(--text-dim); font-style:italic; padding:4px 0;">Đang tải danh sách...</div>
      </div>
    </div>

    <div style="display:flex; flex-direction:column; gap:8px;">
      <!-- Collapsible Add WiFi Form -->
      <div id="addWifiForm" style="display:none; background:rgba(0,229,255,0.06); border:1px solid rgba(0,229,255,0.25); border-radius:12px; padding:12px; margin-bottom:4px;">
        <div style="font-size:12px; font-weight:700; color:var(--accent); margin-bottom:8px;" data-i18n="wifi_add_title">Nhập thông tin WiFi:</div>
        <input type="text" id="newWifiSsid" placeholder="Tên WiFi (SSID)" style="width:100%; padding:10px; border-radius:8px; border:1px solid var(--card-border); background:#070B14; color:#fff; font-size:13px; margin-bottom:8px;">
        <input type="password" id="newWifiPass" placeholder="Mật khẩu WiFi (bỏ trống nếu không pass)" style="width:100%; padding:10px; border-radius:8px; border:1px solid var(--card-border); background:#070B14; color:#fff; font-size:13px; margin-bottom:10px;">
        <div style="display:grid; grid-template-columns:1fr 1fr; gap:8px;">
          <button class="btn btn-accent" style="padding:10px;" onclick="submitNewWifi()" data-i18n="btn_save_wifi">💾 Lưu mạng</button>
          <button class="btn btn-outline" style="padding:10px;" onclick="toggleAddWifiForm(false)" data-i18n="btn_cancel">Hủy</button>
        </div>
      </div>
      <div style="display:grid; grid-template-columns: 1fr 1fr; gap:8px;">
        <button class="btn btn-primary" onclick="toggleAddWifiForm(true)" data-i18n="btn_add_wifi">
          ➕ Nhập WiFi mới
        </button>
        <button class="btn btn-outline" onclick="openWiFiPortal()" style="border-color:var(--accent); color:var(--accent);" data-i18n="btn_qr_portal">
          📷 Quét QR trên CYD
        </button>
      </div>
      <button class="btn btn-outline" style="border-color:rgba(255,59,48,0.4); color:var(--danger); padding:8px; font-size:12px;" onclick="resetWifi()" data-i18n="btn_reset_wifi">
        Xóa hết & Reset WiFi
      </button>
    </div>
  </div>

</div>

<div id="toast">Thông báo</div>

<script>
let currentLang = 0;
const I18N = {
  0: {
    brand_sub: "Màn hình để bàn thông minh",
    status_online: "Trực tuyến",
    sec_cur_title: "Cấu hình Ngoại tệ",
    sec_cur_sub: "Tùy chọn 2 đồng ngoại tệ bạn muốn theo dõi trên màn hình để bàn:",
    cur1_label: "Ngoại tệ 1 (Mục trên):",
    cur2_label: "Ngoại tệ 2 (Mục dưới):",
    btn_save_cur: "Lưu cấu hình ngoại tệ",
    sec_alarm_title: "Báo thức & Lời nhắc SmartDesk",
    sec_alarm_sub: "Thiết lập giờ báo thức và lời nhắn hiển thị trên màn hình để bàn:",
    alarm_en_label: "Kích hoạt báo thức",
    alarm_msg_label: "Nội dung lời nhắc / thông báo khi chuông reo:",
    alarm_msg_ph: "VD: Uống nước, Vận động, Họp online...",
    quick_drink: "💧 Uống nước",
    quick_sport: "🏃 Vận động",
    quick_rest: "☕ Nghỉ ngơi",
    quick_work: "💼 Tan ca",
    btn_save_alarm: "Lưu cài đặt báo thức",
    sec_ctrl_title: "Điều khiển màn hình từ xa",
    page_sub: "Chuyển trang hiển thị:",
    theme_sub: "Bộ sưu tập Giao diện (Theme):",
    bright_label: "Độ sáng màn hình",
    sec_pet_title: "Thú cưng Pomodoro (Desk Pet)",
    sec_pet_sub: "Chọn bạn đồng hành hiển thị ở tâm vòng tròn Pomodoro (gõ phím cùng bạn khi cày deadline):",
    t_pet: "Đã chọn thú cưng: ",
    sec_snd_title: "Âm thanh & Điệu chuông (Loa GPIO 26)",
    snd_startup_label: "Âm báo khởi động:",
    snd_touch_label: "Âm chạm cảm ứng:",
    snd_melody_label: "Điệu chuông báo thức:",
    btn_test_startup: "Thử âm khởi động",
    btn_test_melody: "Thử chuông báo",
    t_test_startup: "Đang phát thử âm khởi động...",
    sec_wifi_title: "Cài đặt Mạng & WiFi",
    wifi_mdns: "• Tên miền mDNS: ",
    wifi_ip_prefix: "• Địa chỉ IP hiện tại: ",
    wifi_rssi_prefix: " (Sóng: ",
    sec_mood_title: "Tâm trạng & Biển hiệu Bàn làm việc (Desk Mood)",
    sec_pack_sub: "Bộ Meme Nhân Vật (Meme Pack):",
    t_pack: "Đã chọn bộ meme: ",
    sec_mood_sub: "Tâm trạng bàn làm việc (Desk Mood):",
    t_mood: "Đã đổi tâm trạng: ",
    mood_custom_prefix: "Tùy chỉnh riêng cho: ",
    mood_custom_toggle: "Tùy chỉnh Tiêu đề & Thông điệp riêng",
    mood_custom_hint: "• Bật để đặt Tiêu đề & Thông điệp riêng cho trạng thái này.<br>• Các trạng thái khác vẫn giữ nguyên câu riêng hoặc câu mặc định song ngữ.",
    mood_tag_label: "Tiêu đề bảng Neon (Tối đa 20 ký tự, in HOA):",
    mood_msg_label: "Câu nhắn phía dưới (Tối đa 36 ký tự):",
    mood_quick_label: "Gợi ý nhanh:",
    btn_save_mood: "Lưu Trạng Thái",
    btn_reset_mood: "Mặc định",
    t_mood_saved: "Đã lưu trạng thái tùy chỉnh!",
    t_mood_default: "Đã khôi phục nội dung mặc định (Song ngữ)!",
    wifi_saved_title: "Mạng đã ghi nhớ (Tối đa 3 mạng):",
    btn_add_wifi: "➕ Nhập WiFi mới",
    btn_qr_portal: "📷 Quét QR trên CYD",
    wifi_add_title: "Nhập thông tin WiFi:",
    btn_save_wifi: "💾 Lưu mạng",
    btn_cancel: "Hủy",
    btn_reset_wifi: "Xóa hết & Reset WiFi",
    t_wifi_saved: "Đã lưu mạng thành công!",
    t_ssid_empty: "Vui lòng nhập tên WiFi!",
    t_wifi_deleted: "Đã xóa mạng: ",
    t_wifi_del_err: "Lỗi xóa mạng!",
    t_wifi_portal_prompt: "SmartDesk sẽ mở giao diện Quét QR cài đặt mạng trên màn hình CYD. Bạn có muốn tiếp tục?",
    t_wifi_del_confirm: "Bạn có chắc muốn xóa mạng này khỏi bộ nhớ SmartDesk?",
    p0: "🕒 Đồng hồ", p1: "📅 Lịch âm", p2: "📈 Tài chính", p3: "🍅 Pomodoro", p4: "🎭 Tâm trạng", p5: "⚙️ Cài đặt",
    t_page: "Đã chuyển: ",
    t_snd_save: "Đã lưu cấu hình âm thanh!",
    t_snd_err: "Lỗi lưu âm thanh",
    t_test_melody: "Đang phát thử chuông trên loa...",
    t_theme: "Đã đổi: ",
    t_cur_updating: "Đang cập nhật tỉ giá",
    t_cur_ok: "Đã đổi tỉ giá sang",
    t_cur_err: "Lỗi cập nhật ngoại tệ!",
    t_conn_err: "Lỗi kết nối!",
    t_alarm_ok: "Đã lưu cài đặt báo thức thành công!",
    t_alarm_err: "Lỗi khi lưu báo thức",
    t_lang_switched: "Đã chuyển sang Tiếng Việt!",
    t_cmd_err: "Lỗi gửi lệnh",
    confirm_wifi_reset: "Xác nhận đổi mạng WiFi?\n\nThiết bị sẽ xóa mật khẩu WiFi hiện tại và khởi động lại vào điểm phát sóng 'CYD-SmartDesk-V2' (192.168.4.1) để bạn kết nối WiFi mới.",
    alert_rebooting: "Đang khởi động lại...\nVui lòng kết nối vào WiFi 'CYD-SmartDesk-V2' trên điện thoại để cài đặt.",
    cur_usd: "USD — Đô la Mỹ", cur_eur: "EUR — Euro", cur_cad: "CAD — Đô la Canada",
    cur_jpy: "JPY — Yên Nhật", cur_gbp: "GBP — Bảng Anh", cur_aud: "AUD — Đô la Úc",
    cur_sgd: "SGD — Đô la Singapore", cur_cny: "CNY — Nhân dân tệ", cur_krw: "KRW — Won Hàn Quốc",
    cur_chf: "CHF — Franc Thụy Sĩ", cur_thb: "THB — Baht Thái",
    snd_0: "Tắt âm (Mute)", snd_1: "Cơ bản (Click)", snd_2: "Giọt nước (Water)", snd_3: "Tiếng bíp (Beep)", snd_4: "Tiếng chim (Chirp)",
    sec_power_title: "🏢 Chế độ Văn phòng & Quản lý Nguồn",
    screen_pwr_label: "Nguồn màn hình CYD:",
    screen_state_on: "🟢 Màn hình đang BẬT",
    screen_state_off: "⚫ Màn hình đang TẮT (Chạm CYD để bật)",
    btn_screen_off: "Tắt màn hình ngay",
    btn_screen_on: "Bật màn hình",
    power_mode_label: "Không gian sử dụng (Profile):",
    mode_office_title: "🏢 Văn phòng (Mặc định)",
    mode_office_sub: "Tắt đen 0% ngoài giờ",
    mode_home_title: "🏠 Ở nhà (Đồng hồ đêm)",
    mode_home_sub: "Giảm mờ 5% xem giờ",
    sleep_sched_en: "Hẹn giờ nghỉ tự động:",
    sleep_sched_en_off: "Hẹn giờ tan ca (Tắt màn hình 0%):",
    sleep_start_lbl_off: "Tan ca (Tắt màn hình):",
    sleep_end_lbl_off: "Vào ca (Bật lại):",
    power_hint_off: "• <b>Tối ưu cho văn phòng:</b> Sau giờ làm việc, khi phòng tắt đèn ra về thiết bị sẽ tự động tắt đen hoàn toàn để tiết kiệm điện & bảo vệ màn hình.<br>• <b>Cảm biến LDR thông minh:</b> Dù đã đến giờ hẹn, nếu phòng còn bật đèn làm thêm (OT), màn hình vẫn giữ sáng để phục vụ bạn.<br>• <b>Chạm để bật:</b> Bất cứ lúc nào bạn chạm tay vào màn hình SmartDesk sẽ lập tức bật sáng lại!",
    sleep_sched_en_hom: "Hẹn giờ đi ngủ (Mờ dịu 5% đêm):",
    sleep_start_lbl_hom: "Giờ đi ngủ (Dịu mờ 5%):",
    sleep_end_lbl_hom: "Giờ thức dậy (Sáng lại):",
    power_hint_hom: "• <b>Đồng hồ đêm êm dịu:</b> Trong khung giờ ngủ, khi phòng tắt đèn màn hình sẽ tự giảm mờ còn 5% để bạn xem giờ ban đêm mà không hề chói mắt.<br>• <b>Tự động theo giấc ngủ:</b> Nếu bạn ngủ sớm tắt đèn trước giờ hẹn, cảm biến LDR sẽ tự động giảm mờ ngay.<br>• <b>Thức khuya bật đèn:</b> Nếu bạn còn thức làm việc, màn hình vẫn giữ độ sáng thoải mái phục vụ bạn!",
    sleep_start_lbl: "Tan ca (Tắt màn hình):",
    sleep_end_lbl: "Vào ca (Bật lại):",
    power_hint: "• <b>Tối ưu cho văn phòng:</b> Sau giờ làm việc, khi phòng tắt đèn ra về thiết bị sẽ tự động tắt đen hoàn toàn để tiết kiệm điện & bảo vệ màn hình.<br>• <b>Cảm biến LDR thông minh:</b> Dù đã đến giờ hẹn, nếu phòng còn bật đèn làm thêm (OT), màn hình vẫn giữ sáng để phục vụ bạn.<br>• <b>Chạm để bật:</b> Bất cứ lúc nào bạn chạm tay vào màn hình SmartDesk sẽ lập tức bật sáng lại!",
    btn_save_power: "Lưu Cài Đặt Hẹn Giờ",
    t_mode_office: "Đã chọn: Chế độ Văn phòng (Mặc định)",
    t_mode_home: "Đã chọn: Chế độ Ở nhà (Đồng hồ đêm)",
    t_screen_turned_off: "Đã tắt màn hình thiết bị!",
    t_screen_turned_on: "Đã bật màn hình thiết bị!",
    t_power_saved: "Đã lưu cài đặt nguồn & lịch làm việc thành công!",
    pwa_title: "Cài đặt SmartDesk App",
    pwa_desc: "Trải nghiệm toàn màn hình như App xịn",
    pwa_btn: "Cài đặt",
    pwa_ios_hint: "Để cài đặt trên iPhone:\n1. Bấm nút Chia sẻ (biểu tượng ⎋ mũi tên lên dưới thanh Safari)\n2. Chọn 'Thêm vào MH chính' (Add to Home Screen).",
    pwa_android_hint: "Để cài đặt SmartDesk App trên Android:\n1. Bấm vào biểu tượng 3 chấm (⋮) ở góc trên bên phải Chrome.\n2. Chọn 'Thêm vào màn hình chính' (hoặc 'Cài đặt ứng dụng').\n3. Bấm 'Thêm' để hoàn tất cài đặt."
  },
  1: {
    brand_sub: "Smart Desktop Companion",
    status_online: "Online",
    sec_cur_title: "Currency Settings",
    sec_cur_sub: "Select 2 currencies to track on your desktop screen:",
    cur1_label: "Currency 1 (Top):",
    cur2_label: "Currency 2 (Bottom):",
    btn_save_cur: "Save Currency Settings",
    sec_alarm_title: "Alarm & Smart Reminders",
    sec_alarm_sub: "Configure alarm time and notification message for desktop screen:",
    alarm_en_label: "Enable Alarm",
    alarm_msg_label: "Reminder message when alarm rings:",
    alarm_msg_ph: "e.g., Drink water, Stretch, Meeting...",
    quick_drink: "💧 Drink water",
    quick_sport: "🏃 Exercise",
    quick_rest: "☕ Short break",
    quick_work: "💼 Off work",
    btn_save_alarm: "Save Alarm Settings",
    sec_ctrl_title: "Remote Screen Control",
    page_sub: "Switch Display Page:",
    theme_sub: "Theme Presets:",
    bright_label: "Screen Brightness",
    sec_power_title: "🏢 Office Mode & Power Management",
    screen_pwr_label: "CYD Screen Power:",
    screen_state_on: "🟢 Screen is ON",
    screen_state_off: "⚫ Screen is OFF (Tap CYD to wake)",
    btn_screen_off: "Turn Screen Off",
    btn_screen_on: "Turn Screen On",
    power_mode_label: "Usage Environment (Profile):",
    mode_office_title: "🏢 Office (Default)",
    mode_office_sub: "Black 0% after work hours",
    mode_home_title: "🏠 Home (Night Clock)",
    mode_home_sub: "Dim 5% night clock",
    sleep_sched_en: "Auto Sleep Schedule:",
    sleep_sched_en_off: "Auto Screen Off Schedule:",
    sleep_start_lbl_off: "Off Work (Screen Off):",
    sleep_end_lbl_off: "Start Work (Screen On):",
    power_hint_off: "• <b>Optimized for Office:</b> After work hours, when room lights are turned off, the screen shuts off completely (0%) to save power and extend LCD life.<br>• <b>Smart Ambient Sensor (LDR):</b> Even past schedule, if office lights remain on (overtime work), the screen stays active for you.<br>• <b>Wake on Touch:</b> Tap anywhere on the CYD screen anytime to wake it up instantly!",
    sleep_sched_en_hom: "Night Clock Sleep Schedule:",
    sleep_start_lbl_hom: "Bedtime (Dim to 5%):",
    sleep_end_lbl_hom: "Wakeup (Restore brightness):",
    power_hint_hom: "• <b>Gentle Night Clock:</b> During bedtime hours, when room lights are turned off, the screen dims to 5% for easy night viewing without glare.<br>• <b>Adaptive Bedtime:</b> If you sleep early and turn off room lights before schedule, the LDR sensor dims the screen immediately.<br>• <b>Late Night Work:</b> If room lights stay on while you work, the screen maintains comfortable brightness for you!",
    sleep_start_lbl: "Off Work (Screen Off):",
    sleep_end_lbl: "Start Work (Screen On):",
    power_hint: "• <b>Optimized for Office:</b> After work hours, when room lights are turned off, the screen shuts off completely (0%) to save power and extend LCD life.<br>• <b>Smart Ambient Sensor (LDR):</b> Even past schedule, if office lights remain on (overtime work), the screen stays active for you.<br>• <b>Wake on Touch:</b> Tap anywhere on the CYD screen anytime to wake it up instantly!",
    btn_save_power: "Save Sleep Schedule",
    t_mode_office: "Selected: Office Mode (Default)",
    t_mode_home: "Selected: Home Mode (Night Clock)",
    t_screen_turned_off: "Screen turned off!",
    t_screen_turned_on: "Screen turned on!",
    t_power_saved: "Power schedule saved successfully!",
    sec_pet_title: "Pomodoro Companion (Desk Pet)",
    sec_pet_sub: "Select your companion in the center of the Pomodoro dial (types along while you focus):",
    t_pet: "Selected companion: ",
    sec_mood_title: "Desk Mood & Vibe Signboard",
    sec_pack_sub: "Meme Character Pack:",
    t_pack: "Selected meme pack: ",
    sec_mood_sub: "Desk Mood / Vibe Signboard:",
    t_mood: "Selected vibe: ",
    mood_custom_prefix: "Customizing for: ",
    mood_custom_toggle: "Custom Status Title & Message",
    mood_custom_hint: "• Enable to set custom Title & Message specifically for this mood.<br>• Other moods will keep their own custom or default bilingual quotes.",
    mood_tag_label: "Neon Sign Title (Max 20 chars, UPPERCASE):",
    mood_msg_label: "Sub-message below (Max 36 chars):",
    mood_quick_label: "Quick presets:",
    btn_save_mood: "Save Status",
    btn_reset_mood: "Reset",
    t_mood_saved: "Custom mood status saved!",
    t_mood_default: "Restored to default bilingual status!",
    sec_snd_title: "Sound & Melodies (Speaker GPIO 26)",
    snd_startup_label: "Startup Boot Sound:",
    snd_touch_label: "Touch sound feedback:",
    snd_melody_label: "Alarm Melody:",
    btn_test_startup: "Test Boot Sound",
    btn_test_melody: "Test Alarm",
    t_test_startup: "Playing startup boot sound...",
    sec_wifi_title: "Network & WiFi Settings",
    wifi_mdns: "• mDNS Domain: ",
    wifi_ip_prefix: "• Current IP Address: ",
    wifi_rssi_prefix: " (Signal: ",
    wifi_saved_title: "Remembered Networks (Max 3):",
    btn_add_wifi: "➕ Enter WiFi Info",
    btn_qr_portal: "📷 Scan QR on CYD",
    wifi_add_title: "Enter WiFi Credentials:",
    btn_save_wifi: "💾 Save Network",
    btn_cancel: "Cancel",
    btn_reset_wifi: "Clear & Reset All",
    t_wifi_saved: "Network saved successfully!",
    t_ssid_empty: "Please enter WiFi SSID!",
    t_wifi_deleted: "Removed network: ",
    t_wifi_del_err: "Failed to remove network!",
    t_wifi_portal_prompt: "SmartDesk will open the on-screen QR Setup Portal on CYD. Continue?",
    t_wifi_del_confirm: "Are you sure you want to remove this network from memory?",
    p0: "🕒 Clock", p1: "📅 Calendar", p2: "📈 Finance", p3: "🍅 Pomodoro", p4: "🎭 Mood", p5: "⚙️ Settings",
    t_page: "Switched to: ",
    t_snd_save: "Sound settings saved!",
    t_snd_err: "Failed to save sound settings",
    t_test_melody: "Playing alarm melody on speaker...",
    t_theme: "Switched to: ",
    t_cur_updating: "Updating exchange rates for",
    t_cur_ok: "Updated exchange rates to",
    t_cur_err: "Failed to update currencies!",
    t_conn_err: "Connection error!",
    t_alarm_ok: "Alarm settings saved successfully!",
    t_alarm_err: "Failed to save alarm settings",
    t_lang_switched: "Switched to English!",
    t_cmd_err: "Failed to send command",
    confirm_wifi_reset: "Confirm WiFi network reset?\n\nThe device will erase current WiFi credentials and restart into AP hotspot 'CYD-SmartDesk-V2' (192.168.4.1) so you can connect to a new WiFi network.",
    alert_rebooting: "Restarting device...\nPlease connect to WiFi 'CYD-SmartDesk-V2' on your phone/PC to configure.",
    cur_usd: "USD — US Dollar", cur_eur: "EUR — Euro", cur_cad: "CAD — Canadian Dollar",
    cur_jpy: "JPY — Japanese Yen", cur_gbp: "GBP — British Pound", cur_aud: "AUD — Australian Dollar",
    cur_sgd: "SGD — Singapore Dollar", cur_cny: "CNY — Chinese Yuan", cur_krw: "KRW — Korean Won",
    cur_chf: "CHF — Swiss Franc", cur_thb: "THB — Thai Baht",
    snd_0: "Mute", snd_1: "Mechanical Click", snd_2: "Water Drop", snd_3: "Soft Beep", snd_4: "Dual Chirp",
    pwa_title: "Install SmartDesk App",
    pwa_desc: "Full-screen standalone app experience",
    pwa_btn: "Install",
    pwa_ios_hint: "To install on iPhone:\n1. Tap the Share button in Safari (icon with arrow up)\n2. Tap 'Add to Home Screen'.",
    pwa_android_hint: "To install SmartDesk App on Android:\n1. Tap the menu icon (3 dots ⋮) at the top-right of Chrome.\n2. Tap 'Add to Home screen' (or 'Install app').\n3. Tap 'Add' to place the app on your home screen."
  }
};

function getDict() {
  return I18N[currentLang] || I18N[0];
}

function applyLanguage(l) {
  currentLang = l;
  const dict = getDict();
  const btn = document.getElementById('langBtn');
  if (btn) btn.textContent = (l === 1) ? '🇬🇧 English' : '🇻🇳 Tiếng Việt';

  document.querySelectorAll('[data-i18n]').forEach(el => {
    const k = el.getAttribute('data-i18n');
    if (dict[k]) {
      if (dict[k].indexOf('<') !== -1) {
        el.innerHTML = dict[k];
      } else {
        el.textContent = dict[k];
      }
    }
  });
  document.querySelectorAll('[data-i18n-ph]').forEach(el => {
    const k = el.getAttribute('data-i18n-ph');
    if (dict[k]) el.placeholder = dict[k];
  });

  const curNames = {
    USD: dict.cur_usd, EUR: dict.cur_eur, CAD: dict.cur_cad,
    JPY: dict.cur_jpy, GBP: dict.cur_gbp, AUD: dict.cur_aud,
    SGD: dict.cur_sgd, CNY: dict.cur_cny, KRW: dict.cur_krw,
    CHF: dict.cur_chf, THB: dict.cur_thb
  };
  ['cur1Select', 'cur2Select'].forEach(selId => {
    const sel = document.getElementById(selId);
    if (sel) {
      Array.from(sel.options).forEach(opt => {
        if (curNames[opt.value]) opt.textContent = curNames[opt.value];
      });
    }
  });

  const sndSel = document.getElementById('touchSndSelect');
  if (sndSel) {
    const sndNames = [dict.snd_0, dict.snd_1, dict.snd_2, dict.snd_3, dict.snd_4];
    Array.from(sndSel.options).forEach((opt, idx) => {
      if (sndNames[idx]) opt.textContent = sndNames[idx];
    });
  }

  const pBtns = document.querySelectorAll('#pageButtons .btn-chip');
  if (pBtns.length >= 6) {
    pBtns[0].textContent = dict.p0;
    pBtns[1].textContent = dict.p1;
    pBtns[2].textContent = dict.p2;
    pBtns[3].textContent = dict.p3;
    pBtns[4].textContent = dict.p4;
    pBtns[5].textContent = dict.p5;
  }

  // Cập nhật bộ nút Meme Pack theo ngôn ngữ
  const packNames = (l === 1) 
    ? ['🥟 Mochi Guy', '⭐ Golden Star', '🐰 White Rabbit']
    : ['🥟 Mochi Guy', '⭐ Sao Vàng', '🐰 Thỏ Trắng'];
  const packBtns = document.querySelectorAll('#packButtons .btn-chip');
  packBtns.forEach((b, idx) => { if (packNames[idx]) b.textContent = packNames[idx]; });

  // Cập nhật các nút Tâm trạng (Desk Mood) theo ngôn ngữ
  const moodNames = (l === 1)
    ? ['🟢 Happy', '🔴 Deep Focus', '⚡ Frustrated', '🟡 Drained 0%', '🟣 Bored', '🎉 Weekend / Party']
    : ['🟢 Vui vẻ', '🔴 Tập trung cao', '⚡ Đang bực mình', '🟡 Sập nguồn 0%', '🟣 Đang chán', '🎉 Cuối tuần / Quẩy'];
  const moodBtns = document.querySelectorAll('#moodButtons .btn-chip');
  moodBtns.forEach((b, idx) => { if (moodNames[idx]) b.textContent = moodNames[idx]; });

  // Cập nhật các nút Thú cưng (Desk Pet) theo ngôn ngữ
  const petNames = (l === 1)
    ? ['🍅 Tomato', '🐱 Pixel Cat', '🐕 Shiba Inu', '🦫 Capybara']
    : ['🍅 Cà chua', '🐱 Mèo Pixel', '🐕 Shiba Inu', '🦫 Capybara'];
  const petBtns = document.querySelectorAll('#petButtons .btn-chip');
  petBtns.forEach((b, idx) => { if (petNames[idx]) b.textContent = petNames[idx]; });

  // Cập nhật tùy chọn Âm khởi động theo ngôn ngữ (5 chế độ âm thanh)
  const startupSel = document.getElementById('startupSndSelect');
  if (startupSel) {
    const bootNames = (l === 1)
      ? ['🎶 Marimba Acoustic', '💎 Crystal Chime', '⚡ Cyber Synth', '🕹️ Retro 8-Bit', '🔇 Mute']
      : ['🎶 Marimba Acoustic', '💎 Pha lê (Crystal Chime)', '⚡ Điện tử (Cyber Synth)', '🕹️ Cổ điển (Retro 8-Bit)', '🔇 Tắt âm (Mute)'];
    Array.from(startupSel.options).forEach((opt, idx) => {
      if (bootNames[idx]) opt.textContent = bootNames[idx];
    });
  }

  // Cập nhật tiêu đề và trạng thái tùy chỉnh của Mood Editor
  updateMoodEditorUI();

  // Cập nhật trạng thái nút nguồn & thông tin chế độ nguồn
  updateScreenPowerUI();
  updatePowerScheduleUI();
}

let currentPowerMode = 0;
let isScreenOff = false;
let officeSchedule = { sh: 18, sm: 30, eh: 7, em: 45, en: true };
let homeSchedule = { sh: 23, sm: 0, eh: 6, em: 30, en: true };

function switchPowerMode(mode) {
  currentPowerMode = mode;
  highlightPowerMode(mode);
  updatePowerScheduleUI();
  const dict = getDict();
  showToast(mode === 0 ? dict.t_mode_office : dict.t_mode_home);
  fetch(`/api/power/schedule?mode=${mode}`).catch(() => {});
}

function highlightPowerMode(mode) {
  const btns = document.querySelectorAll('#powerModeButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === mode));
}

function updatePowerScheduleUI() {
  const dict = getDict();
  const sched = (currentPowerMode === 0) ? officeSchedule : homeSchedule;
  const sInp = document.getElementById('sleepStartTime');
  const eInp = document.getElementById('sleepEndTime');
  const enInp = document.getElementById('sleepScheduleEn');
  const enLabel = document.getElementById('sleepScheduleEnLabel');
  const sLabel = document.getElementById('sleepStartLabel');
  const eLabel = document.getElementById('sleepEndLabel');
  const hintEl = document.getElementById('powerHintText');

  if (sInp && !sInp.matches(':focus')) {
    sInp.value = `${String(sched.sh).padStart(2, '0')}:${String(sched.sm).padStart(2, '0')}`;
  }
  if (eInp && !eInp.matches(':focus')) {
    eInp.value = `${String(sched.eh).padStart(2, '0')}:${String(sched.em).padStart(2, '0')}`;
  }
  if (enInp && !enInp.matches(':focus')) {
    enInp.checked = sched.en;
  }

  if (currentPowerMode === 0) {
    if (enLabel) enLabel.textContent = dict.sleep_sched_en_off || dict.sleep_sched_en;
    if (sLabel) sLabel.textContent = dict.sleep_start_lbl_off || dict.sleep_start_lbl;
    if (eLabel) eLabel.textContent = dict.sleep_end_lbl_off || dict.sleep_end_lbl;
    if (hintEl) hintEl.innerHTML = dict.power_hint_off || dict.power_hint;
  } else {
    if (enLabel) enLabel.textContent = dict.sleep_sched_en_hom || "Hẹn giờ đi ngủ (Mờ dịu 5% đêm):";
    if (sLabel) sLabel.textContent = dict.sleep_start_lbl_hom || "Giờ đi ngủ (Dịu mờ 5%):";
    if (eLabel) eLabel.textContent = dict.sleep_end_lbl_hom || "Giờ thức dậy (Sáng lại):";
    if (hintEl) hintEl.innerHTML = dict.power_hint_hom || "";
  }
}

async function toggleScreenPower() {
  const nextOff = !isScreenOff;
  try {
    const res = await fetch(`/api/power/screen?off=${nextOff ? 1 : 0}`);
    if (res.ok) {
      isScreenOff = nextOff;
      updateScreenPowerUI();
      const dict = getDict();
      showToast(nextOff ? dict.t_screen_turned_off : dict.t_screen_turned_on);
    }
  } catch (e) {
    showToast(getDict().t_conn_err, false);
  }
}

function updateScreenPowerUI() {
  const badge = document.getElementById('screenStateBadge');
  const btn = document.getElementById('screenPowerBtn');
  const btnText = document.getElementById('screenPowerBtnText');
  const dict = getDict();
  if (!dict) return;
  if (isScreenOff) {
    if (badge) {
      badge.textContent = dict.screen_state_off;
      badge.style.color = '#8899B0';
    }
    if (btn) {
      btn.style.borderColor = 'rgba(0, 230, 118, 0.5)';
      btn.style.color = '#00E676';
    }
    if (btnText) btnText.textContent = dict.btn_screen_on;
  } else {
    if (badge) {
      badge.textContent = dict.screen_state_on;
      badge.style.color = 'var(--success)';
    }
    if (btn) {
      btn.style.borderColor = 'rgba(255, 59, 48, 0.5)';
      btn.style.color = '#ff6b6b';
    }
    if (btnText) btnText.textContent = dict.btn_screen_off;
  }
}

async function savePowerSchedule() {
  const en = document.getElementById('sleepScheduleEn').checked ? 1 : 0;
  const startTime = document.getElementById('sleepStartTime').value || (currentPowerMode === 0 ? "18:30" : "23:00");
  const endTime = document.getElementById('sleepEndTime').value || (currentPowerMode === 0 ? "07:45" : "06:30");
  const sParts = startTime.split(':');
  const eParts = endTime.split(':');
  const sh = parseInt(sParts[0], 10);
  const sm = parseInt(sParts[1], 10);
  const eh = parseInt(eParts[0], 10);
  const em = parseInt(eParts[1], 10);

  if (currentPowerMode === 0) {
    officeSchedule = { sh, sm, eh, em, en: !!en };
  } else {
    homeSchedule = { sh, sm, eh, em, en: !!en };
  }

  const dict = getDict();
  try {
    const res = await fetch(`/api/power/schedule?mode=${currentPowerMode}&sh=${sh}&sm=${sm}&eh=${eh}&em=${em}&en=${en}`);
    if (res.ok) {
      showToast(dict.t_power_saved);
    } else {
      showToast(dict.t_conn_err, false);
    }
  } catch (e) {
    showToast(dict.t_conn_err, false);
  }
}

function showToast(msg, isSuccess = true) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.borderColor = isSuccess ? 'var(--accent)' : 'var(--danger)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

let pageDebounce = null;
function switchPage(pageId) {
  highlightPage(pageId);
  const dict = getDict();
  const pageNames = [dict.p0, dict.p1, dict.p2, dict.p3, dict.p4, dict.p5];
  showToast(`${dict.t_page}${pageNames[pageId] || pageId}`);
  clearTimeout(pageDebounce);
  pageDebounce = setTimeout(() => {
    fetch(`/api/page?id=${pageId}`).catch(() => {});
  }, 80);
}

let soundDebounce = null;
function changeSound() {
  const touch = document.getElementById('touchSndSelect').value;
  const melody = document.getElementById('alarmMelodySelect').value;
  const startup = document.getElementById('startupSndSelect').value;
  clearTimeout(soundDebounce);
  soundDebounce = setTimeout(() => {
    const dict = getDict();
    fetch(`/api/sound?touch=${touch}&melody=${melody}&startup=${startup}`)
      .then(r => r.json())
      .then(() => showToast(dict.t_snd_save))
      .catch(() => showToast(dict.t_snd_err, false));
  }, 100);
}

function testStartupSound() {
  showToast(getDict().t_test_startup || "Đang phát thử âm khởi động...");
  fetch('/api/sound?testStartup=1').catch(() => {});
}

function testAlarmMelody() {
  showToast(getDict().t_test_melody);
  fetch('/api/sound?test=1').catch(() => {});
}

const THEME_NAMES = [
  'Cyber Neon', 'Minimal Obsidian', 'Sunset Amber', 'Cyberpunk',
  'Pastel Lavender', 'Nordic Frost', 'Sakura Blossom', 'Emerald Forest'
];

let themeDebounce = null;
function switchTheme(themeId) {
  highlightTheme(themeId);
  const dict = getDict();
  showToast(`${dict.t_theme}${THEME_NAMES[themeId] || themeId}`);
  clearTimeout(themeDebounce);
  themeDebounce = setTimeout(() => {
    fetch(`/api/theme?id=${themeId}`).catch(() => {});
  }, 100);
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

function getPetName(petId) {
  const arr = (currentLang === 1)
    ? ['Tomato', 'Pixel Cat', 'Shiba Inu', 'Capybara']
    : ['Cà chua', 'Mèo Pixel', 'Shiba Inu', 'Capybara'];
  return arr[petId] || ('Pet ' + petId);
}

function highlightPet(id) {
  const btns = document.querySelectorAll('#petButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === id));
}
function switchPet(petId) {
  highlightPet(petId);
  const dict = getDict();
  showToast(`${dict.t_pet}${getPetName(petId)}`);
  fetch(`/api/pet?set=${petId}`).catch(() => {});
}

let currentPomoState = 0; // 0=STOPPED, 1=RUNNING, 2=PAUSED
let currentPomoMode = 0;

function updateWebPomoUI(state, mode, remSec) {
  currentPomoState = state;
  currentPomoMode = mode;
  const modeBtns = document.querySelectorAll('#pomoModeButtons .btn-chip');
  modeBtns.forEach((b, idx) => b.classList.toggle('active', idx === mode));

  const timerEl = document.getElementById('webPomoTimer');
  if (timerEl && remSec !== undefined) {
    const mm = String(Math.floor(remSec / 60)).padStart(2, '0');
    const ss = String(remSec % 60).padStart(2, '0');
    timerEl.textContent = `${mm}:${ss}`;
  }

  const startBtn = document.getElementById('btnWebPomoStart');
  if (startBtn) {
    if (state === 1) {
      startBtn.textContent = (currentLang === 1) ? '⏸️ Pause Pomodoro' : '⏸️ Tạm dừng Pomodoro';
      startBtn.style.background = 'linear-gradient(135deg, #ff5e00, #ff2a55)';
    } else if (state === 2) {
      startBtn.textContent = (currentLang === 1) ? '▶️ Resume Pomodoro' : '▶️ Tiếp tục Pomodoro';
      startBtn.style.background = '';
    } else {
      startBtn.textContent = (currentLang === 1) ? '▶️ Start Pomodoro (Sync EMO)' : '▶️ Bắt đầu Pomodoro (Đồng bộ EMO)';
      startBtn.style.background = '';
    }
  }
}

function toggleWebPomodoro() {
  const act = (currentPomoState === 1) ? 'pause' : 'start';
  fetch(`/api/pomodoro?action=${act}`)
    .then(() => {
      showToast(act === 'start'
        ? ((currentLang === 1) ? 'Pomodoro Started!' : 'Đã kích hoạt Pomodoro & đồng bộ EMO!')
        : ((currentLang === 1) ? 'Pomodoro Paused!' : 'Đã tạm dừng Pomodoro!'));
      setTimeout(fetchStatus, 150);
    })
    .catch(() => {});
}

function resetWebPomodoro() {
  fetch('/api/pomodoro?action=reset')
    .then(() => {
      showToast((currentLang === 1) ? 'Pomodoro Reset!' : 'Đã đặt lại Pomodoro!');
      setTimeout(fetchStatus, 150);
    })
    .catch(() => {});
}

function setWebPomoMode(m) {
  fetch(`/api/pomodoro?action=mode&m=${m}`)
    .then(() => setTimeout(fetchStatus, 150))
    .catch(() => {});
}

function getPackName(packId) {
  const arr = (currentLang === 1)
    ? ['Mochi Guy', 'Golden Star', 'White Rabbit']
    : ['Mochi Guy', 'Sao Vàng', 'Thỏ Trắng'];
  return arr[packId] || ('Pack ' + packId);
}

function highlightPack(id) {
  const btns = document.querySelectorAll('#packButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === id));
}
function switchPack(packId) {
  highlightPack(packId);
  const dict = getDict();
  showToast(`${dict.t_pack || "Bộ meme: "}${getPackName(packId)}`);
  fetch(`/api/mood?pack=${packId}`).catch(() => {});
}

function getMoodName(m) {
  const arr = (currentLang === 1)
    ? ['🟢 Happy', '🔴 Deep Focus', '⚡ Frustrated', '🟡 Drained 0%', '🟣 Bored', '🎉 Weekend / Party']
    : ['🟢 Vui vẻ', '🔴 Tập trung cao', '⚡ Đang bực mình', '🟡 Sập nguồn 0%', '🟣 Đang chán', '🎉 Cuối tuần / Quẩy'];
  return arr[m] || ('Mood ' + m);
}

let currentEditMood = 0;
let customMoods = [
  { en: false, tag: "", msg: "" },
  { en: false, tag: "", msg: "" },
  { en: false, tag: "", msg: "" },
  { en: false, tag: "", msg: "" },
  { en: false, tag: "", msg: "" },
  { en: false, tag: "", msg: "" }
];

const QUICK_SUGGESTIONS = [
  // 0: Happy
  [ { t: 'ĐANG CHILL', m: 'Cà phê sáng không? • Mình bao!' }, { t: 'GOOD VIBES', m: 'Ngày mới tràn đầy năng lượng!' } ],
  // 1: Focus
  [ { t: 'ĐANG SPRINT', m: '100% Focus • Không tiếp khách!' }, { t: 'DEEP WORK', m: 'Đang chạy deadline đến trưa!' } ],
  // 2: Bug
  [ { t: 'ĐANG FIX BUG', m: 'Xin đừng làm phiền • Đang rất quạu!' }, { t: 'DND - CAY CÚ', m: 'Bug chưa fix xong • Chờ một chút!' } ],
  // 3: Low bat
  [ { t: 'SẬP NGUỒN 0%', m: 'Cần nạp trà sữa gấp!' }, { t: 'HẾT PIN RỒI', m: 'Đi sạc pin một lát sẽ quay lại!' } ],
  // 4: Chill
  [ { t: 'BƠ PHỜ / CHÁN', m: 'Chừng nào mới được tan ca đây?' }, { t: 'BUỒN NGỦ QUÁ', m: 'Nghỉ ngơi mắt 5 phút nạp năng lượng!' } ],
  // 5: TGIF
  [ { t: 'QUẨY THÔI!', m: 'Tối nay xóa hết deadline!' }, { t: 'TGIF - CHILL', m: 'Cuối tuần rồi • Đi quẩy ngay thôi!' } ]
];

const QUICK_SUGGESTIONS_EN = [
  // 0: Happy
  [ { t: 'JUST CHILLING', m: 'Morning coffee? * My treat!' }, { t: 'GOOD VIBES', m: 'Full of positive energy!' } ],
  // 1: Focus
  [ { t: 'IN A SPRINT', m: '100% Focus * Do not disturb!' }, { t: 'DEEP WORK', m: 'Crushing deadlines today!' } ],
  // 2: Bug
  [ { t: 'FIXING BUGS', m: 'Please do not interrupt!' }, { t: 'DND - DEBUGGING', m: 'Critical bug in progress!' } ],
  // 3: Low bat
  [ { t: 'DRAINED 0%', m: 'Need boba milk tea ASAP!' }, { t: 'BATTERY EMPTY', m: 'Recharging my brain now!' } ],
  // 4: Chill
  [ { t: 'SO BORED', m: 'When is it clock-out time?' }, { t: 'SLEEPY VIBES', m: 'Need a 5-min power nap!' } ],
  // 5: TGIF
  [ { t: 'PARTY TIME!', m: 'No deadlines tonight!' }, { t: 'TGIF - CHILL', m: 'Weekend mode activated!' } ]
];

function highlightMood(id) {
  const btns = document.querySelectorAll('#moodButtons .btn-chip');
  btns.forEach((b, idx) => b.classList.toggle('active', idx === id));
}

function renderQuickMoodChips(m) {
  const container = document.getElementById('quickMoodContainer');
  if (!container) return;
  const list = ((currentLang === 1) ? QUICK_SUGGESTIONS_EN : QUICK_SUGGESTIONS)[m] || QUICK_SUGGESTIONS[0];
  container.innerHTML = list.map(item => 
    `<button type="button" class="btn-chip" style="font-size: 11px; padding: 4px 8px;" onclick="setQuickMood('${item.t}', '${item.m}')">${item.t}</button>`
  ).join('');
}

function updateMoodEditorUI() {
  const m = currentEditMood;
  const lbl = document.getElementById('curEditMoodName');
  if (lbl) lbl.textContent = getMoodName(m);

  const cm = customMoods[m] || { en: false, tag: "", msg: "" };
  const toggle = document.getElementById('customMoodToggle');
  const wrap = document.getElementById('customMoodInputs');
  if (toggle) toggle.checked = cm.en;
  if (wrap) wrap.style.display = cm.en ? 'block' : 'none';

  const tagInp = document.getElementById('customTagInput');
  const msgInp = document.getElementById('customMsgInput');
  if (tagInp && !tagInp.matches(':focus')) tagInp.value = cm.tag || '';
  if (msgInp && !msgInp.matches(':focus')) msgInp.value = cm.msg || '';

  renderQuickMoodChips(m);
  updateMoodCounters();
}

function switchMood(moodId) {
  highlightMood(moodId);
  currentEditMood = moodId;
  const dict = getDict();
  showToast(`${dict.t_mood}${getMoodName(moodId)}`);
  fetch(`/api/mood?set=${moodId}`).catch(() => {});
  updateMoodEditorUI();
}

function updateMoodCounters() {
  const tagInp = document.getElementById('customTagInput');
  const msgInp = document.getElementById('customMsgInput');
  const tc = document.getElementById('customTagCount');
  const mc = document.getElementById('customMsgCount');
  if (tagInp && tc) tc.textContent = `${tagInp.value.length}/20`;
  if (msgInp && mc) mc.textContent = `${msgInp.value.length}/36`;
}

function setQuickMood(tag, msg) {
  const toggle = document.getElementById('customMoodToggle');
  if (toggle) toggle.checked = true;
  const wrap = document.getElementById('customMoodInputs');
  if (wrap) wrap.style.display = 'block';
  const tagInp = document.getElementById('customTagInput');
  const msgInp = document.getElementById('customMsgInput');
  if (tagInp) tagInp.value = tag;
  if (msgInp) msgInp.value = msg;
  updateMoodCounters();
}

function toggleCustomMood(en) {
  const m = currentEditMood;
  if (!customMoods[m]) customMoods[m] = { en: false, tag: "", msg: "" };
  customMoods[m].en = en;
  const wrap = document.getElementById('customMoodInputs');
  if (wrap) wrap.style.display = en ? 'block' : 'none';
  if (!en) {
    fetch(`/api/mood?m=${m}&custom=0`)
      .then(r => r.json())
      .then(() => showToast(`${getDict().t_mood_default || 'Đã khôi phục mặc định: '} ${getMoodName(m)}!`))
      .catch(() => {});
  }
}

async function saveCustomMood() {
  const m = currentEditMood;
  const en = document.getElementById('customMoodToggle').checked ? 1 : 0;
  let tag = document.getElementById('customTagInput').value.trim();
  let msg = document.getElementById('customMsgInput').value.trim();

  tag = tag.normalize('NFC').toUpperCase();
  msg = msg.normalize('NFC');

  document.getElementById('customTagInput').value = tag;
  document.getElementById('customMsgInput').value = msg;
  updateMoodCounters();

  customMoods[m] = { en: (en === 1), tag: tag, msg: msg };

  try {
    const res = await fetch(`/api/mood?m=${m}&custom=${en}&tag=${encodeURIComponent(tag)}&msg=${encodeURIComponent(msg)}`);
    if (res.ok) {
      showToast(`${getDict().t_mood_saved || 'Đã lưu trạng thái: '} ${getMoodName(m)}!`);
    } else {
      showToast(getDict().t_cmd_err || 'Lỗi khi lưu trạng thái!', false);
    }
  } catch(e) {
    showToast(getDict().t_conn_err || 'Lỗi kết nối!', false);
  }
}

function resetDefaultMood() {
  const m = currentEditMood;
  customMoods[m] = { en: false, tag: "", msg: "" };
  updateMoodEditorUI();
  fetch(`/api/mood?m=${m}&custom=0`)
    .then(r => r.json())
    .then(() => showToast(`${getDict().t_mood_default || 'Đã khôi phục mặc định: '} ${getMoodName(m)}!`))
    .catch(() => {});
}

async function saveCurrencies() {
  const c1 = document.getElementById('cur1Select').value;
  const c2 = document.getElementById('cur2Select').value;
  const dict = getDict();
  showToast(`${dict.t_cur_updating} ${c1} & ${c2}...`);
  try {
    const res = await fetch(`/api/currency?c1=${c1}&c2=${c2}`);
    if (res.ok) {
      showToast(`${dict.t_cur_ok} ${c1} & ${c2}!`);
    } else {
      showToast(dict.t_cur_err, false);
    }
  } catch (e) {
    showToast(dict.t_conn_err, false);
  }
}

function setQuickMsg(type) {
  const msgs = {
    drink: (currentLang === 1) ? 'Drink water & Relax' : 'Uống nước & Thư giãn',
    sport: (currentLang === 1) ? 'Stretch & Move' : 'Đứng dậy & Vươn vai',
    rest:  (currentLang === 1) ? 'Take a 5-min break' : 'Nghỉ ngơi mắt 5 phút',
    work:  (currentLang === 1) ? 'Time to leave work!' : 'Hết giờ làm việc!'
  };
  document.getElementById('alarmMsgInput').value = msgs[type] || type;
}

async function saveAlarmSettings() {
  const timeVal = document.getElementById('alarmTimeInput').value;
  const en = document.getElementById('alarmEnInput').checked ? 1 : 0;
  const msg = encodeURIComponent(document.getElementById('alarmMsgInput').value.trim());
  const dict = getDict();

  let h = 7, m = 0;
  if (timeVal) {
    const parts = timeVal.split(':');
    h = parseInt(parts[0], 10);
    m = parseInt(parts[1], 10);
  }

  try {
    const res = await fetch(`/api/alarm?h=${h}&m=${m}&en=${en}&msg=${msg}`);
    if (res.ok) {
      showToast(dict.t_alarm_ok);
    } else {
      showToast(dict.t_alarm_err, false);
    }
  } catch (e) {
    showToast(dict.t_conn_err, false);
  }
}

let isInitialStatusLoaded = false;
let autoSyncTimer = null;

async function fetchStatus() {
  try {
    const res = await fetch('/status');
    if (!res.ok) throw new Error('Status fetch failed');
    const data = await res.json();

    // Update connection status badge
    const badge = document.getElementById('statusBadge');
    if (badge) {
      badge.textContent = (currentLang === 1) ? 'Online' : 'Trực tuyến';
      badge.style.background = 'rgba(0, 230, 118, 0.15)';
      badge.style.color = 'var(--success)';
      badge.style.borderColor = 'rgba(0, 230, 118, 0.3)';
    }

    const ecoBadge = document.getElementById('ecoBadge');
    if (ecoBadge && data.ecoPaired !== undefined) {
      if (data.ecoPaired) {
        ecoBadge.textContent = (currentLang === 1) ? '🤖 EMO Linked' : '🤖 Đã nối EMO';
        ecoBadge.style.background = 'rgba(0, 229, 255, 0.15)';
        ecoBadge.style.color = 'var(--accent)';
        ecoBadge.style.borderColor = 'rgba(0, 229, 255, 0.35)';
      } else {
        ecoBadge.textContent = (currentLang === 1) ? '🤖 Standalone' : '🤖 Độc lập';
        ecoBadge.style.background = 'rgba(255, 255, 255, 0.06)';
        ecoBadge.style.color = 'var(--text-dim)';
        ecoBadge.style.borderColor = 'rgba(255, 255, 255, 0.12)';
      }
    }

    highlightPage(data.page);
    highlightTheme(data.theme);

    const bSlider = document.getElementById('brightSlider');
    if (bSlider && !bSlider.matches(':active')) {
      bSlider.value = data.brightness;
      document.getElementById('brightVal').textContent = data.brightness + '%';
    }

    const cur1 = document.getElementById('cur1Select');
    if (cur1 && !cur1.matches(':focus') && data.cur1) cur1.value = data.cur1;
    const cur2 = document.getElementById('cur2Select');
    if (cur2 && !cur2.matches(':focus') && data.cur2) cur2.value = data.cur2;

    if (data.ip) document.getElementById('wifiIp').textContent = data.ip;
    if (data.rssi !== undefined) document.getElementById('wifiRssi').textContent = data.rssi;

    if (data.alarmHour !== undefined && data.alarmMinute !== undefined) {
      const hh = String(data.alarmHour).padStart(2, '0');
      const mm = String(data.alarmMinute).padStart(2, '0');
      const timeInp = document.getElementById('alarmTimeInput');
      if (timeInp && !timeInp.matches(':focus')) timeInp.value = `${hh}:${mm}`;
    }
    if (data.alarmEnabled !== undefined) {
      const enInp = document.getElementById('alarmEnInput');
      if (enInp && !enInp.matches(':focus')) enInp.checked = data.alarmEnabled;
    }
    if (data.alarmMsg !== undefined) {
      const msgInp = document.getElementById('alarmMsgInput');
      if (msgInp && !msgInp.matches(':focus') && !msgInp.value) msgInp.value = data.alarmMsg;
    }
    if (data.touchSound !== undefined) {
      const s = document.getElementById('touchSndSelect');
      if (s && !s.matches(':focus')) s.value = data.touchSound;
    }
    if (data.alarmMelody !== undefined) {
      const m = document.getElementById('alarmMelodySelect');
      if (m && !m.matches(':focus')) m.value = data.alarmMelody;
    }
    if (data.startupSound !== undefined) {
      const b = document.getElementById('startupSndSelect');
      if (b && !b.matches(':focus')) b.value = data.startupSound;
    }
    if (data.pet !== undefined) {
      highlightPet(data.pet);
    }
    if (data.pomoState !== undefined) {
      updateWebPomoUI(data.pomoState, data.pomoMode || 0, data.pomoRemSec);
    }
    if (data.mood !== undefined) {
      highlightMood(data.mood);
      if (!isInitialStatusLoaded) {
        currentEditMood = data.mood;
      }
    }
    if (data.pack !== undefined) {
      highlightPack(data.pack);
    }

    if (data.screenOff !== undefined) {
      isScreenOff = data.screenOff;
      updateScreenPowerUI();
    }
    if (data.powerMode !== undefined) {
      currentPowerMode = data.powerMode;
      highlightPowerMode(data.powerMode);
    }
    if (data.offSh !== undefined) {
      officeSchedule = {
        sh: data.offSh,
        sm: data.offSm,
        eh: data.offEh,
        em: data.offEm,
        en: !!data.offEn
      };
    }
    if (data.homSh !== undefined) {
      homeSchedule = {
        sh: data.homSh,
        sm: data.homSm,
        eh: data.homEh,
        em: data.homEm,
        en: !!data.homEn
      };
    }
    updatePowerScheduleUI();

    const tagInp = document.getElementById('customTagInput');
    const msgInp = document.getElementById('customMsgInput');
    const isEditingCustom = (tagInp && tagInp.matches(':focus')) || (msgInp && msgInp.matches(':focus'));
    if (data.customMoods && Array.isArray(data.customMoods) && !isEditingCustom) {
      customMoods = data.customMoods;
      updateMoodEditorUI();
    }

    if (data.lang !== undefined && data.lang !== currentLang) {
      applyLanguage(data.lang);
    }

    if (data.wifiProfiles && Array.isArray(data.wifiProfiles)) {
      renderWifiProfiles(data.wifiProfiles);
    }

    isInitialStatusLoaded = true;
  } catch (e) {
    const badge = document.getElementById('statusBadge');
    if (badge) {
      badge.textContent = (currentLang === 1) ? 'Offline' : 'Mất kết nối';
      badge.style.background = 'rgba(255, 59, 48, 0.15)';
      badge.style.color = 'var(--danger)';
      badge.style.borderColor = 'rgba(255, 59, 48, 0.3)';
    }
  }
}

function renderWifiProfiles(profiles) {
  const container = document.getElementById('wifiProfilesList');
  const countEl = document.getElementById('wifiProfileCount');
  if (countEl) countEl.textContent = `${profiles.length}/3`;
  if (!container) return;
  if (!profiles || profiles.length === 0) {
    container.innerHTML = `<div style="font-size:11px; color:var(--text-dim); font-style:italic; padding:4px 0;">Chưa lưu mạng nào (Đang dùng AP hoặc mạng mặc định).</div>`;
    return;
  }
  container.innerHTML = profiles.map(p => {
    const s = String(p.ssid || '');
    const isAct = !!p.active;
    return `<div style="display:flex; justify-content:space-between; align-items:center; background:rgba(255,255,255,0.04); border:1px solid ${isAct ? 'var(--accent)' : 'var(--card-border)'}; border-radius:10px; padding:7px 12px;">` +
      `<div style="display:flex; align-items:center; gap:8px; overflow:hidden;">` +
        `<span style="font-size:13px; color:${isAct ? 'var(--accent)' : '#fff'}; font-weight:${isAct ? '700' : '500'}; white-space:nowrap; text-overflow:ellipsis; overflow:hidden;">${s}</span>` +
        (isAct ? `<span style="font-size:9px; background:rgba(0,229,255,0.15); color:var(--accent); border:1px solid var(--accent); padding:1px 6px; border-radius:10px; font-weight:700;">ONLINE</span>` : '') +
      `</div>` +
      `<button onclick="deleteWifiProfile('${s}')" style="background:none; border:none; color:var(--danger); cursor:pointer; font-size:15px; padding:2px 8px; border-radius:6px; font-weight:bold;" title="Xóa mạng">✕</button>` +
    `</div>`;
  }).join('');
}

async function deleteWifiProfile(ssid) {
  const dict = getDict();
  if (!confirm((dict.t_wifi_del_confirm || 'Xác nhận xóa?') + '\n' + ssid)) return;
  try {
    const res = await fetch('/api/wifi/delete?ssid=' + encodeURIComponent(ssid), { method: 'POST' });
    const d = await res.json();
    if (d.success) {
      showToast((dict.t_wifi_deleted || 'Đã xóa mạng: ') + ssid);
      fetchStatus();
    } else {
      showToast(dict.t_wifi_del_err || 'Lỗi xóa mạng', false);
    }
  } catch (e) {
    showToast(dict.t_wifi_del_err || 'Lỗi xóa mạng', false);
  }
}

function toggleAddWifiForm(show) {
  const f = document.getElementById('addWifiForm');
  if (f) f.style.display = show ? 'block' : 'none';
  if (show) {
    const inp = document.getElementById('newWifiSsid');
    if (inp) inp.focus();
  }
}

async function submitNewWifi() {
  const ssid = (document.getElementById('newWifiSsid').value || '').trim();
  const pass = document.getElementById('newWifiPass').value || '';
  const dict = getDict();
  if (!ssid) {
    showToast(dict.t_ssid_empty || 'Vui lòng nhập tên WiFi', false);
    return;
  }
  try {
    const res = await fetch(`/api/wifi/add?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`, { method: 'POST' });
    if (res.ok) {
      showToast(dict.t_wifi_saved || 'Đã lưu mạng thành công!');
      toggleAddWifiForm(false);
      document.getElementById('newWifiSsid').value = '';
      document.getElementById('newWifiPass').value = '';
      fetchStatus();
    } else {
      showToast(dict.t_cmd_err || 'Lỗi lưu mạng', false);
    }
  } catch (e) {
    showToast(dict.t_conn_err || 'Lỗi kết nối', false);
  }
}

async function openWiFiPortal() {
  const dict = getDict();
  if (!confirm(dict.t_wifi_portal_prompt || 'Mở màn hình cài đặt WiFi trên CYD?')) return;
  try {
    const res = await fetch('/api/wifi/portal', { method: 'POST' });
    const d = await res.json();
    showToast(d.message || 'Đang mở màn hình cài đặt WiFi...');
  } catch (e) {
    showToast(dict.t_cmd_err || 'Lỗi gửi lệnh', false);
  }
}

function startAutoSync() {
  if (autoSyncTimer) clearInterval(autoSyncTimer);
  autoSyncTimer = setInterval(() => {
    // Only poll when browser tab is visible (zero battery/network drain when hidden)
    if (!document.hidden) {
      fetchStatus();
    }
  }, 3000);
}

document.addEventListener('visibilitychange', () => {
  if (!document.hidden) {
    fetchStatus();
  }
});

async function toggleWebLang() {
  const next = (currentLang === 1) ? 0 : 1;
  applyLanguage(next);
  const dict = getDict();
  try {
    await fetch('/api/lang?set=' + next);
    showToast(dict.t_lang_switched);
  } catch (e) {
    showToast(dict.t_cmd_err, false);
  }
}

function resetWifi() {
  const dict = getDict();
  if (confirm(dict.confirm_wifi_reset)) {
    fetch('/api/wifi/reset', { method: 'POST' })
      .then(res => res.json())
      .then(d => {
        alert(dict.alert_rebooting);
      })
      .catch(e => showToast(dict.t_cmd_err, false));
  }
}

// ── PWA & Service Worker ──────────────────────────────────────────────
if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('/sw.js').catch(() => {});
  });
}

let deferredPrompt = null;
const isStandalone = window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone === true;

window.addEventListener('beforeinstallprompt', (e) => {
  e.preventDefault();
  deferredPrompt = e;
  if (!isStandalone) {
    const banner = document.getElementById('pwaBanner');
    if (banner) banner.style.display = 'flex';
  }
});

function installPWA() {
  const dict = getDict();
  if (deferredPrompt) {
    deferredPrompt.prompt();
    deferredPrompt.userChoice.then((choice) => {
      if (choice.outcome === 'accepted') {
        dismissPWA();
      }
      deferredPrompt = null;
    });
  } else {
    const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream;
    if (isIOS) {
      alert(dict.pwa_ios_hint);
    } else {
      alert(dict.pwa_android_hint);
    }
  }
}

function dismissPWA() {
  const banner = document.getElementById('pwaBanner');
  if (banner) banner.style.display = 'none';
}

window.addEventListener('DOMContentLoaded', () => {
  if (!isStandalone) {
    const banner = document.getElementById('pwaBanner');
    if (banner) banner.style.display = 'flex';
  }
});

applyLanguage(0);
fetchStatus();
startAutoSync();
</script>
</body>
</html>
)rawliteral";

WebDashboard::WebDashboard()
    : _server(80),
      _running(false),
      _routesSetup(false),
      _portalRequested(false),
      _testTouchRequested(false),
      _testAlarmRequested(false),
      _testStartupRequested(false),
      _wifiResetRequested(false),
      _wifiResetTime(0) {}

void WebDashboard::begin() {
  if (_running)
    return;
  if (!_routesSetup) {
    _setupRoutes();
    _routesSetup = true;
  }
  _server.begin();
  _running = true;

  // Vô hiệu hóa WiFi Modem Sleep để phản hồi Web tức thì (không bị lag/delay sau khi idle)
  WiFi.setSleep(false);

  // Kích hoạt mDNS responder: cho phép truy cập qua http://smartdesk.local/
  if (MDNS.begin("smartdesk")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] Responder started at http://smartdesk.local/");
  } else {
    Serial.println("[mDNS] Failed to start responder");
  }

  Serial.printf("[WebDashboard] HTTP Server started at http://%s/ (or "
                "http://smartdesk.local/)\n",
                WiFi.localIP().toString().c_str());
}

void WebDashboard::stop() {
  if (!_running) return;
  _server.end();
  _running = false;
  Serial.println("[WebDashboard] HTTP Server stopped cleanly");
}

void WebDashboard::update() {
  // Xử lý phát thử âm thanh hoặc Reset WiFi trên luồng chính Core 1 (không gây nghẽn async_tcp)
  if (_testTouchRequested) {
    _testTouchRequested = false;
    hardware.playTouchSound();
  }
  if (_testAlarmRequested) {
    _testAlarmRequested = false;
    hardware.playAlarmMelody();
  }
  if (_testStartupRequested) {
    _testStartupRequested = false;
    hardware.playStartupSound();
  }
  if (_wifiResetRequested && (millis() - _wifiResetTime >= 500)) {
    _wifiResetRequested = false;
    Serial.println("[WebDashboard] Executing scheduled WiFi Reset & Reboot...");
    WiFi.disconnect(true, true);
    delay(200);
    ESP.restart();
  }
}

void WebDashboard::_setupRoutes() {
  // ── GET /: Serve mobile dashboard ──────────────────────────────
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // ── GET /manifest.json: PWA Web App Manifest ──────────────────
  _server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/manifest+json", MANIFEST_JSON);
  });

  // ── GET /sw.js: PWA Service Worker ────────────────────────────
  _server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/javascript", SW_JS);
  });

  // ── GET /app-icon.png: PWA App Icon 192x192 ───────────────────
  _server.on("/app-icon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", APP_ICON_PNG, APP_ICON_PNG_LEN);
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
  });

  // ── GET /status: System telemetry ─────────────────────────────
  _server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["ecoPaired"] = ecosystem.isCompanionPaired();
    doc["ecoStatus"] = ecosystem.getStatusText(getLanguage() == LANG_EN);
    doc["page"] = display.getPage();
    doc["theme"] = getThemeIndex();
    doc["themeName"] = getThemeName();
    doc["brightness"] = hardware.getBacklight();
    const WeatherData &w = weather.getData();
    doc["city"] = w.city;
    doc["temp"] = w.temp;
    doc["cur1"] = finance.getCur1();
    doc["cur2"] = finance.getCur2();
    doc["alarmHour"] = display.getAlarmHour();
    doc["alarmMinute"] = display.getAlarmMinute();
    doc["alarmEnabled"] = display.isAlarmEnabled();
    doc["alarmMsg"] = display.getAlarmMessage();
    doc["touchSound"] = hardware.getTouchSound();
    doc["alarmMelody"] = hardware.getAlarmMelody();
    doc["startupSound"] = hardware.getStartupSound();
    doc["lang"] = (int)getLanguage();
    doc["langName"] = getLanguageName();
    doc["pet"] = (int)deskPet.getType();
    doc["petName"] = deskPet.getNameCurrent();
    doc["pomoState"] = display.getPomoState();
    doc["pomoMode"] = display.getPomoMode();
    doc["pomoRemSec"] = display.getPomoRemainingSec();
    doc["mood"] = (int)deskMood.getMood();
    doc["moodName"] = deskMood.getName(deskMood.getMood());
    doc["pack"] = (int)deskMood.getPack();
    doc["packName"] = deskMood.getPackName(deskMood.getPack());
    doc["useCustomMood"] = deskMood.isCurrentCustom();
    doc["customTag"] = deskMood.getCustomTag(deskMood.getMood());
    doc["customMsg"] = deskMood.getCustomMessage(deskMood.getMood());

    JsonArray cArr = doc["customMoods"].to<JsonArray>();
    for (int i = 0; i < MOOD_COUNT; i++) {
        JsonObject mObj = cArr.add<JsonObject>();
        mObj["en"] = deskMood.isCustom((MoodType)i);
        mObj["tag"] = deskMood.getCustomTag((MoodType)i);
        mObj["msg"] = deskMood.getCustomMessage((MoodType)i);
    }

    doc["screenOff"] = hardware.isScreenOff();
    doc["powerMode"] = hardware.getPowerMode();

    uint8_t offSh, offSm, offEh, offEm;
    bool offEn;
    hardware.getOfficeSchedule(offSh, offSm, offEh, offEm, offEn);
    doc["offSh"] = offSh;
    doc["offSm"] = offSm;
    doc["offEh"] = offEh;
    doc["offEm"] = offEm;
    doc["offEn"] = offEn;

    uint8_t homSh, homSm, homEh, homEm;
    bool homEn;
    hardware.getHomeSchedule(homSh, homSm, homEh, homEm, homEn);
    doc["homSh"] = homSh;
    doc["homSm"] = homSm;
    doc["homEh"] = homEh;
    doc["homEm"] = homEm;
    doc["homEn"] = homEn;

    uint8_t sh, sm, eh, em;
    bool sEn;
    hardware.getSleepSchedule(sh, sm, eh, em, sEn);
    doc["sleepStartH"] = sh;
    doc["sleepStartM"] = sm;
    doc["sleepEndH"] = eh;
    doc["sleepEndM"] = em;
    doc["sleepEn"] = sEn;

    JsonArray wifiArr = doc["wifiProfiles"].to<JsonArray>();
    for (uint8_t i = 0; i < wifiProfiles.getProfileCount(); i++) {
        const WiFiProfile* p = wifiProfiles.getProfile(i);
        if (p && strlen(p->ssid) > 0) {
            JsonObject wObj = wifiArr.add<JsonObject>();
            wObj["ssid"] = p->ssid;
            wObj["active"] = (WiFi.SSID() == p->ssid);
        }
    }

    String res;
    serializeJson(doc, res);
    request->send(200, "application/json", res);
  });

  // ── GET /api/pet?set=X ──────────────────────────────────────────
  _server.on("/api/pet", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("set")) {
      uint8_t p = (uint8_t)request->getParam("set")->value().toInt();
      deskPet.setType((DeskPetType)p);
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing set param\"}");
    }
  });

  // ── GET /api/pomodoro?action=start|pause|reset|mode&m=0|1|2 ────
  _server.on("/api/pomodoro", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("action")) {
      request->send(400, "application/json", "{\"error\":\"Missing action\"}");
      return;
    }
    String act = request->getParam("action")->value();
    if (act == "start") {
      display.webPomodoroStart();
    } else if (act == "pause") {
      display.webPomodoroPause();
    } else if (act == "reset") {
      display.webPomodoroReset();
    } else if (act == "mode" && request->hasParam("m")) {
      uint8_t m = (uint8_t)request->getParam("m")->value().toInt();
      display.webPomodoroSetMode(m);
    }
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/mood?set=X&pack=Y&m=I&custom=Z&tag=...&msg=... ──────────
  _server.on("/api/mood", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool updated = false;
    if (request->hasParam("set")) {
      uint8_t m = (uint8_t)request->getParam("set")->value().toInt();
      if (m < MOOD_COUNT) {
        deskMood.setMood((MoodType)m);
        updated = true;
      }
    }
    if (request->hasParam("pack")) {
      uint8_t p = (uint8_t)request->getParam("pack")->value().toInt();
      if (p < PACK_COUNT) {
        deskMood.setPack((MoodPack)p);
        updated = true;
      }
    }
    if (request->hasParam("custom")) {
      uint8_t targetMood = request->hasParam("m") 
                           ? (uint8_t)request->getParam("m")->value().toInt() 
                           : (uint8_t)deskMood.getMood();
      if (targetMood < MOOD_COUNT) {
        bool en = (request->getParam("custom")->value() == "1" || request->getParam("custom")->value() == "true");
        String tag = request->hasParam("tag") ? request->getParam("tag")->value() : "";
        String msg = request->hasParam("msg") ? request->getParam("msg")->value() : "";
        deskMood.setCustomForMood((MoodType)targetMood, en, tag.length() > 0 ? tag.c_str() : nullptr, msg.length() > 0 ? msg.c_str() : nullptr);
        updated = true;
      }
    }
    if (updated) {
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing or invalid param\"}");
    }
  });

  // ── GET /api/alarm?h=X&m=Y&en=Z&msg=... ─────────────────────────
  _server.on("/api/alarm", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint8_t h = request->hasParam("h")
                    ? (uint8_t)request->getParam("h")->value().toInt()
                    : display.getAlarmHour();
    uint8_t m = request->hasParam("m")
                    ? (uint8_t)request->getParam("m")->value().toInt()
                    : display.getAlarmMinute();
    bool en = request->hasParam("en")
                  ? (request->getParam("en")->value() == "1" ||
                     request->getParam("en")->value() == "true")
                  : display.isAlarmEnabled();
    String msg =
        request->hasParam("msg") ? request->getParam("msg")->value() : "";
    display.setAlarm(h, m, en, msg.length() > 0 ? msg.c_str() : nullptr);
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/page?id=X ─────────────────────────────────────────
  _server.on("/api/page", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      uint8_t p = (uint8_t)request->getParam("id")->value().toInt();
      request->send(200, "application/json", "{\"success\":true}");
      display.setPage(p);
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing id\"}");
    }
  });

  // ── GET /api/theme?id=X ────────────────────────────────────────
  _server.on("/api/theme", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      uint8_t id = (uint8_t)request->getParam("id")->value().toInt();
      applyThemePreset(id);
      saveTheme();
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing id\"}");
    }
  });

  // ── GET /api/brightness?val=X ──────────────────────────────────
  _server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("val")) {
      uint8_t val = (uint8_t)request->getParam("val")->value().toInt();
      hardware.setAutoBrightness(false);
      hardware.setBacklight(val);
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing val\"}");
    }
  });

  // ── GET /api/currency?c1=X&c2=Y ───────────────────────────────
  _server.on("/api/currency", HTTP_GET, [](AsyncWebServerRequest *request) {
    String c1 =
        request->hasParam("c1") ? request->getParam("c1")->value() : "USD";
    String c2 =
        request->hasParam("c2") ? request->getParam("c2")->value() : "EUR";
    finance.setCurrencies(c1.c_str(), c2.c_str());
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/sound?touch=X&melody=Y&startup=Z&test=1&testStartup=1 ─
  _server.on("/api/sound", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("touch")) {
      uint8_t t = (uint8_t)request->getParam("touch")->value().toInt();
      hardware.setTouchSound(t);
      _testTouchRequested = true;
    }
    if (request->hasParam("melody")) {
      uint8_t m = (uint8_t)request->getParam("melody")->value().toInt();
      hardware.setAlarmMelody(m);
    }
    if (request->hasParam("startup")) {
      uint8_t s = (uint8_t)request->getParam("startup")->value().toInt();
      hardware.setStartupSound(s);
    }
    if (request->hasParam("test")) {
      _testAlarmRequested = true;
    }
    if (request->hasParam("testStartup")) {
      _testStartupRequested = true;
    }
    hardware.saveSoundSettings();
    display.triggerRender();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/lang?set=X ────────────────────────────────────────
  _server.on("/api/lang", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("set")) {
      uint8_t l = (uint8_t)request->getParam("set")->value().toInt();
      setLanguage((l == 1) ? LANG_EN : LANG_VI);
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing set\"}");
    }
  });

  // ── GET /api/power/screen?off=X ─────────────────────────────────
  _server.on("/api/power/screen", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("off")) {
      bool off = (request->getParam("off")->value() == "1" || request->getParam("off")->value() == "true");
      if (off) hardware.turnScreenOff();
      else hardware.turnScreenOn();
    } else {
      hardware.toggleScreenPower();
    }
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/power/schedule?mode=X&sh=Y&sm=Z&eh=A&em=B&en=C ───
  _server.on("/api/power/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      uint8_t mode = (uint8_t)request->getParam("mode")->value().toInt();
      hardware.setPowerMode(mode);
    }
    if (request->hasParam("sh")) {
      uint8_t sh, sm, eh, em;
      bool en;
      hardware.getSleepSchedule(sh, sm, eh, em, en);
      sh = (uint8_t)request->getParam("sh")->value().toInt();
      if (request->hasParam("sm")) sm = (uint8_t)request->getParam("sm")->value().toInt();
      if (request->hasParam("eh")) eh = (uint8_t)request->getParam("eh")->value().toInt();
      if (request->hasParam("em")) em = (uint8_t)request->getParam("em")->value().toInt();
      if (request->hasParam("en")) {
        en = (request->getParam("en")->value() == "1" || request->getParam("en")->value() == "true");
      }
      hardware.setSleepSchedule(sh, sm, eh, em, en);
    }
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── POST /api/wifi/delete?ssid=X: Remove a specific remembered network ───
  _server.on("/api/wifi/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) || request->hasParam("ssid")) {
      String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : request->getParam("ssid")->value();
      wifiProfiles.deleteProfile(ssid.c_str());
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing ssid\"}");
    }
  });

  // ── POST /api/wifi/portal: Trigger on-screen QR Code Portal ────────────
  _server.on("/api/wifi/portal", HTTP_POST, [](AsyncWebServerRequest *request) {
    webDashboard.requestWiFiPortal();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Dang mo cong cai dat WiFi tren man hinh...\"}");
  });

  // ── POST /api/wifi/add: Add WiFi profile directly from Web UI ──────────
  _server.on("/api/wifi/add", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) || request->hasParam("ssid")) {
      String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : request->getParam("ssid")->value();
      String pass = "";
      if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();
      else if (request->hasParam("pass")) pass = request->getParam("pass")->value();

      if (ssid.length() > 0) {
        wifiProfiles.saveProfile(ssid.c_str(), pass.c_str());
        request->send(200, "application/json", "{\"success\":true}");
        return;
      }
    }
    request->send(400, "application/json", "{\"error\":\"Missing ssid\"}");
  });

  // ── POST /api/wifi/reset: Reset WiFi credentials & restart into AP mode ──
  _server.on("/api/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Dang xoa WiFi va khoi dong "
                  "vao AP mode...\"}");
    Serial.println("[WebDashboard] User requested WiFi Reset -> Scheduling NVS erase & restart...");
    _wifiResetTime = millis();
    _wifiResetRequested = true;
  });
}
