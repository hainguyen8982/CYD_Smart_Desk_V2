// ═══════════════════════════════════════════════════════════════════════
//  Colors.h — Curated 16-bit RGB565 color palette
//  Designed for vibrant, premium look on ILI9341 320×240 TFT
// ═══════════════════════════════════════════════════════════════════════
#ifndef COLORS_H
#define COLORS_H

#include <Arduino.h>

// ─── Utility: compile-time RGB888 → RGB565 ───────────────────────────
#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

// ─── Base ────────────────────────────────────────────────────────────
#define C_BLACK         0x0000
#define C_WHITE         0xFFFF

// ─── Deep Dark Background Palette ────────────────────────────────────
#define C_BG_DARK       RGB565(10, 14, 36)      // #0A0E24  deep space navy
#define C_BG_MID        RGB565(18, 24, 58)      // #12183A  slightly lighter
#define C_BG_LIGHT      RGB565(30, 38, 80)      // #1E2650  card hover

// ─── Pastel Accent Blobs (for background decoration) ─────────────────
#define C_BLOB_PINK     RGB565(255, 140, 200)   // soft pastel pink
#define C_BLOB_CORAL    RGB565(255, 160, 140)   // pastel coral
#define C_BLOB_MINT     RGB565(100, 230, 200)   // pastel mint/teal
#define C_BLOB_LILAC    RGB565(180, 140, 255)   // pastel lilac/purple
#define C_BLOB_YELLOW   RGB565(255, 230, 120)   // pastel warm yellow
#define C_BLOB_BLUE     RGB565(100, 180, 255)   // pastel sky blue

// ─── Primary Accent ──────────────────────────────────────────────────
#define C_CYAN          RGB565(0, 220, 255)     // #00DCFF  primary cyan
#define C_CYAN_DIM      RGB565(0, 140, 180)     // dimmed cyan for subtitles
#define C_CYAN_GLOW     RGB565(80, 240, 255)    // bright glow highlight

// ─── Secondary Accents ───────────────────────────────────────────────
#define C_GOLD          RGB565(255, 215, 0)     // #FFD700  gold / temperature
#define C_AMBER         RGB565(255, 180, 0)     // warm amber
#define C_ORANGE        RGB565(255, 140, 50)    // vibrant orange
#define C_GREEN         RGB565(80, 220, 120)    // fresh green
#define C_RED           RGB565(255, 80, 80)     // alert red

// ─── Glass Card Palette ──────────────────────────────────────────────
#define C_CARD_BG       RGB565(16, 28, 56)      // #101C38  semi-dark glass
#define C_CARD_BORDER   RGB565(40, 70, 120)     // #284678  subtle border
#define C_CARD_SHINE    RGB565(100, 180, 240)   // specular top highlight

// ─── Text ────────────────────────────────────────────────────────────
#define C_TEXT_PRIMARY  C_WHITE
#define C_TEXT_DIM      RGB565(160, 175, 200)   // secondary text
#define C_TEXT_VDIM     RGB565(100, 115, 140)   // tertiary text

// ─── Navigation Bar ─────────────────────────────────────────────────
#define C_NAV_BG        RGB565(20, 40, 80)      // dark nav background
#define C_NAV_BTN       RGB565(40, 160, 220)    // cyan button fill
#define C_NAV_BTN_SHINE RGB565(100, 210, 255)   // button top shine

// ─── Sparkle / Star ──────────────────────────────────────────────────
#define C_SPARKLE_W     C_WHITE
#define C_SPARKLE_CYAN  C_CYAN_GLOW

// ─── Weather-specific ────────────────────────────────────────────────
#define C_SUN_YELLOW    RGB565(255, 210, 50)    // sun core
#define C_SUN_ORANGE    RGB565(255, 170, 30)    // sun ray glow
#define C_CLOUD_WHITE   RGB565(230, 235, 240)   // cloud body
#define C_CLOUD_SHADOW  RGB565(170, 185, 200)   // cloud underside
#define C_RAIN_BLUE     RGB565(80, 160, 255)    // rain drops

// ─── Moon ────────────────────────────────────────────────────────────
#define C_MOON_FACE     RGB565(240, 230, 200)   // lit moon surface
#define C_MOON_SHADOW   RGB565(80, 75, 60)      // dark side of moon
#define C_MOON_CRATER   RGB565(200, 190, 160)   // subtle crater marks

// ─── Runtime hex → RGB565 converter ──────────────────────────────────
inline uint16_t hexToRGB565(const char* hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return C_BLACK;
    long val = strtol(hex + 1, NULL, 16);
    uint8_t r = (val >> 16) & 0xFF;
    uint8_t g = (val >> 8) & 0xFF;
    uint8_t b = val & 0xFF;
    return RGB565(r, g, b);
}

#endif // COLORS_H
