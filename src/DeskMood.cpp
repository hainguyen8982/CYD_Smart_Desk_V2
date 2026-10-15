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
            case MOOD_HAPPY:       return "SAN SANG BAT CHUYEN";
            case MOOD_FOCUS:       return "DANG DEEP WORK: DND";
            case MOOD_BUG:         return "DANG SAN BUG: DUNG CHOC!";
            case MOOD_LOW_BATTERY: return "PIN 1%: CAN NAP TRA SUA";
            case MOOD_CHILL:       return "SIEU CHILL: THOAI MAI";
            case MOOD_TGIF:        return "CUOI TUAN ROI: QUAY THOI!";
            default:               return "SAN SANG BAT CHUYEN";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "READY TO CHAT: COFFEE?";
            case MOOD_FOCUS:       return "DEEP WORK: PLEASE DND";
            case MOOD_BUG:         return "BUG HUNTING: CAUTION!";
            case MOOD_LOW_BATTERY: return "BATTERY: 1% LOW ENERGY";
            case MOOD_CHILL:       return "SUPER CHILL & RELAX";
            case MOOD_TGIF:        return "TGIF: WEEKEND VIBES!";
            default:               return "READY TO CHAT: COFFEE?";
        }
    }
}

const char* DeskMood::getMessage(MoodType m, uint8_t lang) const {
    if (lang == 0) { // VI
        switch (m) {
            case MOOD_HAPPY:       return "Hom nay vui ve * Ranh thi ca phe nhe!";
            case MOOD_FOCUS:       return "Dang chay deadline * Xin dung lam phien!";
            case MOOD_BUG:         return "Bug ngap tran * Tiep can can than keo quau!";
            case MOOD_LOW_BATTERY: return "Nao loading 1% * Can tra sua tiep mau gap!";
            case MOOD_CHILL:       return "Cu tu tu ma lam * Viec dau roi cung xong";
            case MOOD_TGIF:        return "Sap tan ca roi * Toi nay xoa het deadline!";
            default:               return "Chuc mot ngay lam viec tran day nang luong!";
        }
    } else { // EN
        switch (m) {
            case MOOD_HAPPY:       return "Good vibes only * Always open for coffee!";
            case MOOD_FOCUS:       return "In deep focus * Please do not interrupt!";
            case MOOD_BUG:         return "High bug density * Approach with caution!";
            case MOOD_LOW_BATTERY: return "Brain at 1% * Needs milk tea recharge!";
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
        case MOOD_BUG:         return RGB565(255, 95, 20);   // Danger Fire Orange
        case MOOD_LOW_BATTERY: return RGB565(250, 195, 25);  // Warning Amber Yellow
        case MOOD_CHILL:       return RGB565(170, 130, 255); // Pastel Lavender Violet
        case MOOD_TGIF:        return RGB565(255, 60, 175);  // Electric Party Pink
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

// Helper to draw a glowing 4-point sparkle star
static void drawSparkle(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t col) {
    spr.drawLine(x - 4, y, x + 4, y, col);
    spr.drawLine(x, y - 4, x, y + 4, col);
    spr.drawPixel(x, y, C_WHITE);
}

void DeskMood::renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg) {
    // 1. Phông nền toàn màn hình 320x240 (Full-Bleed, KHÔNG CÓ CARD GÒ BÓ)
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
    int16_t cy = 76;

    // Vầng hào quang Ambient Glow tỏa sáng mềm mại sau lưng nhân vật
    spr.fillSmoothCircle(cx, cy, 76, auraCol, bgCol);
    spr.fillSmoothCircle(cx, cy, 58, innerAura, auraCol);

    // 2. Hai nút điều hướng cảm ứng ‹ và › nổi mềm mại ở hai mép màn hình
    spr.fillSmoothRoundRect(10, cy - 20, 24, 40, 12, RGB565(28, 34, 48), bgCol);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(RGB565(180, 200, 230));
    spr.drawString("<", 22, cy, 2);

    spr.fillSmoothRoundRect(286, cy - 20, 24, 40, 12, RGB565(28, 34, 48), bgCol);
    spr.drawString(">", 298, cy, 2);

    // 3. VẼ MASCOT AVATAR KHỔNG LỒ, SIÊU CUTE TẠI TÂM MÀN HÌNH (cx = 160, cy = 76)
    _drawAvatar(spr, cx, cy, _mood, bgCol);

    // 4. BIỂN HIỆU NEON TRẠNG THÁI TO RÕ (Y = 138..172)
    // Người đi qua từ xa 2-3m vẫn đọc được rõ mồn một!
    uint16_t accent = getAccentColor(_mood);
    int16_t tagW = 276;
    int16_t tagH = 34;
    int16_t tagY = 138;
    int16_t tagR = 17;

    // Đổ bóng & Nền viên thuốc Neon rực rỡ
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY + 2, tagW, tagH, tagR, RGB565(4, 6, 10), bgCol);
    spr.fillSmoothRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, accent, bgCol);
    spr.drawRoundRect(cx - tagW / 2, tagY, tagW, tagH, tagR, C_WHITE);

    // Chữ trạng thái to đậm
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_BLACK);
    const char* tagStr = getTag(_mood, lang);
    spr.drawString(tagStr, cx, tagY + tagH / 2, 4);

    // 5. CÂU QUOTE VĂN PHÒNG HÀI HƯỚC (Y = 182)
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_WHITE);
    spr.drawString(getMessage(_mood, lang), cx, 184, 2);

    // 6. CHẤM CHỈ THỊ 6 TRẠNG THÁI (Y = 208)
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
//  VẼ 6 MASCOT SIÊU CUTE, BIỂU CẢM SỐNG ĐỘNG (KHỔNG LỒ R=38px..44px)
// ═══════════════════════════════════════════════════════════════════════
void DeskMood::_drawAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t bgCol) {
    switch (m) {
        // ─────────────────────────────────────────────────────────────
        // 0. MOOD_HAPPY: Bé Mochi Mặt Trời / Chú Thỏ Vàng Cười Tỏa Nắng
        // ─────────────────────────────────────────────────────────────
        case MOOD_HAPPY: {
            uint16_t yellow = RGB565(255, 222, 60);
            uint16_t outline = RGB565(65, 35, 15);
            uint16_t pink = RGB565(255, 125, 165);

            // Hai tai thỏ tròn cute trên đầu
            spr.fillSmoothCircle(cx - 24, cy - 36, 14, yellow, bgCol);
            spr.fillSmoothCircle(cx - 24, cy - 36, 8, pink, yellow);
            spr.drawCircle(cx - 24, cy - 36, 14, outline);

            spr.fillSmoothCircle(cx + 24, cy - 36, 14, yellow, bgCol);
            spr.fillSmoothCircle(cx + 24, cy - 36, 8, pink, yellow);
            spr.drawCircle(cx + 24, cy - 36, 14, outline);

            // Mầm cây xanh mọc trên đầu siêu dễ thương 🌱
            spr.fillSmoothCircle(cx - 4, cy - 42, 5, RGB565(60, 215, 90), bgCol);
            spr.fillSmoothCircle(cx + 4, cy - 43, 5, RGB565(60, 215, 90), bgCol);
            spr.drawFastVLine(cx, cy - 42, 6, RGB565(40, 160, 60));

            // Khuôn mặt tròn phúng phính khổng lồ (R = 38px)
            spr.fillSmoothCircle(cx, cy, 38, yellow, bgCol);
            spr.drawCircle(cx, cy, 38, outline);
            spr.drawCircle(cx, cy, 37, outline);

            // Đôi mắt to tròn long lanh Anime (Glossy Sparkling Eyes)
            spr.fillSmoothCircle(cx - 15, cy - 4, 7, outline, yellow);
            spr.fillCircle(cx - 17, cy - 6, 3, C_WHITE);
            spr.drawPixel(cx - 13, cy - 2, C_WHITE);

            spr.fillSmoothCircle(cx + 15, cy - 4, 7, outline, yellow);
            spr.fillCircle(cx + 13, cy - 6, 3, C_WHITE);
            spr.drawPixel(cx + 17, cy - 2, C_WHITE);

            // Đôi má hồng đào to bự e thẹn
            spr.fillSmoothCircle(cx - 25, cy + 9, 8, pink, yellow);
            spr.fillSmoothCircle(cx + 25, cy + 9, 8, pink, yellow);

            // Miệng cười :3 há to khoe lưỡi hồng cực ngọt ngào
            spr.fillSmoothCircle(cx, cy + 13, 9, RGB565(210, 35, 75), yellow);
            spr.fillRect(cx - 10, cy + 5, 20, 8, yellow);
            spr.fillCircle(cx, cy + 17, 6, RGB565(255, 145, 180)); // Lưỡi

            // Ngôi sao lấp lánh xung quanh ✨
            drawSparkle(spr, cx - 50, cy - 20, RGB565(255, 235, 110));
            drawSparkle(spr, cx + 52, cy - 16, RGB565(80, 245, 255));
            drawSparkle(spr, cx - 46, cy + 28, RGB565(255, 140, 190));
            drawSparkle(spr, cx + 48, cy + 26, RGB565(255, 235, 110));
            break;
        }

        // ─────────────────────────────────────────────────────────────
        // 1. MOOD_FOCUS: Mèo Gamer / Coder Siêu Tập Trung (Tai Nghe Neon Đỏ)
        // ─────────────────────────────────────────────────────────────
        case MOOD_FOCUS: {
            uint16_t catBg = RGB565(30, 38, 52);
            uint16_t neonRed = RGB565(255, 45, 65);
            uint16_t outline = RGB565(14, 18, 26);

            // Tai mèo nhọn 2 bên
            spr.fillTriangle(cx - 36, cy - 20, cx - 20, cy - 46, cx - 8, cy - 26, catBg);
            spr.fillTriangle(cx - 30, cy - 22, cx - 20, cy - 40, cx - 12, cy - 26, neonRed);
            spr.fillTriangle(cx + 8, cy - 26, cx + 20, cy - 46, cx + 36, cy - 20, catBg);
            spr.fillTriangle(cx + 12, cy - 26, cx + 20, cy - 40, cx + 30, cy - 22, neonRed);

            // Vòm tai nghe Gaming hầm hố
            spr.drawArc(cx, cy - 2, 45, 37, 215, 145, RGB565(18, 22, 32), bgCol, true);
            spr.drawArc(cx, cy - 2, 43, 40, 220, 140, neonRed, bgCol, true);

            // Hai ốp tai nghe khổng lồ phát sáng Neon
            spr.fillSmoothRoundRect(cx - 50, cy - 14, 16, 36, 8, neonRed, bgCol);
            spr.fillSmoothRoundRect(cx - 47, cy - 10, 10, 28, 5, RGB565(20, 24, 34), neonRed);
            spr.fillSmoothCircle(cx - 42, cy + 4, 3, C_WHITE, RGB565(20, 24, 34));

            spr.fillSmoothRoundRect(cx + 34, cy - 14, 16, 36, 8, neonRed, bgCol);
            spr.fillSmoothRoundRect(cx + 37, cy - 10, 10, 28, 5, RGB565(20, 24, 34), neonRed);
            spr.fillSmoothCircle(cx + 42, cy + 4, 3, C_WHITE, RGB565(20, 24, 34));

            // Cần micro uốn ra miệng có đèn LED xanh
            spr.drawArc(cx - 26, cy + 18, 18, 16, 120, 180, RGB565(100, 110, 130), bgCol, true);
            spr.fillSmoothCircle(cx - 10, cy + 24, 4, RGB565(0, 230, 255), bgCol);

            // Khuôn mặt mèo tròn
            spr.fillSmoothCircle(cx, cy, 38, catBg, bgCol);
            spr.drawCircle(cx, cy, 38, outline);

            // Kính Visor công nghệ phát sáng đỏ rực
            spr.fillSmoothRoundRect(cx - 28, cy - 9, 56, 18, 6, neonRed, catBg);
            spr.drawFastHLine(cx - 22, cy - 4, 44, C_WHITE);
            spr.fillSmoothCircle(cx - 14, cy, 4, C_WHITE, neonRed);
            spr.fillSmoothCircle(cx + 14, cy, 4, C_WHITE, neonRed);

            // Mũi và ria mép công nghệ
            spr.fillTriangle(cx - 3, cy + 13, cx + 3, cy + 13, cx, cy + 16, neonRed);
            spr.drawLine(cx - 18, cy + 16, cx - 32, cy + 14, RGB565(120, 140, 170));
            spr.drawLine(cx - 18, cy + 20, cx - 30, cy + 22, RGB565(120, 140, 170));
            spr.drawLine(cx + 18, cy + 16, cx + 32, cy + 14, RGB565(120, 140, 170));
            spr.drawLine(cx + 18, cy + 20, cx + 30, cy + 22, RGB565(120, 140, 170));

            // Ngọn lửa tập trung bốc lên đỉnh đầu 🔥
            spr.fillTriangle(cx - 8, cy - 40, cx, cy - 58, cx + 8, cy - 40, RGB565(255, 90, 20));
            spr.fillTriangle(cx - 4, cy - 40, cx, cy - 52, cx + 4, cy - 40, RGB565(255, 230, 40));
            break;
        }

        // ─────────────────────────────────────────────────────────────
        // 2. MOOD_BUG: Bé Quỷ Lửa Phồng Má Bốc Khói (Tức Giận Vì Bug)
        // ─────────────────────────────────────────────────────────────
        case MOOD_BUG: {
            uint16_t redFire = RGB565(255, 55, 35);
            uint16_t hornCol = RGB565(255, 215, 20);

            // Sừng quỷ nhỏ màu vàng trên trán
            spr.fillTriangle(cx - 24, cy - 30, cx - 18, cy - 50, cx - 10, cy - 34, hornCol);
            spr.fillTriangle(cx + 10, cy - 34, cx + 18, cy - 50, cx + 24, cy - 30, hornCol);

            // Hai đám khói xì ra từ tai 💨 💨
            // Khói trái
            spr.fillSmoothCircle(cx - 46, cy - 10, 10, C_WHITE, bgCol);
            spr.fillSmoothCircle(cx - 56, cy - 18, 7, C_WHITE, bgCol);
            spr.fillSmoothCircle(cx - 64, cy - 10, 5, C_WHITE, bgCol);
            // Khói phải
            spr.fillSmoothCircle(cx + 46, cy - 10, 10, C_WHITE, bgCol);
            spr.fillSmoothCircle(cx + 56, cy - 18, 7, C_WHITE, bgCol);
            spr.fillSmoothCircle(cx + 64, cy - 10, 5, C_WHITE, bgCol);

            // Mặt tròn béo ú phồng má tức tối (R = 38px)
            spr.fillSmoothCircle(cx, cy, 38, redFire, bgCol);
            spr.drawCircle(cx, cy, 38, RGB565(140, 20, 10));

            // Dấu chữ thập gân máu giận dữ 💢 bên thái dương
            spr.drawLine(cx + 20, cy - 24, cx + 28, cy - 24, C_WHITE);
            spr.drawLine(cx + 24, cy - 28, cx + 24, cy - 20, C_WHITE);

            // Lông mày xếch ngược giận dữ (\   /)
            spr.drawLine(cx - 24, cy - 16, cx - 8, cy - 8, C_BLACK);
            spr.drawLine(cx - 24, cy - 17, cx - 8, cy - 9, C_BLACK);
            spr.drawLine(cx + 8,  cy - 8,  cx + 24, cy - 16, C_BLACK);
            spr.drawLine(cx + 8,  cy - 9,  cx + 24, cy - 17, C_BLACK);

            // Mắt giận dữ (> <)
            spr.drawLine(cx - 20, cy - 6, cx - 10, cy - 1, C_WHITE);
            spr.drawLine(cx - 20, cy + 4, cx - 10, cy - 1, C_WHITE);
            spr.drawLine(cx + 20, cy - 6, cx + 10, cy - 1, C_WHITE);
            spr.drawLine(cx + 20, cy + 4, cx + 10, cy - 1, C_WHITE);

            // Miệng nghiến răng ken két (Hàm răng trắng đều)
            spr.fillSmoothRoundRect(cx - 16, cy + 12, 32, 14, 4, C_WHITE, redFire);
            spr.drawRoundRect(cx - 16, cy + 12, 32, 14, 4, C_BLACK);
            spr.drawFastHLine(cx - 14, cy + 19, 28, C_BLACK);
            for (int t = 0; t < 4; t++) {
                spr.drawFastVLine(cx - 10 + t * 7, cy + 13, 12, C_BLACK);
            }

            // Con bọ pixel nhỏ bị gạch chéo 🪲
            spr.fillCircle(cx - 48, cy + 24, 6, RGB565(20, 20, 20));
            spr.drawLine(cx - 54, cy + 18, cx - 42, cy + 30, RGB565(255, 45, 65));
            spr.drawLine(cx - 42, cy + 18, cx - 54, cy + 30, RGB565(255, 45, 65));
            break;
        }

        // ─────────────────────────────────────────────────────────────
        // 3. MOOD_LOW_BATTERY: Bé Slime Chảy Xệ Đuối Sức (Pin 1% Buồn Ngủ)
        // ─────────────────────────────────────────────────────────────
        case MOOD_LOW_BATTERY: {
            uint16_t slimeCol = RGB565(155, 190, 225);
            uint16_t shadow = RGB565(110, 145, 185);

            // Cục Pin 1% to đùng nằm gục trên đầu 🪫
            int16_t bx = cx - 28, by = cy - 48;
            spr.fillSmoothRoundRect(bx, by, 52, 20, 5, RGB565(28, 36, 50), bgCol);
            spr.drawRoundRect(bx, by, 52, 20, 5, RGB565(245, 190, 30));
            spr.fillRect(bx + 52, by + 5, 4, 10, RGB565(245, 190, 30)); // Cực pin
            // Vạch pin đỏ 1% nhấp nháy
            spr.fillRoundRect(bx + 4, by + 3, 10, 14, 2, RGB565(255, 45, 45));
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(250, 200, 30));
            spr.drawString("1%", bx + 32, by + 10, 2);

            // Thân Slime tan chảy xệ xuống (Chảy nhão ra vì kiệt sức)
            spr.fillSmoothRoundRect(cx - 46, cy - 14, 92, 48, 24, slimeCol, bgCol);
            // Giọt nước chảy rớt ở đáy
            spr.fillSmoothCircle(cx - 28, cy + 32, 7, slimeCol, bgCol);
            spr.fillSmoothCircle(cx + 4,  cy + 34, 8, slimeCol, bgCol);
            spr.fillSmoothCircle(cx + 30, cy + 31, 6, slimeCol, bgCol);

            // Đôi mắt xoáy chóng mặt (@ _ @) hoặc rũ xuống
            spr.drawCircle(cx - 18, cy, 6, shadow);
            spr.drawCircle(cx - 18, cy, 3, shadow);
            spr.drawPixel(cx - 18, cy, shadow);

            spr.drawCircle(cx + 18, cy, 6, shadow);
            spr.drawCircle(cx + 18, cy, 3, shadow);
            spr.drawPixel(cx + 18, cy, shadow);

            // Miệng lượn sóng mệt mỏi
            spr.drawLine(cx - 12, cy + 16, cx - 4, cy + 13, shadow);
            spr.drawLine(cx - 4,  cy + 13, cx + 4, cy + 18, shadow);
            spr.drawLine(cx + 4,  cy + 18, cx + 12, cy + 15, shadow);

            // Bong bóng mũi ngủ gật to tròn thở phập phồng 💤
            spr.fillSmoothCircle(cx + 12, cy + 6, 9, RGB565(210, 235, 255), slimeCol);
            spr.fillCircle(cx + 10, cy + 4, 3, C_WHITE);

            // Chữ "z Z Z" bay lên
            spr.setTextColor(RGB565(140, 175, 220));
            spr.drawString("z", cx + 48, cy - 24, 2);
            spr.drawString("Z", cx + 58, cy - 38, 2);
            break;
        }

        // ─────────────────────────────────────────────────────────────
        // 4. MOOD_CHILL: Đại Ca Capybara Đeo Kính Râm, Quả Cam Trên Đầu
        // ─────────────────────────────────────────────────────────────
        case MOOD_CHILL: {
            uint16_t brown = RGB565(175, 125, 75);
            uint16_t darkBrown = RGB565(85, 55, 30);
            uint16_t muzzle = RGB565(140, 95, 55);

            // Hai tai tròn nhỏ cụp hai bên
            spr.fillSmoothCircle(cx - 40, cy - 14, 10, brown, bgCol);
            spr.fillSmoothCircle(cx - 40, cy - 14, 5, darkBrown, brown);
            spr.fillSmoothCircle(cx + 40, cy - 14, 10, brown, bgCol);
            spr.fillSmoothCircle(cx + 40, cy - 14, 5, darkBrown, brown);

            // Quả quýt Yuzu vàng mọng nước trên đỉnh đầu 🍊
            spr.fillSmoothCircle(cx, cy - 42, 14, RGB565(255, 145, 15), bgCol);
            spr.drawCircle(cx, cy - 42, 14, RGB565(180, 80, 10));
            // Cuống và lá xanh
            spr.drawFastVLine(cx, cy - 58, 4, RGB565(40, 120, 40));
            spr.fillSmoothCircle(cx + 5, cy - 57, 4, RGB565(50, 200, 60), bgCol);

            // Đầu Capybara hình chữ nhật bo tròn đặc trưng (To bự W=82, H=68)
            spr.fillSmoothRoundRect(cx - 40, cy - 26, 80, 64, 24, brown, bgCol);
            spr.drawRoundRect(cx - 40, cy - 26, 80, 64, 24, darkBrown);

            // Mõm Capybara to bản phía dưới
            spr.fillSmoothRoundRect(cx - 24, cy + 6, 48, 28, 12, muzzle, brown);
            // Mũi to màu đen
            spr.fillSmoothRoundRect(cx - 10, cy + 10, 20, 8, 4, darkBrown, muzzle);
            // Nụ cười mỉm chi đắc đạo cực chill (Zen Smile)
            spr.drawFastHLine(cx - 8, cy + 26, 16, darkBrown);
            spr.drawLine(cx - 8, cy + 26, cx - 12, cy + 23, darkBrown);
            spr.drawLine(cx + 8, cy + 26, cx + 12, cy + 23, darkBrown);

            // KÍNH RÂM THUG LIFE ĐEN NGẦU ĐEO TRÊN MẮT 😎
            // Mắt kính trái
            spr.fillSmoothRoundRect(cx - 36, cy - 14, 32, 18, 4, RGB565(12, 14, 18), brown);
            spr.drawRoundRect(cx - 36, cy - 14, 32, 18, 4, C_WHITE);
            spr.drawLine(cx - 30, cy - 12, cx - 18, cy + 2, C_WHITE); // Vệt chói kính trắng
            spr.drawLine(cx - 24, cy - 12, cx - 12, cy + 2, C_WHITE);

            // Mắt kính phải
            spr.fillSmoothRoundRect(cx + 4, cy - 14, 32, 18, 4, RGB565(12, 14, 18), brown);
            spr.drawRoundRect(cx + 4, cy - 14, 32, 18, 4, C_WHITE);
            spr.drawLine(cx + 10, cy - 12, cx + 22, cy + 2, C_WHITE); // Vệt chói kính trắng
            spr.drawLine(cx + 16, cy - 12, cx + 28, cy + 2, C_WHITE);

            // Cầu nối kính râm
            spr.fillRect(cx - 4, cy - 8, 8, 4, RGB565(12, 14, 18));

            // Ly cà phê bốc khói nghi ngút ở góc ☕
            spr.fillRoundRect(cx + 46, cy + 12, 16, 18, 3, RGB565(220, 230, 245));
            spr.drawFastVLine(cx + 62, cy + 15, 10, RGB565(220, 230, 245)); // quai ly
            spr.drawArc(cx + 52, cy + 6, 8, 6, 40, 140, C_WHITE, bgCol, true); // khói
            break;
        }

        // ─────────────────────────────────────────────────────────────
        // 5. MOOD_TGIF: Cún Shiba Đội Nón Sinh Nhật Quẩy Tiệc Cuối Tuần
        // ─────────────────────────────────────────────────────────────
        case MOOD_TGIF: {
            uint16_t cream = RGB565(255, 215, 130);
            uint16_t white = C_WHITE;
            uint16_t pink = RGB565(255, 105, 160);

            // Mũ tiệc Party Cone Hat sọc màu sặc sỡ trên đầu 🎉
            spr.fillTriangle(cx - 16, cy - 36, cx, cy - 64, cx + 16, cy - 36, RGB565(255, 50, 140));
            // Sọc vàng neon trên nón
            spr.fillTriangle(cx - 10, cy - 44, cx, cy - 64, cx + 10, cy - 44, RGB565(255, 230, 20));
            spr.fillTriangle(cx - 5,  cy - 52, cx, cy - 64, cx + 5,  cy - 52, RGB565(0, 230, 255));
            // Cục bông gòn tròn trên chóp nón
            spr.fillSmoothCircle(cx, cy - 66, 6, white, bgCol);

            // Tai cún vểnh hai bên
            spr.fillTriangle(cx - 38, cy - 16, cx - 26, cy - 44, cx - 14, cy - 24, cream);
            spr.fillTriangle(cx + 14, cy - 24, cx + 26, cy - 44, cx + 38, cy - 16, cream);

            // Mặt tròn xoe Shiba phấn khích cực độ
            spr.fillSmoothCircle(cx, cy, 38, cream, bgCol);
            spr.drawCircle(cx, cy, 38, RGB565(180, 120, 50));

            // Mảng lông trắng 2 bên má
            spr.fillSmoothCircle(cx - 18, cy + 12, 18, white, cream);
            spr.fillSmoothCircle(cx + 18, cy + 12, 18, white, cream);

            // Mắt cười híp tít chữ V hạnh phúc ( ≧ ▽ ≦ )
            spr.drawLine(cx - 20, cy - 6, cx - 12, cy - 11, C_BLACK);
            spr.drawLine(cx - 12, cy - 11, cx - 4, cy - 6, C_BLACK);
            spr.drawLine(cx - 20, cy - 5, cx - 12, cy - 10, C_BLACK);
            spr.drawLine(cx - 12, cy - 10, cx - 4, cy - 5, C_BLACK);

            spr.drawLine(cx + 4, cy - 6, cx + 12, cy - 11, C_BLACK);
            spr.drawLine(cx + 12, cy - 11, cx + 20, cy - 6, C_BLACK);
            spr.drawLine(cx + 4, cy - 5, cx + 12, cy - 10, C_BLACK);
            spr.drawLine(cx + 12, cy - 10, cx + 20, cy - 5, C_BLACK);

            // Đôi má hồng phấn rực rỡ
            spr.fillSmoothCircle(cx - 24, cy + 6, 8, pink, white);
            spr.fillSmoothCircle(cx + 24, cy + 6, 8, pink, white);

            // Mũi đen nhỏ
            spr.fillSmoothCircle(cx, cy + 4, 4, RGB565(30, 20, 20), white);

            // Miệng há to hết cỡ lè lưỡi phấn khích
            spr.fillSmoothCircle(cx, cy + 16, 11, RGB565(190, 25, 60), white);
            spr.fillRect(cx - 12, cy + 8, 24, 8, white);
            spr.fillCircle(cx, cy + 20, 7, pink); // Lưỡi lè dài

            // Pháo hoa kim tuyến & hoa giấy tung tóe khắp màn hình 🎊
            spr.fillRect(cx - 56, cy - 30, 5, 5, RGB565(255, 50, 140));
            spr.fillRect(cx + 52, cy - 28, 6, 4, RGB565(0, 230, 255));
            spr.fillRect(cx - 48, cy + 22, 4, 6, RGB565(255, 230, 20));
            spr.fillRect(cx + 56, cy + 18, 5, 5, RGB565(80, 245, 100));
            drawSparkle(spr, cx - 64, cy - 8, RGB565(255, 230, 20));
            drawSparkle(spr, cx + 64, cy - 4, RGB565(255, 60, 180));
            break;
        }
    }
}

bool DeskMood::handleTouch(int16_t x, int16_t y) {
    // Chạm vào 2/3 trên màn hình (để không chạm nhầm vào Dock bên dưới khi xuất hiện)
    if (y < 196) {
        if (x < 90) {
            prevMood();
        } else {
            nextMood();
        }
        hardware.playClick();
        return true;
    }
    return false;
}
