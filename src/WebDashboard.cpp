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
#include "AnniversaryManager.h"
#include "EcosystemService.h"
#include "AppIcon.h"
#include "WebDashboardHtmlGz.h"
#include "WiFiProfileManager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Update.h>
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
const CACHE_NAME = 'cyd-smartdesk-v2.3';
self.addEventListener('install', e => {
  self.skipWaiting();
});
self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys => Promise.all(
      keys.map(k => caches.delete(k))
    )).then(() => self.clients.claim())
  );
});
self.addEventListener('fetch', e => {
  // Always fetch fresh network content; never serve stale HTML or API responses
  return;
});
)rawliteral";

// HTML/CSS/JS Dashboard page stored in PROGMEM
static String urlDecode(const String& str) {
    String decoded = "";
    decoded.reserve(str.length());
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            char c1 = str[i + 1];
            char c2 = str[i + 2];
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int h1 = hexVal(c1);
            int h2 = hexVal(c2);
            if (h1 >= 0 && h2 >= 0) {
                uint8_t b = (uint8_t)((h1 << 4) | h2);
                decoded += (char)b;
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            decoded += ' ';
            continue;
        }
        decoded += str[i];
    }
    return decoded;
}

static void safeAsciiTrim(String& s) {
    int start = 0;
    while (start < (int)s.length() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        start++;
    }
    int end = (int)s.length() - 1;
    while (end >= start && (s[end] == ' ' || s[end] == '\t' || s[end] == '\r' || s[end] == '\n')) {
        end--;
    }
    if (start > 0 || end < (int)s.length() - 1) {
        s = (end >= start) ? s.substring(start, end + 1) : "";
    }
}

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

  // Kích hoạt mDNS duy nhất theo đuôi MAC (vd: smartdesk-f91c.local) để tránh trùng nhiều máy trong văn phòng
  char mdnsHost[24], devId[12];
  EcosystemService::getMdnsHost(mdnsHost, sizeof(mdnsHost));
  EcosystemService::getDeviceId(devId, sizeof(devId));
  if (MDNS.begin(mdnsHost)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] Responder started at http://%s.local/\n", mdnsHost);
  } else {
    Serial.println("[mDNS] Failed to start responder");
  }

  Serial.printf("[WebDashboard] DeviceID=%s | http://%s/ | http://%s.local/\n",
                devId, WiFi.localIP().toString().c_str(), mdnsHost);
}

void WebDashboard::stop() {
  if (!_running) return;
  _server.end();
  _running = false;
  Serial.println("[WebDashboard] HTTP Server stopped cleanly");
}

void WebDashboard::update() {
  // Xử lý phát thử âm thanh, lệnh UI/Hardware hoặc Reset WiFi trên luồng chính Core 1
  if (_hasPendingPage) {
    _hasPendingPage = false;
    display.setPage(_pendingPage);
  }
  if (_pendingPomoAct != 0) {
    uint8_t act = _pendingPomoAct;
    uint8_t m = _pendingPomoMode;
    _pendingPomoAct = 0;
    if (act == 1) display.webPomodoroStart();
    else if (act == 2) display.webPomodoroPause();
    else if (act == 3) display.webPomodoroReset();
    else if (act == 4) display.webPomodoroSetMode(m);
  }
  if (_pendingScreenCmd != 0) {
    uint8_t cmd = _pendingScreenCmd;
    _pendingScreenCmd = 0;
    if (cmd == 1) hardware.turnScreenOff();
    else if (cmd == 2) hardware.turnScreenOn();
    else if (cmd == 3) hardware.toggleScreenPower();
  }
  if (_pendingEcoCmd != 0) {
    uint8_t cmd = _pendingEcoCmd;
    _pendingEcoCmd = 0;
    if (cmd == 1) ecosystem.startPairingMode(30000);
    else if (cmd == 2) ecosystem.unpairCompanion();
  }
  if (_hasPendingAlarm) {
    _hasPendingAlarm = false;
    display.setAlarm(_pendingAlarmH, _pendingAlarmM, _pendingAlarmEn,
                     _pendingAlarmHasMsg ? _pendingAlarmMsg : nullptr);
  }
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
  if (_otaInProgress && !_otaRebootRequested &&
      (millis() - _otaLastChunkMs >= 15000UL)) {
    _otaInProgress = false;
    Update.abort();
    Serial.println("[OTA] Aborted: client disconnected (15s idle timeout)");
  }
  if (_otaRebootRequested && (millis() - _otaRebootTime >= 800)) {
    _otaRebootRequested = false;
    Serial.println("[OTA] Rebooting into new firmware...");
    delay(100);
    ESP.restart();
  }
}

