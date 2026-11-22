// ═══════════════════════════════════════════════════════════════════════
//  DeskMood.cpp — Office Status & Mood Signboard Engine
//  CYD SmartDesk V2 — Social Vibe Billboard for Modern Desks
// ═══════════════════════════════════════════════════════════════════════
#include "DeskMood.h"
#include "Colors.h"
#include "Theme.h"
#include "HardwareManager.h"
#include "MemeBitmaps.h"
#include "EcosystemService.h"
#include <Preferences.h>

DeskMood deskMood;

DeskMood::DeskMood() : _mood(MOOD_HAPPY), _pack(PACK_MOCHI) {
    for (int i = 0; i < MOOD_COUNT; i++) {
        _customEnabled[i] = false;
        _customTags[i][0] = '\0';
        _customMsgs[i][0] = '\0';
    }
}

void DeskMood::begin() {
    _loadSettings();
    _syncHardwareLed();
}

void DeskMood::setMood(MoodType m, bool broadcastEco) {
    if (m >= MOOD_COUNT) m = MOOD_HAPPY;
    _mood = m;
    _saveSettings();
    _syncHardwareLed();
    if (broadcastEco) {
        ecosystem.sendEvent(EVT_MOOD_SYNC, (uint16_t)_mood);
    }
}

void DeskMood::nextMood() {
    uint8_t next = ((uint8_t)_mood + 1) % MOOD_COUNT;
    setMood((MoodType)next);
}

void DeskMood::prevMood() {
    uint8_t prev = ((uint8_t)_mood + MOOD_COUNT - 1) % MOOD_COUNT;
    setMood((MoodType)prev);
}

void DeskMood::setPack(MoodPack p) {
    if (p >= PACK_COUNT) p = PACK_MOCHI;
    _pack = p;
    _saveSettings();
}

void DeskMood::nextPack() {
    uint8_t next = ((uint8_t)_pack + 1) % PACK_COUNT;
    setPack((MoodPack)next);
}

const char* DeskMood::getPackName(MoodPack p) const {
    switch (p) {
        case PACK_MOCHI: return "Mochi Guy";
        case PACK_STAR:  return "Cute Star";
        case PACK_BUNNY: return "Cute Bunny";
        default:         return "Mochi Guy";
    }
}

const char* DeskMood::getName(MoodType m) const {
    switch (m) {
        case MOOD_HAPPY:       return "Vui vẻ";
        case MOOD_FOCUS:       return "Tập trung";
        case MOOD_BUG:         return "Đang bực mình";
        case MOOD_LOW_BATTERY: return "Sập nguồn";
        case MOOD_CHILL:       return "Đang chán";
        case MOOD_TGIF:        return "Cuối tuần / Quẩy";
        default:               return "Vui vẻ";
    }
}

const char* DeskMood::getIcon(MoodType m) const {
    switch (m) {
        case MOOD_HAPPY:       return "🟢";
        case MOOD_FOCUS:       return "🔴";
        case MOOD_BUG:         return "⚡";
        case MOOD_LOW_BATTERY: return "🟡";
        case MOOD_CHILL:       return "🟣";
        case MOOD_TGIF:        return "🎉";
        default:               return "🟢";
    }
}

const char* DeskMood::getTag(MoodType m, uint8_t lang) const {
    if (lang == 0) { // VI
        switch (m) {
            case MOOD_HAPPY:       return "VUI VẺ";
            case MOOD_FOCUS:       return "TẬP TRUNG CAO ĐỘ";
            case MOOD_BUG:         return "ĐANG CAY CÚ!";
            case MOOD_LOW_BATTERY: return "SẬP NGUỒN 0%";
            case MOOD_CHILL:       return "ĐANG CHÁN / BƠ PHỜ";
            case MOOD_TGIF:        return "QUẨY THÔI - CUỐI TUẦN!";
            default:               return "VUI VẺ";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "HAPPY & CHILL";
            case MOOD_FOCUS:       return "DEEP WORK - FOCUS";
            case MOOD_BUG:         return "VERY ANGRY / DND!";
            case MOOD_LOW_BATTERY: return "SHUTDOWN 0%";
            case MOOD_CHILL:       return "BORED & EXHAUSTED";
            case MOOD_TGIF:        return "PARTY TIME - TGIF!";
            default:               return "HAPPY & CHILL";
        }
    }
}

