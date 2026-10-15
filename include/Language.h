// ═══════════════════════════════════════════════════════════════════════
//  Language.h — Bilingual (VI / EN) System for CYD SmartDesk V2
//  Stores zero-RAM PROGMEM lookup tables & persists choice in NVS
// ═══════════════════════════════════════════════════════════════════════
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <Arduino.h>

enum Language {
    LANG_VI = 0,
    LANG_EN = 1
};

enum TextKey {
    // ── Common / Headers ──
    TXT_SETTINGS,
    TXT_SYSTEM_IP,
    TXT_TEST,
    TXT_AUTO,
    TXT_BACK,
    TXT_CLOSE,

    // ── Page 4 Settings Rows ──
    TXT_ROW_LANGUAGE,
    TXT_ROW_THEME,
    TXT_ROW_TOUCH_SND,
    TXT_ROW_ALARM_MEL,
    TXT_ROW_BRIGHTNESS,

    // ── Page 2 Pomodoro ──
    TXT_POMO_FOCUS,
    TXT_POMO_SHORT_BREAK,
    TXT_POMO_LONG_BREAK,
    TXT_POMO_START,
    TXT_POMO_PAUSE,
    TXT_POMO_RESET,
    TXT_POMO_COMPLETED,

    // ── Page 3 Alarm ──
    TXT_ALARM_TITLE,
    TXT_ALARM_SAVE_ON,
    TXT_ALARM_DISMISS,
    TXT_ALARM_MSG_DEFAULT,

    // ── Page 1 Finance ──
    TXT_FINANCE_SJC,
    TXT_FINANCE_WORLD,
    TXT_FINANCE_FOREX,
    TXT_FINANCE_7D,
    TXT_FINANCE_BUY,
    TXT_FINANCE_SELL,

    // ── Page 0 Modals & Weather ──
    TXT_ASTRONOMY_LUNAR,
    TXT_LUNAR_DATE,
    TXT_DAY_CANCHI,
    TXT_YEAR_CANCHI,
    TXT_TEMP,
    TXT_HUMIDITY,
    TXT_WIND,
    TXT_UV,

    TXT_KEY_COUNT
};

void initLanguage();
Language getLanguage();
void setLanguage(Language lang);
void toggleLanguage();
const char* getLanguageName();

// Main localized text accessor
const char* L(TextKey key);

// Specialized bilingual helpers
const char* getTouchSoundNameI18n(uint8_t id);
const char* getAlarmMelodyNameI18n(uint8_t id);
const char* getDayNameI18n(uint8_t wday);
const char* getShortDayNameI18n(uint8_t wday);
const char* getMoonPhaseNameI18n(uint8_t lunarDay);
const char* getWeatherConditionI18n(uint8_t type, bool isNight, bool isPopup);

#endif // LANGUAGE_H
