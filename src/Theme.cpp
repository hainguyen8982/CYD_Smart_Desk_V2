// ═══════════════════════════════════════════════════════════════════════
//  Theme.cpp — Runtime theme manager & presets for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "Theme.h"
#include <Preferences.h>
#include <stdlib.h>

ThemeTokens theme;
static volatile uint8_t currentThemeIdx = 0;
static volatile bool minimalUiEnabled = true;  // Default to Minimal Zen UI (Apple StandBy / Braun style)
static const uint8_t TOTAL_THEMES = 8;

static const char* THEME_NAMES[] = {
    "Cyber Neon",
    "Minimal Obsidian",
    "Sunset Amber",
    "Cyberpunk",
    "Pastel Lavender",
    "Nordic Frost",
    "Sakura Blossom",
    "Emerald Forest"
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
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#0C1218");
    theme.cardBorder = hexToRGB565("#264A5E");
    theme.accent     = hexToRGB565("#64D2FF"); // Apple Ice Cyan
    theme.accentSec  = hexToRGB565("#EBBE5F"); // Champagne Gold
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FF5F57");
    theme.yellow     = hexToRGB565("#EBBE5F");
    theme.purple     = hexToRGB565("#BF5AF2");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#AAB2BE");
    theme.textVDim   = hexToRGB565("#606672");
    theme.dockBg     = hexToRGB565("#121418");
    theme.dockActive = hexToRGB565("#64D2FF");
    theme.track      = hexToRGB565("#1C2630");
}

static void applyMinimalObsidian() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#0C1412");
    theme.cardBorder = hexToRGB565("#22483A");
    theme.accent     = hexToRGB565("#34D399"); // Emerald Mint
    theme.accentSec  = hexToRGB565("#64D2FF"); // Ice Cyan
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FF5F57");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#A78BFA");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#AAB2BE");
    theme.textVDim   = hexToRGB565("#606672");
    theme.dockBg     = hexToRGB565("#121418");
    theme.dockActive = hexToRGB565("#34D399");
    theme.track      = hexToRGB565("#1A2622");
}

static void applySunsetAmber() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#141008");
    theme.cardBorder = hexToRGB565("#523A14");
    theme.accent     = hexToRGB565("#FFB703"); // Sunset Amber Gold
    theme.accentSec  = hexToRGB565("#FF5F57"); // Coral Red
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FF5F57");
    theme.yellow     = hexToRGB565("#FFB703");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#C4B098");
    theme.textVDim   = hexToRGB565("#665A4C");
    theme.dockBg     = hexToRGB565("#14120E");
    theme.dockActive = hexToRGB565("#FFB703");
    theme.track      = hexToRGB565("#2A2012");
}

static void applyCyberpunk() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#160A12");
    theme.cardBorder = hexToRGB565("#521E3A");
    theme.accent     = hexToRGB565("#FF3C96"); // Hot Magenta Pink
    theme.accentSec  = hexToRGB565("#00E5FF"); // Electric Cyan
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FF5F57");
    theme.yellow     = hexToRGB565("#FFE500");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#C0A4B8");
    theme.textVDim   = hexToRGB565("#685060");
    theme.dockBg     = hexToRGB565("#140E12");
    theme.dockActive = hexToRGB565("#FF3C96");
    theme.track      = hexToRGB565("#2A1422");
}

static void applyPastelLavender() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#120E18");
    theme.cardBorder = hexToRGB565("#3E2A56");
    theme.accent     = hexToRGB565("#C084FC"); // Soft Lavender
    theme.accentSec  = hexToRGB565("#F472B6"); // Rose Pink
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FDE047");
    theme.purple     = hexToRGB565("#E879F9");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#B8A8D4");
    theme.textVDim   = hexToRGB565("#645878");
    theme.dockBg     = hexToRGB565("#121016");
    theme.dockActive = hexToRGB565("#C084FC");
    theme.track      = hexToRGB565("#221A2E");
}