const char* DeskMood::getMessage(MoodType m, uint8_t lang) const {
    if (lang == 0) { // VI
        switch (m) {
            case MOOD_HAPPY:       return "Hôm nay vui vẻ • Cà phê không?";
            case MOOD_FOCUS:       return "Đang chạy deadline • Xin đừng làm phiền!";
            case MOOD_BUG:         return "Đang cay cú • Tiếp cận cẩn thận kẻo quạu!";
            case MOOD_LOW_BATTERY: return "Não sắp sập nguồn • Cần nạp trà sữa gấp!";
            case MOOD_CHILL:       return "Quá là chán chường • Chừng nào mới tan ca?";
            case MOOD_TGIF:        return "Sắp tan ca rồi • Tối nay xóa hết deadline!";
            default:               return "Chúc một ngày làm việc tràn đầy năng lượng!";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "Good vibes only • Always open for coffee!";
            case MOOD_FOCUS:       return "In deep focus • Please do not interrupt!";
            case MOOD_BUG:         return "Dangerously angry • Approach with caution!";
            case MOOD_LOW_BATTERY: return "Brain at 0% • Needs milk tea recharge!";
            case MOOD_CHILL:       return "Completely bored • When is shift ending?";
            case MOOD_TGIF:        return "Weekend is here • Time to celebrate!";
            default:               return "Have a wonderful productive day!";
        }
    }
}

uint16_t DeskMood::getAccentColor(MoodType m) const {
    switch (m) {
        case MOOD_HAPPY:       return RGB565(35, 230, 140);  // Neon Emerald
        case MOOD_FOCUS:       return RGB565(255, 45, 65);   // Cyber Crimson
        case MOOD_BUG:         return RGB565(255, 80, 20);   // Angry Flame Orange
        case MOOD_LOW_BATTERY: return RGB565(250, 195, 25);  // Warning Amber Yellow
        case MOOD_CHILL:       return RGB565(170, 130, 255); // Pastel Lavender
        case MOOD_TGIF:        return RGB565(255, 60, 175);  // Party Pink
        default:               return RGB565(35, 230, 140);
    }
}

void DeskMood::_syncHardwareLed() {
    switch (_mood) {
        case MOOD_HAPPY:       hardware.setRGB(COLOR_GREEN);   break;
        case MOOD_FOCUS:       hardware.setRGB(COLOR_RED);     break;
        case MOOD_BUG:         hardware.setRGB(COLOR_YELLOW);  break;
        case MOOD_LOW_BATTERY: hardware.setRGB(COLOR_MAGENTA); break;
        case MOOD_CHILL:       hardware.setRGB(COLOR_BLUE);    break;
        case MOOD_TGIF:        hardware.setRGB(COLOR_CYAN);    break;
        default:               hardware.setRGB(COLOR_GREEN);   break;
    }
}

