// ═══════════════════════════════════════════════════════════════════════
//  DeskMood.cpp — Office Status & Mood Signboard Engine
//  CYD SmartDesk V2 — Social Vibe Billboard for Modern Desks
// ═══════════════════════════════════════════════════════════════════════
#include "DeskMood.h"
#include "Colors.h"
#include "HardwareManager.h"
#include <Preferences.h>
#include <math.h>

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

static void drawSparkle(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t col) {
    spr.drawLine(x - 4, y, x + 4, y, col);
    spr.drawLine(x, y - 4, x, y + 4, col);
    spr.drawPixel(x, y, C_WHITE);
}

// ═══════════════════════════════════════════════════════════════════════
//  RENDER SIGNBOARD FULL-BLEED (TỰ ĐỘNG SCALE BÓNG THEO ĐỘ DÀI TEXT)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg) {
    // 1. Phông nền toàn màn hình 320x240 theo từng Mood
    uint16_t bgCol = themeBg;
    uint16_t auraCol = themeBg;
    uint16_t innerAura = themeBg;

    switch (_mood) {
        case MOOD_HAPPY:
            bgCol = RGB565(8, 22, 16);
            auraCol = RGB565(16, 48, 32);
            innerAura = RGB565(26, 75, 50);
            break;
        case MOOD_FOCUS:
            bgCol = RGB565(24, 8, 12);
            auraCol = RGB565(52, 14, 22);
            innerAura = RGB565(82, 22, 34);
            break;
        case MOOD_BUG:
            bgCol = RGB565(26, 12, 4);
            auraCol = RGB565(56, 26, 8);
            innerAura = RGB565(88, 40, 12);
            break;
        case MOOD_LOW_BATTERY:
            bgCol = RGB565(10, 14, 24);
            auraCol = RGB565(22, 32, 54);
            innerAura = RGB565(35, 52, 85);
            break;
        case MOOD_CHILL:
            bgCol = RGB565(18, 12, 28);
            auraCol = RGB565(38, 24, 58);
            innerAura = RGB565(60, 38, 92);
            break;
        case MOOD_TGIF:
            bgCol = RGB565(24, 8, 30);
            auraCol = RGB565(56, 16, 68);
            innerAura = RGB565(88, 26, 105);
            break;
    }

    spr.fillSprite(bgCol);

    int16_t cx = 160;
    int16_t cy = 72;

    // Vầng hào quang Ambient Glow tỏa sáng mềm mại sau lưng nhân vật
    spr.fillSmoothCircle(cx, cy, 76, auraCol, bgCol);
    spr.fillSmoothCircle(cx, cy, 58, innerAura, auraCol);

    // 2. VẼ MASCOT TÙY THEO BỘ PACK ĐANG CHỌN (Mochi / Star / Bunny)
    switch (_pack) {
        case PACK_MOCHI:
            _drawMochiAvatar(spr, cx, cy, _mood, bgCol);
            break;
        case PACK_STAR:
            _drawStarAvatar(spr, cx, cy, _mood, bgCol);
            break;
        case PACK_BUNNY:
            _drawBunnyAvatar(spr, cx, cy, _mood, bgCol);
            break;
    }

    // 3. BIỂN HIỆU NEON TỰ ĐỘNG SCALE BÓNG CHUẨN XÁC THEO ĐỘ DÀI TEXT (Y = 138..172)
    uint16_t accent = getAccentColor(_mood);
    const char* tagStr = getTag(_mood, lang);

    uint8_t fontToUse = 4;
    int16_t textW = spr.textWidth(tagStr, fontToUse);
    // Nếu chữ quá dài so với màn hình thì dùng font 2 để không bị tràn
    if (textW > 260) {
        fontToUse = 2;
        textW = spr.textWidth(tagStr, fontToUse);
    }

    // Bóng tự co giãn (padding 16px mỗi bên, bo tròn hoàn hảo)
    int16_t tagW = textW + 32;
    if (tagW < 130) tagW = 130;
    if (tagW > 312) tagW = 312;
    int16_t tagH = (fontToUse == 4) ? 34 : 26;
    int16_t tagY = (fontToUse == 4) ? 138 : 142;
    int16_t tagR = tagH / 2;

    // Đổ bóng & Nền viên thuốc Neon
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY + 2, tagW, tagH, tagR, RGB565(4, 6, 10), bgCol);
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, accent, bgCol);
    spr.drawRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, C_WHITE);

    // Chữ căn giữa chuẩn xác, không bao giờ bị tràn
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_BLACK);
    spr.drawString(tagStr, cx, tagY + tagH / 2, fontToUse);

    // 4. CÂU QUOTE VĂN PHÒNG HÀI HƯỚC (Y = 184)
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_WHITE);
    spr.drawString(getMessage(_mood, lang), cx, 184, 2);

    // 5. CHẤM CHỈ THỊ 6 TRẠNG THÁI (Y = 208)
    int16_t dotStartX = cx - (MOOD_COUNT * 16) / 2 + 8;
    for (int i = 0; i < MOOD_COUNT; i++) {
        int16_t dx = dotStartX + i * 16;
        if (i == (int)_mood) {
            spr.fillSmoothRoundRect(dx - 8, 206, 16, 6, 3, accent, bgCol);
        } else {
            spr.fillSmoothCircle(dx, 209, 3, RGB565(55, 68, 90), bgCol);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  BỘ 1: BÉ ĐẦU TRỌC TRẮNG (MOCHI GUY / QUBY)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::_drawMochiAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t bgCol) {
    uint16_t skin = C_WHITE;
    uint16_t lineCol = RGB565(30, 25, 25);
    uint16_t pink = RGB565(255, 130, 160);

    switch (m) {
        case MOOD_HAPPY: {
            // Đầu trọc nghiêng vui vẻ
            spr.fillSmoothCircle(cx - 2, cy - 8, 30, skin, bgCol);
            spr.drawCircle(cx - 2, cy - 8, 30, lineCol);

            // Thân tròn nhỏ đang nhảy múa
            spr.fillSmoothRoundRect(cx - 14, cy + 16, 26, 24, 10, skin, bgCol);
            spr.drawRoundRect(cx - 14, cy + 16, 26, 24, 10, lineCol);

            // Tay trái vẫy chào vui sướng giơ cao
            spr.drawLine(cx + 12, cy + 18, cx + 28, cy - 2, lineCol);
            spr.drawLine(cx + 11, cy + 19, cx + 27, cy - 1, lineCol);
            spr.fillSmoothCircle(cx + 28, cy - 3, 5, skin, bgCol);

            // Mắt cười híp tít (^ ^)
            spr.drawLine(cx - 16, cy - 10, cx - 10, cy - 14, lineCol);
            spr.drawLine(cx - 10, cy - 14, cx - 4, cy - 10, lineCol);
            spr.drawLine(cx + 2, cy - 10, cx + 8, cy - 14, lineCol);
            spr.drawLine(cx + 8, cy - 14, cx + 14, cy - 10, lineCol);

            // Má hồng to bự
            spr.fillSmoothCircle(cx - 18, cy - 4, 6, pink, skin);
            spr.fillSmoothCircle(cx + 18, cy - 4, 6, pink, skin);

            // Miệng cười há to khoe lưỡi
            spr.fillSmoothCircle(cx, cy + 2, 7, RGB565(210, 40, 75), skin);
            spr.fillRect(cx - 8, cy - 4, 16, 6, skin);
            break;
        }

        case MOOD_FOCUS: {
            // Đầu trọc cúi chăm chú
            spr.fillSmoothCircle(cx, cy - 10, 30, skin, bgCol);
            spr.drawCircle(cx, cy - 10, 30, lineCol);

            // Mặt bàn làm việc
            spr.fillRoundRect(cx - 45, cy + 24, 90, 8, 3, RGB565(160, 110, 65));

            // Quyển vở đang mở ra
            spr.fillRoundRect(cx - 25, cy + 14, 50, 16, 2, skin);
            spr.drawRoundRect(cx - 25, cy + 14, 50, 16, 2, lineCol);
            spr.drawFastVLine(cx, cy + 14, 16, lineCol); // gáy sách

            // Tay cầm bút cắm cúi viết ✍️
            spr.drawLine(cx - 18, cy + 18, cx - 8, cy + 8, lineCol);
            spr.drawLine(cx - 8, cy + 8, cx - 3, cy + 18, lineCol); // bút

            // Lông mày chau lại cực kỳ tập trung
            spr.drawLine(cx - 15, cy - 14, cx - 6, cy - 11, lineCol);
            spr.drawLine(cx + 6, cy - 11, cx + 15, cy - 14, lineCol);

            // Hai chấm mắt đen chăm chú
            spr.fillCircle(cx - 10, cy - 6, 3, lineCol);
            spr.fillCircle(cx + 10, cy - 6, 3, lineCol);

            // Miệng mím lại
            spr.drawFastHLine(cx - 4, cy + 2, 8, lineCol);

            // Tia tập trung lóe sáng quanh đầu
            spr.drawLine(cx - 28, cy - 28, cx - 36, cy - 36, C_WHITE);
            spr.drawLine(cx + 28, cy - 28, cx + 36, cy - 36, C_WHITE);
            break;
        }

        case MOOD_BUG: {
            // Đầu trọc tức giận
            spr.fillSmoothCircle(cx, cy - 8, 30, skin, bgCol);
            spr.drawCircle(cx, cy - 8, 30, lineCol);

            // Đám khói đen giận dữ bốc lên từ thái dương 💢
            spr.fillSmoothCircle(cx + 28, cy - 32, 8, RGB565(60, 50, 50), bgCol);
            spr.fillSmoothCircle(cx + 36, cy - 40, 6, RGB565(60, 50, 50), bgCol);
            spr.drawLine(cx + 22, cy - 24, cx + 26, cy - 28, RGB565(255, 45, 65));

            // Hai tay khoanh trước ngực hầm hố
            spr.fillSmoothRoundRect(cx - 18, cy + 16, 36, 16, 8, skin, bgCol);
            spr.drawRoundRect(cx - 18, cy + 16, 36, 16, 8, lineCol);
            spr.drawLine(cx - 10, cy + 24, cx + 10, cy + 24, lineCol);

            // Lông mày xếch ngược giận dữ (\   /)
            spr.drawLine(cx - 18, cy - 16, cx - 6, cy - 10, lineCol);
            spr.drawLine(cx - 18, cy - 17, cx - 6, cy - 11, lineCol);
            spr.drawLine(cx + 6, cy - 10, cx + 18, cy - 16, lineCol);
            spr.drawLine(cx + 6, cy - 11, cx + 18, cy - 17, lineCol);

            // Mắt giận dữ
            spr.fillCircle(cx - 10, cy - 6, 3, lineCol);
            spr.fillCircle(cx + 10, cy - 6, 3, lineCol);

            // Má hồng phồng lên vì giận
            spr.fillSmoothCircle(cx - 18, cy, 6, pink, skin);
            spr.fillSmoothCircle(cx + 18, cy, 6, pink, skin);

            // Miệng chữ ︿ mếu máo giận dỗi
            spr.drawLine(cx - 6, cy + 8, cx, cy + 4, lineCol);
            spr.drawLine(cx, cy + 4, cx + 6, cy + 8, lineCol);
            break;
        }

        case MOOD_LOW_BATTERY: {
            // Cục pin 0% to trên đầu nhấp nháy 🪫
            int16_t bx = cx - 24, by = cy - 54;
            spr.fillSmoothRoundRect(bx, by, 48, 18, 4, RGB565(30, 36, 48), bgCol);
            spr.drawRoundRect(bx, by, 48, 18, 4, RGB565(255, 60, 60));
            spr.fillRect(bx + 48, by + 4, 3, 10, RGB565(255, 60, 60)); // cực pin
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(255, 60, 60));
            spr.drawString("0%", bx + 24, by + 9, 2);

            // Đầu và người gục rũ xuống vì kiệt sức
            spr.fillSmoothCircle(cx - 6, cy - 4, 30, skin, bgCol);
            spr.drawCircle(cx - 6, cy - 4, 30, lineCol);

            // Thân gục xuống
            spr.fillSmoothRoundRect(cx - 4, cy + 18, 28, 22, 10, skin, bgCol);
            spr.drawRoundRect(cx - 4, cy + 18, 28, 22, 10, lineCol);

            // Mắt nhắm nghiền mệt mỏi (hai vạch ngang rũ xuống)
            spr.drawLine(cx - 20, cy - 6, cx - 10, cy - 4, lineCol);
            spr.drawLine(cx - 4, cy - 4, cx + 4, cy - 2, lineCol);

            // Giọt mồ hôi rơi bên má 💧
            spr.fillSmoothCircle(cx + 18, cy - 2, 4, RGB565(100, 200, 255), skin);

            // Miệng méo xệch mệt mỏi
            spr.drawLine(cx - 12, cy + 6, cx - 2, cy + 7, lineCol);
            break;
        }

        case MOOD_CHILL: {
            // Bàn làm việc
            spr.fillRoundRect(cx - 45, cy + 24, 90, 8, 3, RGB565(160, 110, 65));

            // Đầu tựa vào tay chống cằm
            spr.fillSmoothCircle(cx - 2, cy - 8, 30, skin, bgCol);
            spr.drawCircle(cx - 2, cy - 8, 30, lineCol);

            // Cánh tay chống cằm (khuỷu tay trên bàn, bàn tay đỡ má)
            spr.drawLine(cx - 28, cy + 24, cx - 18, cy + 10, lineCol);
            spr.drawLine(cx - 27, cy + 24, cx - 17, cy + 10, lineCol);
            spr.fillSmoothCircle(cx - 18, cy + 10, 5, skin, bgCol); // bàn tay đỡ cằm

            // Mắt đơ, buồn chán nửa nhắm nửa mở (- -)
            spr.drawLine(cx - 16, cy - 6, cx - 6, cy - 6, lineCol);
            spr.drawFastVLine(cx - 11, cy - 5, 3, lineCol); // con ngươi
            spr.drawLine(cx + 4, cy - 6, cx + 14, cy - 6, lineCol);
            spr.drawFastVLine(cx + 9, cy - 5, 3, lineCol);

            // Má hồng
            spr.fillSmoothCircle(cx - 16, cy + 2, 5, pink, skin);
            spr.fillSmoothCircle(cx + 16, cy + 2, 5, pink, skin);

            // Miệng một đường thẳng ngang đơ (Deadpan line)
            spr.drawFastHLine(cx - 6, cy + 6, 14, lineCol);
            break;
        }

        case MOOD_TGIF: {
            // Đầu trọc đeo kính râm nhảy múa quẩy nhiệt tình
            spr.fillSmoothCircle(cx, cy - 8, 30, skin, bgCol);
            spr.drawCircle(cx, cy - 8, 30, lineCol);

            // Đèn Disco Ball lấp lánh trên đỉnh đầu 🪩
            spr.fillSmoothCircle(cx, cy - 46, 10, RGB565(180, 210, 240), bgCol);
            spr.drawCircle(cx, cy - 46, 10, C_WHITE);
            spr.drawFastHLine(cx - 8, cy - 46, 16, C_WHITE);
            spr.drawFastVLine(cx, cy - 54, 8, RGB565(120, 130, 150)); // dây treo

            // KÍNH RÂM TRÒN ĐEN NGẦU 😎
            spr.fillSmoothCircle(cx - 12, cy - 8, 8, RGB565(15, 15, 20), skin);
            spr.fillSmoothCircle(cx + 12, cy - 8, 8, RGB565(15, 15, 20), skin);
            spr.drawFastHLine(cx - 4, cy - 8, 8, RGB565(15, 15, 20)); // gọng nối
            spr.drawPixel(cx - 14, cy - 10, C_WHITE); // ánh phản chiếu
            spr.drawPixel(cx + 10, cy - 10, C_WHITE);

            // Miệng há to hết cỡ quẩy tưng bừng
            spr.fillSmoothCircle(cx, cy + 8, 9, RGB565(210, 35, 75), skin);
            spr.fillRect(cx - 9, cy + 1, 18, 7, skin);
            spr.fillCircle(cx, cy + 11, 5, pink);

            // Hai tay và chân giơ lên nhảy múa 🕺
            spr.drawLine(cx - 12, cy + 16, cx - 26, cy + 2, lineCol);
            spr.drawLine(cx + 12, cy + 16, cx + 26, cy + 2, lineCol);

            // Tia sáng sân khấu và pháo hoa kim tuyến 🎊
            drawSparkle(spr, cx - 44, cy - 24, RGB565(0, 230, 255));
            drawSparkle(spr, cx + 44, cy - 24, RGB565(255, 60, 180));
            spr.fillRect(cx - 38, cy + 18, 4, 4, RGB565(255, 230, 20));
            spr.fillRect(cx + 38, cy + 18, 4, 4, RGB565(0, 230, 255));
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  BỘ 2: NGÔI SAO VÀNG DỄ THƯƠNG (CUTE STAR)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::_drawStarAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t bgCol) {
    uint16_t starYel = RGB565(255, 222, 65);
    uint16_t starLine = RGB565(75, 45, 15);
    uint16_t pink = RGB565(255, 125, 160);

    // Vẽ thân ngôi sao béo ú 5 cánh bo tròn
    auto drawStarBody = [&](uint16_t fillCol) {
        spr.fillSmoothCircle(cx, cy, 24, fillCol, bgCol);
        // Cánh trên
        spr.fillSmoothCircle(cx, cy - 30, 11, fillCol, bgCol);
        spr.fillTriangle(cx - 18, cy - 8, cx, cy - 30, cx + 18, cy - 8, fillCol);
        // Cánh phải trên
        spr.fillSmoothCircle(cx + 32, cy - 8, 11, fillCol, bgCol);
        spr.fillTriangle(cx + 12, cy - 20, cx + 32, cy - 8, cx + 10, cy + 14, fillCol);
        // Cánh phải dưới
        spr.fillSmoothCircle(cx + 20, cy + 24, 11, fillCol, bgCol);
        spr.fillTriangle(cx + 4, cy + 8, cx + 20, cy + 24, cx - 12, cy + 18, fillCol);
        // Cánh trái dưới
        spr.fillSmoothCircle(cx - 20, cy + 24, 11, fillCol, bgCol);
        spr.fillTriangle(cx - 4, cy + 8, cx - 20, cy + 24, cx + 12, cy + 18, fillCol);
        // Cánh trái trên
        spr.fillSmoothCircle(cx - 32, cy - 8, 11, fillCol, bgCol);
        spr.fillTriangle(cx - 12, cy - 20, cx - 32, cy - 8, cx - 10, cy + 14, fillCol);
    };

    switch (m) {
        case MOOD_HAPPY: {
            drawStarBody(starYel);

            // Mắt nháy tinh nghịch: Trái mở to long lanh, Phải nháy mắt ( ^ )
            spr.fillSmoothCircle(cx - 12, cy - 4, 5, starLine, starYel);
            spr.fillCircle(cx - 13, cy - 6, 2, C_WHITE);

            // Mắt phải nháy
            spr.drawLine(cx + 8, cy - 3, cx + 14, cy - 7, starLine);
            spr.drawLine(cx + 14, cy - 7, cx + 20, cy - 3, starLine);

            // Má hồng
            spr.fillSmoothCircle(cx - 18, cy + 3, 5, pink, starYel);
            spr.fillSmoothCircle(cx + 18, cy + 3, 5, pink, starYel);

            // Miệng cười mở to
            spr.fillSmoothCircle(cx, cy + 6, 6, RGB565(210, 35, 75), starYel);
            spr.fillRect(cx - 6, cy + 1, 12, 5, starYel);

            // Ngôi sao nhỏ bay xung quanh ✨
            drawSparkle(spr, cx - 44, cy - 24, RGB565(255, 235, 120));
            drawSparkle(spr, cx + 46, cy - 20, RGB565(255, 235, 120));
            break;
        }

        case MOOD_FOCUS: {
            drawStarBody(starYel);

            // Bàn & Quyển vở đang mở
            spr.fillRoundRect(cx - 36, cy + 20, 72, 6, 2, RGB565(160, 110, 65));
            spr.fillRoundRect(cx - 18, cy + 12, 36, 12, 2, C_WHITE);
            spr.drawFastVLine(cx, cy + 12, 12, starLine);

            // Tay cầm bút lông viết chữ
            spr.drawLine(cx + 8, cy + 14, cx + 16, cy + 2, starLine);

            // Lông mày nghiêm nghị, tập trung cao độ
            spr.drawLine(cx - 15, cy - 9, cx - 6, cy - 6, starLine);
            spr.drawLine(cx + 6, cy - 6, cx + 15, cy - 9, starLine);

            spr.fillCircle(cx - 10, cy - 2, 3, starLine);
            spr.fillCircle(cx + 10, cy - 2, 3, starLine);

            // Bong bóng suy nghĩ có hình bánh răng ⚙️
            spr.fillSmoothCircle(cx + 36, cy - 32, 10, RGB565(220, 230, 245), bgCol);
            spr.drawCircle(cx + 36, cy - 32, 5, RGB565(100, 110, 130));
            break;
        }

        case MOOD_BUG: {
            drawStarBody(RGB565(255, 190, 50));

            // Đám khói đỏ tức giận bốc lên từ đỉnh đầu 💢
            spr.fillSmoothCircle(cx + 30, cy - 34, 8, RGB565(255, 55, 45), bgCol);
            spr.fillSmoothCircle(cx + 38, cy - 42, 6, RGB565(255, 55, 45), bgCol);

            // Hai tay khoanh trước ngực
            spr.fillSmoothRoundRect(cx - 14, cy + 12, 28, 12, 6, starYel, bgCol);
            spr.drawRoundRect(cx - 14, cy + 12, 28, 12, 6, starLine);

            // Mày xếch ngược giận dữ
            spr.drawLine(cx - 16, cy - 10, cx - 5, cy - 5, starLine);
            spr.drawLine(cx + 5, cy - 5, cx + 16, cy - 10, starLine);

            spr.fillCircle(cx - 10, cy - 2, 3, starLine);
            spr.fillCircle(cx + 10, cy - 2, 3, starLine);

            // Miệng tức giận xệ xuống
            spr.drawLine(cx - 6, cy + 8, cx, cy + 4, starLine);
            spr.drawLine(cx, cy + 4, cx + 6, cy + 8, starLine);
            break;
        }

        case MOOD_LOW_BATTERY: {
            // Ngôi sao ủ rũ tông màu xám nhạt kiệt sức
            drawStarBody(RGB565(190, 185, 160));

            // Cục Pin 0% màu đỏ trên đầu 🪫
            int16_t bx = cx - 22, by = cy - 52;
            spr.fillSmoothRoundRect(bx, by, 44, 16, 4, RGB565(30, 36, 48), bgCol);
            spr.drawRoundRect(bx, by, 44, 16, 4, RGB565(255, 60, 60));
            spr.fillRect(bx + 44, by + 4, 3, 8, RGB565(255, 60, 60));
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(255, 60, 60));
            spr.drawString("0%", bx + 22, by + 8, 2);

            // Mắt rũ xuống mệt mỏi
            spr.drawLine(cx - 16, cy - 2, cx - 6, cy + 1, starLine);
            spr.drawLine(cx + 6, cy + 1, cx + 16, cy - 2, starLine);

            // Giọt mồ hôi rơi trên trán 💧
            spr.fillSmoothCircle(cx + 16, cy - 12, 4, RGB565(100, 200, 255), starYel);

            // Miệng thở dài
            spr.drawFastHLine(cx - 6, cy + 8, 12, starLine);
            break;
        }

        case MOOD_CHILL: {
            drawStarBody(starYel);

            // Bàn làm việc
            spr.fillRoundRect(cx - 40, cy + 22, 80, 6, 2, RGB565(160, 110, 65));

            // Cánh tay chống cằm
            spr.drawLine(cx - 22, cy + 22, cx - 14, cy + 10, starLine);
            spr.fillSmoothCircle(cx - 14, cy + 10, 4, starYel, bgCol);

            // Mắt đơ ngang một đường (- -)
            spr.drawLine(cx - 16, cy - 4, cx - 6, cy - 4, starLine);
            spr.drawFastVLine(cx - 11, cy - 3, 3, starLine);
            spr.drawLine(cx + 6, cy - 4, cx + 16, cy - 4, starLine);
            spr.drawFastVLine(cx + 11, cy - 3, 3, starLine);

            // Miệng một đường ngang thư thái
            spr.drawFastHLine(cx - 6, cy + 7, 12, starLine);
            break;
        }

        case MOOD_TGIF: {
            drawStarBody(starYel);

            // KÍNH RÂM ĐEN NGẦU ĐEO TRÊN MẮT NGÔI SAO 😎
            spr.fillSmoothCircle(cx - 12, cy - 4, 8, RGB565(15, 15, 20), starYel);
            spr.fillSmoothCircle(cx + 12, cy - 4, 8, RGB565(15, 15, 20), starYel);
            spr.drawFastHLine(cx - 4, cy - 4, 8, RGB565(15, 15, 20));
            spr.drawPixel(cx - 14, cy - 6, C_WHITE);
            spr.drawPixel(cx + 10, cy - 6, C_WHITE);

            // Miệng cười há to sung sướng
            spr.fillSmoothCircle(cx, cy + 8, 7, RGB565(210, 35, 75), starYel);
            spr.fillRect(cx - 8, cy + 2, 16, 6, starYel);

            // Pháo hoa kim tuyến bay xung quanh 🎊
            drawSparkle(spr, cx - 48, cy - 20, RGB565(0, 230, 255));
            drawSparkle(spr, cx + 48, cy - 20, RGB565(255, 60, 180));
            spr.fillRect(cx - 40, cy + 16, 5, 5, RGB565(255, 230, 20));
            spr.fillRect(cx + 40, cy + 16, 5, 5, RGB565(0, 230, 255));
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  BỘ 3: BÉ THỎ TRẮNG DỄ THƯƠNG (CUTE BUNNY)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::_drawBunnyAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t bgCol) {
    uint16_t fur = C_WHITE;
    uint16_t lineCol = RGB565(35, 25, 25);
    uint16_t pink = RGB565(255, 140, 175);

    auto drawEars = [&](int16_t leftYOffset, int16_t rightYOffset) {
        // Tai trái
        spr.fillSmoothRoundRect(cx - 20, cy - 54 + leftYOffset, 14, 34, 7, fur, bgCol);
        spr.drawRoundRect(cx - 20, cy - 54 + leftYOffset, 14, 34, 7, lineCol);
        spr.fillSmoothRoundRect(cx - 18, cy - 48 + leftYOffset, 10, 24, 5, pink, fur);

        // Tai phải
        spr.fillSmoothRoundRect(cx + 6, cy - 54 + rightYOffset, 14, 34, 7, fur, bgCol);
        spr.drawRoundRect(cx + 6, cy - 54 + rightYOffset, 14, 34, 7, lineCol);
        spr.fillSmoothRoundRect(cx + 8, cy - 48 + rightYOffset, 10, 24, 5, pink, fur);
    };

    switch (m) {
        case MOOD_HAPPY: {
            drawEars(0, 0);

            // Đầu thỏ tròn xoe
            spr.fillSmoothCircle(cx, cy, 32, fur, bgCol);
            spr.drawCircle(cx, cy, 32, lineCol);

            // Hai tay giơ cao reo hò \o/
            spr.drawLine(cx - 22, cy + 18, cx - 34, cy + 2, lineCol);
            spr.drawLine(cx + 22, cy + 18, cx + 34, cy + 2, lineCol);

            // Mắt cười híp chữ V hạnh phúc
            spr.drawLine(cx - 16, cy - 6, cx - 10, cy - 10, lineCol);
            spr.drawLine(cx - 10, cy - 10, cx - 4, cy - 6, lineCol);
            spr.drawLine(cx + 4, cy - 6, cx + 10, cy - 10, lineCol);
            spr.drawLine(cx + 10, cy - 10, cx + 16, cy - 6, lineCol);

            // Má hồng to bự
            spr.fillSmoothCircle(cx - 20, cy + 4, 6, pink, fur);
            spr.fillSmoothCircle(cx + 20, cy + 4, 6, pink, fur);

            // Mũi và Miệng há to khoe lưỡi
            spr.fillCircle(cx, cy - 1, 3, pink);
            spr.fillSmoothCircle(cx, cy + 10, 8, RGB565(210, 35, 75), fur);
            spr.fillRect(cx - 8, cy + 2, 16, 8, fur);

            // Trái tim bay lơ lửng 💕
            drawSparkle(spr, cx - 46, cy - 24, pink);
            drawSparkle(spr, cx + 46, cy - 24, pink);
            break;
        }

        case MOOD_FOCUS: {
            drawEars(0, 0);

            // Đầu thỏ cắm cúi viết bài
            spr.fillSmoothCircle(cx, cy, 32, fur, bgCol);
            spr.drawCircle(cx, cy, 32, lineCol);

            // Bàn làm việc & Sách
            spr.fillRoundRect(cx - 45, cy + 24, 90, 8, 3, RGB565(160, 110, 65));
            spr.fillRoundRect(cx - 22, cy + 14, 44, 16, 2, fur);
            spr.drawRoundRect(cx - 22, cy + 14, 44, 16, 2, lineCol);
            spr.drawFastVLine(cx, cy + 14, 16, lineCol);

            // Tay cầm bút viết ✍️
            spr.drawLine(cx - 16, cy + 16, cx - 6, cy + 6, lineCol);

            // Lông mày nghiêm nghị, tập trung
            spr.drawLine(cx - 16, cy - 12, cx - 6, cy - 8, lineCol);
            spr.drawLine(cx + 6, cy - 8, cx + 16, cy - 12, lineCol);

            spr.fillCircle(cx - 10, cy - 4, 3, lineCol);
            spr.fillCircle(cx + 10, cy - 4, 3, lineCol);

            // Mũi & miệng :3
            spr.fillCircle(cx, cy + 2, 2, pink);
            spr.drawFastHLine(cx - 4, cy + 6, 8, lineCol);

            // Bánh răng tư duy ⚙️
            spr.fillSmoothCircle(cx + 36, cy - 30, 9, RGB565(210, 225, 245), bgCol);
            spr.drawCircle(cx + 36, cy - 30, 4, RGB565(100, 110, 130));
            break;
        }

        case MOOD_BUG: {
            drawEars(4, 4); // Tai hơi cụp ra sau vì bực mình

            spr.fillSmoothCircle(cx, cy, 32, fur, bgCol);
            spr.drawCircle(cx, cy, 32, lineCol);

            // Hai tay khoanh trước ngực
            spr.fillSmoothRoundRect(cx - 18, cy + 18, 36, 14, 7, fur, bgCol);
            spr.drawRoundRect(cx - 18, cy + 18, 36, 14, 7, lineCol);

            // Gân máu chữ thập đỏ giận dữ 💢 bên thái dương
            spr.drawLine(cx + 20, cy - 14, cx + 28, cy - 14, RGB565(255, 45, 65));
            spr.drawLine(cx + 24, cy - 18, cx + 24, cy - 10, RGB565(255, 45, 65));

            // Đám khói trắng xì ra 💨
            spr.fillSmoothCircle(cx + 38, cy - 18, 6, C_WHITE, bgCol);
            spr.fillSmoothCircle(cx + 46, cy - 22, 5, C_WHITE, bgCol);

            // Mắt giận dữ liếc xéo
            spr.drawLine(cx - 16, cy - 10, cx - 6, cy - 5, lineCol);
            spr.drawLine(cx + 6, cy - 5, cx + 16, cy - 10, lineCol);

            spr.fillCircle(cx - 10, cy - 2, 3, lineCol);
            spr.fillCircle(cx + 10, cy - 2, 3, lineCol);

            // Mũi và Miệng mím giận dỗi
            spr.fillCircle(cx, cy + 3, 2, pink);
            spr.drawLine(cx - 5, cy + 8, cx, cy + 5, lineCol);
            spr.drawLine(cx, cy + 5, cx + 5, cy + 8, lineCol);
            break;
        }

        case MOOD_LOW_BATTERY: {
            // Tai thỏ cụp hẳn nằm ngang vì kiệt sức
            spr.fillSmoothRoundRect(cx - 48, cy - 24, 28, 12, 6, fur, bgCol);
            spr.drawRoundRect(cx - 48, cy - 24, 28, 12, 6, lineCol);

            // Pin 0% màu đỏ trên đầu 🪫
            int16_t bx = cx - 22, by = cy - 46;
            spr.fillSmoothRoundRect(bx, by, 44, 16, 4, RGB565(30, 36, 48), bgCol);
            spr.drawRoundRect(bx, by, 44, 16, 4, RGB565(255, 60, 60));
            spr.fillRect(bx + 44, by + 4, 3, 8, RGB565(255, 60, 60));
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(255, 60, 60));
            spr.drawString("0%", bx + 22, by + 8, 2);

            // Đầu thỏ gục sát xuống bàn
            spr.fillSmoothCircle(cx, cy + 4, 30, fur, bgCol);
            spr.drawCircle(cx, cy + 4, 30, lineCol);

            // Mắt nhắm nghiền mệt mỏi
            spr.drawLine(cx - 14, cy + 2, cx - 4, cy + 4, lineCol);
            spr.drawLine(cx + 4, cy + 4, cx + 14, cy + 2, lineCol);

            // Làn khói hồn bay phấp phới 💭
            spr.setTextColor(RGB565(140, 170, 210));
            spr.drawString("z", cx + 38, cy - 8, 2);
            spr.drawString("Z", cx + 46, cy - 20, 2);
            break;
        }

        case MOOD_CHILL: {
            drawEars(0, 0);

            // Bàn làm việc
            spr.fillRoundRect(cx - 45, cy + 24, 90, 8, 3, RGB565(160, 110, 65));

            // Đầu thỏ tựa vào tay
            spr.fillSmoothCircle(cx, cy, 32, fur, bgCol);
            spr.drawCircle(cx, cy, 32, lineCol);

            // Tay chống cằm
            spr.drawLine(cx - 24, cy + 24, cx - 16, cy + 12, lineCol);
            spr.fillSmoothCircle(cx - 16, cy + 12, 4, fur, bgCol);

            // Mắt đơ, buồn ngủ (- -)
            spr.drawLine(cx - 16, cy - 4, cx - 6, cy - 4, lineCol);
            spr.drawFastVLine(cx - 11, cy - 3, 3, lineCol);
            spr.drawLine(cx + 6, cy - 4, cx + 16, cy - 4, lineCol);
            spr.drawFastVLine(cx + 11, cy - 3, 3, lineCol);

            // Má hồng
            spr.fillSmoothCircle(cx - 18, cy + 4, 5, pink, fur);
            spr.fillSmoothCircle(cx + 18, cy + 4, 5, pink, fur);

            // Miệng một đường thẳng đơ
            spr.drawFastHLine(cx - 6, cy + 8, 12, lineCol);
            break;
        }

        case MOOD_TGIF: {
            // Nón sinh nhật Party Hat kẹp giữa 2 tai thỏ 🎉
            spr.fillTriangle(cx - 12, cy - 32, cx, cy - 60, cx + 12, cy - 32, RGB565(255, 50, 140));
            spr.fillTriangle(cx - 6, cy - 42, cx, cy - 60, cx + 6, cy - 42, RGB565(255, 230, 20));
            spr.fillSmoothCircle(cx, cy - 62, 5, C_WHITE, bgCol);

            drawEars(6, 6);

            // Đầu thỏ đeo kính râm
            spr.fillSmoothCircle(cx, cy, 32, fur, bgCol);
            spr.drawCircle(cx, cy, 32, lineCol);

            // KÍNH RÂM ĐEN NGẦU 😎
            spr.fillSmoothCircle(cx - 12, cy - 4, 8, RGB565(15, 15, 20), fur);
            spr.fillSmoothCircle(cx + 12, cy - 4, 8, RGB565(15, 15, 20), fur);
            spr.drawFastHLine(cx - 4, cy - 4, 8, RGB565(15, 15, 20));
            spr.drawPixel(cx - 14, cy - 6, C_WHITE);
            spr.drawPixel(cx + 10, cy - 6, C_WHITE);

            // Miệng há to quẩy hết mình
            spr.fillSmoothCircle(cx, cy + 10, 8, RGB565(210, 35, 75), fur);
            spr.fillRect(cx - 8, cy + 3, 16, 7, fur);

            // Quả cầu Disco & Ánh sáng sân khấu 🪩
            drawSparkle(spr, cx - 48, cy - 20, RGB565(0, 230, 255));
            drawSparkle(spr, cx + 48, cy - 20, RGB565(255, 60, 180));
            spr.fillRect(cx - 40, cy + 16, 5, 5, RGB565(255, 230, 20));
            spr.fillRect(cx + 40, cy + 16, 5, 5, RGB565(80, 245, 120));
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  TOUCH TRỰC TIẾP TRÊN MÀN HÌNH ĐỂ XOAY VÒNG CHUYỂN TRẠNG THÁI
// ═══════════════════════════════════════════════════════════════════════
bool DeskMood::handleTouch(int16_t x, int16_t y) {
    // Chạm vào bất kỳ vị trí nào trên meme (trên vùng Dock) để chuyển tâm trạng kế tiếp
    if (y < 196) {
        nextMood(); // Xoay vòng tròn 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 0
        hardware.playClick();
        return true;
    }
    return false;
}
