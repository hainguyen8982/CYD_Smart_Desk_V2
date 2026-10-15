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

DeskMood::DeskMood() : _mood(MOOD_HAPPY) {}

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

const char* DeskMood::getName(MoodType m) const {
    switch (m) {
        case MOOD_HAPPY:       return "Happy";
        case MOOD_FOCUS:       return "Deep Work";
        case MOOD_BUG:         return "Bug Hunting";
        case MOOD_LOW_BATTERY: return "Low Battery";
        case MOOD_CHILL:       return "Chill";
        case MOOD_TGIF:        return "TGIF Weekend";
        default:               return "Happy";
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
            case MOOD_HAPPY:       return "READY TO CHAT";
            case MOOD_FOCUS:       return "DO NOT DISTURB";
            case MOOD_BUG:         return "DANGER: BUG HUNTING";
            case MOOD_LOW_BATTERY: return "BATTERY: 1%";
            case MOOD_CHILL:       return "JUST CHILLIN";
            case MOOD_TGIF:        return "TGIF: CUOI TUAN!";
            default:               return "READY TO CHAT";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "READY TO CHAT";
            case MOOD_FOCUS:       return "DO NOT DISTURB";
            case MOOD_BUG:         return "DANGER: BUG HUNTING";
            case MOOD_LOW_BATTERY: return "BATTERY: 1%";
            case MOOD_CHILL:       return "JUST CHILLIN";
            case MOOD_TGIF:        return "TGIF: WEEKEND!";
            default:               return "READY TO CHAT";
        }
    }
}

const char* DeskMood::getMessage(MoodType m, uint8_t lang) const {
    if (lang == 0) { // VI
        switch (m) {
            case MOOD_HAPPY:       return "Hom nay vui ve * Tra sua khum?";
            case MOOD_FOCUS:       return "Dang tap trung * Xin dung lam phien!";
            case MOOD_BUG:         return "Bug ngap dau * Tiep can can than!";
            case MOOD_LOW_BATTERY: return "Nao dang loading * Can nap nang luong";
            case MOOD_CHILL:       return "Viec hom nay cu de... ngay mai";
            case MOOD_TGIF:        return "Sap tan ca roi * Cuoi tuan toi ben!";
            default:               return "Chuc mot ngay lam viec tuyet voi!";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "Good vibes today * Coffee break?";
            case MOOD_FOCUS:       return "In the zone * Please do not disturb!";
            case MOOD_BUG:         return "Caution: High bug density zone!";
            case MOOD_LOW_BATTERY: return "Brain loading 1% * Running on coffee";
            case MOOD_CHILL:       return "Keep calm & let it flow";
            case MOOD_TGIF:        return "Ready for weekend * Celebrate time!";
            default:               return "Have a wonderful productive day!";
        }
    }
}

uint16_t DeskMood::getAccentColor(MoodType m) const {
    switch (m) {
        case MOOD_HAPPY:       return RGB565(35, 215, 140);  // Emerald Mint Green
        case MOOD_FOCUS:       return RGB565(255, 45, 65);   // Crimson Red
        case MOOD_BUG:         return RGB565(255, 105, 25);  // Fire Amber Orange
        case MOOD_LOW_BATTERY: return RGB565(245, 185, 30);  // Warning Amber
        case MOOD_CHILL:       return RGB565(150, 115, 250); // Pastel Lavender
        case MOOD_TGIF:        return RGB565(255, 65, 175);  // Party Magenta Pink
        default:               return RGB565(35, 215, 140);
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
        uint8_t val = prefs.getUChar("mood", (uint8_t)MOOD_HAPPY);
        if (val < MOOD_COUNT) {
            _mood = (MoodType)val;
        } else {
            _mood = MOOD_HAPPY;
        }
        prefs.end();
    }
}

void DeskMood::_saveSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_mood", false)) {
        prefs.putUChar("mood", (uint8_t)_mood);
        prefs.end();
    }
}

