// ═══════════════════════════════════════════════════════════════════════
//  WeatherService.cpp — WiFi + NTP + OpenWeatherMap implementation
//  CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "WeatherService.h"
#include "Display.h"
#include "NetworkTask.h"
#include "WiFiProfileManager.h"
#include "HardwareManager.h"
#include "TouchManager.h"
#include "EcosystemService.h"
#include "vn_lunar.h"
#include <WiFiManager.h>
#include <esp_wifi.h>

WeatherService weather;

WeatherService::WeatherService()
    : _mutex(xSemaphoreCreateMutex()), _lastFetch(0), _lastNtpAttempt(0), _timeSynced(false), _cityDetected(false),
      _lat(10.8231f), _lon(106.6297f) {
    memset(&_data, 0, sizeof(_data));
    strncpy(_data.city, "TP. HCM", sizeof(_data.city)); // Default to Saigon coordinates

    _data.forecast[0].tempMin = 24.0f; _data.forecast[0].tempMax = 33.0f;
    _data.forecast[0].type = WEATHER_CLEAR;
    strlcpy(_data.forecast[0].dayName, "THU 4", sizeof(_data.forecast[0].dayName));
    strlcpy(_data.forecast[0].condText, "Nang dep", sizeof(_data.forecast[0].condText));

    _data.forecast[1].tempMin = 24.0f; _data.forecast[1].tempMax = 31.0f;
    _data.forecast[1].type = WEATHER_RAIN;
    strlcpy(_data.forecast[1].dayName, "THU 5", sizeof(_data.forecast[1].dayName));
    strlcpy(_data.forecast[1].condText, "Co mua", sizeof(_data.forecast[1].condText));

    _data.forecast[2].tempMin = 25.0f; _data.forecast[2].tempMax = 34.0f;
    _data.forecast[2].type = WEATHER_CLOUDY;
    strlcpy(_data.forecast[2].dayName, "THU 6", sizeof(_data.forecast[2].dayName));
    strlcpy(_data.forecast[2].condText, "Co may", sizeof(_data.forecast[2].condText));
}

void WeatherService::begin() {
    _connectWiFi();
    if (isConnected()) {
        display.showBootStatus("Syncing NTP Time...", 75);
        _syncTime();

        // Chờ nhanh tối đa 4s để đồng bộ giờ ngay lúc khởi động
        unsigned long startWait = millis();
        while (millis() - startWait < 4000) {
            time_t now = time(NULL);
            if (now > 1600000000) {
                _timeSynced = true;
                struct tm t;
                localtime_r(&now, &t);
                Serial.printf("[NTP] Time synced on boot: %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
                display.showBootStatus("NTP Time Synced!", 88);
                delay(250);
                break;
            }
            delay(150);
        }

        display.showBootStatus("Updating Weather...", 94);
        fetchDirect();
        display.showBootStatus("System Ready!", 100);
        delay(400);
    } else {
        display.showBootStatus("System Ready!", 100);
        delay(400);
    }
}

WeatherData WeatherService::getData() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    WeatherData copy = _data;
    if (_mutex) xSemaphoreGive(_mutex);
    return copy;
}

