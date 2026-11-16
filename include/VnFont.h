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

    static uint16_t decodeNextUTF8(const char*& p) {
        uint8_t c0 = (uint8_t)(*p++);
        if (c0 < 0x80) return c0;
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
            uint16_t code = decodeNextUTF8(p);
            if (findGlyph(face, code, g)) {
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
            uint16_t code = decodeNextUTF8(p);
            if (!findGlyph(face, code, g)) {
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