static void applyNordicFrost() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#0A1218");
    theme.cardBorder = hexToRGB565("#204258");
    theme.accent     = hexToRGB565("#38BDF8"); // Arctic Sky Blue
    theme.accentSec  = hexToRGB565("#2DD4BF"); // Mint Aqua
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FB7185");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#A78BFA");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#9CC4DC");
    theme.textVDim   = hexToRGB565("#526878");
    theme.dockBg     = hexToRGB565("#0E1418");
    theme.dockActive = hexToRGB565("#38BDF8");
    theme.track      = hexToRGB565("#18242E");
}

static void applySakuraBlossom() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#160C12");
    theme.cardBorder = hexToRGB565("#4E263A");
    theme.accent     = hexToRGB565("#F472B6"); // Sakura Pink
    theme.accentSec  = hexToRGB565("#FDBA74"); // Warm Peach
    theme.green      = hexToRGB565("#4ADE80");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FDE047");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#C8A0B2");
    theme.textVDim   = hexToRGB565("#68505C");
    theme.dockBg     = hexToRGB565("#140E12");
    theme.dockActive = hexToRGB565("#F472B6");
    theme.track      = hexToRGB565("#281820");
}

static void applyEmeraldForest() {
    theme.bg         = 0x0000;
    theme.card       = hexToRGB565("#0A1410");
    theme.cardBorder = hexToRGB565("#1C4632");
    theme.accent     = hexToRGB565("#10B981"); // Forest Emerald
    theme.accentSec  = hexToRGB565("#A3E635"); // Lime Gold
    theme.green      = hexToRGB565("#10B981");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#8CCCB0");
    theme.textVDim   = hexToRGB565("#4C6A5C");
    theme.dockBg     = hexToRGB565("#0E1412");
    theme.dockActive = hexToRGB565("#10B981");
    theme.track      = hexToRGB565("#16261E");
}

void applyThemePreset(uint8_t index) {
    uint8_t nextIdx = index % TOTAL_THEMES;
    switch (nextIdx) {
        case 0: applyCyberNeon(); break;
        case 1: applyMinimalObsidian(); break;
        case 2: applySunsetAmber(); break;
        case 3: applyCyberpunk(); break;
        case 4: applyPastelLavender(); break;
        case 5: applyNordicFrost(); break;
        case 6: applySakuraBlossom(); break;
        case 7: applyEmeraldForest(); break;
    }
    currentThemeIdx = nextIdx;
}

void nextTheme() {
    currentThemeIdx = (currentThemeIdx + 1) % TOTAL_THEMES;
    applyThemePreset(currentThemeIdx);
    saveTheme();
}

void prevTheme() {
    currentThemeIdx = (currentThemeIdx + TOTAL_THEMES - 1) % TOTAL_THEMES;
    applyThemePreset(currentThemeIdx);
    saveTheme();
}

void saveTheme() {
    Preferences prefs;
    prefs.begin("cyd_theme", false);
    prefs.putUChar("idx", currentThemeIdx);
    prefs.putBool("minimal", minimalUiEnabled);
    prefs.end();
    Serial.printf("[Theme] Saved theme index: %d (%s) | MinimalZen: %d\n",
                  currentThemeIdx, THEME_NAMES[currentThemeIdx], minimalUiEnabled ? 1 : 0);
}

void initTheme() {
    Preferences prefs;
    prefs.begin("cyd_theme", true);
    currentThemeIdx = prefs.getUChar("idx", 0);
    minimalUiEnabled = prefs.getBool("minimal", true);
    prefs.end();
    if (currentThemeIdx >= TOTAL_THEMES) currentThemeIdx = 0;
    applyThemePreset(currentThemeIdx);
    Serial.printf("[Theme] Initialized theme: %s (idx: %d) | MinimalZen: %d\n",
                  THEME_NAMES[currentThemeIdx], currentThemeIdx, minimalUiEnabled ? 1 : 0);
}

const char* getThemeName() {
    return THEME_NAMES[currentThemeIdx];
}

uint8_t getThemeIndex() {
    return currentThemeIdx;
}

bool isMinimalUiMode() {
    return minimalUiEnabled;
}

void setMinimalUiMode(bool enabled) {
    minimalUiEnabled = enabled;
    saveTheme();
}

void toggleMinimalUiMode() {
    minimalUiEnabled = !minimalUiEnabled;
    saveTheme();
}