void DeskMood::renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg) {
    // 1. Phông nền Dark Cosmic tĩnh
    spr.fillSprite(themeBg);

    // 2. Header Bar trang nhã (Y = 6..22)
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(RGB565(140, 160, 190));
    spr.drawString((lang == 0 ? "BANG TAM TRANG" : "DESK VIBE SIGNBOARD"), 20, 8, 2);

    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(RGB565(80, 100, 130));
    spr.drawString("SmartDesk V2", 300, 10, 1);

    // 3. Khung Thẻ Biển Báo Glass Card trung tâm (X = 20..300, Y = 26..186, H = 160)
    int16_t cardX = 20, cardY = 26, cardW = 280, cardH = 160, cardR = 14;
    uint16_t cardBg = RGB565(14, 18, 28);
    uint16_t accent = getAccentColor(_mood);

    // Bóng đổ & Nền
    spr.fillRoundRect(cardX, cardY + 2, cardW, cardH, cardR, RGB565(5, 8, 14));
    spr.fillRoundRect(cardX, cardY, cardW, cardH, cardR, cardBg);
    // Viền phát sáng theo màu tâm trạng
    spr.drawRoundRect(cardX, cardY, cardW, cardH, cardR, accent);

    // Specular top highlight
    spr.drawFastHLine(cardX + cardR, cardY, cardW - cardR * 2, RGB565(180, 210, 255));

    // 4. Hai nút mũi tên chuyển nhanh [<] và [>]
    spr.fillSmoothCircle(38, 70, 13, RGB565(22, 28, 42), cardBg);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(RGB565(160, 180, 210));
    spr.drawString("<", 38, 70, 2);

    spr.fillSmoothCircle(282, 70, 13, RGB565(22, 28, 42), cardBg);
    spr.setTextColor(RGB565(160, 180, 210));
    spr.drawString(">", 282, 70, 2);

    // 5. Vẽ Avatar Biểu Cảm To Đẹp ở giữa (cx = 160, cy = 68)
    int16_t cx = 160, cy = 66;
    _drawAvatar(spr, cx, cy, _mood, cardBg);

    // 6. Viên thuốc Tag trạng thái Neon (Y = 106..128)
    int16_t tagW = 196, tagH = 22, tagY = 106;
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagH / 2, accent, cardBg);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_BLACK);
    spr.drawString(getTag(_mood, lang), cx, tagY + tagH / 2, 2);

    // 7. Câu khẩu hiệu / Lời nhắn vui (Y = 138)
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_WHITE);
    spr.drawString(getMessage(_mood, lang), cx, 140, 2);

    // 8. Chấm chỉ thị 6 tâm trạng ở đáy thẻ (Y = 166)
    int16_t dotStartX = cx - (MOOD_COUNT * 14) / 2 + 7;
    for (int i = 0; i < MOOD_COUNT; i++) {
        int16_t dx = dotStartX + i * 14;
        if (i == (int)_mood) {
            spr.fillSmoothRoundRect(dx - 6, 164, 13, 5, 2, accent, cardBg);
        } else {
            spr.fillSmoothCircle(dx, 166, 2, RGB565(45, 58, 80), cardBg);
        }
    }
}

