// ═══════════════════════════════════════════════════════════════════════
//  Language.cpp — Bilingual (VI / EN) Implementation for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "Language.h"
#include <Preferences.h>

static Language _currentLang = LANG_VI;

static const char* const STRINGS_VI[TXT_KEY_COUNT] = {
    // TXT_SETTINGS
    "CAI DAT",
    // TXT_SYSTEM_IP
    "smartdesk.local",
    // TXT_TEST
    "THU",
    // TXT_AUTO
    "AUTO",
    // TXT_BACK
    "QUAY LAI",
    // TXT_CLOSE
    "DONG",

    // Page 4 Settings Rows
    // TXT_ROW_LANGUAGE
    "Ngon ngu (Language)",
    // TXT_ROW_THEME
    "Giao dien (Theme)",
    // TXT_ROW_TOUCH_SND
    "Am cham cam ung",
    // TXT_ROW_ALARM_MEL
    "Dieu chuong bao",
    // TXT_ROW_BRIGHTNESS
    "Do sang",

    // Page 2 Pomodoro
    // TXT_POMO_FOCUS
    "TAP TRUNG",
    // TXT_POMO_SHORT_BREAK
    "NGHI NGAN",
    // TXT_POMO_LONG_BREAK
    "NGHI DAI",
    // TXT_POMO_START
    "BAT DAU",
    // TXT_POMO_PAUSE
    "TAM DUNG",
    // TXT_POMO_RESET
    "DAT LAI",
    // TXT_POMO_COMPLETED
    "HOAN THANH!",

    // Page 3 Alarm
    // TXT_ALARM_TITLE
    "BAO THUC",
    // TXT_ALARM_SAVE_ON
    "Luu & Bat",
    // TXT_ALARM_DISMISS
    "Tat bao thuc",
    // TXT_ALARM_MSG_DEFAULT
    "Bao thuc!",

    // Page 1 Finance
    // TXT_FINANCE_SJC
    "VANG SJC 9999",
    // TXT_FINANCE_WORLD
    "VANG THE GIOI",
    // TXT_FINANCE_FOREX
    "TI GIA NGOAI TE",
    // TXT_FINANCE_7D
    "Lich su 7 ngay",
    // TXT_FINANCE_BUY
    "Mua:",
    // TXT_FINANCE_SELL
    "Ban:",

    // Page 0 Modals & Weather
    // TXT_ASTRONOMY_LUNAR
    "THIEN VAN & AM LICH",
    // TXT_LUNAR_DATE
    "Ngay am lich:",
    // TXT_DAY_CANCHI
    "Ngay can chi:",
    // TXT_YEAR_CANCHI
    "Nam can chi:",
    // TXT_TEMP
    "Nhiet do",
    // TXT_HUMIDITY
    "Do am",
    // TXT_WIND
    "Gio",
    // TXT_UV
    "Chi so UV"
};

static const char* const STRINGS_EN[TXT_KEY_COUNT] = {
    // TXT_SETTINGS
    "SETTINGS",
    // TXT_SYSTEM_IP
    "smartdesk.local",
    // TXT_TEST
    "TEST",
    // TXT_AUTO
    "AUTO",
    // TXT_BACK
    "BACK",
    // TXT_CLOSE
    "CLOSE",

    // Page 4 Settings Rows
    // TXT_ROW_LANGUAGE
    "Language",
    // TXT_ROW_THEME
    "Theme Style",
    // TXT_ROW_TOUCH_SND
    "Touch Sound",
    // TXT_ROW_ALARM_MEL
    "Alarm Melody",
    // TXT_ROW_BRIGHTNESS
    "Brightness",

    // Page 2 Pomodoro
    // TXT_POMO_FOCUS
    "FOCUS",
    // TXT_POMO_SHORT_BREAK
    "SHORT BREAK",
    // TXT_POMO_LONG_BREAK
    "LONG BREAK",
    // TXT_POMO_START
    "START",
    // TXT_POMO_PAUSE
    "PAUSE",
    // TXT_POMO_RESET
    "RESET",
    // TXT_POMO_COMPLETED
    "COMPLETED!",

    // Page 3 Alarm
    // TXT_ALARM_TITLE
    "ALARM CLOCK",
    // TXT_ALARM_SAVE_ON
    "Save & Set",
    // TXT_ALARM_DISMISS
    "Dismiss Alarm",
    // TXT_ALARM_MSG_DEFAULT
    "Alarm Alert!",

    // Page 1 Finance
    // TXT_FINANCE_SJC
    "SJC GOLD 9999",
    // TXT_FINANCE_WORLD
    "WORLD GOLD",
    // TXT_FINANCE_FOREX
    "EXCHANGE RATES",
    // TXT_FINANCE_7D
    "7-Day History",
    // TXT_FINANCE_BUY
    "Buy:",
    // TXT_FINANCE_SELL
    "Sell:",

    // Page 0 Modals & Weather
    // TXT_ASTRONOMY_LUNAR
    "ASTRONOMY & LUNAR",
    // TXT_LUNAR_DATE
    "Lunar Date:",
    // TXT_DAY_CANCHI
    "Day Stem-Branch:",
    // TXT_YEAR_CANCHI
    "Year Stem-Branch:",
    // TXT_TEMP
    "Temperature",
    // TXT_HUMIDITY
    "Humidity",
    // TXT_WIND
    "Wind",
    // TXT_UV
    "UV Index"
};