void WebDashboard::_setupRoutes() {
  // ── GET /: Serve gzipped mobile dashboard (4x faster, ~67KB Flash saved) ──
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(
        200, "text/html; charset=utf-8", INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
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
    char devId[12], mdnsHost[24], peerMac[20];
    EcosystemService::getDeviceId(devId, sizeof(devId));
    EcosystemService::getMdnsHost(mdnsHost, sizeof(mdnsHost));
    ecosystem.getPairedMacStr(peerMac, sizeof(peerMac));
    doc["deviceId"] = devId;
    doc["mdnsHost"] = String(mdnsHost) + ".local";
    doc["ecoPaired"] = ecosystem.isCompanionPaired();
    doc["ecoLocked"] = ecosystem.isPairedLocked();
    doc["ecoPairing"] = ecosystem.isPairingMode();
    doc["ecoPeerMac"] = peerMac;
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
    doc["minimalUi"] = isMinimalUiMode();

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
  _server.on("/api/pomodoro", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!request->hasParam("action")) {
      request->send(400, "application/json", "{\"error\":\"Missing action\"}");
      return;
    }
    String act = request->getParam("action")->value();
    if (act == "start") {
      _pendingPomoAct = 1;
    } else if (act == "pause") {
      _pendingPomoAct = 2;
    } else if (act == "reset") {
      _pendingPomoAct = 3;
    } else if (act == "mode" && request->hasParam("m")) {
      _pendingPomoMode = (uint8_t)request->getParam("m")->value().toInt();
      _pendingPomoAct = 4;
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
  _server.on("/api/alarm", HTTP_GET, [this](AsyncWebServerRequest *request) {
    _pendingAlarmH = request->hasParam("h")
                         ? (uint8_t)request->getParam("h")->value().toInt()
                         : display.getAlarmHour();
    _pendingAlarmM = request->hasParam("m")
                         ? (uint8_t)request->getParam("m")->value().toInt()
                         : display.getAlarmMinute();
    _pendingAlarmEn = request->hasParam("en")
                          ? (request->getParam("en")->value() == "1" ||
                             request->getParam("en")->value() == "true")
                          : display.isAlarmEnabled();
    String msg =
        request->hasParam("msg") ? request->getParam("msg")->value() : "";
    _pendingAlarmHasMsg = (msg.length() > 0);
    if (_pendingAlarmHasMsg) {
      strlcpy(_pendingAlarmMsg, msg.c_str(), sizeof(_pendingAlarmMsg));
    }
    _hasPendingAlarm = true;
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── GET /api/page?id=X ─────────────────────────────────────────
  _server.on("/api/page", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      _pendingPage = (uint8_t)request->getParam("id")->value().toInt();
      _hasPendingPage = true;
      request->send(200, "application/json", "{\"success\":true}");
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

  // ── GET /api/uistyle?minimal=0|1 ───────────────────────────────
  _server.on("/api/uistyle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("minimal")) {
      bool m = (request->getParam("minimal")->value() == "1" || request->getParam("minimal")->value() == "true");
      setMinimalUiMode(m);
      display.triggerRender();
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing minimal\"}");
    }
  });

  // ── GET /api/brightness?val=X ──────────────────────────────────
  _server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("val")) {
      uint8_t val = (uint8_t)request->getParam("val")->value().toInt();
      hardware.setAutoBrightness(false);
      hardware.setBacklight(val);
      if (val > 0) hardware.extendManualWake();
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
  _server.on("/api/power/screen", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("off")) {
      bool off = (request->getParam("off")->value() == "1" || request->getParam("off")->value() == "true");
      _pendingScreenCmd = off ? 1 : 2;
    } else {
      _pendingScreenCmd = 3;
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

  // ── GET /api/anniversaries: List all saved anniversaries + today's ticker ──
  _server.on("/api/anniversaries", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool isEn = (getLanguage() == LANG_EN);
    JsonDocument doc;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (uint8_t i = 0; i < anniversaryMgr.getCount(); i++) {
      const AnniversaryItem* it = anniversaryMgr.getItem(i);
      if (!it) continue;
      JsonObject obj = arr.add<JsonObject>();
      obj["day"] = it->day;
      obj["month"] = it->month;
      obj["isLunar"] = it->isLunar;
      obj["title"] = it->title;
      // also include short keys for JS
      obj["d"] = it->day;
      obj["m"] = it->month;
      obj["l"] = it->isLunar;
      obj["t"] = it->title;
    }
    doc["count"] = anniversaryMgr.getCount();
    doc["today"] = anniversaryMgr.getTodayTickerMessage(isEn);

    String res;
    serializeJson(doc, res);
    request->send(200, "application/json", res);
  });

  // ── GET & POST /api/anniversaries/add: Add personal anniversary ──
  auto addAnnivHandler = [](AsyncWebServerRequest *request) {
    uint8_t day = 0, month = 0;
    bool isLunar = false;
    String titleRaw = "";

    // Safely extract parameters from GET query or POST body
    for (size_t i = 0; i < request->params(); i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (!p) continue;
        String name = p->name();
        String val = p->value();
        if (name == "d" || name == "day") day = (uint8_t)val.toInt();
        else if (name == "m" || name == "month") month = (uint8_t)val.toInt();
        else if (name == "l" || name == "isLunar") isLunar = (val == "1" || val == "true");
        else if (name == "t" || name == "title") titleRaw = val;
    }

    String title = urlDecode(titleRaw);
    safeAsciiTrim(title);

    if (title.length() == 0) {
      title = (getLanguage() == LANG_EN) ? "Special Anniversary" : "Kỷ niệm đặc biệt";
    }

    if (day < 1 || day > 31 || month < 1 || month > 12) {
      Serial.printf("[WebDashboard] Add anniversary rejected: day=%d, month=%d, titleRaw='%s'\n", day, month, titleRaw.c_str());
      AsyncWebServerResponse *res = request->beginResponse(400, "application/json", "{\"success\":false,\"error\":\"Ngày hoặc tháng không hợp lệ (1-31, 1-12)\"}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
      return;
    }

    if (anniversaryMgr.getCount() >= MAX_ANNIVERSARIES) {
      AsyncWebServerResponse *res = request->beginResponse(400, "application/json", "{\"success\":false,\"error\":\"Đã đạt giới hạn tối đa 32 ngày kỷ niệm!\"}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
      return;
    }

    bool ok = anniversaryMgr.addItem(day, month, isLunar, title.c_str());
    Serial.printf("[WebDashboard] Add anniversary: %d/%d (%s) - '%s' -> %s (total: %d)\n",
                  day, month, isLunar ? "AL" : "DL", title.c_str(), ok ? "OK" : "FAIL", anniversaryMgr.getCount());

    if (ok) {
      display.resetTickerCache();
      display.triggerRender();
      AsyncWebServerResponse *res = request->beginResponse(200, "application/json", "{\"success\":true}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
    } else {
      AsyncWebServerResponse *res = request->beginResponse(500, "application/json", "{\"success\":false,\"error\":\"Không thể ghi vào bộ nhớ flash NVS\"}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
    }
  };
  _server.on("/api/anniversaries/add", HTTP_GET, addAnnivHandler);
  _server.on("/api/anniversaries/add", HTTP_POST, addAnnivHandler);

  // ── GET & POST /api/anniversaries/delete?i=X ───────────────────────
  auto delAnnivHandler = [](AsyncWebServerRequest *request) {
    uint8_t idx = 255;
    for (size_t i = 0; i < request->params(); i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p && (p->name() == "i" || p->name() == "index")) {
            idx = (uint8_t)p->value().toInt();
            break;
        }
    }
    if (idx == 255) {
      AsyncWebServerResponse *res = request->beginResponse(400, "application/json", "{\"success\":false,\"error\":\"Missing i param\"}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
      return;
    }
    bool ok = anniversaryMgr.deleteItem(idx);
    if (ok) {
      display.resetTickerCache();
      display.triggerRender();
      AsyncWebServerResponse *res = request->beginResponse(200, "application/json", "{\"success\":true}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
    } else {
      AsyncWebServerResponse *res = request->beginResponse(400, "application/json", "{\"success\":false,\"error\":\"Index out of range\"}");
      res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(res);
    }
  };
  _server.on("/api/anniversaries/delete", HTTP_GET, delAnnivHandler);
  _server.on("/api/anniversaries/delete", HTTP_POST, delAnnivHandler);

  // ── POST /api/eco/pair & /api/eco/unpair: Khóa cặp MAC EMO (chống nhiễu văn phòng) ──
  _server.on("/api/eco/pair", HTTP_POST, [this](AsyncWebServerRequest *request) {
    _pendingEcoCmd = 1;
    request->send(200, "application/json", "{\"success\":true}");
  });
  _server.on("/api/eco/unpair", HTTP_POST, [this](AsyncWebServerRequest *request) {
    _pendingEcoCmd = 2;
    request->send(200, "application/json", "{\"success\":true}");
  });

  // ── POST /api/ota: Web Firmware OTA Update (dual-slot min_spiffs.csv) ──
  _server.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        bool ok = !Update.hasError() && Update.isFinished();
        _otaInProgress = false;
        String errMsg = ok ? "" : String(Update.errorString());
        String body = ok ? "{\"success\":true}"
                         : ("{\"success\":false,\"error\":\"" + errMsg + "\"}");
        AsyncWebServerResponse *res =
            request->beginResponse(ok ? 200 : 500, "application/json", body);
        res->addHeader("Connection", "close");
        request->send(res);
        if (ok) {
          _otaRebootTime = millis();
          _otaRebootRequested = true;
        }
      },
      [this](AsyncWebServerRequest *request, String filename, size_t index,
             uint8_t *data, size_t len, bool final) {
        _otaLastChunkMs = millis();
        if (!index) {
          _otaInProgress = true;
          size_t expectedSize = UPDATE_SIZE_UNKNOWN;
          if (request->hasParam("size")) {
            expectedSize = (size_t)request->getParam("size")->value().toInt();
          }
          Serial.printf("[OTA] Start: %s (%u B, FreeHeap=%u)\n",
                        filename.c_str(), (unsigned)expectedSize, ESP.getFreeHeap());
          if (!Update.begin(expectedSize, U_FLASH)) {
            Update.printError(Serial);
          }
        }
        if (len && !Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }
        if (final) {
          if (Update.end(true)) {
            Serial.printf("[OTA] Update Success: %u bytes\n", index + len);
          } else {
            Update.printError(Serial);
            _otaInProgress = false;
          }
        }
      });
}
