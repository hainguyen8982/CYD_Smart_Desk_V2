// ═══════════════════════════════════════════════════════════════════════
//  Theme.cpp — Runtime theme manager & presets for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#include "Theme.h"
#include <Preferences.h>
#include <stdlib.h>

ThemeTokens theme;
static uint8_t currentThemeIdx = 0;
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
    theme.bg         = hexToRGB565("#070B14");
    theme.card       = hexToRGB565("#142E86"); // Navy Blue Glass (RGB332: R=1, G=1, B=2)
    theme.cardBorder = hexToRGB565("#00D8F6"); // Glowing Neon Cyan
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
    theme.bg         = hexToRGB565("#080808");
    theme.card       = hexToRGB565("#242424"); // Neutral Dark Slate (RGB332: R=1, G=1, B=0)
    theme.cardBorder = hexToRGB565("#444444"); // Metallic Border
    theme.accent     = hexToRGB565("#00E676"); // Emerald
    theme.accentSec  = hexToRGB565("#38BDF8"); // Sky Blue
    theme.green      = hexToRGB565("#00E676");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#A78BFA");
    theme.textMain   = hexToRGB565("#F1F5F9");
    theme.textDim    = hexToRGB565("#74849B");
    theme.textVDim   = hexToRGB565("#383E48");
    theme.dockBg     = hexToRGB565("#0E1117");
    theme.dockActive = hexToRGB565("#00E676");
    theme.track      = hexToRGB565("#18202A");
}

static void applySunsetAmber() {
    theme.bg         = hexToRGB565("#140A04");
    theme.card       = hexToRGB565("#582E08"); // Warm Amber Caramel (RGB332: R=2, G=1, B=0)
    theme.cardBorder = hexToRGB565("#B46410"); // Radiant Gold Amber
    theme.accent     = hexToRGB565("#FFB703"); // Amber Gold
    theme.accentSec  = hexToRGB565("#FB8500"); // Sunset Orange
    theme.green      = hexToRGB565("#10B981");
    theme.red        = hexToRGB565("#EF4444");
    theme.yellow     = hexToRGB565("#FFB703");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFF7ED");
    theme.textDim    = hexToRGB565("#C49A70");
    theme.textVDim   = hexToRGB565("#5E3612");
    theme.dockBg     = hexToRGB565("#1C0E06");
    theme.dockActive = hexToRGB565("#FFB703");
    theme.track      = hexToRGB565("#341A06");
}

static void applyCyberpunk() {
    theme.bg         = hexToRGB565("#0E0414");
    theme.card       = hexToRGB565("#600C60"); // Electric Magenta (RGB332: R=3, G=0, B=1)
    theme.cardBorder = hexToRGB565("#C810A0"); // Hot Neon Pink
    theme.accent     = hexToRGB565("#FF007F"); // Hot Magenta
    theme.accentSec  = hexToRGB565("#00F0FF"); // Cyan
    theme.green      = hexToRGB565("#39FF14");
    theme.red        = hexToRGB565("#FF003C");
    theme.yellow     = hexToRGB565("#FFE500");
    theme.purple     = hexToRGB565("#B026FF");
    theme.textMain   = hexToRGB565("#FFFFFF");
    theme.textDim    = hexToRGB565("#B068FF");
    theme.textVDim   = hexToRGB565("#5C1A6E");
    theme.dockBg     = hexToRGB565("#160520");
    theme.dockActive = hexToRGB565("#FF007F");
    theme.track      = hexToRGB565("#32083E");
}

static void applyPastelLavender() {
    // Sắc tím hoa oải hương nhẹ nhàng, thanh tao, quý phái
    theme.bg         = hexToRGB565("#0D0818"); // Nền tím đêm sâu lắng
    theme.card       = hexToRGB565("#3C2282"); // Kính tím Lavender êm dịu (RGB332: R=2, G=1, B=2)
    theme.cardBorder = hexToRGB565("#8854D0"); // Viền hoa cà Lilac thanh thoát
    theme.accent     = hexToRGB565("#C084FC"); // Tím Lavender dịu êm
    theme.accentSec  = hexToRGB565("#F472B6"); // Hồng phấn phớt nhẹ
    theme.green      = hexToRGB565("#34D399"); // Xanh ngọc dịu
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FDE047");
    theme.purple     = hexToRGB565("#E879F9");
    theme.textMain   = hexToRGB565("#FAF5FF"); // Trắng ánh tím sang trọng
    theme.textDim    = hexToRGB565("#B8A8D4"); // Tím khói thanh nhã
    theme.textVDim   = hexToRGB565("#5C4C7E");
    theme.dockBg     = hexToRGB565("#140D24");
    theme.dockActive = hexToRGB565("#C084FC");
    theme.track      = hexToRGB565("#2E1C52");
}

