// ═══════════════════════════════════════════════════════════════════════
//  Theme.cpp — Runtime theme manager & presets for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "Theme.h"
#include <Preferences.h>
#include <stdlib.h>

ThemeTokens theme;
static uint8_t currentThemeIdx = 0;
static const uint8_t TOTAL_THEMES = 4;

static const char* THEME_NAMES[] = {
    "Cyber Neon",
    "Minimal Obsidian",
    "Sunset Amber",
    "Cyberpunk"
};

uint16_t hexToRGB565(const char* hex) {
    if (!hex) return 0x0000;
    if (hex[0] == '#') hex++;
    uint32_t rgb = strtoul(hex, nullptr, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b = (rgb >>  0) & 0xFF;
    return ((uint16_t)(r >> 3) << 11) |
           ((uint16_t)(g >> 2) <<  5) |
           ((uint16_t)(b >> 3));
}

static void applyCyberNeon() {
    theme.bg         = hexToRGB565("#070B14");
    theme.card       = hexToRGB565("#162238"); // Translucent navy glass tint
    theme.cardBorder = hexToRGB565("#385E90"); // Radiant glassmorphic border
    theme.accent     = hexToRGB565("#00E5FF"); // Neon Cyan
    theme.accentSec  = hexToRGB565("#FF7700"); // Neon Orange
    theme.green      = hexToRGB565("#00E676");
    theme.red        = hexToRGB565("#FF3B30");
    theme.yellow     = hexToRGB565("#FFD60A");
    theme.purple     = hexToRGB565("#BF5AF2");
    theme.textMain   = hexToRGB565("#F8FAFC");
    theme.textDim    = hexToRGB565("#8899B0");
    theme.textVDim   = hexToRGB565("#475569");
    theme.dockBg     = hexToRGB565("#0E1626");
    theme.dockActive = hexToRGB565("#00E5FF");
    theme.track      = hexToRGB565("#1E2D48");
}

static void applyMinimalObsidian() {
    theme.bg         = hexToRGB565("#090B0E");
    theme.card       = hexToRGB565("#14181E");
    theme.cardBorder = hexToRGB565("#222832");
    theme.accent     = hexToRGB565("#00E676"); // Emerald
    theme.accentSec  = hexToRGB565("#38BDF8"); // Sky Blue
    theme.green      = hexToRGB565("#00E676");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#A78BFA");
    theme.textMain   = hexToRGB565("#F1F5F9");
    theme.textDim    = hexToRGB565("#64748B");
    theme.textVDim   = hexToRGB565("#2E3542");
    theme.dockBg     = hexToRGB565("#0E1117");
    theme.dockActive = hexToRGB565("#00E676");
    theme.track      = hexToRGB565("#18202A");
}

static void applySunsetAmber() {
    theme.bg         = hexToRGB565("#120A16");
    theme.card       = hexToRGB565("#1E1226");
    theme.cardBorder = hexToRGB565("#301D3C");
    theme.accent     = hexToRGB565("#FFB703"); // Amber Gold
    theme.accentSec  = hexToRGB565("#FB8500"); // Sunset Orange
    theme.green      = hexToRGB565("#10B981");
    theme.red        = hexToRGB565("#EF4444");
    theme.yellow     = hexToRGB565("#FFB703");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFF1F2");
    theme.textDim    = hexToRGB565("#9F7E9B");
    theme.textVDim   = hexToRGB565("#422A4E");
    theme.dockBg     = hexToRGB565("#170D1E");
    theme.dockActive = hexToRGB565("#FFB703");
    theme.track      = hexToRGB565("#281634");
}

static void applyCyberpunk() {
    theme.bg         = hexToRGB565("#080310");
    theme.card       = hexToRGB565("#160829");
    theme.cardBorder = hexToRGB565("#2D124E");
    theme.accent     = hexToRGB565("#FF007F"); // Hot Magenta
    theme.accentSec  = hexToRGB565("#00F0FF"); // Cyan
    theme.green      = hexToRGB565("#39FF14");
    theme.red        = hexToRGB565("#FF003C");
    theme.yellow     = hexToRGB565("#FFE500");
    theme.purple     = hexToRGB565("#B026FF");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#8C52FF");
    theme.textVDim   = hexToRGB565("#3E186D");
    theme.dockBg     = hexToRGB565("#0D051A");
    theme.dockActive = hexToRGB565("#FF007F");
    theme.track      = hexToRGB565("#240B3F");
}

void applyThemePreset(uint8_t index) {
    currentThemeIdx = index % TOTAL_THEMES;
    switch (currentThemeIdx) {
        case 0: applyCyberNeon(); break;
        case 1: applyMinimalObsidian(); break;
        case 2: applySunsetAmber(); break;
        case 3: applyCyberpunk(); break;
    }
}

void nextTheme() {
    currentThemeIdx = (currentThemeIdx + 1) % TOTAL_THEMES;
    applyThemePreset(currentThemeIdx);
    saveTheme();
}

void saveTheme() {
    Preferences prefs;
    prefs.begin("cyd_theme", false);
    prefs.putUChar("idx", currentThemeIdx);
    prefs.end();
    Serial.printf("[Theme] Saved theme index: %d (%s)\n", currentThemeIdx, THEME_NAMES[currentThemeIdx]);
}

void initTheme() {
    Preferences prefs;
    prefs.begin("cyd_theme", true);
    currentThemeIdx = prefs.getUChar("idx", 0);
    prefs.end();
    if (currentThemeIdx >= TOTAL_THEMES) currentThemeIdx = 0;
    applyThemePreset(currentThemeIdx);
    Serial.printf("[Theme] Initialized theme: %s (idx: %d)\n", THEME_NAMES[currentThemeIdx], currentThemeIdx);
}

const char* getThemeName() {
    return THEME_NAMES[currentThemeIdx];
}

uint8_t getThemeIndex() {
    return currentThemeIdx;
}
