// ═══════════════════════════════════════════════════════════════════════
//  WiFiProfileManager.cpp — Multi-WiFi profile implementation
// ═══════════════════════════════════════════════════════════════════════
#include "WiFiProfileManager.h"
#include <Preferences.h>
#include <esp_wifi.h>
#include <time.h>

WiFiProfileManager wifiProfiles;

WiFiProfileManager::WiFiProfileManager()
    : _count(0), _loaded(false) {
    memset(_profiles, 0, sizeof(_profiles));
}

void WiFiProfileManager::begin() {
    if (_loaded) return;
    _loaded = true;
    _load();

    // Auto-migrate legacy stored credentials if multi-wifi list is currently empty
    if (_count == 0) {
        wifi_config_t conf;
        memset(&conf, 0, sizeof(conf));
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
            char legacySSID[33] = {0};
            char legacyPass[65] = {0};
            memcpy(legacySSID, conf.sta.ssid, 32);
            memcpy(legacyPass, conf.sta.password, 64);
            if (strlen(legacySSID) > 0) {
                Serial.printf("[WiFiProfiles] Auto-migrated existing stored WiFi: '%s'\n", legacySSID);
                strncpy(_profiles[0].ssid, legacySSID, 32);
                _profiles[0].ssid[32] = '\0';
                strncpy(_profiles[0].password, legacyPass, 64);
                _profiles[0].password[64] = '\0';
                _profiles[0].lastUsed = (uint32_t)time(NULL);
                _count = 1;
                _save();
            }
        }
        if (_count == 0 && WiFi.SSID().length() > 0) {
            Serial.printf("[WiFiProfiles] Auto-migrated WiFi.SSID(): '%s'\n", WiFi.SSID().c_str());
            strncpy(_profiles[0].ssid, WiFi.SSID().c_str(), 32);
            _profiles[0].ssid[32] = '\0';
            strncpy(_profiles[0].password, WiFi.psk().c_str(), 64);
            _profiles[0].password[64] = '\0';
            _profiles[0].lastUsed = (uint32_t)time(NULL);
            _count = 1;
            _save();
        }
    }
}

const WiFiProfile* WiFiProfileManager::getProfile(uint8_t idx) const {
    if (idx < _count) {
        return &_profiles[idx];
    }
    return nullptr;
}

void WiFiProfileManager::_load() {
    Preferences prefs;
    if (prefs.begin("cyd_multi_wifi", true)) {
        _count = prefs.getUChar("cnt", 0);
        if (_count > MAX_WIFI_PROFILES) _count = MAX_WIFI_PROFILES;

        for (uint8_t i = 0; i < _count; i++) {
            char keySSID[12], keyPass[12], keyTime[12];
            snprintf(keySSID, sizeof(keySSID), "s%d", i);
            snprintf(keyPass, sizeof(keyPass), "p%d", i);
            snprintf(keyTime, sizeof(keyTime), "t%d", i);

            String s = prefs.getString(keySSID, "");
            String p = prefs.getString(keyPass, "");
            uint32_t t = prefs.getUInt(keyTime, 0);

            strncpy(_profiles[i].ssid, s.c_str(), 32);
            _profiles[i].ssid[32] = '\0';
            strncpy(_profiles[i].password, p.c_str(), 64);
            _profiles[i].password[64] = '\0';
            _profiles[i].lastUsed = t;
        }
        prefs.end();
        Serial.printf("[WiFiProfiles] Loaded %d saved networks from NVS\n", _count);
        for (uint8_t i = 0; i < _count; i++) {
            Serial.printf("  • [%d] SSID: '%s'\n", i, _profiles[i].ssid);
        }
    }
}

