// ═══════════════════════════════════════════════════════════════════════
//  VnFont.h — Zero-RAM Anti-Aliased UTF-8 Vietnamese Sprite Renderer
//  Drop-in subclass of TFT_eSprite for CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#ifndef VN_FONT_H
#define VN_FONT_H

#include <Arduino.h>
#include <math.h>
#include <TFT_eSPI.h>
#include "VnFontData.h"

class VnSprite : public TFT_eSprite {
public:
    explicit VnSprite(TFT_eSPI* tft) : TFT_eSprite(tft) {}

    // ───────────────────────────────────────────────────────────────────
    //  True Sub-Pixel SDF Anti-Aliased Shapes (Live Sprite Pixel Blend)
    //  Eliminates TFT_eSPI 0xFFFF white-corner speckles, concentric
    //  border thinning/beading, and static bg_color mismatches.
    // ───────────────────────────────────────────────────────────────────
    void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color, uint32_t bg_color = 0x00FFFFFF) {
        (void)bg_color;
        if (w <= 0 || h <= 0) return;
        int32_t maxR = (w < h ? w : h) / 2;
        if (r > maxR) r = maxR;
        if (r <= 0) {
            fillRect(x, y, w, h, color);
            return;
        }

        // Detect concentric inner fill after an outer border fillSmoothRoundRect:
        // Preserves a solid 0.50px border plateau around all 4 rounded corners
        // so 1px/2px borders never thin out or bead at diagonal pixels.
        int32_t t = x - _lastRRX;
        bool isConcentricInner = (t >= 1 && t <= 3) &&
                                 (y == _lastRRY + t) &&
                                 (w == _lastRRW - 2 * t) &&
                                 (h == _lastRRH - 2 * t) &&
                                 (r == _lastRRR - t);

        _lastRRX = x;
        _lastRRY = y;
        _lastRRW = w;
        _lastRRH = h;
        _lastRRR = r;

        // 1. Fill the 3 non-corner interior rectangles
        if (w > 2 * r) {
            fillRect(x + r, y, w - 2 * r, r, color);
            fillRect(x + r, y + h - r, w - 2 * r, r, color);
        }
        if (h > 2 * r) {
            fillRect(x, y + r, w, h - 2 * r, color);
        }

        // 2. Sub-pixel Euclidean SDF for the 4 (r x r) corner quadrants
        int32_t cxL = x + r;
        int32_t cxR = x + w - 1 - r;
        int32_t cyT = y + r;
        int32_t cyB = y + h - 1 - r;

        float rSolid = isConcentricInner ? ((float)r - 0.15f) : ((float)r + 0.25f);
        float rEnd   = isConcentricInner ? ((float)r + 0.75f) : ((float)r + 1.15f);
        float invSpan = 255.0f / (rEnd - rSolid);
        uint16_t fg = (uint16_t)color;

        for (int32_t dy = 1; dy <= r; dy++) {
            for (int32_t dx = 1; dx <= r; dx++) {
                float d = sqrtf((float)(dx * dx + dy * dy));
                if (d >= rEnd) continue;

                int32_t px[4] = { cxL - dx, cxR + dx, cxL - dx, cxR + dx };
                int32_t py[4] = { cyT - dy, cyT - dy, cyB + dy, cyB + dy };

                if (d <= rSolid) {
                    for (int k = 0; k < 4; k++) {
                        drawPixel(px[k], py[k], fg);
                    }
                } else {
                    uint8_t alpha = (uint8_t)((rEnd - d) * invSpan);
                    if (alpha == 0) continue;
                    for (int k = 0; k < 4; k++) {
                        if (px[k] >= 0 && px[k] < width() && py[k] >= 0 && py[k] < height()) {
                            uint16_t bg = readPixel(px[k], py[k]);
                            drawPixel(px[k], py[k], alphaBlend(alpha, fg, bg));
                        }
                    }
                }
            }
        }
    }

    void drawSmoothRoundRect(int32_t x, int32_t y, int32_t r, int32_t ir, int32_t w, int32_t h, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF, uint8_t quadrants = 0xF) {
        (void)bg_color;
        if (w <= 0 || h <= 0) return;
        // Convert TFT_eSPI (w = right - left, h = bottom - top) inclusive span to pixel dimensions
        int32_t rw = w + 1;
        int32_t rh = h + 1;
        int32_t maxR = (rw < rh ? rw : rh) / 2;
        if (r > maxR) r = maxR;
        if (ir < 0) ir = 0;
        if (ir >= r) ir = r - 1;

        int32_t strokeW = r - ir;
        uint16_t fg = (uint16_t)fg_color;

        // Straight border segments
        if (rw > 2 * r) {
            fillRect(x + r, y, rw - 2 * r, strokeW, fg);
            fillRect(x + r, y + rh - strokeW, rw - 2 * r, strokeW, fg);
        }
        if (rh > 2 * r) {
            fillRect(x, y + r, strokeW, rh - 2 * r, fg);
            fillRect(x + rw - strokeW, y + r, strokeW, rh - 2 * r, fg);
        }

        // 4 anti-aliased corner arcs blended against true sprite pixels
        int32_t cxL = x + r;
        int32_t cxR = x + rw - 1 - r;
        int32_t cyT = y + r;
        int32_t cyB = y + rh - 1 - r;

        float rInnerAA    = (float)ir - 0.15f;
        float rInnerSolid = (float)ir + 0.75f;
        float rOuterSolid = (float)r  + 0.25f;
        float rOuterAA    = (float)r  + 1.15f;

        for (int32_t dy = 1; dy <= r + 1; dy++) {
            for (int32_t dx = 1; dx <= r + 1; dx++) {
                float d = sqrtf((float)(dx * dx + dy * dy));
                if (d <= rInnerAA || d >= rOuterAA) continue;

                uint8_t alpha = 255;
                if (d < rInnerSolid) {
                    alpha = (uint8_t)(((d - rInnerAA) / (rInnerSolid - rInnerAA)) * 255.0f);
                } else if (d > rOuterSolid) {
                    alpha = (uint8_t)(((rOuterAA - d) / (rOuterAA - rOuterSolid)) * 255.0f);
                }
                if (alpha == 0) continue;

                int32_t px[4] = { cxL - dx, cxR + dx, cxR + dx, cxL - dx };
                int32_t py[4] = { cyT - dy, cyT - dy, cyB + dy, cyB + dy };

                for (int k = 0; k < 4; k++) {
                    if (!(quadrants & (1 << k))) continue;
                    if (px[k] >= 0 && px[k] < width() && py[k] >= 0 && py[k] < height()) {
                        if (alpha == 255) {
                            drawPixel(px[k], py[k], fg);
                        } else {
                            uint16_t bg = readPixel(px[k], py[k]);
                            drawPixel(px[k], py[k], alphaBlend(alpha, fg, bg));
                        }
                    }
                }
            }
        }
    }

    void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t color, uint32_t bg_color = 0x00FFFFFF) {
        (void)bg_color;
        if (r <= 0) {
            drawPixel(x, y, color);
            return;
        }
        float rSolid = (float)r - 0.25f;
        float rEnd   = (float)r + 0.75f;
        uint16_t fg = (uint16_t)color;

        for (int32_t dy = -r - 1; dy <= r + 1; dy++) {
            int32_t py = y + dy;
            if (py < 0 || py >= height()) continue;
            for (int32_t dx = -r - 1; dx <= r + 1; dx++) {
                int32_t px = x + dx;
                if (px < 0 || px >= width()) continue;
                float d = sqrtf((float)(dx * dx + dy * dy));
                if (d >= rEnd) continue;
                if (d <= rSolid) {
                    drawPixel(px, py, fg);
                } else {
                    uint8_t alpha = (uint8_t)((rEnd - d) * 255.0f);
                    if (alpha > 0) {
                        uint16_t bg = readPixel(px, py);
                        drawPixel(px, py, alphaBlend(alpha, fg, bg));
                    }
                }
            }
        }
    }

    void drawSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF) {
        (void)bg_color;
        if (r <= 0) return;
        float rInnerAA    = (float)r - 0.85f;
        float rInnerSolid = (float)r - 0.15f;
        float rOuterSolid = (float)r + 0.15f;
        float rOuterAA    = (float)r + 0.85f;
        uint16_t fg = (uint16_t)fg_color;

        for (int32_t dy = -r - 1; dy <= r + 1; dy++) {
            int32_t py = y + dy;
            if (py < 0 || py >= height()) continue;
            for (int32_t dx = -r - 1; dx <= r + 1; dx++) {
                int32_t px = x + dx;
                if (px < 0 || px >= width()) continue;
                float d = sqrtf((float)(dx * dx + dy * dy));
                if (d <= rInnerAA || d >= rOuterAA) continue;

                uint8_t alpha = 255;
                if (d < rInnerSolid) {
                    alpha = (uint8_t)(((d - rInnerAA) / (rInnerSolid - rInnerAA)) * 255.0f);
                } else if (d > rOuterSolid) {
                    alpha = (uint8_t)(((rOuterAA - d) / (rOuterAA - rOuterSolid)) * 255.0f);
                }
                if (alpha == 255) {
                    drawPixel(px, py, fg);
                } else if (alpha > 0) {
                    uint16_t bg = readPixel(px, py);
                    drawPixel(px, py, alphaBlend(alpha, fg, bg));
                }
            }
        }
    }

    int16_t textWidth(const char* string, uint8_t font) {
        if (!string || !*string) return 0;
        const VnFontFace* face = selectFace(string, font);
        if (!face) {
            return TFT_eSprite::textWidth(string, font);
        }
        return measureWidth(face, string);
    }

    int16_t textWidth(const char* string) {
        return textWidth(string, textfont);
    }

    int16_t textWidth(const String& string, uint8_t font) {
        return textWidth(string.c_str(), font);
    }

    int16_t textWidth(const String& string) {
        return textWidth(string.c_str(), textfont);
    }

    int16_t drawString(const char* string, int32_t poX, int32_t poY, uint8_t font) {
        if (!string || !*string) return 0;
        const VnFontFace* face = selectFace(string, font);
        if (!face) {
            return TFT_eSprite::drawString(string, poX, poY, font);
        }
        return renderVnString(face, string, poX, poY);
    }

    int16_t drawString(const char* string, int32_t poX, int32_t poY) {
        return drawString(string, poX, poY, textfont);
    }

    int16_t drawString(const String& string, int32_t poX, int32_t poY, uint8_t font) {
        return drawString(string.c_str(), poX, poY, font);
    }

    int16_t drawString(const String& string, int32_t poX, int32_t poY) {
        return drawString(string.c_str(), poX, poY, textfont);
    }

    void setVnClip(int32_t x, int32_t y, int32_t w, int32_t h) {
        _vnClipX0 = x;
        _vnClipY0 = y;
        _vnClipX1 = x + w;
        _vnClipY1 = y + h;
        _hasVnClip = true;
    }

    void resetVnClip() {
        _hasVnClip = false;
    }