// Days of week
static const char* const DAYS_VI[] = {
    "CHU NHAT", "THU HAI", "THU BA", "THU TU",
    "THU NAM", "THU SAU", "THU BAY"
};

static const char* const DAYS_EN[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY"
};

// Touch sound names
static const char* const TOUCH_SND_NAMES_VI[] = {
    "Tat am", "Phim co", "Giot nuoc", "Bip nhe", "Chip chip"
};
static const char* const TOUCH_SND_NAMES_EN[] = {
    "Mute", "Mechanical", "Water Drop", "Soft Beep", "Dual Chirp"
};

// Alarm melody names
static const char* const ALARM_NAMES_VI[] = {
    "Co dien", "Nhe nhang", "Cyberpunk", "Em diu"
};
static const char* const ALARM_NAMES_EN[] = {
    "Classic", "Melodic", "Cyberpunk", "Gentle Chime"
};

void initLanguage() {
    Preferences prefs;
    if (prefs.begin("cyd_lang", true)) {
        uint8_t l = prefs.getUChar("lang", 0);
        _currentLang = (l == 1) ? LANG_EN : LANG_VI;
        prefs.end();
    }
}

Language getLanguage() {
    return _currentLang;
}

void setLanguage(Language lang) {
    _currentLang = lang;
    Preferences prefs;
    if (prefs.begin("cyd_lang", false)) {
        prefs.putUChar("lang", (uint8_t)_currentLang);
        prefs.end();
    }
}

void toggleLanguage() {
    setLanguage((_currentLang == LANG_VI) ? LANG_EN : LANG_VI);
}

const char* getLanguageName() {
    return (_currentLang == LANG_VI) ? "Tieng Viet" : "English";
}

const char* L(TextKey key) {
    if (key >= TXT_KEY_COUNT) return "";
    return (_currentLang == LANG_VI) ? STRINGS_VI[key] : STRINGS_EN[key];
}

const char* getTouchSoundNameI18n(uint8_t id) {
    if (id >= 5) id = 1;
    return (_currentLang == LANG_VI) ? TOUCH_SND_NAMES_VI[id] : TOUCH_SND_NAMES_EN[id];
}

const char* getAlarmMelodyNameI18n(uint8_t id) {
    if (id >= 4) id = 1;
    return (_currentLang == LANG_VI) ? ALARM_NAMES_VI[id] : ALARM_NAMES_EN[id];
}

const char* getDayNameI18n(uint8_t wday) {
    if (wday >= 7) wday = 0;
    return (_currentLang == LANG_VI) ? DAYS_VI[wday] : DAYS_EN[wday];
}

const char* getShortDayNameI18n(uint8_t wday) {
    if (wday >= 7) wday = 0;
    static const char* const SHORT_VI[] = { "CN", "T2", "T3", "T4", "T5", "T6", "T7" };
    static const char* const SHORT_EN[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    return (_currentLang == LANG_VI) ? SHORT_VI[wday] : SHORT_EN[wday];
}

const char* getMoonPhaseNameI18n(uint8_t lunarDay) {
    bool vi = (_currentLang == LANG_VI);
    if (lunarDay <= 1)       return vi ? "Trang Non" : "New Moon";
    else if (lunarDay <= 6)  return vi ? "Trang Luoi Liem" : "Waxing Crescent";
    else if (lunarDay <= 8)  return vi ? "Ban Nguyet Dau" : "First Quarter";
    else if (lunarDay <= 13) return vi ? "Trang Truong Dan" : "Waxing Gibbous";
    else if (lunarDay <= 16) return vi ? "Trang Tron (Ram)" : "Full Moon";
    else if (lunarDay <= 21) return vi ? "Trang Khuyet Dan" : "Waning Gibbous";
    else if (lunarDay <= 23) return vi ? "Ban Nguyet Cuoi" : "Last Quarter";
    else                     return vi ? "Trang Tan Cuoi" : "Waning Crescent";
}

const char* getWeatherConditionI18n(uint8_t type, bool isNight, bool isPopup) {
    bool vi = (_currentLang == LANG_VI);
    if (!isPopup) {
        // Short version for Page 0 card
        switch (type) {
            case 0: return isNight ? (vi ? "Dem quang" : "Clear Night") : (vi ? "Troi nang" : "Sunny");
            case 1: return vi ? "Co may" : "Partly Cloudy";
            case 2: return vi ? "Co mua" : "Rain";
            case 3: return vi ? "Mua dong" : "Thunderstorm";
            case 4: return vi ? "Tuyet" : "Snow";
            case 5: return vi ? "Suong mu" : "Misty / Fog";
            default: return vi ? "Troi dep" : "Clear Sky";
        }
    } else {
        // Longer descriptive version for popup modal
        switch (type) {
            case 0: return isNight ? (vi ? "Dem quang" : "Clear Night") : (vi ? "Troi nang dep" : "Sunny & Clear");
            case 1: return vi ? "Nhieu may" : "Cloudy";
            case 2: return vi ? "Troi mua rao" : "Showers";
            case 3: return vi ? "Mua dong set" : "Thunderstorm";
            case 4: return vi ? "Tuyet roi" : "Snowfall";
            case 5: return vi ? "Suong mu" : "Heavy Fog";
            default: return vi ? "Troi mat me" : "Pleasant";
        }
    }
}

