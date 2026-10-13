// ═══════════════════════════════════════════════════════════════════════
//  WeatherService.cpp — WiFi + NTP + OpenWeatherMap implementation
//  CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "WeatherService.h"
#include "Display.h"
#include <WiFiManager.h>

WeatherService weather;

WeatherService::WeatherService()
    : _lastFetch(0), _lastNtpAttempt(0), _timeSynced(false), _cityDetected(false),
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
        display.showBootStatus("Syncing NTP Time...");
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
                display.showBootStatus("Time Synced!");
                delay(300);
                break;
            }
            delay(150);
        }

        _detectCityFromIP();
        _fetchWeather();
    }
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

    unsigned long now = millis();
    if (now - _lastFetch >= WEATHER_FETCH_INTERVAL || _lastFetch == 0) {
        if (_timeSynced) {
            _fetchWeather();
            _lastFetch = now;
        }
    }
}

void WeatherService::fetchNow() {
    if (isConnected()) {
        _fetchWeather();
        _lastFetch = millis();
    }
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
    if (_data.sunrise == 0 || _data.sunset == 0) {
        // Fallback: 6 AM – 6 PM is day
        struct tm t;
        localtime_r(&now, &t);
        return (t.tm_hour < 6 || t.tm_hour >= 18);
    }
    return (now < _data.sunrise || now > _data.sunset);
}

uint8_t WeatherService::getLunarDay() const {
    time_t now = time(NULL);
    if (now < 1600000000) return 0;
    struct tm t;
    localtime_r(&now, &t);
    // Simplified synodic month calculation
    // Known new moon: Jan 6, 2000 18:14 UTC (2451550.26 JD)
    double jd = 367.0 * (1900 + t.tm_year) 
              - floor(7.0 * ((1900 + t.tm_year) + floor(((t.tm_mon + 1) + 9.0) / 12.0)) / 4.0) 
              + floor(275.0 * (t.tm_mon + 1) / 9.0) 
              + t.tm_mday + 1721013.5;
    double daysSinceNew = jd - 2451550.26;
    double phase = fmod(daysSinceNew, 29.53058867);
    if (phase < 0) phase += 29.53058867;
    return (uint8_t)phase;
}

// ─── Private implementation ──────────────────────────────────────────

void WeatherService::_connectWiFi() {
    Serial.println("[WiFi] Starting WiFiManager...");
    display.showBootStatus("Connecting WiFi...");

    WiFiManager wm;
    wm.setConfigPortalTimeout(120);
    wm.setConnectTimeout(15);
    wm.setBreakAfterConfig(true);

    wm.setAPCallback([](WiFiManager* myWiFiManager) {
        Serial.println("[WiFi] Entered AP Config Portal Mode");
        display.showBootStatus("AP: CYD-SmartDesk-V2 (192.168.4.1)");
    });

    if (!wm.autoConnect("CYD-SmartDesk-V2")) {
        Serial.println("[WiFi] Failed to connect, continuing offline");
        display.showBootStatus("WiFi failed. Continuing offline...");
        delay(1000);
        return;
    }
    Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    display.showBootStatus("WiFi Connected!");
    delay(500);
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
            String cStr = String(city ? city : (region ? region : ""));
            if (cStr.indexOf("Ho Chi Minh") >= 0 || cStr.indexOf("Saigon") >= 0) {
                strlcpy(_data.city, "TP. HCM", sizeof(_data.city));
            } else if (cStr.indexOf("Hanoi") >= 0 || cStr.indexOf("Ha Noi") >= 0) {
                strlcpy(_data.city, "HA NOI", sizeof(_data.city));
            } else if (cStr.indexOf("Da Nang") >= 0) {
                strlcpy(_data.city, "DA NANG", sizeof(_data.city));
            } else if (cStr.indexOf("Can Tho") >= 0) {
                strlcpy(_data.city, "CAN THO", sizeof(_data.city));
            } else if (cStr.indexOf("Hai Phong") >= 0) {
                strlcpy(_data.city, "HAI PHONG", sizeof(_data.city));
            } else if (city && strlen(city) > 0) {
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
            _data.temp       = doc["current"]["temperature_2m"] | 26.0f;
            _data.humidity   = doc["current"]["relative_humidity_2m"] | 75;
            _data.windSpeed  = doc["current"]["wind_speed_10m"] | 10.0f;
            _data.uvIndex    = doc["current"]["uv_index"] | 0.0f;
            int wCode        = doc["current"]["weather_code"] | 0;

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