static String sanitizeAscii(const String& input) {
    String out = "";
    out.reserve(input.length());
    size_t i = 0;
    while (i < input.length()) {
        uint8_t c = (uint8_t)input[i];
        if (c < 0x80) {
            out += (char)c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.length()) {
            uint16_t u = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
            if (u == 0x0111 || u == 0x0110) out += (u == 0x0110 ? 'D' : 'd'); // đ / Đ
            else if (u >= 0xC0 && u <= 0xC5) out += 'A';
            else if (u >= 0xE0 && u <= 0xE5) out += 'a';
            else if (u >= 0xC8 && u <= 0xCB) out += 'E';
            else if (u >= 0xE8 && u <= 0xEB) out += 'e';
            else if (u >= 0xCC && u <= 0xCF) out += 'I';
            else if (u >= 0xEC && u <= 0xEF) out += 'i';
            else if (u >= 0xD2 && u <= 0xD6) out += 'O';
            else if (u >= 0xF2 && u <= 0xF6) out += 'o';
            else if (u >= 0xD9 && u <= 0xDC) out += 'U';
            else if (u >= 0xF9 && u <= 0xFC) out += 'u';
            else if (u == 0xDD) out += 'Y';
            else if (u == 0xFD) out += 'y';
            else if (u == 0x0102 || u == 0x0103) out += (u == 0x0102 ? 'A' : 'a'); // Ă, ă
            else if (u == 0x01A0 || u == 0x01A1) out += (u == 0x01A0 ? 'O' : 'o'); // Ơ, ơ
            else if (u == 0x01AF || u == 0x01B0) out += (u == 0x01AF ? 'U' : 'u'); // Ư, ư
            else out += ' ';
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.length()) {
            uint16_t u = ((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F);
            if (u >= 0x1EA0 && u <= 0x1EB7) out += (u % 2 == 0 ? 'A' : 'a');
            else if (u >= 0x1EB8 && u <= 0x1EC7) out += (u % 2 == 0 ? 'E' : 'e');
            else if (u >= 0x1EC8 && u <= 0x1ECB) out += (u % 2 == 0 ? 'I' : 'i');
            else if (u >= 0x1ECC && u <= 0x1EE3) out += (u % 2 == 0 ? 'O' : 'o');
            else if (u >= 0x1EE4 && u <= 0x1EF1) out += (u % 2 == 0 ? 'U' : 'u');
            else if (u >= 0x1EF2 && u <= 0x1EF9) out += (u % 2 == 0 ? 'Y' : 'y');
            else out += ' ';
            i += 3;
        } else {
            i++;
        }
    }
    return out;
}

void DeskMood::setCustomForMood(MoodType m, bool enabled, const char* tag, const char* msg) {
    if (m >= MOOD_COUNT) return;
    _customEnabled[m] = enabled;
    if (tag != nullptr) {
        String cleanTag = String(tag);
        cleanTag.trim();
        strncpy(_customTags[m], cleanTag.c_str(), sizeof(_customTags[m]) - 1);
        _customTags[m][sizeof(_customTags[m]) - 1] = '\0';
    }
    if (msg != nullptr) {
        String cleanMsg = String(msg);
        cleanMsg.trim();
        strncpy(_customMsgs[m], cleanMsg.c_str(), sizeof(_customMsgs[m]) - 1);
        _customMsgs[m][sizeof(_customMsgs[m]) - 1] = '\0';
    }
    _saveSettings();
}

void DeskMood::resetMoodToDefault(MoodType m) {
    if (m >= MOOD_COUNT) return;
    _customEnabled[m] = false;
    _customTags[m][0] = '\0';
    _customMsgs[m][0] = '\0';
    _saveSettings();
}

void DeskMood::_loadSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_mood", true)) {
        uint8_t m = prefs.getUChar("mood", (uint8_t)MOOD_HAPPY);
        _mood = (m < MOOD_COUNT) ? (MoodType)m : MOOD_HAPPY;
        uint8_t p = prefs.getUChar("pack", (uint8_t)PACK_MOCHI);
        _pack = (p < PACK_COUNT) ? (MoodPack)p : PACK_MOCHI;
        uint8_t mask = prefs.getUChar("c_mask", 0);
        for (int i = 0; i < MOOD_COUNT; i++) {
            _customEnabled[i] = (mask & (1 << i)) != 0;
            char kTag[8], kMsg[8];
            snprintf(kTag, sizeof(kTag), "t_%d", i);
            snprintf(kMsg, sizeof(kMsg), "m_%d", i);
            String t = prefs.getString(kTag, "");
            String msg = prefs.getString(kMsg, "");
            strncpy(_customTags[i], t.c_str(), sizeof(_customTags[i]) - 1);
            _customTags[i][sizeof(_customTags[i]) - 1] = '\0';
            strncpy(_customMsgs[i], msg.c_str(), sizeof(_customMsgs[i]) - 1);
            _customMsgs[i][sizeof(_customMsgs[i]) - 1] = '\0';
        }
        prefs.end();
    }
}

void DeskMood::_saveSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_mood", false)) {
        prefs.putUChar("mood", (uint8_t)_mood);
        prefs.putUChar("pack", (uint8_t)_pack);
        uint8_t mask = 0;
        for (int i = 0; i < MOOD_COUNT; i++) {
            if (_customEnabled[i]) mask |= (1 << i);
            char kTag[8], kMsg[8];
            snprintf(kTag, sizeof(kTag), "t_%d", i);
            snprintf(kMsg, sizeof(kMsg), "m_%d", i);
            prefs.putString(kTag, _customTags[i]);
            prefs.putString(kMsg, _customMsgs[i]);
        }
        prefs.putUChar("c_mask", mask);
        prefs.end();
    }
}

static void drawMemeBitmap160_16bit(TFT_eSPI& tft, int16_t startX, int16_t startY, const MemeRleBitmap* bmp) {
    if (!bmp || !bmp->palette || !bmp->rle) return;
    // Stream Palette+RLE8 directly from PROGMEM to 16-bit ST7789/ILI9341 (0 bytes RAM!)
    tft.startWrite();
    tft.setAddrWindow(startX, startY, MEME_BITMAP_W, MEME_BITMAP_H);
    const uint16_t* pal = bmp->palette;
    const uint8_t*  rle = bmp->rle;
    uint32_t i = 0;
    const uint32_t len = bmp->rleSize;
    while (i < len) {
        uint8_t ctrl = pgm_read_byte(&rle[i++]);
        if (ctrl & 0x80) {
            uint8_t cnt = (ctrl & 0x7F) + 1;
            uint8_t idx = pgm_read_byte(&rle[i++]);
            uint16_t col = pgm_read_word(&pal[idx]);
            tft.pushColor(col, cnt);
        } else {
            uint8_t cnt = ctrl + 1;
            while (cnt--) {
                uint8_t idx = pgm_read_byte(&rle[i++]);
                tft.pushColor(pgm_read_word(&pal[idx]));
            }
        }
    }
    tft.endWrite();
}

