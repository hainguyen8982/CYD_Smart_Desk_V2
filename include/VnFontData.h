// ═══════════════════════════════════════════════════════════════════════
//  VnFontData.h — Unified 4-bit Anti-Aliased Font Structs & Declarations
//  Font bitmap data lives in ZenFonts.h (PROGMEM Flash, 0 bytes RAM)
// ═══════════════════════════════════════════════════════════════════════
#ifndef VN_FONT_DATA_H
#define VN_FONT_DATA_H

#include <Arduino.h>

struct VnGlyph {
    uint16_t unicode;
    uint8_t  width;
    uint8_t  height;
    uint8_t  xAdvance;
    int8_t   xOffset;
    int8_t   yOffset;
    uint32_t bitmapOffset;
};

struct VnFontFace {
    uint16_t       glyphCount;
    uint8_t        lineHeight;
    uint8_t        baseline;
    uint8_t        spaceWidth;
    const VnGlyph* glyphs;
    const uint8_t* bitmaps;
};

// Unified Zen Font faces (defined once in ZenFonts.h / Display.cpp)
extern const VnFontFace ZEN_FONT_CLOCK;
extern const VnFontFace ZEN_FONT_NUM;
extern const VnFontFace ZEN_FONT_LIGHT;

#endif // VN_FONT_DATA_H