private:
    bool    _hasVnClip = false;
    int32_t _vnClipX0 = 0;
    int32_t _vnClipY0 = 0;
    int32_t _vnClipX1 = 320;
    int32_t _vnClipY1 = 240;
    int32_t _lastRRX = -9999;
    int32_t _lastRRY = -9999;
    int32_t _lastRRW = -9999;
    int32_t _lastRRH = -9999;
    int32_t _lastRRR = -9999;

    static bool hasUTF8(const char* s) {
        while (*s) {
            if (((uint8_t)*s) >= 0x80) return true;
            s++;
        }
        return false;
    }

    static bool hasAlphaOrUTF8(const char* s) {
        while (*s) {
            uint8_t c = (uint8_t)*s;
            if (c >= 0x80 || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                return true;
            }
            s++;
        }
        return false;
    }

    static const VnFontFace* selectFace(const char* s, uint8_t font) {
        if (font == 1) {
            return &VN_FONT_SMALL;
        }
        if (font == 2) {
            return &VN_FONT_MEDIUM;
        }
        if (font == 4) {
            return hasAlphaOrUTF8(s) ? &VN_FONT_LARGE : nullptr;
        }
        return nullptr;
    }

    // ── Vector icon glyphs for symbols/emoji not present in the bitmap font ──
    enum : uint8_t { SYM_NONE = 0, SYM_ZERO, SYM_STAR, SYM_HEART, SYM_FLOWER,
                     SYM_YINYANG, SYM_RING, SYM_CAKE, SYM_CANDLE };

    static uint8_t symbolType(uint32_t c) {
        // Zero-width modifiers: variation selectors, ZWJ, keycap, skin tones
        if (c == 0xFE0F || c == 0xFE0E || c == 0x200D || c == 0x20E3 ||
            (c >= 0x1F3FB && c <= 0x1F3FF)) return SYM_ZERO;
        switch (c) {
            case 0x2605: case 0x2606: case 0x2B50: case 0x1F31F: case 0x2728:
            case 0x1F389: case 0x1F38A: case 0x1F381:             return SYM_STAR;
            case 0x2764: case 0x2665: case 0x2661: case 0x1F9E1:
            case 0x1F90D: case 0x1F5A4:                            return SYM_HEART;
            case 0x273F: case 0x2740: case 0x1F338: case 0x1F33A:
            case 0x1F337: case 0x1F339: case 0x1F33C:              return SYM_FLOWER;
            case 0x262F:                                           return SYM_YINYANG;
            case 0x1F48D: case 0x1F48E:                            return SYM_RING;
            case 0x1F382: case 0x1F370: case 0x1F9C1:              return SYM_CAKE;
            case 0x1F56F:                                          return SYM_CANDLE;
            default: break;
        }
        if (c >= 0x1F493 && c <= 0x1F49F) return SYM_HEART;
        // Any other emoji / dingbat → generic star so nothing renders as blank
        if ((c >= 0x1F300 && c <= 0x1FAFF) || (c >= 0x2600 && c <= 0x27BF)) return SYM_STAR;
        return SYM_NONE;
    }

    static int16_t symbolSize(const VnFontFace* face) {
        int16_t s = face->baseline;
        if (s < 7) s = 7;
        return s;
    }

    static int16_t symbolAdvance(const VnFontFace* face, uint8_t type) {
        if (type == SYM_ZERO) return 0;
        return symbolSize(face) + 2;
    }

    static bool inStar(float u, float v) {
        // 5-point star, point up, outer R=1, inner r=0.42 (even-odd ray cast)
        static float px[10], py[10];
        static bool init = false;
        if (!init) {
            for (int i = 0; i < 10; i++) {
                float a = -1.5707963f + i * 0.6283185f;
                float r = (i & 1) ? 0.42f : 1.0f;
                px[i] = cosf(a) * r;
                py[i] = sinf(a) * r + 0.08f;
            }
            init = true;
        }
        bool in = false;
        for (int i = 0, j = 9; i < 10; j = i++) {
            if (((py[i] > v) != (py[j] > v)) &&
                (u < (px[j] - px[i]) * (v - py[i]) / (py[j] - py[i]) + px[i])) in = !in;
        }
        return in;
    }

    // Returns color id: 0 none, 1 text, 2 red/pink, 3 dim, 4 orange flame, 5 ice white
    static uint8_t shapeColor(uint8_t type, float u, float v) {
        switch (type) {
            case SYM_STAR:
                return inStar(u, v) ? 1 : 0;
            case SYM_HEART: {
                float x = u * 1.25f, y = -v * 1.25f + 0.25f;
                float a = x * x + y * y - 1.0f;
                return (a * a * a - x * x * y * y * y <= 0.0f) ? 2 : 0;
            }
            case SYM_FLOWER: {
                if (u * u + v * v < 0.07f) return 4;
                static const float fxp[5] = { 0.0f, 0.5231f, 0.3233f, -0.3233f, -0.5231f };
                static const float fyp[5] = { -0.55f, -0.1700f, 0.4450f, 0.4450f, -0.1700f };
                for (int i = 0; i < 5; i++) {
                    float dx = u - fxp[i], dy = v - fyp[i];
                    if (dx * dx + dy * dy < 0.17f) return 1;
                }
                return 0;
            }
            case SYM_YINYANG: {
                float r2 = u * u + v * v;
                if (r2 > 1.0f) return 0;
                if (r2 > 0.78f) return 1;                         // outline ring
                float dT = u * u + (v + 0.45f) * (v + 0.45f);
                float dB = u * u + (v - 0.45f) * (v - 0.45f);
                if (dT < 0.02f) return 3;
                if (dB < 0.02f) return 1;
                if (dT < 0.20f) return 1;
                if (dB < 0.20f) return 3;
                return (u < 0.0f) ? 1 : 3;
            }
            case SYM_RING: {
                if (fabsf(u) + fabsf(v + 0.55f) < 0.34f) return 5; // diamond
                float d2 = u * u + (v - 0.25f) * (v - 0.25f);
                return (d2 < 0.49f && d2 > 0.22f) ? 1 : 0;
            }
            case SYM_CAKE: {
                float fx = u / 0.16f, fy = (v + 0.68f) / 0.26f;
                if (fx * fx + fy * fy < 1.0f) return 4;            // flame
                if (fabsf(u) < 0.09f && v > -0.45f && v < 0.0f) return 1; // candle
                if (fabsf(u) < 0.88f && v >= 0.0f && v < 0.25f) return 2; // frosting
                if (fabsf(u) < 0.88f && v >= 0.25f && v < 0.92f) return 1; // body
                return 0;
            }
            case SYM_CANDLE: {
                float fx = u / 0.2f, fy = (v + 0.5f) / 0.36f;
                if (fx * fx + fy * fy < 1.0f) return 4;
                if (fabsf(u) < 0.04f && v > -0.2f && v < -0.05f) return 3; // wick
                if (fabsf(u) < 0.3f && v >= -0.05f && v < 0.95f) return 1;
                return 0;
            }
            default: return 0;
        }
    }

    void drawSymbol(uint8_t type, int32_t x, int32_t top, int16_t sz) {
        uint16_t fg = textcolor;
        uint16_t pal[6];
        pal[0] = 0;
        pal[1] = fg;
        pal[2] = color565(255, 80, 120);
        pal[3] = alphaBlend(90, fg, color565(12, 18, 30));
        pal[4] = color565(255, 165, 40);
        pal[5] = color565(205, 240, 255);
        float inv = 2.0f / (float)sz;
        for (int16_t ry = 0; ry < sz; ry++) {
            int32_t py = top + ry;
            if (py < 0 || py >= height()) continue;
            if (_hasVnClip && (py < _vnClipY0 || py >= _vnClipY1)) continue;
            for (int16_t rx = 0; rx < sz; rx++) {
                int32_t px = x + rx;
                if (px < 0 || px >= width()) continue;
                if (_hasVnClip && (px < _vnClipX0 || px >= _vnClipX1)) continue;
                // 2x2 supersampling for anti-aliased edges
                uint8_t cnt[6] = {0, 0, 0, 0, 0, 0};
                for (int sy = 0; sy < 2; sy++) {
                    for (int sx = 0; sx < 2; sx++) {
                        float u = (rx + 0.25f + 0.5f * sx) * inv - 1.0f;
                        float v = (ry + 0.25f + 0.5f * sy) * inv - 1.0f;
                        cnt[shapeColor(type, u, v)]++;
                    }
                }
                uint8_t best = 0, hits = 0;
                for (uint8_t k = 1; k < 6; k++) {
                    hits += cnt[k];
                    if (cnt[k] > cnt[best] || best == 0) { if (cnt[k]) best = k; }
                }
                if (hits == 0 || best == 0) continue;
                if (hits == 4) {
                    drawPixel(px, py, pal[best]);
                } else {
                    uint16_t bg = readPixel(px, py);
                    drawPixel(px, py, alphaBlend(hits * 64 - 1, pal[best], bg));
                }
            }
        }
    }

    static uint32_t decodeNextUTF8(const char*& p) {
        uint8_t c0 = (uint8_t)(*p++);
        if (c0 < 0x80) return c0;
        if ((c0 & 0xF8) == 0xF0) {
            uint8_t c1 = (uint8_t)p[0];
            if ((c1 & 0xC0) != 0x80) return c0;
            uint8_t c2 = (uint8_t)p[1];
            if ((c2 & 0xC0) != 0x80) return c0;
            uint8_t c3 = (uint8_t)p[2];
            if ((c3 & 0xC0) != 0x80) return c0;
            p += 3;
            return ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) |
                   ((uint32_t)(c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        if ((c0 & 0xE0) == 0xC0) {
            uint8_t c1 = (uint8_t)(*p);
            if ((c1 & 0xC0) == 0x80) {
                p++;
                return ((uint16_t)(c0 & 0x1F) << 6) | (c1 & 0x3F);
            }
            return c0;
        }
        if ((c0 & 0xF0) == 0xE0) {
            uint8_t c1 = (uint8_t)p[0];
            uint8_t c2 = (uint8_t)p[1];
            if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                p += 2;
                return ((uint16_t)(c0 & 0x0F) << 12) | ((uint16_t)(c1 & 0x3F) << 6) | (c2 & 0x3F);
            }
            return c0;
        }
        return c0;
    }

    static bool findGlyph(const VnFontFace* face, uint16_t code, VnGlyph& out) {
        int32_t low = 0;
        int32_t high = (int32_t)face->glyphCount - 1;
        while (low <= high) {
            int32_t mid = (low + high) >> 1;
            uint16_t u = pgm_read_word(&face->glyphs[mid].unicode);
            if (u == code) {
                memcpy_P(&out, &face->glyphs[mid], sizeof(VnGlyph));
                return true;
            } else if (u < code) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return false;
    }

    static int16_t measureWidth(const VnFontFace* face, const char* s) {
        int16_t w = 0;
        const char* p = s;
        VnGlyph g;
        while (*p) {
            uint32_t code = decodeNextUTF8(p);
            uint8_t sym = (code > 0xFFFF || code >= 0x2600 || code == 0x200D) ? symbolType(code) : SYM_NONE;
            if (sym != SYM_NONE) {
                w += symbolAdvance(face, sym);
            } else if (code <= 0xFFFF && findGlyph(face, (uint16_t)code, g)) {
                w += g.xAdvance;
            } else {
                w += face->spaceWidth;
            }
        }
        return w;
    }

    int16_t renderVnString(const VnFontFace* face, const char* string, int32_t poX, int32_t poY) {
        int16_t cwidth = measureWidth(face, string);
        int16_t cheight = face->lineHeight;
        int16_t baseline = face->baseline;

        switch (textdatum) {
            case TC_DATUM:   poX -= cwidth / 2; break;
            case TR_DATUM:   poX -= cwidth; break;
            case ML_DATUM:   poY -= cheight / 2; break;
            case MC_DATUM:   poX -= cwidth / 2; poY -= cheight / 2; break;
            case MR_DATUM:   poX -= cwidth; poY -= cheight / 2; break;
            case BL_DATUM:   poY -= cheight; break;
            case BC_DATUM:   poX -= cwidth / 2; poY -= cheight; break;
            case BR_DATUM:   poX -= cwidth; poY -= cheight; break;
            case L_BASELINE: poY -= baseline; break;
            case C_BASELINE: poX -= cwidth / 2; poY -= baseline; break;
            case R_BASELINE: poX -= cwidth; poY -= baseline; break;
            default: break;
        }

        int32_t curX = poX;
        const char* p = string;
        VnGlyph g;
        uint16_t fg = textcolor;

        while (*p) {
            uint32_t code = decodeNextUTF8(p);
            uint8_t sym = (code > 0xFFFF || code >= 0x2600 || code == 0x200D) ? symbolType(code) : SYM_NONE;
            if (sym != SYM_NONE) {
                if (sym != SYM_ZERO) {
                    int16_t sz = symbolSize(face);
                    int32_t top = poY + baseline - sz;
                    drawSymbol(sym, curX + 1, top, sz);
                }
                curX += symbolAdvance(face, sym);
                continue;
            }
            if (code > 0xFFFF || !findGlyph(face, (uint16_t)code, g)) {
                curX += face->spaceWidth;
                continue;
            }
            if (g.width > 0 && g.height > 0) {
                int32_t gx0 = curX + g.xOffset;
                int32_t gy0 = poY + g.yOffset;
                uint32_t bitIdx = 0;
                const uint8_t* bmpBase = face->bitmaps + g.bitmapOffset;

                for (uint8_t ry = 0; ry < g.height; ry++) {
                    int32_t py = gy0 + ry;
                    for (uint8_t rx = 0; rx < g.width; rx++) {
                        uint8_t byteVal = pgm_read_byte(bmpBase + (bitIdx >> 1));
                        uint8_t nibble = (bitIdx & 1) ? (byteVal & 0x0F) : (byteVal >> 4);
                        bitIdx++;

                        if (nibble == 0) continue;
                        int32_t px = gx0 + rx;
                        if (px < 0 || px >= width() || py < 0 || py >= height()) continue;
                        if (_hasVnClip && (px < _vnClipX0 || px >= _vnClipX1 || py < _vnClipY0 || py >= _vnClipY1)) continue;

                        if (nibble == 15) {
                            drawPixel(px, py, fg);
                        } else {
                            uint8_t alpha = (nibble << 4) | nibble;
                            uint16_t bg = readPixel(px, py);
                            drawPixel(px, py, alphaBlend(alpha, fg, bg));
                        }
                    }
                }
            }
            curX += g.xAdvance;
        }

        return cwidth;
    }
};

#endif // VN_FONT_H