void WeatherService::update() {
    if (!isConnected()) return;

    if (!_timeSynced) {
        time_t now = time(NULL);
        if (now > 1600000000) {
            _timeSynced = true;
            struct tm t;
            localtime_r(&now, &t);
            Serial.printf("[NTP] Time synced in background: %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
        } else {
            unsigned long nowMs = millis();
            if (nowMs - _lastNtpAttempt >= 10000 || _lastNtpAttempt == 0) {
                _syncTime();
            }
        }
    }
}

void WeatherService::fetchNow() {
    // Non-blocking trigger to background Core 0 task
    networkTask.triggerWeather();
}

void WeatherService::fetchDirect() {
    if (isConnected()) {
        _detectCityFromIP();
        _fetchWeather();
        _lastFetch = millis();
    }
}

void WeatherService::fetchBackground() {
    fetchDirect();
}

WeatherType WeatherService::getType() const {
    uint16_t id = _data.weatherId;
    if (id >= 200 && id < 300) return WEATHER_STORM;
    if (id >= 300 && id < 600) return WEATHER_RAIN;
    if (id >= 600 && id < 700) return WEATHER_SNOW;
    if (id >= 700 && id < 800) return WEATHER_MIST;
    if (id == 800)             return WEATHER_CLEAR;
    if (id > 800)              return WEATHER_CLOUDY;
    return WEATHER_CLEAR;
}

bool WeatherService::isNight() const {
    time_t now = time(NULL);
    if (now < 1600000000) return false; // time not synced
    struct tm t;
    localtime_r(&now, &t);
    int curMin = t.tm_hour * 60 + t.tm_min;
    if (_data.sunriseHour == 0 && _data.sunsetHour == 0) {
        // Fallback: 06:00 – 18:00 is day
        return (curMin < 6 * 60 || curMin >= 18 * 60);
    }
    int riseMin = _data.sunriseHour * 60 + _data.sunriseMin;
    int setMin  = _data.sunsetHour * 60 + _data.sunsetMin;
    return (curMin < riseMin || curMin >= setMin);
}

uint8_t WeatherService::getLunarDay() const {
    time_t now = time(NULL);
    if (now < 1600000000) return 1;
    struct tm t;
    localtime_r(&now, &t);
    LunarDate ld = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    return (uint8_t)ld.day;
}

// ─── Private implementation ──────────────────────────────────────────

void WeatherService::_connectWiFi() {
    Serial.println("[WiFi] Starting WiFi Connection Manager...");
    display.showBootStatus("Connecting WiFi...", 35);

    WiFi.mode(WIFI_STA);
    delay(50);
    wifiProfiles.begin();

    // 1. Nếu đã có mạng lưu trước đó (hoặc vừa tự động migrate từ ESP32 flash)
    if (wifiProfiles.getProfileCount() > 0) {
        display.showBootStatus("Connecting Known WiFi...", 45);
        if (wifiProfiles.connectBest()) {
            Serial.printf("[WiFi] Connected to known WiFi! IP: %s\n", WiFi.localIP().toString().c_str());
            WiFi.setAutoReconnect(true);
            WiFi.persistent(true);
            display.showBootStatus("WiFi Connected!", 68);
            delay(300);
            return;
        }
        Serial.println("[WiFi] Known networks not reachable. Checking direct flash...");
    }

    // 2. Thử WiFi.begin() trực tiếp lần cuối (dùng thông tin ESP32 NVS tự động) nếu chưa kết nối
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Trying direct WiFi.begin() from ESP32 flash...");
        WiFi.begin();
        unsigned long tryMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - tryMs < 4500) {
            delay(200);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected via direct WiFi.begin()! SSID: %s, IP: %s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            wifiProfiles.saveProfile(WiFi.SSID().c_str(), WiFi.psk().c_str());
            display.showBootStatus("WiFi Connected!", 68);
            delay(300);
            return;
        }
    }

    // 3. Nếu thực sự không kết nối được -> Bật Màn hình Mã QR Cài đặt WiFi
    Serial.println("[WiFi] No reachable network. Launching QR Code Setup...");
    startWiFiSetupPortal();
}

