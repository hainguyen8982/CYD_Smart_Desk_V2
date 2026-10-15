// ═══════════════════════════════════════════════════════════════════════
//  DeskPet.cpp — Virtual Desk Pet Mascot for Pomodoro Focus Engine
//  CYD SmartDesk V2 — Multi-Character Companion System
// ═══════════════════════════════════════════════════════════════════════
#include "DeskPet.h"
#include "Colors.h"
#include <Preferences.h>
#include <math.h>

DeskPet deskPet;

DeskPet::DeskPet() : _type(PET_TOMATO) {}

void DeskPet::begin() {
    _loadSettings();
}

void DeskPet::setType(DeskPetType type) {
    if (type >= PET_COUNT) type = PET_TOMATO;
    _type = type;
    _saveSettings();
}

void DeskPet::nextPet() {
    uint8_t next = ((uint8_t)_type + 1) % PET_COUNT;
    setType((DeskPetType)next);
}

const char* DeskPet::getName(DeskPetType type) const {
    switch (type) {
        case PET_TOMATO:   return "Tomato";
        case PET_CAT:      return "Tuxedo Cat";
        case PET_SHIBA:    return "Shiba Inu";
        case PET_CAPYBARA: return "Capybara";
        default:           return "Tomato";
    }
}

const char* DeskPet::getIcon(DeskPetType type) const {
    switch (type) {
        case PET_TOMATO:   return "🍅";
        case PET_CAT:      return "🐱";
        case PET_SHIBA:    return "🐕";
        case PET_CAPYBARA: return "🦫";
        default:           return "🍅";
    }
}

void DeskPet::_loadSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_pet", true)) {
        uint8_t val = prefs.getUChar("type", (uint8_t)PET_TOMATO);
        if (val < PET_COUNT) {
            _type = (DeskPetType)val;
        } else {
            _type = PET_TOMATO;
        }
        prefs.end();
    }
}

void DeskPet::_saveSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_pet", false)) {
        prefs.putUChar("type", (uint8_t)_type);
        prefs.end();
    }
}

