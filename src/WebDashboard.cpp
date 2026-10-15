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
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

WebDashboard webDashboard;

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

/* Action Buttons */
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
    <div style="display: flex; gap: 8px; align-items: center;">
      <button id="langBtn" class="btn-chip" style="font-size: 11px; padding: 4px 10px; font-weight: 700; color: var(--accent); border-color: var(--accent);" onclick="toggleWebLang()">🇻🇳 Tiếng Việt</button>
      <div class="badge-online" id="statusBadge" data-i18n="status_online">Trực tuyến</div>
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

  <!-- Alarm & Smart Reminder Section -->
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

  <!-- Remote Control Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>
      <span data-i18n="sec_ctrl_title">Điều khiển màn hình từ xa</span>
    </div>

    <div style="font-size: 12px; color: var(--text-dim); margin-bottom: 8px;" data-i18n="page_sub">Chuyển trang hiển thị:</div>
    <div id="pageButtons" style="display:grid; grid-template-columns: repeat(5, 1fr); gap: 6px;">
      <button class="btn-chip" onclick="switchPage(0)">Đồng hồ</button>
      <button class="btn-chip" onclick="switchPage(1)">Lịch âm</button>
      <button class="btn-chip" onclick="switchPage(2)">Tài chính</button>
      <button class="btn-chip" onclick="switchPage(3)">Pomodoro</button>
      <button class="btn-chip" onclick="switchPage(4)">⚙️ Cài đặt</button>
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

  <!-- Sound & Audio Settings Section -->
  <div class="card">
    <div class="card-title">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 5L6 9H2v6h4l5 4V5z"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"/></svg>
      <span data-i18n="sec_snd_title">Âm thanh & Điệu chuông (Loa GPIO 26)</span>
    </div>
    <div class="grid-2">
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
      <div>
        <label style="font-size: 12px; color: var(--text-dim); display: block; margin-bottom: 6px;" data-i18n="snd_melody_label">Điệu chuông báo thức:</label>
        <select class="select-control" id="alarmMelodySelect" onchange="changeSound()">
          <option value="0">Classic Alarm</option>
          <option value="1">Melodic Chime</option>
          <option value="2">Cyber Rhythm</option>
          <option value="3">Gentle Morning</option>
        </select>
      </div>
    </div>
    <div style="margin-top: 12px;">
      <button class="btn btn-primary" onclick="testAlarmMelody()">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg>
        <span data-i18n="btn_test_melody">Phát thử chuông báo thức</span>
      </button>
    </div>
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
    <button class="btn btn-outline" style="border-color:var(--danger); color:var(--danger); width:100%;" onclick="resetWifi()" data-i18n="btn_reset_wifi">
      Đổi mạng WiFi / Chuyển mạng mới
    </button>
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
    sec_snd_title: "Âm thanh & Điệu chuông (Loa GPIO 26)",
    snd_touch_label: "Âm chạm cảm ứng:",
    snd_melody_label: "Điệu chuông báo thức:",
    btn_test_melody: "Phát thử chuông báo thức",
    sec_wifi_title: "Cài đặt Mạng & WiFi",
    wifi_mdns: "• Tên miền mDNS: ",
    wifi_ip_prefix: "• Địa chỉ IP hiện tại: ",
    wifi_rssi_prefix: " (Sóng: ",
    btn_reset_wifi: "Đổi mạng WiFi / Chuyển mạng mới",
    p0: "Đồng hồ", p1: "Lịch âm", p2: "Tài chính", p3: "Pomodoro", p4: "⚙️ Cài đặt",
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
    snd_0: "Tắt âm (Mute)", snd_1: "Cơ bản (Click)", snd_2: "Giọt nước (Water)", snd_3: "Tiếng bíp (Beep)", snd_4: "Tiếng chim (Chirp)"
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
    sec_snd_title: "Sound & Melodies (Speaker GPIO 26)",
    snd_touch_label: "Touch sound feedback:",
    snd_melody_label: "Alarm Melody:",
    btn_test_melody: "Test Alarm Melody",
    sec_wifi_title: "Network & WiFi Settings",
    wifi_mdns: "• mDNS Domain: ",
    wifi_ip_prefix: "• Current IP Address: ",
    wifi_rssi_prefix: " (Signal: ",
    btn_reset_wifi: "Reset WiFi / Setup New Network",
    p0: "Clock", p1: "Calendar", p2: "Finance", p3: "Pomodoro", p4: "⚙️ Settings",
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
    snd_0: "Mute", snd_1: "Mechanical Click", snd_2: "Water Drop", snd_3: "Soft Beep", snd_4: "Dual Chirp"
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
    if (dict[k]) el.textContent = dict[k];
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
  if (pBtns.length >= 5) {
    pBtns[0].textContent = dict.p0;
    pBtns[1].textContent = dict.p1;
    pBtns[2].textContent = dict.p2;
    pBtns[3].textContent = dict.p3;
    pBtns[4].textContent = dict.p4;
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
  const pageNames = [dict.p0, dict.p1, dict.p2, dict.p3, dict.p4];
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
  clearTimeout(soundDebounce);
  soundDebounce = setTimeout(() => {
    const dict = getDict();
    fetch(`/api/sound?touch=${touch}&melody=${melody}`)
      .then(r => r.json())
      .then(() => showToast(dict.t_snd_save))
      .catch(() => showToast(dict.t_snd_err, false));
  }, 100);
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
    drink: (currentLang === 1) ? 'Drink water & Relax' : 'Uong nuoc & Thu gian',
    sport: (currentLang === 1) ? 'Stretch & Move' : 'Dung day & Vuon vai',
    rest:  (currentLang === 1) ? 'Take a 5-min break' : 'Nghi ngoi mat 5 phut',
    work:  (currentLang === 1) ? 'Time to leave work!' : 'Het gio lam viec!'
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

async function fetchStatus() {
  try {
    const res = await fetch('/status');
    const data = await res.json();
    highlightPage(data.page);
    highlightTheme(data.theme);
    document.getElementById('brightSlider').value = data.brightness;
    document.getElementById('brightVal').textContent = data.brightness + '%';
    if (data.cur1) document.getElementById('cur1Select').value = data.cur1;
    if (data.cur2) document.getElementById('cur2Select').value = data.cur2;
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
    if (data.lang !== undefined) {
      applyLanguage(data.lang);
    }
  } catch (e) {}
}

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

applyLanguage(0);
fetchStatus();
</script>
</body>
</html>
)rawliteral";