void WiFiProfileManager::_save() {
    Preferences prefs;
    if (prefs.begin("cyd_multi_wifi", false)) {
        prefs.putUChar("cnt", _count);

        for (uint8_t i = 0; i < MAX_WIFI_PROFILES; i++) {
            char keySSID[12], keyPass[12], keyTime[12];
            snprintf(keySSID, sizeof(keySSID), "s%d", i);
            snprintf(keyPass, sizeof(keyPass), "p%d", i);
            snprintf(keyTime, sizeof(keyTime), "t%d", i);

            if (i < _count) {
                prefs.putString(keySSID, _profiles[i].ssid);
                prefs.putString(keyPass, _profiles[i].password);
                prefs.putUInt(keyTime, _profiles[i].lastUsed);
            } else {
                prefs.remove(keySSID);
                prefs.remove(keyPass);
                prefs.remove(keyTime);
            }
        }
        prefs.end();
        Serial.printf("[WiFiProfiles] Saved %d networks to NVS\n", _count);
    }
}

bool WiFiProfileManager::saveProfile(const char* ssid, const char* pass) {
    if (!ssid || strlen(ssid) == 0) return false;

    begin();

    // 1. Check if SSID already exists in saved profiles -> Update password
    for (uint8_t i = 0; i < _count; i++) {
        if (strcmp(_profiles[i].ssid, ssid) == 0) {
            strncpy(_profiles[i].password, pass ? pass : "", 64);
            _profiles[i].password[64] = '\0';
            _profiles[i].lastUsed = (uint32_t)time(NULL);
            _save();
            Serial.printf("[WiFiProfiles] Updated password for existing network '%s'\n", ssid);
            return true;
        }
    }

    // 2. Add as new profile if slots available
    if (_count < MAX_WIFI_PROFILES) {
        strncpy(_profiles[_count].ssid, ssid, 32);
        _profiles[_count].ssid[32] = '\0';
        strncpy(_profiles[_count].password, pass ? pass : "", 64);
        _profiles[_count].password[64] = '\0';
        _profiles[_count].lastUsed = (uint32_t)time(NULL);
        _count++;
        _save();
        Serial.printf("[WiFiProfiles] Added new network '%s' (Slot %d)\n", ssid, _count - 1);
        return true;
    }

    // 3. All slots full (3/3) -> Replace oldest (Least Recently Used)
    uint8_t oldestIdx = 0;
    uint32_t oldestTime = _profiles[0].lastUsed;
    for (uint8_t i = 1; i < _count; i++) {
        if (_profiles[i].lastUsed < oldestTime) {
            oldestTime = _profiles[i].lastUsed;
            oldestIdx = i;
        }
    }

    Serial.printf("[WiFiProfiles] Slots full -> Replacing oldest network '%s' with '%s'\n",
                  _profiles[oldestIdx].ssid, ssid);
    strncpy(_profiles[oldestIdx].ssid, ssid, 32);
    _profiles[oldestIdx].ssid[32] = '\0';
    strncpy(_profiles[oldestIdx].password, pass ? pass : "", 64);
    _profiles[oldestIdx].password[64] = '\0';
    _profiles[oldestIdx].lastUsed = (uint32_t)time(NULL);
    _save();
    return true;
}

bool WiFiProfileManager::deleteProfile(const char* ssid) {
    if (!ssid) return false;
    begin();

    for (uint8_t i = 0; i < _count; i++) {
        if (strcmp(_profiles[i].ssid, ssid) == 0) {
            // Shift remaining profiles left
            for (uint8_t j = i; j < _count - 1; j++) {
                _profiles[j] = _profiles[j + 1];
            }
            memset(&_profiles[_count - 1], 0, sizeof(WiFiProfile));
            _count--;
            _save();
            Serial.printf("[WiFiProfiles] Deleted network '%s'\n", ssid);
            return true;
        }
    }
    return false;
}

void WiFiProfileManager::clearAll() {
    _count = 0;
    memset(_profiles, 0, sizeof(_profiles));
    _save();
    Serial.println("[WiFiProfiles] Cleared all saved networks");
}

