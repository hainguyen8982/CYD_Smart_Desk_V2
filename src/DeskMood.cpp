// ═══════════════════════════════════════════════════════════════════════
//  DeskMood.cpp — Office Status & Mood Signboard Engine
//  CYD SmartDesk V2 — Social Vibe Billboard for Modern Desks
// ═══════════════════════════════════════════════════════════════════════
#include "DeskMood.h"
#include "Colors.h"
#include "HardwareManager.h"
#include "MemeBitmaps.h"
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

void DeskMood::setMood(MoodType m) {
    if (m >= MOOD_COUNT) m = MOOD_HAPPY;
    _mood = m;
    _saveSettings();
    _syncHardwareLed();
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
        case MOOD_HAPPY:       return "Vui ve";
        case MOOD_FOCUS:       return "Tap trung";
        case MOOD_BUG:         return "Dang buc minh";
        case MOOD_LOW_BATTERY: return "Sap nguon";
        case MOOD_CHILL:       return "Dang chan";
        case MOOD_TGIF:        return "Cuoi tuan / Quay";
        default:               return "Vui ve";
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
            case MOOD_HAPPY:       return "VUI VE";
            case MOOD_FOCUS:       return "TAP TRUNG CAO DO";
            case MOOD_BUG:         return "DANG CAY CU!";
            case MOOD_LOW_BATTERY: return "SAP NGUON 0%";
            case MOOD_CHILL:       return "DANG CHAN / BO PHO";
            case MOOD_TGIF:        return "QUAY THOI - CUOI TUAN!";
            default:               return "VUI VE";
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
            case MOOD_HAPPY:       return "Hom nay vui ve * Ca phe khong?";
            case MOOD_FOCUS:       return "Dang chay deadline * Xin dung lam phien!";
            case MOOD_BUG:         return "Dang cay cu * Tiep can can than keo quau!";
            case MOOD_LOW_BATTERY: return "Nao sap sap nguon * Can nap tra sua gap!";
            case MOOD_CHILL:       return "Qua la chan chuong * Chung nao moi tan ca?";
            case MOOD_TGIF:        return "Sap tan ca roi * Toi nay xoa het deadline!";
            default:               return "Chuc mot ngay lam viec tran day nang luong!";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "Good vibes only * Always open for coffee!";
            case MOOD_FOCUS:       return "In deep focus * Please do not interrupt!";
            case MOOD_BUG:         return "Dangerously angry * Approach with caution!";
            case MOOD_LOW_BATTERY: return "Brain at 0% * Needs milk tea recharge!";
            case MOOD_CHILL:       return "Completely bored * When is shift ending?";
            case MOOD_TGIF:        return "Weekend is here * Time to celebrate!";
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
        String cleanTag = sanitizeAscii(String(tag));
        cleanTag.toUpperCase();
        cleanTag.trim();
        strncpy(_customTags[m], cleanTag.c_str(), sizeof(_customTags[m]) - 1);
        _customTags[m][sizeof(_customTags[m]) - 1] = '\0';
    }
    if (msg != nullptr) {
        String cleanMsg = sanitizeAscii(String(msg));
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

static void drawMemeBitmap160(TFT_eSprite& spr, int16_t startX, int16_t startY, const uint16_t* bmp) {
    if (!bmp) return;
    for (int r = 0; r < MEME_BITMAP_H; r++) {
        int16_t py = startY + r;
        if (py < 0 || py >= 240) continue;
        int rowIdx = r * MEME_BITMAP_W;
        for (int c = 0; c < MEME_BITMAP_W; c++) {
            int16_t px = startX + c;
            if (px < 0 || px >= 320) continue;
            uint16_t color = pgm_read_word(&bmp[rowIdx + c]);
            if (color != MEME_TRANSPARENT_COLOR) {
                spr.drawPixel(px, py, color);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  RENDER SIGNBOARD (MEME KHỔNG LỒ 160x160 > 2/3 MÀN HÌNH - KHÔNG VÒNG TRÒN)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg) {
    // 1. Phông nền toàn màn hình 320x240 sạch sẽ, phẳng hoàn toàn theo Theme
    spr.fillSprite(themeBg);

    int16_t cx = 160;
    int16_t cy = 82; // Tâm ảnh: Meme chiếm từ Y=2 đến Y=162 (160px > 2/3 màn hình)

    // 2. IN TRỰC TIẾP HÌNH MEME GỐC KHỔNG LỒ (160x160) VỚI NỀN TRONG SUỐT
    const uint16_t* memeBmp = getMemeBitmap((uint8_t)_pack, (uint8_t)_mood);
    drawMemeBitmap160(spr, cx - MEME_BITMAP_W / 2, cy - MEME_BITMAP_H / 2, memeBmp);

    // 2b. NÚT ĐỔI BỘ MEME GÓC TRÊN BÊN PHẢI (ICON 2 MŨI TÊN XOAY TRÒN, 4 GÓC BO TRÒN R=8, KHÔNG VÁT CẠNH)
    int16_t pBtnW = 28;
    int16_t pBtnH = 28;
    int16_t pBtnX = 320 - pBtnW - 8; // X = 284..312 (Cách xa meme > 44px, 100% không đè lên tranh)
    int16_t pBtnY = 8;
    int16_t pBtnR = 8; // Bo tròn 4 góc r=8 mềm mại, chuẩn Rounded Container, tuyệt đối không vát cạnh

    // Đổ bóng & Nền kính đen mờ (Obsidian Frosted Glass)
    spr.fillSmoothRoundRect(pBtnX, pBtnY + 2, pBtnW, pBtnH, pBtnR, RGB565(2, 4, 8), themeBg);
    spr.fillSmoothRoundRect(pBtnX, pBtnY, pBtnW, pBtnH, pBtnR, RGB565(18, 24, 36), themeBg);
    spr.drawRoundRect(pBtnX, pBtnY, pBtnW, pBtnH, pBtnR, RGB565(55, 70, 95));

    // Vẽ icon 2 mũi tên xoay tròn (Cycle / Swap) màu accent neon
    uint16_t accent = getAccentColor(_mood);
    int16_t icx = pBtnX + 14;
    int16_t icy = pBtnY + 14;

    // Mũi tên trên (quay sang phải rồi chúc xuống):
    spr.drawFastHLine(icx - 5, icy - 5, 8, accent);
    spr.drawFastVLine(icx - 6, icy - 4, 4, accent);
    spr.drawLine(icx, icy - 8, icx + 3, icy - 5, accent);
    spr.drawLine(icx, icy - 2, icx + 3, icy - 5, accent);

    // Mũi tên dưới (quay sang trái rồi hất lên):
    spr.drawFastHLine(icx - 2, icy + 5, 8, accent);
    spr.drawFastVLine(icx + 6, icy + 1, 4, accent);
    spr.drawLine(icx, icy + 8, icx - 3, icy + 5, accent);
    spr.drawLine(icx, icy + 2, icx - 3, icy + 5, accent);

    // 3. BIỂN BÁO NEON TỰ ĐỘNG SCALE BÓNG CHUẨN XÁC THEO ĐỘ DÀI TEXT (Y = 166..194)
    // Nếu mood hiện tại được custom -> Hiển thị text custom riêng của mood đó!
    // Nếu không -> Hiển thị text mặc định song ngữ (VI/EN) chuẩn của mood đó!
    const char* tagStr = (_customEnabled[_mood] && _customTags[_mood][0] != '\0')
                         ? _customTags[_mood]
                         : getTag(_mood, lang);

    uint8_t fontToUse = 2; // Font 2 đậm, gọn gàng và đẹp mắt dưới meme khổng lồ
    int16_t textW = spr.textWidth(tagStr, fontToUse);

    // Bóng tự co giãn vừa vặn (padding 18px mỗi bên, bo tròn viên thuốc r=14)
    int16_t tagW = textW + 36;
    if (tagW < 140) tagW = 140;
    if (tagW > 308) tagW = 308;
    int16_t tagH = 26;
    int16_t tagY = 166;
    int16_t tagR = tagH / 2;

    // Đổ bóng & Nền viên thuốc Neon
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY + 2, tagW, tagH, tagR, RGB565(4, 6, 10), themeBg);
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, accent, themeBg);
    spr.drawRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, C_WHITE);

    // Chữ căn giữa chuẩn xác, không bao giờ bị tràn
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_BLACK);
    spr.drawString(tagStr, cx, tagY + tagH / 2, fontToUse);

    // 4. CÂU QUOTE VĂN PHÒNG HÀI HƯỚC HOẶC CUSTOM RIÊNG CỦA MOOD (Y = 199)
    const char* msgStr = (_customEnabled[_mood] && _customMsgs[_mood][0] != '\0')
                         ? _customMsgs[_mood]
                         : getMessage(_mood, lang);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_WHITE);
    spr.drawString(msgStr, cx, 200, 2);

    // 5. CHẤM CHỈ THỊ 6 TRẠNG THÁI (Y = 222)
    int16_t dotStartX = cx - (MOOD_COUNT * 14) / 2 + 7;
    for (int i = 0; i < MOOD_COUNT; i++) {
        int16_t dx = dotStartX + i * 14;
        if (i == (int)_mood) {
            spr.fillSmoothRoundRect(dx - 6, 220, 13, 5, 2, accent, themeBg);
        } else {
            spr.fillSmoothCircle(dx, 222, 2, RGB565(60, 75, 100), themeBg);
        }
    }
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

    // 2. Chạm vào bất kỳ vị trí nào trên meme (trên vùng Dock) để chuyển tâm trạng kế tiếp
    if (y < 196) {
        nextMood(); // Xoay vòng tròn 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 0
        hardware.playClick();
        return true;
    }
    return false;
}
