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
    void update();
    bool isRunning() const { return _running; }

private:
    AsyncWebServer _server;
    bool           _running;
    void           _setupRoutes();
};

extern WebDashboard webDashboard;

#endif // WEB_DASHBOARD_H
