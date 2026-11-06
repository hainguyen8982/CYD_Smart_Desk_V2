// ═══════════════════════════════════════════════════════════════════════
//  WiFiProfileManager.h — Multi-WiFi profile management for CYD SmartDesk
//  Stores up to 3 known networks (Office, Home, Hotspot) in NVS Flash.
//  Automatically scans and connects to the best in-range network.
// ═══════════════════════════════════════════════════════════════════════
#ifndef WIFI_PROFILE_MANAGER_H
#define WIFI_PROFILE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

#define MAX_WIFI_PROFILES 3

struct WiFiProfile {
    char ssid[33];
    char password[65];
    uint32_t lastUsed;
};

class WiFiProfileManager {
public:
    WiFiProfileManager();
    void begin();

    uint8_t getProfileCount() const { return _count; }
    const WiFiProfile* getProfile(uint8_t idx) const;
    bool saveProfile(const char* ssid, const char* pass);
    bool deleteProfile(const char* ssid);
    void clearAll();

    // Automatically scans and connects to the strongest in-range saved profile
    bool connectBest();

private:
    WiFiProfile _profiles[MAX_WIFI_PROFILES];
    uint8_t     _count;
    bool        _loaded;

    void _load();
    void _save();
};

extern WiFiProfileManager wifiProfiles;

#endif // WIFI_PROFILE_MANAGER_H