void DeskPet::render(TFT_eSprite& spr, int16_t cx, int16_t cy,
                     uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                     uint16_t bgColor) {
    switch (_type) {
        case PET_TOMATO:
            _drawTomato(spr, cx, cy, (pomoMode == 0), bgColor);
            break;
        case PET_CAT:
            _drawCat(spr, cx, cy, pomoState, pomoMode, remainingSec, bgColor);
            break;
        case PET_SHIBA:
            _drawShiba(spr, cx, cy, pomoState, pomoMode, remainingSec, bgColor);
            break;
        case PET_CAPYBARA:
            _drawCapybara(spr, cx, cy, pomoState, pomoMode, remainingSec, bgColor);
            break;
        default:
            _drawTomato(spr, cx, cy, (pomoMode == 0), bgColor);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Quả Cà Chua (Classic Pomodoro Mascot)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawTomato(TFT_eSprite& spr, int16_t cx, int16_t cy, bool isFocus, uint16_t bgColor) {
    // 1. Bóng đổ mềm mại
    spr.fillSmoothCircle(cx - 9, cy + 27, 7, RGB565(15, 20, 30), bgColor);
    spr.fillSmoothCircle(cx + 9, cy + 27, 7, RGB565(15, 20, 30), bgColor);
    spr.fillRect(cx - 9, cy + 22, 18, 9, RGB565(15, 20, 30));

    // 2. Thân quả cà chua mập mạp (Đỏ tươi)
    uint16_t bodyColor = RGB565(255, 30, 0);
    spr.fillSmoothCircle(cx - 12, cy + 1, 26, bodyColor, bgColor);
    spr.fillSmoothCircle(cx + 12, cy + 1, 26, bodyColor, bgColor);
    spr.fillSmoothCircle(cx, cy + 5, 25, bodyColor, bgColor);
    spr.fillRect(cx - 12, cy - 18, 24, 40, bodyColor);

    // 3. Vệt sáng phản chiếu
    spr.fillSmoothCircle(cx - 15, cy - 8, 5, RGB565(255, 120, 80), bodyColor);
    spr.fillSmoothCircle(cx - 10, cy - 13, 3, RGB565(255, 160, 120), bodyColor);

    // 4. Cuống và đài lá xanh biếc
    uint16_t stemColor = RGB565(20, 110, 40);
    uint16_t leafColor = RGB565(35, 190, 60);
    spr.fillRoundRect(cx - 3, cy - 29, 6, 11, 2, stemColor);
    spr.fillTriangle(cx - 1, cy - 18, cx - 18, cy - 25, cx - 4, cy - 18, leafColor);
    spr.fillTriangle(cx + 1, cy - 18, cx + 18, cy - 25, cx + 4, cy - 18, leafColor);
    spr.fillTriangle(cx - 8, cy - 18, cx + 8, cy - 18, cx, cy - 10, RGB565(50, 215, 85));

    // 5. Khuôn mặt anime dễ thương
    uint16_t eyeColor = RGB565(20, 20, 28);
    if (isFocus) {
        spr.fillSmoothCircle(cx - 13, cy + 3, 5, eyeColor, bodyColor);
        spr.drawPixel(cx - 14, cy + 2, C_WHITE);
        spr.drawPixel(cx - 13, cy + 2, C_WHITE);

        spr.fillSmoothCircle(cx + 13, cy + 3, 5, eyeColor, bodyColor);
        spr.drawPixel(cx + 12, cy + 2, C_WHITE);
        spr.drawPixel(cx + 13, cy + 2, C_WHITE);

        spr.drawPixel(cx - 4, cy + 12, RGB565(55, 15, 15));
        spr.drawPixel(cx - 3, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx - 2, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx - 1, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx,     cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx + 1, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx + 2, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx + 3, cy + 13, RGB565(55, 15, 15));
        spr.drawPixel(cx + 4, cy + 12, RGB565(55, 15, 15));
    } else {
        spr.drawLine(cx - 16, cy + 5, cx - 11, cy, RGB565(45, 15, 15));
        spr.drawLine(cx - 11, cy, cx - 6, cy + 5, RGB565(45, 15, 15));
        spr.drawLine(cx + 6, cy + 5, cx + 11, cy, RGB565(45, 15, 15));
        spr.drawLine(cx + 11, cy, cx + 16, cy + 5, RGB565(45, 15, 15));
        spr.drawFastHLine(cx - 4, cy + 13, 9, RGB565(55, 15, 15));
    }

    // 6. Đôi má hồng
    spr.fillSmoothCircle(cx - 22, cy + 8, 4, RGB565(255, 95, 60), bodyColor);
    spr.fillSmoothCircle(cx + 22, cy + 8, 4, RGB565(255, 95, 60), bodyColor);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Mèo Tuxedo Pixel (Pixel Cat Mascot)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawCat(TFT_eSprite& spr, int16_t cx, int16_t cy,
                       uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                       uint16_t bgColor) {
    uint16_t furDark = RGB565(32, 36, 46);       // Đen than Tuxedo
    uint16_t furWhite = RGB565(250, 252, 255);    // Trắng tinh
    uint16_t earPink = RGB565(255, 160, 185);     // Hồng pastel
    uint16_t nosePink = RGB565(255, 120, 145);

    // 1. Bóng đổ
    spr.fillSmoothCircle(cx, cy + 26, 18, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai mèo nhọn xinh xắn
    // Tai trái
    spr.fillTriangle(cx - 18, cy - 8, cx - 13, cy - 28, cx - 4, cy - 14, furDark);
    spr.fillTriangle(cx - 16, cy - 9, cx - 13, cy - 24, cx - 6, cy - 14, earPink);
    // Tai phải
    spr.fillTriangle(cx + 4, cy - 14, cx + 13, cy - 28, cx + 18, cy - 8, furDark);
    spr.fillTriangle(cx + 6, cy - 14, cx + 13, cy - 24, cx + 16, cy - 9, earPink);

    // 3. Đầu mèo tròn mập
    spr.fillSmoothCircle(cx, cy - 3, 19, furDark, bgColor);

    // 4. Mõm & yếm ngực trắng
    spr.fillSmoothCircle(cx - 5, cy + 5, 8, furWhite, furDark);
    spr.fillSmoothCircle(cx + 5, cy + 5, 8, furWhite, furDark);

    // 5. Thân mèo ngồi
    spr.fillRoundRect(cx - 14, cy + 9, 28, 17, 7, furDark);
    spr.fillRoundRect(cx - 6, cy + 9, 12, 14, 4, furWhite);

    // Mũi tam giác hồng
    spr.fillTriangle(cx - 2, cy + 2, cx + 2, cy + 2, cx, cy + 5, nosePink);

    // Râu mèo trắng mảnh
    spr.drawFastHLine(cx - 20, cy + 4, 7, furWhite);
    spr.drawFastHLine(cx - 21, cy + 7, 7, furWhite);
    spr.drawFastHLine(cx + 14, cy + 4, 7, furWhite);
    spr.drawFastHLine(cx + 15, cy + 7, 7, furWhite);

    // Đuôi mèo bên trái uốn cong
    spr.drawCircle(cx - 16, cy + 18, 5, furDark);
    spr.fillCircle(cx - 18, cy + 15, 2, furWhite); // chóp đuôi trắng

    // 6. Xử lý biểu cảm và đạo cụ theo trạng thái Pomodoro
    if (pomoState == 0) {
        // ── Trạng thái NGỦ (Stopped) ──
        // Đôi mắt nhắm cong thư giãn
        spr.drawFastHLine(cx - 12, cy - 3, 6, RGB565(15, 18, 24));
        spr.drawFastHLine(cx + 6,  cy - 3, 6, RGB565(15, 18, 24));

        // Chữ "Z z" bay lên
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(80, 210, 255));
        spr.drawString("Z", cx + 20, cy - 20, 2);
        spr.drawString("z", cx + 27, cy - 27, 1);
    } else if (pomoState == 2) {
        // ── Trạng thái TẠM DỪNG (Paused) ──
        // Mắt tròn ngơ ngác nhìn người dùng
        spr.fillSmoothCircle(cx - 8, cy - 4, 4, RGB565(30, 215, 140), furDark);
        spr.fillSmoothCircle(cx + 8, cy - 4, 4, RGB565(30, 215, 140), furDark);
        spr.drawPixel(cx - 9, cy - 5, C_WHITE);
        spr.drawPixel(cx + 7, cy - 5, C_WHITE);

        // Dấu "?" màu vàng
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("?", cx + 18, cy - 22, 2);
    } else {
        // ── Trạng thái ĐANG CHẠY (Running) ──
        if (pomoMode == 0) {
            // Focus: Đang cày cuốc cùng laptop
            // Mắt to tròn long lanh tập trung
            spr.fillSmoothCircle(cx - 8, cy - 4, 4, RGB565(35, 210, 140), furDark);
            spr.fillSmoothCircle(cx + 8, cy - 4, 4, RGB565(35, 210, 140), furDark);
            spr.fillCircle(cx - 8, cy - 4, 2, RGB565(15, 20, 28));
            spr.fillCircle(cx + 8, cy - 4, 2, RGB565(15, 20, 28));
            spr.drawPixel(cx - 9, cy - 5, C_WHITE);
            spr.drawPixel(cx + 7, cy - 5, C_WHITE);

            // Laptop mini
            int16_t lapX = cx - 15, lapY = cy + 12;
            spr.fillRoundRect(lapX, lapY + 6, 30, 9, 2, RGB565(160, 175, 195)); // bàn phím
            spr.fillRoundRect(lapX + 2, lapY, 26, 7, 1, RGB565(40, 50, 70));     // màn hình
            spr.fillRoundRect(lapX + 3, lapY + 1, 24, 5, 1, RGB565(70, 210, 255)); // màn hình phát sáng

            // Hai chân mèo múp gõ phím luân phiên theo nhịp giây
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 6, lapY + 6 - (step ? 2 : 0), 3, furWhite, furDark);
            spr.fillSmoothCircle(cx + 6, lapY + 6 - (!step ? 2 : 0), 3, furWhite, furDark);
        } else {
            // Break: Cầm ly trà sữa xả hơi
            // Mắt híp cười ( ^ ^ )
            spr.drawLine(cx - 11, cy - 2, cx - 8, cy - 6, furDark);
            spr.drawLine(cx - 8, cy - 6, cx - 5, cy - 2, furDark);
            spr.drawLine(cx + 5, cy - 2, cx + 8, cy - 6, furDark);
            spr.drawLine(cx + 8, cy - 6, cx + 11, cy - 2, furDark);

            // Ly trà sữa boba
            spr.fillRoundRect(cx - 5, cy + 11, 10, 14, 2, RGB565(240, 210, 175));
            spr.fillRect(cx - 6, cy + 9, 12, 3, RGB565(255, 120, 140)); // nắp
            spr.drawFastVLine(cx, cy + 4, 6, RGB565(100, 220, 140));    // ống hút
            // Hạt trân châu
            spr.drawPixel(cx - 2, cy + 20, RGB565(40, 25, 20));
            spr.drawPixel(cx + 2, cy + 20, RGB565(40, 25, 20));
            spr.drawPixel(cx,     cy + 18, RGB565(40, 25, 20));

            // Trái tim nhỏ màu đỏ
            spr.fillCircle(cx + 17, cy - 16, 2, RGB565(255, 60, 90));
            spr.fillCircle(cx + 21, cy - 16, 2, RGB565(255, 60, 90));
            spr.fillTriangle(cx + 15, cy - 15, cx + 23, cy - 15, cx + 19, cy - 11, RGB565(255, 60, 90));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Chó Shiba Inu Pixel (Pixel Shiba Mascot)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawShiba(TFT_eSprite& spr, int16_t cx, int16_t cy,
                         uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                         uint16_t bgColor) {
    uint16_t furOrange = RGB565(240, 148, 42);     // Vàng cam Shiba ấm áp
    uint16_t furWhite  = RGB565(250, 246, 238);    // Trắng kem
    uint16_t earInner  = RGB565(250, 180, 170);    // Lòng tai hồng phấn

    // 1. Bóng đổ
    spr.fillSmoothCircle(cx, cy + 26, 18, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai tam giác vểnh
    spr.fillTriangle(cx - 18, cy - 7, cx - 13, cy - 27, cx - 4, cy - 14, furOrange);
    spr.fillTriangle(cx - 16, cy - 8, cx - 13, cy - 23, cx - 6, cy - 14, earInner);

    spr.fillTriangle(cx + 4, cy - 14, cx + 13, cy - 27, cx + 18, cy - 7, furOrange);
    spr.fillTriangle(cx + 6, cy - 14, cx + 13, cy - 23, cx + 16, cy - 8, earInner);

    // 3. Đầu tròn Shiba
    spr.fillSmoothCircle(cx, cy - 2, 20, furOrange, bgColor);

    // 4. Má trắng phúng phính đặc trưng Shiba
    spr.fillSmoothCircle(cx - 8, cy + 4, 10, furWhite, furOrange);
    spr.fillSmoothCircle(cx + 8, cy + 4, 10, furWhite, furOrange);
    spr.fillRoundRect(cx - 10, cy + 3, 20, 13, 5, furWhite);

    // 5. Thân & chân
    spr.fillRoundRect(cx - 14, cy + 10, 28, 16, 6, furOrange);
    spr.fillRoundRect(cx - 5, cy + 10, 10, 14, 3, furWhite);

    // Hai đốm lông mày tròn trắng trên mắt (đặc trưng Shiba Inu)
    spr.fillCircle(cx - 7, cy - 11, 2, furWhite);
    spr.fillCircle(cx + 7, cy - 11, 2, furWhite);

    // Mũi tam giác đen
    spr.fillTriangle(cx - 2, cy + 1, cx + 2, cy + 1, cx, cy + 4, RGB565(20, 20, 25));

    // Lưỡi hồng thè nhẹ siêu đáng yêu
    spr.fillSmoothCircle(cx, cy + 8, 2, RGB565(255, 110, 130), furWhite);

    // Đuôi cuộn xoắn bên phải
    spr.fillCircle(cx + 17, cy + 16, 4, furOrange);
    spr.fillCircle(cx + 17, cy + 14, 2, furWhite);

    // 6. Trạng thái Pomodoro
    if (pomoState == 0) {
        // Ngủ: Mắt nhắm tít hạnh phúc ( ˘ω˘ )
        spr.drawFastHLine(cx - 12, cy - 3, 6, RGB565(35, 25, 20));
        spr.drawFastHLine(cx + 6,  cy - 3, 6, RGB565(35, 25, 20));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 185, 45));
        spr.drawString("Z", cx + 20, cy - 20, 2);
        spr.drawString("z", cx + 27, cy - 27, 1);
    } else if (pomoState == 2) {
        // Tạm dừng: Nghiêng đầu dấu hỏi
        spr.fillSmoothCircle(cx - 8, cy - 3, 4, RGB565(35, 20, 15), furOrange);
        spr.fillSmoothCircle(cx + 8, cy - 3, 4, RGB565(35, 20, 15), furOrange);
        spr.drawPixel(cx - 9, cy - 4, C_WHITE);
        spr.drawPixel(cx + 7, cy - 4, C_WHITE);

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("?", cx + 19, cy - 22, 2);
    } else {
        // Đang chạy
        if (pomoMode == 0) {
            // Focus: Gõ laptop mini
            spr.fillSmoothCircle(cx - 8, cy - 3, 4, RGB565(35, 20, 15), furOrange);
            spr.fillSmoothCircle(cx + 8, cy - 3, 4, RGB565(35, 20, 15), furOrange);
            spr.drawPixel(cx - 9, cy - 4, C_WHITE);
            spr.drawPixel(cx + 7, cy - 4, C_WHITE);

            // Khăn quàng cổ màu đỏ (Bandana)
            spr.fillTriangle(cx - 7, cy + 8, cx + 7, cy + 8, cx, cy + 13, RGB565(240, 40, 50));

            // Laptop xanh navy
            int16_t lapX = cx - 15, lapY = cy + 13;
            spr.fillRoundRect(lapX, lapY + 5, 30, 9, 2, RGB565(45, 60, 85));
            spr.fillRoundRect(lapX + 2, lapY, 26, 6, 1, RGB565(25, 35, 55));
            spr.fillRoundRect(lapX + 3, lapY + 1, 24, 4, 1, RGB565(245, 185, 45)); // màn hình sáng vàng

            // Chân gõ luân phiên
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 6, lapY + 5 - (step ? 2 : 0), 3, furWhite, furOrange);
            spr.fillSmoothCircle(cx + 6, lapY + 5 - (!step ? 2 : 0), 3, furWhite, furOrange);
        } else {
            // Break: Cười toe toét
            spr.drawLine(cx - 11, cy - 1, cx - 8, cy - 5, RGB565(35, 20, 15));
            spr.drawLine(cx - 8, cy - 5, cx - 5, cy - 1, RGB565(35, 20, 15));
            spr.drawLine(cx + 5, cy - 1, cx + 8, cy - 5, RGB565(35, 20, 15));
            spr.drawLine(cx + 8, cy - 5, cx + 11, cy - 1, RGB565(35, 20, 15));

            // Cốc nước cam có lát chanh
            spr.fillRoundRect(cx - 5, cy + 11, 10, 14, 2, RGB565(255, 160, 40));
            spr.drawFastVLine(cx + 1, cy + 4, 7, RGB565(40, 210, 120));

            // Ngôi sao vàng nhỏ
            spr.drawString("*", cx + 18, cy - 15, 2);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Chuột lang nước Capybara (Pixel Capybara Mascot)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawCapybara(TFT_eSprite& spr, int16_t cx, int16_t cy,
                            uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                            uint16_t bgColor) {
    uint16_t furBrown = RGB565(165, 115, 65);       // Nâu quế Capybara
    uint16_t muzzleDark = RGB565(120, 78, 40);      // Mõm nâu sẫm
    uint16_t orangeCol = RGB565(255, 135, 15);      // Quả cam trên đầu
    uint16_t leafGreen = RGB565(40, 185, 55);

    // 1. Bóng đổ
    spr.fillSmoothCircle(cx, cy + 26, 20, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai tròn nhỏ hai bên đầu
    spr.fillSmoothCircle(cx - 17, cy - 10, 4, muzzleDark, bgColor);
    spr.fillSmoothCircle(cx + 17, cy - 10, 4, muzzleDark, bgColor);

    // 3. Đầu & Thân hộp tròn đặc trưng Capybara
    spr.fillSmoothRoundRect(cx - 18, cy - 12, 36, 36, 12, furBrown, bgColor);

    // 4. Mõm to vuông vức
    spr.fillSmoothRoundRect(cx - 11, cy - 2, 22, 20, 7, muzzleDark, furBrown);

    // Hai lỗ mũi nhỏ
    spr.drawPixel(cx - 3, cy + 7, RGB565(35, 20, 10));
    spr.drawPixel(cx + 3, cy + 7, RGB565(35, 20, 10));

    // 5. ĐẶC TRƯNG HUYỀN THOẠI: Quả cam thăng bằng trên đỉnh đầu!
    spr.fillSmoothCircle(cx, cy - 18, 6, orangeCol, bgColor);
    spr.fillTriangle(cx, cy - 24, cx + 5, cy - 27, cx + 3, cy - 23, leafGreen);

    // 6. Trạng thái Pomodoro
    if (pomoState == 0) {
        // Ngủ siêu chill: mắt một mí nhắm ngang
        spr.drawFastHLine(cx - 13, cy - 5, 5, RGB565(40, 25, 15));
        spr.drawFastHLine(cx + 8,  cy - 5, 5, RGB565(40, 25, 15));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(orangeCol);
        spr.drawString("Z", cx + 21, cy - 18, 2);
        spr.drawString("z", cx + 27, cy - 25, 1);
    } else if (pomoState == 2) {
        // Tạm dừng: Mắt mở hé
        spr.fillRect(cx - 13, cy - 6, 5, 2, RGB565(30, 15, 10));
        spr.fillRect(cx + 8,  cy - 6, 5, 2, RGB565(30, 15, 10));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("...", cx + 21, cy - 20, 2);
    } else {
        // Đang chạy
        if (pomoMode == 0) {
            // Focus: Ngồi gõ phím với vẻ mặt bình thản không chút âu lo
            spr.fillRect(cx - 13, cy - 6, 5, 2, RGB565(30, 15, 10));
            spr.fillRect(cx + 8,  cy - 6, 5, 2, RGB565(30, 15, 10));

            // Laptop màu xám bạc
            int16_t lapX = cx - 15, lapY = cy + 12;
            spr.fillRoundRect(lapX, lapY + 6, 30, 9, 2, RGB565(140, 145, 155));
            spr.fillRoundRect(lapX + 2, lapY, 26, 7, 1, RGB565(60, 65, 75));
            spr.fillRoundRect(lapX + 3, lapY + 1, 24, 5, 1, RGB565(100, 230, 180)); // màn hình mint green

            // Chân trước nâu gõ phím
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 6, lapY + 6 - (step ? 2 : 0), 3, muzzleDark, furBrown);
            spr.fillSmoothCircle(cx + 6, lapY + 6 - (!step ? 2 : 0), 3, muzzleDark, furBrown);
        } else {
            // Break: Ngâm mình onsen chill thư thái
            spr.drawFastHLine(cx - 13, cy - 5, 5, RGB565(40, 25, 15));
            spr.drawFastHLine(cx + 8,  cy - 5, 5, RGB565(40, 25, 15));

            // Bồn tắm gỗ viền quanh đáy
            spr.fillRoundRect(cx - 20, cy + 14, 40, 12, 3, RGB565(135, 90, 50));
            spr.drawFastHLine(cx - 18, cy + 16, 36, RGB565(80, 50, 25));

            // Làn khói hơi nước bốc lên nhẹ nhàng
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(200, 225, 255));
            spr.drawString("~", cx - 16, cy - 16, 2);
            spr.drawString("~", cx + 16, cy - 16, 2);
        }
    }
}
