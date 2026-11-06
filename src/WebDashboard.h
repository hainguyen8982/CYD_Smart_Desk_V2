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
    void           _setupRoutes();
};

extern WebDashboard webDashboard;

#endif // WEB_DASHBOARD_H