void WeatherService::startWiFiSetupPortal() {
    Serial.println("[WiFi] Launching On-Screen QR Code Setup Portal...");
    bool wasConnectedBeforePortal = (WiFi.status() == WL_CONNECTED);

    // Hiển thị Màn hình Mã QR trực quan trên CYD
    display.renderWiFiSetupScreen("WIFI:S:CYD-SmartDesk-V2;T:nopass;;");

    WiFiManager wm;
    wm.setConfigPortalTimeout(120);
    wm.setConfigPortalBlocking(false); // Non-blocking để xử lý cảm ứng nút Bỏ qua
    wm.setBreakAfterConfig(true);
    wm.setDarkMode(true);
    wm.setTitle("CYD SmartDesk V2");
    wm.setCustomHeadElement("<style>:root{--bg:#070B14;--accent:#00E5FF;}body{background:#070B14!important;color:#F8FAFC!important;font-family:-apple-system,sans-serif;}h1{color:#00E5FF!important;}button{background:linear-gradient(135deg,#00E5FF,#0077B6)!important;color:#070B14!important;font-weight:bold!important;border-radius:10px!important;padding:12px!important;border:none!important;}</style>");

    wm.startConfigPortal("CYD-SmartDesk-V2");

    unsigned long startMs = millis();
    unsigned long lastTickMs = millis();
    bool userCancelled = false;
    bool newConfigConnected = false;

    while (millis() - startMs < 120000UL) {
        if (wm.process()) {
            newConfigConnected = true;
            break;
        }

        unsigned long elapsed = millis() - startMs;
        int remainingSec = 120 - (int)(elapsed / 1000);
        if (remainingSec < 0) remainingSec = 0;

        // Cập nhật màn hình đếm ngược mỗi giây
        if (millis() - lastTickMs >= 1000) {
            lastTickMs = millis();
            display.renderWiFiSetupScreen("WIFI:S:CYD-SmartDesk-V2;T:nopass;;", remainingSec);
        }

        // Kiểm tra người dùng có chạm nút [ ✕ Bỏ qua / Dùng Offline ] (khu vực nút góc dưới bên trái)
        // Bỏ qua 350ms đầu tiên để tránh nhận nhầm thao tác nhấc ngón tay khi vừa bấm nút [WIFI QR]
        uint16_t tx = 0, ty = 0;
        if (elapsed > 350 && touch.checkTap(&tx, &ty)) {
            Serial.printf("[WiFi] Tap detected at (%d, %d)\n", tx, ty);
            if (tx <= 150 && ty >= 175) {
                Serial.println("[WiFi] User tapped 'Skip / Exit QR Setup'");
                hardware.playTouchSound();
                userCancelled = true;
                break;
            }
        }

        // Nếu trước đó chưa có WiFi mà tự kết nối thành công, hoặc portal đã đóng sau khi lưu cấu hình
        if ((!wasConnectedBeforePortal && WiFi.status() == WL_CONNECTED) || !wm.getConfigPortalActive()) {
            break;
        }

        delay(15);
    }

    if (userCancelled) {
        wm.stopConfigPortal();
        if (WiFi.status() != WL_CONNECTED) {
            display.showBootStatus("WiFi Offline Mode", 60);
            delay(400);
        }
        return;
    }

    if (newConfigConnected || WiFi.status() == WL_CONNECTED) {
        // Lưu mạng mới vào danh bạ đa mạng NVS
        String newSSID = wm.getWiFiSSID();
        String newPass = wm.getWiFiPass();
        if (newSSID.length() == 0) {
            newSSID = WiFi.SSID();
        }
        if (newPass.length() == 0) {
            newPass = WiFi.psk();
        }
        if (newPass.length() == 0) {
            wifi_config_t conf;
            memset(&conf, 0, sizeof(conf));
            if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
                newPass = String((char*)conf.sta.password);
            }
        }
        if (newSSID.length() > 0) {
            wifiProfiles.saveProfile(newSSID.c_str(), newPass.c_str());
        }

        Serial.printf("[WiFi] Setup Success! SSID: %s, IP: %s\n", newSSID.c_str(), WiFi.localIP().toString().c_str());
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);

        // Hiển thị màn hình thành công rực rỡ kèm chuông vui vẻ
        display.renderWiFiConnectedScreen(newSSID.c_str(), WiFi.localIP().toString().c_str());
        hardware.playStartupSound();
        delay(2200);
    } else {
        wm.stopConfigPortal();
        display.showBootStatus("WiFi Offline Mode", 60);
        delay(400);
    }
}

void WeatherService::_syncTime() {
    Serial.println("[NTP] Configuring NTP time sync (Google, Cloudflare, pool.ntp.org)...");
    configTime(TIMEZONE_OFFSET_SEC, 0, "time.google.com", "time.cloudflare.com", "pool.ntp.org");
    _lastNtpAttempt = millis();
}

