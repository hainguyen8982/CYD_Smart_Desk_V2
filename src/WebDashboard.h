// ═══════════════════════════════════════════════════════════════════════
//  WebDashboard.h — Modern mobile-first Web UI for CYD SmartDesk V2
//  Features: Client-side auto-crop/resize wallpaper upload, theme switcher,
//            brightness slider, system telemetry & diagnostics
// ═══════════════════════════════════════════════════════════════════════
#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebDashboard {
public:
    WebDashboard();
    void begin();
    void stop();
    void update();
    bool isRunning() const { return _running; }

    void requestWiFiPortal() { _portalRequested = true; }
    bool isPortalRequested() const { return _portalRequested; }
    void clearPortalRequest() { _portalRequested = false; }
    bool isOtaInProgress() const { return _otaInProgress || _otaRebootRequested; }

private:
    AsyncWebServer _server;
    bool           _running;
    bool           _routesSetup;
    volatile bool  _portalRequested;
    volatile bool  _testTouchRequested;
    volatile bool  _testAlarmRequested;
    volatile bool  _testStartupRequested;
    volatile bool  _wifiResetRequested;
    unsigned long  _wifiResetTime;
    volatile bool  _otaInProgress = false;
    volatile bool  _otaRebootRequested = false;
    unsigned long  _otaRebootTime = 0;

    // Deferred Core 1 UI/Hardware commands (prevents Core 0 async_tcp races)
    volatile bool    _hasPendingPage = false;
    volatile uint8_t _pendingPage = 0;
    volatile uint8_t _pendingPomoAct = 0;   // 1=start, 2=pause, 3=reset, 4=mode
    volatile uint8_t _pendingPomoMode = 0;
    volatile uint8_t _pendingScreenCmd = 0; // 1=off, 2=on, 3=toggle
    volatile uint8_t _pendingEcoCmd = 0;    // 1=pair 30s, 2=unpair
    volatile bool    _hasPendingAlarm = false;
    uint8_t          _pendingAlarmH = 7;
    uint8_t          _pendingAlarmM = 0;
    bool             _pendingAlarmEn = false;
    bool             _pendingAlarmHasMsg = false;
    char             _pendingAlarmMsg[80] = {0};

    void           _setupRoutes();
};

extern WebDashboard webDashboard;

#endif // WEB_DASHBOARD_H