WebDashboard::WebDashboard() : _server(80), _running(false) {}

void WebDashboard::begin() {
  if (_running)
    return;
  _setupRoutes();
  _server.begin();
  _running = true;

  // Vô hiệu hóa WiFi Modem Sleep để phản hồi Web tức thì (không bị lag/delay
  // sau khi idle)
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

void WebDashboard::update() {
  // ESPAsyncWebServer is asynchronous, no loop polling needed
}

void WebDashboard::_setupRoutes() {
  // ── GET /: Serve mobile dashboard ──────────────────────────────
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // ── GET /status: System telemetry ─────────────────────────────
  _server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["freeHeap"] = ESP.getFreeHeap();
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
    doc["lang"] = (int)getLanguage();
    doc["langName"] = getLanguageName();

    String res;
    serializeJson(doc, res);
    request->send(200, "application/json", res);
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
      request->send(200, "application/json", "{\"success\":true}");
      applyThemePreset(id);
      saveTheme();
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

  // ── GET /api/sound?touch=X&melody=Y&test=1 ─────────────────────
  _server.on("/api/sound", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("touch")) {
      uint8_t t = (uint8_t)request->getParam("touch")->value().toInt();
      hardware.setTouchSound(t);
      hardware.playTouchSound();
    }
    if (request->hasParam("melody")) {
      uint8_t m = (uint8_t)request->getParam("melody")->value().toInt();
      hardware.setAlarmMelody(m);
    }
    if (request->hasParam("test")) {
      hardware.playAlarmMelody();
    }
    hardware.saveSoundSettings();
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

  // ── POST /api/wifi/reset: Reset WiFi credentials & restart into AP mode ──
  _server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json",
                  "{\"success\":true,\"message\":\"Dang xoa WiFi va khoi dong "
                  "vao AP mode...\"}");
    Serial.println("[WebDashboard] User requested WiFi Reset -> Erasing NVS "
                   "and restarting into AP mode...");
    delay(600);
    WiFi.disconnect(true, true);
    delay(200);
    ESP.restart();
  });
}
