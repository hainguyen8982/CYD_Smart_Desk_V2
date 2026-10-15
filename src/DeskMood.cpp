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

DeskMood::DeskMood() : _mood(MOOD_HAPPY), _pack(PACK_MOCHI) {}

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
        case MOOD_LOW_BATTERY: return "Can nang luong";
        case MOOD_CHILL:       return "Dang chan / Chill";
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
            case MOOD_BUG:         return "DANG BUC MINH!";
            case MOOD_LOW_BATTERY: return "CAN NANG LUONG 0%";
            case MOOD_CHILL:       return "DANG CHAN / CHILL";
            case MOOD_TGIF:        return "QUAY THOI - CUOI TUAN!";
            default:               return "VUI VE";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "HAPPY & CHILL";
            case MOOD_FOCUS:       return "DEEP WORK - FOCUS";
            case MOOD_BUG:         return "VERY ANGRY / DND!";
            case MOOD_LOW_BATTERY: return "LOW BATTERY 0%";
            case MOOD_CHILL:       return "BORED / SUPER CHILL";
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
            case MOOD_CHILL:       return "Cu binh tinh ma lam * Van su tuy duyen";
            case MOOD_TGIF:        return "Sap tan ca roi * Toi nay xoa het deadline!";
            default:               return "Chuc mot ngay lam viec tran day nang luong!";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "Good vibes only * Always open for coffee!";
            case MOOD_FOCUS:       return "In deep focus * Please do not interrupt!";
            case MOOD_BUG:         return "Dangerously angry * Approach with caution!";
            case MOOD_LOW_BATTERY: return "Brain at 0% * Needs milk tea recharge!";
            case MOOD_CHILL:       return "Keep calm & let it flow * No rush today";
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

void DeskMood::_loadSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_mood", true)) {
        uint8_t m = prefs.getUChar("mood", (uint8_t)MOOD_HAPPY);
        _mood = (m < MOOD_COUNT) ? (MoodType)m : MOOD_HAPPY;
        uint8_t p = prefs.getUChar("pack", (uint8_t)PACK_MOCHI);
        _pack = (p < PACK_COUNT) ? (MoodPack)p : PACK_MOCHI;
        prefs.end();
    }
}

void DeskMood::_saveSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_mood", false)) {
        prefs.putUChar("mood", (uint8_t)_mood);
        prefs.putUChar("pack", (uint8_t)_pack);
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

    // 2b. NÚT ĐỔI BỘ MEME GÓC TRÊN BÊN PHẢI (4 GÓC BO TRÒN CAPSULE, KHÔNG VÁT CẠNH)
    int16_t pBtnW = 98;
    int16_t pBtnH = 22;
    int16_t pBtnX = 320 - pBtnW - 8; // X = 214..312
    int16_t pBtnY = 8;
    int16_t pBtnR = pBtnH / 2; // Bo tròn 4 góc r=11 mềm mại, tuyệt đối không vát cạnh

    // Đổ bóng & Nền kính đen mờ (Obsidian Frosted Glass)
    spr.fillSmoothRoundRect(pBtnX, pBtnY + 2, pBtnW, pBtnH, pBtnR, RGB565(2, 4, 8), themeBg);
    spr.fillSmoothRoundRect(pBtnX, pBtnY, pBtnW, pBtnH, pBtnR, RGB565(18, 24, 36), themeBg);
    spr.drawRoundRect(pBtnX, pBtnY, pBtnW, pBtnH, pBtnR, RGB565(55, 70, 95));

    // Chấm chỉ thị neon theo mood
    uint16_t accent = getAccentColor(_mood);
    spr.fillSmoothCircle(pBtnX + 10, pBtnY + pBtnH / 2, 3, accent, RGB565(18, 24, 36));

    // Tên gói meme đang hiển thị (Mochi Guy / Cute Star / Cute Bunny)
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(RGB565(220, 230, 245));
    spr.drawString(getPackName(_pack), pBtnX + 18, pBtnY + pBtnH / 2, 2);

    // Mũi tên chuyển đổi '>' neon
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(accent);
    spr.drawString(">", pBtnX + pBtnW - 6, pBtnY + pBtnH / 2, 2);

    // 3. BIỂN BÁO NEON TỰ ĐỘNG SCALE BÓNG CHUẨN XÁC THEO ĐỘ DÀI TEXT (Y = 166..194)
    const char* tagStr = getTag(_mood, lang);

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

    // 4. CÂU QUOTE VĂN PHÒNG HÀI HƯỚC (Y = 199)
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_WHITE);
    spr.drawString(getMessage(_mood, lang), cx, 200, 2);

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
    // 1. Chạm vào nút Đổi Bộ Meme (Góc trên bên phải: X >= 200 && Y <= 40)
    if (x >= 200 && y <= 40) {
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