void DeskMood::renderPageZen16(TFT_eSPI& tft, VnSprite& spr, const VnFontFace* lightFont, uint8_t lang) {
    const uint16_t zenBg = C_BLACK;
    const uint16_t zenBorder = RGB565(64, 70, 82);
    const uint16_t zenMuted = RGB565(200, 206, 216);
    const uint16_t zenDim = RGB565(96, 102, 114);
    const uint16_t zenTrack = RGB565(56, 62, 72);
    const uint16_t swapCol = RGB565(180, 188, 200);

    tft.fillScreen(zenBg);

    const int16_t cx = 160;
    const MemeRleBitmap* memeBmp = getMemeBitmap((uint8_t)_pack, (uint8_t)_mood);
    drawMemeBitmap160_16bit(tft, 80, 2, memeBmp);

    // Top-right Pack Swap button (X=284..312, Y=8..36 -> inside X>=265 && Y<=46)
    VnSprite::drawOutlineRoundRect16(tft, 284, 8, 28, 28, 8, zenBg, zenBorder, zenBg);
    int16_t icx = 298, icy = 22;
    tft.drawFastHLine(icx - 5, icy - 5, 8, swapCol);
    tft.drawFastVLine(icx - 6, icy - 4, 4, swapCol);
    tft.drawLine(icx, icy - 8, icx + 3, icy - 5, swapCol);
    tft.drawLine(icx, icy - 2, icx + 3, icy - 5, swapCol);
    tft.drawFastHLine(icx - 2, icy + 5, 8, swapCol);
    tft.drawFastVLine(icx + 6, icy + 1, 4, swapCol);
    tft.drawLine(icx, icy + 8, icx - 3, icy + 5, swapCol);
    tft.drawLine(icx, icy + 2, icx - 3, icy + 5, swapCol);

    // Outline Neon Mood Pill (Y=165..191)
    const char* tagStr = (_customEnabled[_mood] && _customTags[_mood][0] != '\0')
                         ? _customTags[_mood]
                         : getTag(_mood, lang);
    uint16_t accent = getAccentColor(_mood);
    int16_t textW = spr.measureSmoothFace(lightFont, tagStr);
    int16_t tagW = textW + 34;
    if (tagW < 128) tagW = 128;
    if (tagW > 300) tagW = 300;
    int16_t tagX = cx - tagW / 2;
    char tagFit[64];
    VnSprite::fitText(lightFont, tagStr, tagW - 30, tagFit, sizeof(tagFit));
    VnSprite::drawSmoothFace16(tft, lightFont, tagFit, cx, 178, MC_DATUM,
                               accent, zenBg, tagX + 14, 168, tagW - 28, 20);
    VnSprite::drawOutlineRing16(tft, tagX, 165, tagW, 26, 13, zenBg, accent, zenBg);

    // Quote / Message line (Y=205)
    const char* msgStr = (_customEnabled[_mood] && _customMsgs[_mood][0] != '\0')
                         ? _customMsgs[_mood]
                         : getMessage(_mood, lang);
    char msgFit[112];
    VnSprite::fitText(lightFont, msgStr, 296, msgFit, sizeof(msgFit));
    VnSprite::drawSmoothFace16(tft, lightFont, msgFit, cx, 205, MC_DATUM,
                               zenMuted, zenBg, 10, 194, 300, 22);

    // 6 Smooth Status Indicator Dots (Y=224)
    int16_t dotStartX = cx - (MOOD_COUNT * 14) / 2 + 7;
    for (int i = 0; i < MOOD_COUNT; i++) {
        int16_t dx = dotStartX + i * 14;
        if (i == (int)_mood) {
            VnSprite::fillSmoothRoundRect16(tft, dx - 6, 222, 13, 5, 2, accent, zenBg);
        } else {
            VnSprite::fillSmoothRoundRect16(tft, dx - 2, 222, 5, 5, 2, zenTrack, zenBg);
        }
    }

    // Minimal Dock indicator bar at bottom edge
    tft.fillRoundRect(134, 236, 52, 3, 1, zenDim);
}

// ═══════════════════════════════════════════════════════════════════════
//  TOUCH TRỰC TIẾP TRÊN MEME ĐỂ XOAY VÒNG CHUYỂN TRẠNG THÁI
// ═══════════════════════════════════════════════════════════════════════
bool DeskMood::handleTouch(int16_t x, int16_t y) {
    // 1. Chạm vào nút Icon Đổi Bộ Meme (Góc trên bên phải: X >= 265 && Y <= 46)
    if (x >= 265 && y <= 46) {
        nextPack(); // Đổi Mochi Guy -> Cute Star -> Cute Bunny -> Mochi Guy
        hardware.playClick();
        return true;
    }

    // 2. Chạm vào bất kỳ vị trí nào trên meme hoặc câu Quote (trừ vùng viên thuốc Dock ở đáy) để chuyển tâm trạng kế tiếp
    if (y < 208 || x < 105 || x > 215) {
        nextMood(); // Xoay vòng tròn 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 0
        hardware.playClick();
        return true;
    }
    return false;
}
