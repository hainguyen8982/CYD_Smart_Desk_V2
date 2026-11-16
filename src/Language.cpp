// ═══════════════════════════════════════════════════════════════════════
//  Language.cpp — Bilingual (VI / EN) Implementation for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "Language.h"
#include <Preferences.h>

static Language _currentLang = LANG_VI;

static const char* const STRINGS_VI[TXT_KEY_COUNT] = {
    // TXT_SETTINGS
    "CÀI ĐẶT",
    // TXT_SYSTEM_IP
    "smartdesk.local",
    // TXT_TEST
    "THỬ",
    // TXT_AUTO
    "AUTO",
    // TXT_BACK
    "QUAY LẠI",
    // TXT_CLOSE
    "ĐÓNG",

    // Page 4 Settings Rows
    // TXT_ROW_LANGUAGE
    "Ngôn ngữ (Language)",
    // TXT_ROW_THEME
    "Giao diện (Theme)",
    // TXT_ROW_TOUCH_SND
    "Âm chạm cảm ứng",
    // TXT_ROW_ALARM_MEL
    "Điệu chuông báo",
    // TXT_ROW_STARTUP_SND
    "Âm khởi động",
    // TXT_ROW_BRIGHTNESS
    "Độ sáng",

    // Page 2 Pomodoro
    // TXT_POMO_FOCUS
    "TẬP TRUNG",
    // TXT_POMO_SHORT_BREAK
    "NGHỈ NGẮN",
    // TXT_POMO_LONG_BREAK
    "NGHỈ DÀI",
    // TXT_POMO_START
    "BẮT ĐẦU",
    // TXT_POMO_PAUSE
    "TẠM DỪNG",
    // TXT_POMO_RESET
    "ĐẶT LẠI",
    // TXT_POMO_COMPLETED
    "HOÀN THÀNH!",

    // Page 3 Alarm
    // TXT_ALARM_TITLE
    "BÁO THỨC",
    // TXT_ALARM_SAVE_ON
    "Lưu & Bật",
    // TXT_ALARM_DISMISS
    "Tắt báo thức",
    // TXT_ALARM_MSG_DEFAULT
    "Báo thức!",

    // Page 1 Finance
    // TXT_FINANCE_SJC
    "VÀNG SJC 9999",
    // TXT_FINANCE_WORLD
    "VÀNG THẾ GIỚI",
    // TXT_FINANCE_FOREX
    "TỶ GIÁ NGOẠI TỆ",
    // TXT_FINANCE_7D
    "Lịch sử 7 ngày",
    // TXT_FINANCE_BUY
    "Mua:",
    // TXT_FINANCE_SELL
    "Bán:",

    // Page 0 Modals & Weather
    // TXT_ASTRONOMY_LUNAR
    "THIÊN VĂN & ÂM LỊCH",
    // TXT_LUNAR_DATE
    "Ngày âm lịch:",
    // TXT_DAY_CANCHI
    "Ngày can chi:",
    // TXT_YEAR_CANCHI
    "Năm can chi:",
    // TXT_TEMP
    "Nhiệt độ",
    // TXT_HUMIDITY
    "Độ ẩm",
    // TXT_WIND
    "Gió",
    // TXT_UV
    "Chỉ số UV"
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
    // TXT_ROW_STARTUP_SND
    "Boot Sound",
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
    "CHỦ NHẬT", "THỨ HAI", "THỨ BA", "THỨ TƯ",
    "THỨ NĂM", "THỨ SÁU", "THỨ BẢY"
};

static const char* const DAYS_EN[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY"
};

// Touch sound names
static const char* const TOUCH_SND_NAMES_VI[] = {
    "Tắt âm", "Phím cơ", "Giọt nước", "Bíp nhẹ", "Chíp chíp"
};
static const char* const TOUCH_SND_NAMES_EN[] = {
    "Mute", "Mechanical", "Water Drop", "Soft Beep", "Dual Chirp"
};

// Alarm melody names
static const char* const ALARM_NAMES_VI[] = {
    "Cổ điển", "Nhẹ nhàng", "Cyber", "Êm dịu"
};
static const char* const ALARM_NAMES_EN[] = {
    "Classic", "Melodic", "Cyber", "Gentle"
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
    return (_currentLang == LANG_VI) ? "Tiếng Việt" : "English";
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

static const char* const STARTUP_NAMES_VI[] = {
    "Marimba",
    "Pha lê",
    "Cyber",
    "Retro",
    "Tắt âm"
};

static const char* const STARTUP_NAMES_EN[] = {
    "Marimba",
    "Crystal",
    "Cyber",
    "Retro",
    "Mute"
};

const char* getStartupSoundNameI18n(uint8_t id) {
    if (id >= 5) id = 0;
    return (_currentLang == LANG_VI) ? STARTUP_NAMES_VI[id] : STARTUP_NAMES_EN[id];
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

const char* getMiniDayNameI18n(uint8_t wday) {
    if (wday >= 7) wday = 0;
    static const char* const MINI_VI[] = { "CN", "T2", "T3", "T4", "T5", "T6", "T7" };
    static const char* const MINI_EN[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
    return (_currentLang == LANG_VI) ? MINI_VI[wday] : MINI_EN[wday];
}

const char* getMoonPhaseNameI18n(uint8_t lunarDay) {
    bool vi = (_currentLang == LANG_VI);
    if (lunarDay <= 1)       return vi ? "Trăng Non" : "New Moon";
    else if (lunarDay <= 6)  return vi ? "Trăng Lưỡi Liềm" : "Waxing Crescent";
    else if (lunarDay <= 8)  return vi ? "Bán Nguyệt Đầu" : "First Quarter";
    else if (lunarDay <= 13) return vi ? "Trăng Trương Dần" : "Waxing Gibbous";
    else if (lunarDay <= 16) return vi ? "Trăng Tròn (Rằm)" : "Full Moon";
    else if (lunarDay <= 21) return vi ? "Trăng Khuyết Dần" : "Waning Gibbous";
    else if (lunarDay <= 23) return vi ? "Bán Nguyệt Cuối" : "Last Quarter";
    else                     return vi ? "Trăng Tàn Cuối" : "Waning Crescent";
}

const char* getWeatherConditionI18n(uint8_t type, bool isNight, bool isPopup) {
    bool vi = (_currentLang == LANG_VI);
    if (!isPopup) {
        // Short version for Page 0 card
        switch (type) {
            case 0: return isNight ? (vi ? "Đêm quang" : "Clear Night") : (vi ? "Trời nắng" : "Sunny");
            case 1: return vi ? "Có mây" : "Partly Cloudy";
            case 2: return vi ? "Có mưa" : "Rain";
            case 3: return vi ? "Mưa dông" : "Thunderstorm";
            case 4: return vi ? "Tuyết" : "Snow";
            case 5: return vi ? "Sương mù" : "Misty / Fog";
            default: return vi ? "Trời đẹp" : "Clear Sky";
        }
    } else {
        // Longer descriptive version for popup modal
        switch (type) {
            case 0: return isNight ? (vi ? "Đêm quang" : "Clear Night") : (vi ? "Trời nắng đẹp" : "Sunny & Clear");
            case 1: return vi ? "Nhiều mây" : "Cloudy";
            case 2: return vi ? "Trời mưa rào" : "Showers";
            case 3: return vi ? "Mưa dông sét" : "Thunderstorm";
            case 4: return vi ? "Tuyết rơi" : "Snowfall";
            case 5: return vi ? "Sương mù" : "Heavy Fog";
            default: return vi ? "Trời mát mẻ" : "Pleasant";
        }
    }
}