bool WiFiProfileManager::connectBest() {
    begin();
    if (_count == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // 1. Single profile: connect directly without scan delay (~1.5s)
    if (_count == 1) {
        Serial.printf("[WiFiProfiles] Single saved network '%s', connecting directly...\n", _profiles[0].ssid);
        WiFi.begin(_profiles[0].ssid, _profiles[0].password);
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 9000) {
            delay(200);
        }
        if (WiFi.status() == WL_CONNECTED) {
            _profiles[0].lastUsed = (uint32_t)time(NULL);
            _save();
            Serial.printf("[WiFiProfiles] Connected to '%s'! IP: %s\n", _profiles[0].ssid, WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.printf("[WiFiProfiles] Failed to connect to single profile '%s'\n", _profiles[0].ssid);
        return false;
    }

    // 2. Multiple profiles: try the Most Recently Used (MRU) first for 4 seconds
    uint8_t mruIdx = 0;
    uint32_t mruTime = _profiles[0].lastUsed;
    for (uint8_t i = 1; i < _count; i++) {
        if (_profiles[i].lastUsed > mruTime) {
            mruTime = _profiles[i].lastUsed;
            mruIdx = i;
        }
    }

    Serial.printf("[WiFiProfiles] Trying MRU network '%s' first...\n", _profiles[mruIdx].ssid);
    WiFi.begin(_profiles[mruIdx].ssid, _profiles[mruIdx].password);
    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 4500) {
        delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
        _profiles[mruIdx].lastUsed = (uint32_t)time(NULL);
        _save();
        Serial.printf("[WiFiProfiles] Connected to MRU network '%s'! IP: %s\n", _profiles[mruIdx].ssid, WiFi.localIP().toString().c_str());
        return true;
    }

    // 3. MRU not in range (moved location). Scan for other saved networks
    Serial.println("[WiFiProfiles] MRU not connected. Scanning for other saved networks...");
    int n = WiFi.scanNetworks(false, false, false, 300);
    Serial.printf("[WiFiProfiles] Found %d APs\n", n);

    int bestIdx = -1;
    int bestRssi = -999;
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            String s = WiFi.SSID(i);
            int r = WiFi.RSSI(i);
            for (uint8_t p = 0; p < _count; p++) {
                if (s.equals(_profiles[p].ssid)) {
                    Serial.printf("  • Match: '%s' (RSSI: %d dBm)\n", _profiles[p].ssid, r);
                    if (r > bestRssi) {
                        bestRssi = r;
                        bestIdx = p;
                    }
                }
            }
        }
        WiFi.scanDelete();
    }

    if (bestIdx >= 0) {
        Serial.printf("[WiFiProfiles] Connecting to best scanned network '%s' (RSSI: %d dBm)...\n",
                      _profiles[bestIdx].ssid, bestRssi);
        WiFi.begin(_profiles[bestIdx].ssid, _profiles[bestIdx].password);
        startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
            delay(200);
        }
        if (WiFi.status() == WL_CONNECTED) {
            _profiles[bestIdx].lastUsed = (uint32_t)time(NULL);
            _save();
            Serial.printf("[WiFiProfiles] Connected to '%s'! IP: %s\n", _profiles[bestIdx].ssid, WiFi.localIP().toString().c_str());
            return true;
        }
    }

    // 4. Fallback: try any other saved profile (in case SSID was hidden)
    for (uint8_t p = 0; p < _count; p++) {
        if (p == mruIdx || p == bestIdx) continue;
        Serial.printf("[WiFiProfiles] Fallback attempt to '%s'...\n", _profiles[p].ssid);
        WiFi.begin(_profiles[p].ssid, _profiles[p].password);
        startMs = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startMs < 5000) {
            delay(200);
        }
        if (WiFi.status() == WL_CONNECTED) {
            _profiles[p].lastUsed = (uint32_t)time(NULL);
            _save();
            Serial.printf("[WiFiProfiles] Connected to '%s'! IP: %s\n", _profiles[p].ssid, WiFi.localIP().toString().c_str());
            return true;
        }
    }

    return false;
}