void WeatherService::_detectCityFromIP() {
    if (_cityDetected) return;
    Serial.println("[GeoIP] Auto-detecting city and coordinates from IP...");
    HTTPClient http;
    http.begin("http://ip-api.com/json?fields=status,city,regionName,lat,lon");
    http.setTimeout(4000);
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err && doc["status"] == "success") {
            const char* city = doc["city"];
            const char* region = doc["regionName"];
            float lat = doc["lat"] | 0.0f;
            float lon = doc["lon"] | 0.0f;

            if (lat != 0.0f && lon != 0.0f) {
                _lat = lat;
                _lon = lon;
            }

            // Standard clean Vietnamese city name
            // Combine city and region to improve detection of districts within major cities
            String cityStr = city ? String(city) : "";
            String regionStr = region ? String(region) : "";
            String combined = cityStr + " " + regionStr;
            // Normalize: remove diacritics for simple substring checks (optional – kept simple)
            if (combined.indexOf("Ho Chi Minh") >= 0 || combined.indexOf("Saigon") >= 0) {
                // Any district (e.g., "Quận Bốn") inside Ho Chi Minh City should map to TP. HCM
                strlcpy(_data.city, "TP. HCM", sizeof(_data.city));
            } else if (combined.indexOf("Hanoi") >= 0 || combined.indexOf("Ha Noi") >= 0) {
                strlcpy(_data.city, "HA NOI", sizeof(_data.city));
            } else if (combined.indexOf("Da Nang") >= 0) {
                strlcpy(_data.city, "DA NANG", sizeof(_data.city));
            } else if (combined.indexOf("Can Tho") >= 0) {
                strlcpy(_data.city, "CAN THO", sizeof(_data.city));
            } else if (combined.indexOf("Hai Phong") >= 0) {
                strlcpy(_data.city, "HAI PHONG", sizeof(_data.city));
            } else if (city && strlen(city) > 0) {
                // Fallback: use the raw city name (e.g., a smaller town)
                strlcpy(_data.city, city, sizeof(_data.city));
            }
            _cityDetected = true;
            Serial.printf("[GeoIP] ✓ Detected: %s (Lat: %.4f, Lon: %.4f)\n", _data.city, _lat, _lon);
        }
    }
    http.end();
}

