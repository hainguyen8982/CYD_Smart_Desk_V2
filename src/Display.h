// ═══════════════════════════════════════════════════════════════════════
//  Display.h — 16-bit color display manager for ESP32 CYD
//  CYD SmartDesk V2 — Commercial-grade premium UI
// ═══════════════════════════════════════════════════════════════════════
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "Colors.h"
#include "Theme.h"

enum ActivePage {
    PAGE_CLOCK = 0,
    PAGE_CALENDAR = 1,
    PAGE_FINANCE = 2,
    PAGE_POMODORO = 3
};

class Display {
public:
    Display();
    void begin();
    void update();
    void showBootStatus(const char* status);

    // ── Navigation & Touch ────────────────────────────────────
    void setPage(uint8_t p);
    uint8_t getPage() const { return _currentPage; }
    bool handleTouch(uint16_t x, uint16_t y);
    void wakeDock();

    // ── Custom Wallpaper ──────────────────────────────────────
    void notifyWallpaperUpdated();
    bool hasWallpaper() const { return _hasWallpaper; }
    static bool tjpgRenderCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

private:
    bool          _hasWallpaper;
    bool          _wpCheckNeeded;

    TFT_eSPI      _tft;
    TFT_eSprite   _spr;           // Full-screen double-buffer sprite
    bool          _sprReady;
    unsigned long _lastRender;

    // ── Page & Dock State ─────────────────────────────────────
    uint8_t       _currentPage;
    bool          _dockVisible;
    unsigned long _lastDockActivity;

    // ── Popup Modal State ─────────────────────────────────────
    enum PopupState {
        POPUP_NONE = 0,
        POPUP_WEATHER,
        POPUP_LUNAR
    };
    PopupState    _activePopup;
    void          renderWeatherPopup();
    void          renderLunarPopup();

    // ── Calendar Navigation State ─────────────────────────────
    int16_t       _calYear;
    int8_t        _calMonth;
    void          prevCalMonth();
    void          nextCalMonth();
    void          resetCalMonth();

    // ── Page renderers ───────────────────────────────────────
    void renderPage0_WeatherClock();
    void renderPage1_Calendar();
    void renderPage2_Finance();
    void renderPage3_Pomodoro();

    // ── Smart Dock & Calibration ─────────────────────────────
    void renderDock();
    void renderCalibrationScreen();
    void drawDockIconHome(int16_t cx, int16_t cy, bool isActive);
    void drawDockIconCalendar(int16_t cx, int16_t cy, bool isActive);
    void drawDockIconFinance(int16_t cx, int16_t cy, bool isActive);
    void drawDockIconPomodoro(int16_t cx, int16_t cy, bool isActive);
    void drawDockIconTheme(int16_t cx, int16_t cy, bool isActive);

    // ── Background & Visual Elements ─────────────────────────
    void drawBackground();
    void drawBlob(int16_t cx, int16_t cy, int16_t r, uint16_t color);
    void drawSparkle(int16_t cx, int16_t cy, int16_t size, uint16_t color);
    void drawStarDot(int16_t cx, int16_t cy, uint16_t color);
    void drawGlassCard(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius);
    void drawGlassCardAccent(int16_t x, int16_t y, int16_t w, int16_t h,
                             int16_t radius, uint16_t accentColor);

    // ── Weather icon drawing ─────────────────────────────────
    void drawWeatherIcon(int16_t cx, int16_t cy, uint8_t weatherType, bool isNight);
    void drawSunIcon(int16_t cx, int16_t cy, int16_t r);
    void drawCloudIcon(int16_t cx, int16_t cy, int16_t w, int16_t h);
    void drawPartlyCloudyIcon(int16_t cx, int16_t cy);
    void drawRainIcon(int16_t cx, int16_t cy);
    void drawStormIcon(int16_t cx, int16_t cy);
    void drawMoonIcon(int16_t cx, int16_t cy, int16_t r, uint8_t lunarDay, uint16_t bgTint = 0);
    void drawForecastIcon(int16_t cx, int16_t cy, uint8_t weatherType);
};

extern Display display;

#endif // DISPLAY_H