static void applyNordicFrost() {
    // Băng tuyết Bắc Âu — Xanh da trời lạnh thanh lịch
    theme.bg         = hexToRGB565("#040E1A");
    theme.card       = hexToRGB565("#0E4484"); // Arctic Ice Blue Glass (RGB332: R=0, G=2, B=2)
    theme.cardBorder = hexToRGB565("#2888D0"); // Viền băng tuyết sáng
    theme.accent     = hexToRGB565("#38BDF8"); // Sky Blue
    theme.accentSec  = hexToRGB565("#2DD4BF"); // Mint Aqua
    theme.green      = hexToRGB565("#34D399");
    theme.red        = hexToRGB565("#FB7185");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#A78BFA");
    theme.textMain   = hexToRGB565("#F0F9FF");
    theme.textDim    = hexToRGB565("#80D4FC");
    theme.textVDim   = hexToRGB565("#20547C");
    theme.dockBg     = hexToRGB565("#081628");
    theme.dockActive = hexToRGB565("#38BDF8");
    theme.track      = hexToRGB565("#103254");
}

static void applySakuraBlossom() {
    // Hoa anh đào Nhật Bản — Tone hồng đào đậm đà & dễ chịu
    theme.bg         = hexToRGB565("#14060C");
    theme.card       = hexToRGB565("#681C38"); // Dusty Rose Glass (RGB332: R=3, G=1, B=1)
    theme.cardBorder = hexToRGB565("#C04874"); // Viền hoa anh đào tươi
    theme.accent     = hexToRGB565("#F472B6"); // Sakura Pink
    theme.accentSec  = hexToRGB565("#FDBA74"); // Peach Warm
    theme.green      = hexToRGB565("#4ADE80");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FDE047");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#FFF1F2");
    theme.textDim    = hexToRGB565("#C8A0B2");
    theme.textVDim   = hexToRGB565("#602A3E");
    theme.dockBg     = hexToRGB565("#1C0C14");
    theme.dockActive = hexToRGB565("#F472B6");
    theme.track      = hexToRGB565("#3A1424");
}

static void applyEmeraldForest() {
    // Rừng ngọc lục bảo — Thư giãn mắt, trong lành
    theme.bg         = hexToRGB565("#041208");
    theme.card       = hexToRGB565("#0C441E"); // Deep Emerald Glass (RGB332: R=0, G=2, B=0)
    theme.cardBorder = hexToRGB565("#189C4C"); // Viền ngọc lục bảo tươi
    theme.accent     = hexToRGB565("#10B981"); // Emerald Mint
    theme.accentSec  = hexToRGB565("#A3E635"); // Lime Gold
    theme.green      = hexToRGB565("#10B981");
    theme.red        = hexToRGB565("#F87171");
    theme.yellow     = hexToRGB565("#FBBF24");
    theme.purple     = hexToRGB565("#C084FC");
    theme.textMain   = hexToRGB565("#ECFDF5");
    theme.textDim    = hexToRGB565("#78E8BC");
    theme.textVDim   = hexToRGB565("#1C5C36");
    theme.dockBg     = hexToRGB565("#081A0E");
    theme.dockActive = hexToRGB565("#10B981");
    theme.track      = hexToRGB565("#103C20");
}

void applyThemePreset(uint8_t index) {
    currentThemeIdx = index % TOTAL_THEMES;
    switch (currentThemeIdx) {
        case 0: applyCyberNeon(); break;
        case 1: applyMinimalObsidian(); break;
        case 2: applySunsetAmber(); break;
        case 3: applyCyberpunk(); break;
        case 4: applyPastelLavender(); break;
        case 5: applyNordicFrost(); break;
        case 6: applySakuraBlossom(); break;
        case 7: applyEmeraldForest(); break;
    }
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