void WeatherService::_fetchWeather() {
    Serial.printf("[Weather] Fetching Open-Meteo for %s (%.4f, %.4f)...\n", _data.city, _lat, _lon);

    char url[320];
    snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code,uv_index&daily=sunrise,sunset,weather_code,temperature_2m_max,temperature_2m_min&forecast_days=4&timezone=auto",
        _lat, _lon);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();

    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && !doc["current"].isNull()) {
            if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
            _data.temp       = doc["current"]["temperature_2m"] | 26.0f;
            _data.humidity   = doc["current"]["relative_humidity_2m"] | 75;
            _data.windSpeed  = doc["current"]["wind_speed_10m"] | 10.0f;
            _data.uvIndex    = doc["current"]["uv_index"] | 0.0f;
            int wCode        = doc["current"]["weather_code"] | 0;
            _data.wmoCode    = (uint8_t)constrain(wCode, 0, 99);

            // Daily astronomical sunrise & sunset
            const char* sRise = doc["daily"]["sunrise"][0];
            const char* sSet  = doc["daily"]["sunset"][0];
            if (sRise && strlen(sRise) >= 16) {
                int h = 5, m = 48;
                if (sscanf(sRise + 11, "%d:%d", &h, &m) == 2) {
                    _data.sunriseHour = (uint8_t)h;
                    _data.sunriseMin  = (uint8_t)m;
                }
            } else {
                _data.sunriseHour = 5; _data.sunriseMin = 48;
            }
            if (sSet && strlen(sSet) >= 16) {
                int h = 17, m = 35;
                if (sscanf(sSet + 11, "%d:%d", &h, &m) == 2) {
                    _data.sunsetHour = (uint8_t)h;
                    _data.sunsetMin  = (uint8_t)m;
                }
            } else {
                _data.sunsetHour = 17; _data.sunsetMin = 35;
            }

            // Map WMO code
            if (wCode == 0) {
                _data.weatherId = 800; // Clear
                strlcpy(_data.main, "Clear", sizeof(_data.main));
                strlcpy(_data.description, "Troi quang dang", sizeof(_data.description));
            } else if (wCode >= 1 && wCode <= 3) {
                _data.weatherId = 802; // Cloudy
                strlcpy(_data.main, "Clouds", sizeof(_data.main));
                strlcpy(_data.description, (wCode == 1) ? "It may, troi dep" : "May rai rac", sizeof(_data.description));
            } else if (wCode == 45 || wCode == 48) {
                _data.weatherId = 741; // Fog / Mist
                strlcpy(_data.main, "Mist", sizeof(_data.main));
                strlcpy(_data.description, "Suong mu", sizeof(_data.description));
            } else if ((wCode >= 51 && wCode <= 67) || (wCode >= 80 && wCode <= 82)) {
                _data.weatherId = 500; // Rain
                strlcpy(_data.main, "Rain", sizeof(_data.main));
                strlcpy(_data.description, "Mua rao", sizeof(_data.description));
            } else if (wCode >= 95) {
                _data.weatherId = 200; // Storm
                strlcpy(_data.main, "Storm", sizeof(_data.main));
                strlcpy(_data.description, "Mua dong set", sizeof(_data.description));
            } else {
                _data.weatherId = 801;
                strlcpy(_data.main, "Clouds", sizeof(_data.main));
                strlcpy(_data.description, "Nhieu may", sizeof(_data.description));
            }

            // ── Parse 3-day forecast (Next 3 days: apiDay = 1, 2, 3) ──
            time_t nowT = time(NULL);
            struct tm curTm;
            localtime_r(&nowT, &curTm);
            const char* dayNames[] = { "C.NHAT", "THU 2", "THU 3", "THU 4", "THU 5", "THU 6", "THU 7" };

            for (int d = 0; d < 3; d++) {
                int apiDay = d + 1; // Days 1, 2, 3 (next 3 days!)
                if (!doc["daily"]["temperature_2m_max"][apiDay].isNull()) {
                    _data.forecast[d].tempMax = doc["daily"]["temperature_2m_max"][apiDay] | 32.0f;
                    _data.forecast[d].tempMin = doc["daily"]["temperature_2m_min"][apiDay] | 24.0f;
                    int codeD = doc["daily"]["weather_code"][apiDay] | 0;

                    if (codeD == 0) {
                        _data.forecast[d].type = WEATHER_CLEAR;
                        strlcpy(_data.forecast[d].condText, "Nang dep", sizeof(_data.forecast[d].condText));
                    } else if (codeD <= 3) {
                        _data.forecast[d].type = WEATHER_CLOUDY;
                        strlcpy(_data.forecast[d].condText, "Co may", sizeof(_data.forecast[d].condText));
                    } else if (codeD == 45 || codeD == 48) {
                        _data.forecast[d].type = WEATHER_MIST;
                        strlcpy(_data.forecast[d].condText, "Suong mu", sizeof(_data.forecast[d].condText));
                    } else if ((codeD >= 51 && codeD <= 67) || (codeD >= 80 && codeD <= 82)) {
                        _data.forecast[d].type = WEATHER_RAIN;
                        strlcpy(_data.forecast[d].condText, "Co mua", sizeof(_data.forecast[d].condText));
                    } else if (codeD >= 95) {
                        _data.forecast[d].type = WEATHER_STORM;
                        strlcpy(_data.forecast[d].condText, "Mua dong", sizeof(_data.forecast[d].condText));
                    } else {
                        _data.forecast[d].type = WEATHER_CLOUDY;
                        strlcpy(_data.forecast[d].condText, "May nhe", sizeof(_data.forecast[d].condText));
                    }
                }
                int wday = (curTm.tm_wday + apiDay) % 7;
                strlcpy(_data.forecast[d].dayName, dayNames[wday], sizeof(_data.forecast[d].dayName));
            }

            _data.valid = true;
            if (_mutex) xSemaphoreGive(_mutex);

            // Đồng bộ ngay nhiệt độ & độ ẩm vừa lấy được sang EMO Companion qua ESP-NOW
            ecosystem.broadcastWeatherSync();

            Serial.printf("[Weather] ✓ Open-Meteo OK: %.1f°C, %s, %d%%RH, Wind: %.1f km/h, UV: %.1f | Next 3 days forecast loaded\n",
                _data.temp, _data.main, _data.humidity, _data.windSpeed, _data.uvIndex);
        } else {
            Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[Weather] HTTP error: %d\n", code);
    }
    http.end();
}
