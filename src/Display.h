// ═══════════════════════════════════════════════════════════════════════
//  Display.h — 16-bit color display manager for ESP32 CYD
//  CYD SmartDesk V2 — Commercial-grade premium UI
// ═══════════════════════════════════════════════════════════════════════
#ifndef DISPLAY_H
#define DISPLAY_H

#include "Colors.h"
#include "Config.h"
#include "Theme.h"
#include "VnFont.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

enum ActivePage {
  PAGE_CLOCK = 0,
  PAGE_CALENDAR = 1,
  PAGE_FINANCE = 2,
  PAGE_POMODORO = 3,
  PAGE_MOOD = 4,
  PAGE_SETTINGS = 5
};

class Display {
public:
  Display();
  void begin();
  void update();
  void playBootAnimation();
  void drawFabXLogo(int16_t x, int16_t y, uint8_t bloomAlpha = 0);
  void showBootStatus(const char *status, uint8_t progressPercent = 0);

  // ── Navigation & Touch ────────────────────────────────────
  void setPage(uint8_t p);
  uint8_t getPage() const { return _currentPage; }
  bool handleTouch(uint16_t x, uint16_t y);
  void wakeDock();
  void triggerRender() {
    _lastRender = 0;
    _zen16ScreenValid = false;
  }
  void resetTickerCache() {
    _tickerResetPending = true;
    _lastRender = 0;
  }

  // Chỉ bật kéo trượt ngang 25ms khi đang ở đúng thanh Slider Độ sáng của trang
  // Cài đặt
  bool isBrightnessSliderActive() const {
    return (_currentPage == PAGE_SETTINGS) && (_activePopup == POPUP_NONE) &&
           !_dockVisible && !_alarmRinging;
  }

  // Chỉ bật giữ lặp lại (Hold-to-repeat) cho các nút tăng/giảm (Giờ/Phút báo
  // thức, Độ sáng -/+, Tháng lịch)
  bool isHoldRepeatAllowed(uint16_t startX, uint16_t startY) const {
    if (_alarmRinging || _dockVisible)
      return false;
    if (_activePopup == POPUP_ALARM) {
      return (startX >= 30 && startX <= 290 && startY >= 40 && startY <= 168);
    }
    if (_activePopup != POPUP_NONE)
      return false;
    if (_currentPage == PAGE_SETTINGS) {
      return (startY <= 55 &&
              (startX <= 56 || (startX >= 200 && startX < 244)));
    }
    if (_currentPage == PAGE_CALENDAR) {
      return (startY <= 50 && (startX <= 66 || startX >= 250));
    }
    return false;
  }

  // ── Alarm API ─────────────────────────────────────────────
  void setAlarm(uint8_t h, uint8_t m, bool enabled, const char *msg = nullptr);
  uint8_t getAlarmHour() const { return _alarmHour; }
  uint8_t getAlarmMinute() const { return _alarmMinute; }
  bool isAlarmEnabled() const { return _alarmEnabled; }
  bool isAlarmRinging() const { return _alarmRinging; }
  const char *getAlarmMessage() const { return _alarmMessage; }
  void dismissAlarm();

  // ── Ecosystem & Web Pomodoro API ──────────────────────────
  void syncRemotePomodoroStart(uint16_t minutes = 25);
  void syncRemotePomodoroStop();
  void webPomodoroStart();
  void webPomodoroPause();
  void webPomodoroReset();
  void webPomodoroSetMode(uint8_t mode);
  uint8_t getPomoState() const { return (uint8_t)_pomoState; }
  uint8_t getPomoMode() const { return (uint8_t)_pomoMode; }
  uint16_t getPomoRemainingSec() const { return _pomoRemainingSec; }
  uint16_t getPomoTotalSec() const { return _pomoTotalSec; }

  // ── WiFi Setup Portal Screen ──────────────────────────────
  void renderWiFiSetupScreen(const char *qrText = nullptr,
                             int remainingSec = 120);
  void renderWiFiConnectedScreen(const char *ssid, const char *ip);

private:
  TFT_eSPI _tft;
  VnSprite
      _spr; // Full-screen double-buffer sprite with UTF-8 Vietnamese support
  bool _sprReady;
  volatile unsigned long _lastRender;

  // ── Page & Dock State ─────────────────────────────────────
  uint8_t _currentPage;
  bool _dockVisible;
  unsigned long _lastDockActivity;

  // ── Popup Modal State ─────────────────────────────────────
  enum PopupState {
    POPUP_NONE = 0,
    POPUP_WEATHER,
    POPUP_LUNAR,
    POPUP_ALARM,
    POPUP_FINANCE_GOLD,
    POPUP_FINANCE_CURRENCY
  };
  PopupState _activePopup;
  uint8_t _currencyTab;         // 0 = cur1, 1 = cur2, 2 = BTC
  uint8_t _settingsTab;         // 0 = Theme, 1 = Sound, 2 = System
  bool _settingsSoundIsStartup; // Toggle between Alarm melody and Boot sound on
                                // Settings row 4
  void renderWeatherPopupZen16();
  void renderLunarPopupZen16();
  void renderAlarmPopup();
  void renderAlarmTimeOnly();
  void renderAlarmToggleBtnOnly();
  void renderAlarmRingingScreenZen16();
  void renderFinanceGoldPopupZen16();
  void renderFinanceCurrencyPopupZen16();
  void drawFinanceSparkline(int16_t x, int16_t y, int16_t w, int16_t h,
                            const float *pts, uint8_t count, uint16_t color,
                            bool areaFill);