void DeskMood::_drawAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t cardBg) {
    switch (m) {
        case MOOD_HAPPY: {
            // Mặt trời / Nụ cười tỏa nắng
            spr.fillSmoothCircle(cx, cy, 22, RGB565(255, 215, 0), cardBg);
            // Má hồng
            spr.fillSmoothCircle(cx - 12, cy + 5, 4, RGB565(255, 110, 140), RGB565(255, 215, 0));
            spr.fillSmoothCircle(cx + 12, cy + 5, 4, RGB565(255, 110, 140), RGB565(255, 215, 0));
            // Mắt cười híp ( ^ ^ )
            spr.drawLine(cx - 13, cy - 2, cx - 8, cy - 6, RGB565(30, 20, 25));
            spr.drawLine(cx - 8, cy - 6, cx - 3, cy - 2, RGB565(30, 20, 25));
            spr.drawLine(cx + 3, cy - 2, cx + 8, cy - 6, RGB565(30, 20, 25));
            spr.drawLine(cx + 8, cy - 6, cx + 13, cy - 2, RGB565(30, 20, 25));
            // Miệng cười ngọt ngào
            spr.fillSmoothCircle(cx, cy + 8, 6, RGB565(45, 15, 20), RGB565(255, 215, 0));
            spr.fillRect(cx - 7, cy + 2, 14, 6, RGB565(255, 215, 0));
            break;
        }

        case MOOD_FOCUS: {
            // Tai nghe Cyber Headset phát sáng
            // Vòm tai nghe
            spr.drawArc(cx, cy - 2, 22, 18, 220, 140, RGB565(60, 75, 95), cardBg, true);
            // Hai bên đệm tai nghe phát sáng đỏ
            spr.fillRoundRect(cx - 24, cy - 6, 8, 18, 3, RGB565(255, 45, 65));
            spr.fillRoundRect(cx + 16, cy - 6, 8, 18, 3, RGB565(255, 45, 65));
            // Mặt tập trung
            spr.fillSmoothCircle(cx, cy, 15, RGB565(30, 38, 52), cardBg);
            // Kính visor neon đỏ
            spr.fillRoundRect(cx - 11, cy - 5, 22, 8, 3, RGB565(255, 45, 65));
            spr.drawFastHLine(cx - 8, cy - 2, 16, C_WHITE);
            // Sóng âm thanh hai bên
            spr.drawFastVLine(cx - 28, cy - 2, 10, RGB565(255, 80, 100));
            spr.drawFastVLine(cx + 28, cy - 2, 10, RGB565(255, 80, 100));
            break;
        }

        case MOOD_BUG: {
            // Con bọ Bug bốc khói / Tia sét cảnh báo ⚡
            uint16_t bugCol = RGB565(255, 95, 20);
            // Thân bọ tròn
            spr.fillSmoothCircle(cx, cy + 2, 16, bugCol, cardBg);
            // Đầu bọ
            spr.fillSmoothCircle(cx, cy - 12, 9, RGB565(210, 60, 15), cardBg);
            // Râu bọ 2 bên
            spr.drawLine(cx - 3, cy - 18, cx - 12, cy - 26, bugCol);
            spr.drawLine(cx + 3, cy - 18, cx + 12, cy - 26, bugCol);
            // Chân bọ
            spr.drawLine(cx - 14, cy - 4, cx - 22, cy - 8, bugCol);
            spr.drawLine(cx - 15, cy + 3, cx - 24, cy + 3, bugCol);
            spr.drawLine(cx - 14, cy + 10, cx - 22, cy + 14, bugCol);
            spr.drawLine(cx + 14, cy - 4, cx + 22, cy - 8, bugCol);
            spr.drawLine(cx + 15, cy + 3, cx + 24, cy + 3, bugCol);
            spr.drawLine(cx + 14, cy + 10, cx + 22, cy + 14, bugCol);
            // Mắt tức giận (> <)
            spr.drawLine(cx - 6, cy - 14, cx - 2, cy - 11, C_WHITE);
            spr.drawLine(cx - 6, cy - 8,  cx - 2, cy - 11, C_WHITE);
            spr.drawLine(cx + 6, cy - 14, cx + 2, cy - 11, C_WHITE);
            spr.drawLine(cx + 6, cy - 8,  cx + 2, cy - 11, C_WHITE);
            break;
        }

        case MOOD_LOW_BATTERY: {
            // Cục Pin 1% màu đỏ nhấp nháy
            int16_t bx = cx - 24, by = cy - 12;
            spr.drawRoundRect(bx, by, 44, 24, 4, RGB565(245, 185, 30));
            spr.drawRoundRect(bx + 1, by + 1, 42, 22, 3, RGB565(245, 185, 30));
            // Cực dương pin
            spr.fillRect(bx + 44, by + 6, 4, 12, RGB565(245, 185, 30));
            // Vạch pin 1% đỏ rực
            spr.fillRoundRect(bx + 4, by + 4, 8, 16, 2, RGB565(255, 45, 45));
            // Biểu tượng sấm sét / chữ Z
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(245, 185, 30));
            spr.drawString("1%", cx + 7, cy, 2);
            break;
        }

        case MOOD_CHILL: {
            // Mặt Capybara ngầu đeo kính râm 😎
            uint16_t brown = RGB565(165, 115, 65);
            spr.fillSmoothRoundRect(cx - 18, cy - 10, 36, 30, 10, brown, cardBg);
            // Quả cam trên đầu
            spr.fillSmoothCircle(cx, cy - 16, 5, RGB565(255, 135, 15), cardBg);
            spr.drawPixel(cx + 2, cy - 21, RGB565(40, 185, 55));
            // Kính râm đen siêu ngầu
            spr.fillRoundRect(cx - 16, cy - 5, 14, 10, 3, RGB565(20, 20, 25));
            spr.fillRoundRect(cx + 2,  cy - 5, 14, 10, 3, RGB565(20, 20, 25));
            spr.drawFastHLine(cx - 2, cy - 2, 4, RGB565(20, 20, 25));
            spr.drawLine(cx - 14, cy - 3, cx - 7, cy + 3, C_WHITE); // ánh phản chiếu kính
            spr.drawLine(cx + 4,  cy - 3, cx + 11, cy + 3, C_WHITE);
            // Miệng nhếch cười
            spr.drawFastHLine(cx - 4, cy + 12, 8, RGB565(40, 25, 15));
            break;
        }

        case MOOD_TGIF: {
            // Ly bia cụng nhau hoặc Cocktail tiệc tùng 🎉
            uint16_t beerCol = RGB565(255, 190, 20);
            // Ly 1
            spr.fillRoundRect(cx - 18, cy - 4, 14, 20, 3, beerCol);
            spr.fillSmoothCircle(cx - 11, cy - 6, 7, C_WHITE, cardBg); // Bọt bia
            spr.drawFastVLine(cx - 22, cy - 1, 14, RGB565(200, 150, 20)); // quai ly
            // Ly 2 cụng vào
            spr.fillRoundRect(cx + 4, cy - 4, 14, 20, 3, beerCol);
            spr.fillSmoothCircle(cx + 11, cy - 6, 7, C_WHITE, cardBg); // Bọt bia
            spr.drawFastVLine(cx + 22, cy - 1, 14, RGB565(200, 150, 20)); // quai ly
            // Ngôi sao lấp lánh xung quanh
            spr.setTextColor(RGB565(255, 80, 180));
            spr.drawString("*", cx, cy - 16, 2);
            spr.drawString("+", cx - 18, cy + 18, 1);
            spr.drawString("+", cx + 18, cy + 18, 1);
            break;
        }
    }
}

bool DeskMood::handleTouch(int16_t x, int16_t y) {
    if (y < 185) {
        if (x < 70) {
            prevMood();
        } else if (x > 250) {
            nextMood();
        } else {
            nextMood();
        }
        hardware.playClick();
        return true;
    }
    return false;
}
