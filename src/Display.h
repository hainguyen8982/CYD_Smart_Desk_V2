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
    void playBootAnimation();
    void drawFabXLogo(int16_t x, int16_t y);
    void showBootStatus(const char* status, uint8_t progressPercent = 0);

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
        POPUP_LUNAR,
        POPUP_ALARM
    };
    PopupState    _activePopup;
    void          renderWeatherPopup();
    void          renderLunarPopup();
    void          renderAlarmPopup();
    void          renderAlarmTimeOnly();
    void          renderAlarmToggleBtnOnly();
    void          renderAlarmRingingScreen();

    // ── Alarm Engine ──────────────────────────────────────────
    bool          _alarmEnabled;
    uint8_t       _alarmHour;          // 0..23
    uint8_t       _alarmMinute;        // 0..59
    bool          _alarmRinging;
    unsigned long _alarmRingStart;
    unsigned long _lastAlarmBeepTick;
    bool          _alarmSnoozed;
    time_t        _alarmSnoozeUntil;
    int           _alarmLastTriggerDay;
    unsigned long _alarmAutoCloseTime;

    void          loadAlarmSettings();
    void          saveAlarmSettings();
    void          checkAlarmTick();
    void          triggerAlarm();
    void          dismissAlarm();
    void          snoozeAlarm();
    void          drawBellIcon(int16_t cx, int16_t cy, bool isEnabled, uint16_t customColor = 0);

    // ── Calendar Navigation State ─────────────────────────────
    int16_t       _calYear;
    int8_t        _calMonth;
    void          prevCalMonth();
    void          nextCalMonth();
    void          resetCalMonth();

    // ── Pomodoro Focus Engine ─────────────────────────────────
    enum PomoMode {
        POMO_FOCUS = 0,       // 25 phút
        POMO_SHORT_BREAK = 1, // 5 phút
        POMO_LONG_BREAK = 2   // 15 phút
    };
    enum PomoState {
        POMO_STOPPED = 0,
        POMO_RUNNING = 1,
        POMO_PAUSED = 2
    };

    PomoMode      _pomoMode;
    PomoState     _pomoState;
    uint16_t      _pomoTotalSec;
    uint16_t      _pomoRemainingSec;
    uint8_t       _pomoSession;       // 1..4
    unsigned long _pomoLastSecTick;

    void          pomoSetMode(PomoMode mode);
    void          pomoStart();
    void          pomoPause();
    void          pomoReset();
    void          pomoTick();
    void          drawTomatoMascot(int16_t cx, int16_t cy, bool isFocus);

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