  // ── Alarm Engine ──────────────────────────────────────────
  bool _alarmEnabled;
  uint8_t _alarmHour;   // 0..23
  uint8_t _alarmMinute; // 0..59
  bool _alarmRinging;
  unsigned long _alarmRingStart;
  unsigned long _lastAlarmBeepTick;
  bool _alarmSnoozed;
  time_t _alarmSnoozeUntil;
  int _alarmLastTriggerDay;
  int _alarmLastTriggerMin;
  unsigned long _alarmAutoCloseTime;
  volatile bool _alarmPopupDirty = false;
  char _alarmMessage[80];

  void loadAlarmSettings();
  void saveAlarmSettings();
  void checkAlarmTick();
  void triggerAlarm();
  void snoozeAlarm();
  void drawBellIcon(int16_t cx, int16_t cy, bool isEnabled,
                    uint16_t customColor = 0);

  // ── Calendar Navigation State ─────────────────────────────
  int16_t _calYear;
  int8_t _calMonth;
  void prevCalMonth();
  void nextCalMonth();
  void resetCalMonth();

  // ── Pomodoro Focus Engine ─────────────────────────────────
  enum PomoMode {
    POMO_FOCUS = 0,       // 25 phút
    POMO_SHORT_BREAK = 1, // 5 phút
    POMO_LONG_BREAK = 2   // 15 phút
  };
  enum PomoState { POMO_STOPPED = 0, POMO_RUNNING = 1, POMO_PAUSED = 2 };

  PomoMode _pomoMode;
  PomoState _pomoState;
  uint16_t _pomoTotalSec;
  uint16_t _pomoRemainingSec;
  uint8_t _pomoSession; // 1..4
  unsigned long _pomoLastSecTick;

  void pomoSetMode(PomoMode mode);
  void pomoStart();
  void pomoPause();
  void pomoReset();
  void pomoTick();

  // ── Anniversary & Perpetual Calendar Ticker ──────────────
  int16_t _tickerOffset;
  unsigned long _lastTickerTick;
  unsigned long _lastPetTick = 0;
  uint8_t _tickerCachedDay;
  volatile bool _tickerResetPending = false;
  String _cachedTickerMsg;
  void renderClockTicker16(bool forceFull, bool needBody, int todayMday);

  // ── Unified 16-bit Page renderers (ZEN & VIBE modes) ─────
  void renderPage0_MinimalZen16(bool forceFull);
  void renderPage0_VibeBento16(bool forceFull);
  void renderPage1_CalendarZen16();
  void renderPage2_FinanceZen16();
  void renderPage3_PomodoroZen16(bool forceFull);
  void renderPage3_PomodoroVibe16(bool forceFull);
  void renderPage4_MoodZen16();
  void renderPage5_SettingsZen16(bool forceFull);
  void renderSettingsBrightnessBarZen16(bool forceFull);
  volatile bool _zen16ScreenValid = false;
  uint8_t _zen16LastPage = 255;
  uint8_t _zen16LastPopup = 0;
  uint8_t _zen16LastCurrencyTab = 255;
  uint8_t _zen16LastThemeIdx = 255;
  bool _zen16LastMinimalUi = true;
  bool _zen16LastDock = false;
  int _zen16LastMin = -1;
  int16_t _zen16LastCalYear = 0;
  int8_t _zen16LastCalMonth = 0;
  uint16_t _zen16LastPomoSec = 65535;
  uint8_t _zen16LastPomoState = 255;
  uint8_t _zen16LastPomoMode = 255;
  bool _zen16LastAlarmEn = false;
  int _zen16LastBri = -1;
  bool _zen16LastAutoBri = false;
  bool _zen16LastWifiOk = false;
  uint8_t _zen16SetDirty = 0x3F;
  bool handleTouchSettings(int16_t x, int16_t y);

  // ── Smart Dock & Calibration ────────────────────────────
  void renderDock();
  void renderCalibrationScreen();
  void drawDockIconHome(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconCalendar(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconFinance(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconPomodoro(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconMood(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconTheme(int16_t cx, int16_t cy, bool isActive);
  void drawDockIconPower(int16_t cx, int16_t cy);

  // ── Weather & Moon icon drawing ──────────────────────────
  void drawMinimalWeatherIcon(int16_t cx, int16_t cy, uint8_t weatherType,
                              bool isNight, uint16_t bgCol);
  void drawMoonIcon(int16_t cx, int16_t cy, int16_t r, uint8_t lunarDay,
                    uint16_t bgTint = 0);
};

extern Display display;

#endif // DISPLAY_H
