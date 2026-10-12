// ═══════════════════════════════════════════════════════════════════════
//  Theme.h — Semantic Design System Tokens for CYD SmartDesk V2
//  Guarantees: Theme changes DO NOT affect layout geometry (0 pixel shift)
// ═══════════════════════════════════════════════════════════════════════
#ifndef THEME_H
#define THEME_H

#include <Arduino.h>

// ─── Semantic Color Tokens ───────────────────────────────────────────
struct ThemeTokens {
    uint16_t bg;          // Main background
    uint16_t card;        // Glass card fill
    uint16_t cardBorder;  // Subtle glass card border
    uint16_t accent;      // Primary accent (cyan / emerald / gold)
    uint16_t accentSec;   // Secondary accent (orange / amber / coral)
    uint16_t green;       // Success / increase indicator
    uint16_t red;         // Warning / decrease indicator / alarm
    uint16_t yellow;      // Gold price / lunar rằm / sun
    uint16_t purple;      // Special highlights / pomodoro break
    uint16_t textMain;    // Primary high-contrast text
    uint16_t textDim;     // Secondary muted text
    uint16_t textVDim;    // Inactive labels
    uint16_t dockBg;      // Bottom dock background
    uint16_t dockActive;  // Active dock pill highlight
    uint16_t track;       // Gauge / sparkline base track
};

extern ThemeTokens theme;

// Theme functions
void initTheme();
void saveTheme();
void applyThemePreset(uint8_t index);
void nextTheme();
const char* getThemeName();
uint8_t getThemeIndex();

// Helper
uint16_t hexToRGB565(const char* hex);

#endif // THEME_H
