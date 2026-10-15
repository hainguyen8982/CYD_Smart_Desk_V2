// ═══════════════════════════════════════════════════════════════════════
//  DeskPet.cpp — Virtual Desk Pet Mascot for Pomodoro Focus Engine
//  CYD SmartDesk V2 — Multi-Character Companion System (Large HD Sprites)
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
// 1. Quả Cà Chua (Classic Pomodoro Mascot — Phóng lớn đẫy đà)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawTomato(TFT_eSprite& spr, int16_t cx, int16_t cy, bool isFocus, uint16_t bgColor) {
    // 1. Bóng đổ mềm mại
    spr.fillSmoothCircle(cx - 12, cy + 30, 9, RGB565(15, 20, 30), bgColor);
    spr.fillSmoothCircle(cx + 12, cy + 30, 9, RGB565(15, 20, 30), bgColor);
    spr.fillRect(cx - 12, cy + 24, 24, 12, RGB565(15, 20, 30));

    // 2. Thân quả cà chua mập mạp (Đỏ tươi Rực rỡ, ĐK ~60px)
    uint16_t bodyColor = RGB565(255, 30, 0);
    spr.fillSmoothCircle(cx - 14, cy + 1, 29, bodyColor, bgColor);
    spr.fillSmoothCircle(cx + 14, cy + 1, 29, bodyColor, bgColor);
    spr.fillSmoothCircle(cx, cy + 6, 28, bodyColor, bgColor);
    spr.fillRect(cx - 14, cy - 20, 28, 44, bodyColor);

    // 3. Vệt sáng phản chiếu (Highlights)
    spr.fillSmoothCircle(cx - 18, cy - 9, 6, RGB565(255, 125, 85), bodyColor);
    spr.fillSmoothCircle(cx - 12, cy - 15, 4, RGB565(255, 165, 125), bodyColor);

    // 4. Cuống và đài lá xanh biếc
    uint16_t stemColor = RGB565(20, 110, 40);
    uint16_t leafColor = RGB565(35, 195, 65);
    spr.fillRoundRect(cx - 4, cy - 33, 8, 14, 3, stemColor);
    spr.fillTriangle(cx - 2, cy - 20, cx - 22, cy - 28, cx - 5, cy - 20, leafColor);
    spr.fillTriangle(cx + 2, cy - 20, cx + 22, cy - 28, cx + 5, cy - 20, leafColor);
    spr.fillTriangle(cx - 10, cy - 20, cx + 10, cy - 20, cx, cy - 11, RGB565(55, 220, 90));

    // 5. Khuôn mặt anime dễ thương
    uint16_t eyeColor = RGB565(20, 20, 28);
    if (isFocus) {
        spr.fillSmoothCircle(cx - 15, cy + 3, 6, eyeColor, bodyColor);
        spr.drawPixel(cx - 16, cy + 2, C_WHITE);
        spr.drawPixel(cx - 15, cy + 2, C_WHITE);

        spr.fillSmoothCircle(cx + 15, cy + 3, 6, eyeColor, bodyColor);
        spr.drawPixel(cx + 14, cy + 2, C_WHITE);
        spr.drawPixel(cx + 15, cy + 2, C_WHITE);

        spr.drawPixel(cx - 5, cy + 14, RGB565(55, 15, 15));
        spr.drawPixel(cx - 4, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx - 3, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx - 2, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx - 1, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx,     cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx + 1, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx + 2, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx + 3, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx + 4, cy + 15, RGB565(55, 15, 15));
        spr.drawPixel(cx + 5, cy + 14, RGB565(55, 15, 15));
    } else {
        spr.drawLine(cx - 19, cy + 5, cx - 13, cy - 1, RGB565(45, 15, 15));
        spr.drawLine(cx - 13, cy - 1, cx - 7, cy + 5, RGB565(45, 15, 15));
        spr.drawLine(cx + 7, cy + 5, cx + 13, cy - 1, RGB565(45, 15, 15));
        spr.drawLine(cx + 13, cy - 1, cx + 19, cy + 5, RGB565(45, 15, 15));
        spr.drawFastHLine(cx - 5, cy + 14, 11, RGB565(55, 15, 15));
    }

    // 6. Đôi má hồng phúng phính
    spr.fillSmoothCircle(cx - 25, cy + 8, 5, RGB565(255, 95, 60), bodyColor);
    spr.fillSmoothCircle(cx + 25, cy + 8, 5, RGB565(255, 95, 60), bodyColor);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Mèo Tuxedo Pixel (Phóng lớn ~56×70px, chi tiết laptop sắc nét)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawCat(TFT_eSprite& spr, int16_t cx, int16_t cy,
                       uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                       uint16_t bgColor) {
    uint16_t furDark = RGB565(30, 34, 44);       // Đen than Tuxedo
    uint16_t furWhite = RGB565(250, 252, 255);    // Trắng tuyết
    uint16_t earPink = RGB565(255, 160, 185);     // Hồng phấn
    uint16_t nosePink = RGB565(255, 120, 145);

    // 1. Bóng đổ mềm mại
    spr.fillSmoothCircle(cx, cy + 28, 24, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai mèo lớn sắc nét
    // Tai trái
    spr.fillTriangle(cx - 24, cy - 8, cx - 17, cy - 35, cx - 6, cy - 16, furDark);
    spr.fillTriangle(cx - 21, cy - 10, cx - 17, cy - 30, cx - 9, cy - 16, earPink);
    // Tai phải
    spr.fillTriangle(cx + 6, cy - 16, cx + 17, cy - 35, cx + 24, cy - 8, furDark);
    spr.fillTriangle(cx + 9, cy - 16, cx + 17, cy - 30, cx + 21, cy - 10, earPink);

    // 3. Đầu mèo tròn lớn (Bán kính 24px)
    spr.fillSmoothCircle(cx, cy - 5, 24, furDark, bgColor);

    // 4. Má và yếm ngực trắng phồng
    spr.fillSmoothCircle(cx - 7, cy + 6, 11, furWhite, furDark);
    spr.fillSmoothCircle(cx + 7, cy + 6, 11, furWhite, furDark);

    // 5. Thân mèo ngồi vững chãi
    spr.fillRoundRect(cx - 19, cy + 10, 38, 22, 9, furDark);
    spr.fillRoundRect(cx - 8, cy + 10, 16, 18, 5, furWhite);

    // Mũi tam giác hồng
    spr.fillTriangle(cx - 3, cy + 2, cx + 3, cy + 2, cx, cy + 6, nosePink);

    // Râu mèo trắng mảnh dài
    spr.drawFastHLine(cx - 27, cy + 4, 10, furWhite);
    spr.drawFastHLine(cx - 28, cy + 8, 10, furWhite);
    spr.drawFastHLine(cx + 18, cy + 4, 10, furWhite);
    spr.drawFastHLine(cx + 19, cy + 8, 10, furWhite);

    // Đuôi mèo bên trái uốn cong mềm mại
    spr.drawCircle(cx - 22, cy + 22, 6, furDark);
    spr.fillCircle(cx - 25, cy + 18, 3, furWhite); // chóp đuôi trắng

    // 6. Xử lý biểu cảm và đạo cụ theo trạng thái Pomodoro
    if (pomoState == 0) {
        // ── Trạng thái NGỦ (Stopped) ──
        // Mắt nhắm cong thư giãn
        spr.drawFastHLine(cx - 16, cy - 5, 8, RGB565(15, 18, 24));
        spr.drawFastHLine(cx + 8,  cy - 5, 8, RGB565(15, 18, 24));

        // Chữ "Z z" bay lên
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(80, 215, 255));
        spr.drawString("Z", cx + 24, cy - 24, 2);
        spr.drawString("z", cx + 33, cy - 31, 1);
    } else if (pomoState == 2) {
        // ── Trạng thái TẠM DỪNG (Paused) ──
        // Mắt tròn ngơ ngác nhìn bạn
        spr.fillSmoothCircle(cx - 10, cy - 6, 5, RGB565(30, 215, 140), furDark);
        spr.fillSmoothCircle(cx + 10, cy - 6, 5, RGB565(30, 215, 140), furDark);
        spr.drawPixel(cx - 11, cy - 7, C_WHITE);
        spr.drawPixel(cx + 9, cy - 7, C_WHITE);

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("?", cx + 24, cy - 25, 4);
    } else {
        // ── Trạng thái ĐANG CHẠY (Running) ──
        if (pomoMode == 0) {
            // Focus: Đang cày cuốc cùng laptop
            // Mắt to tròn long lanh tập trung
            spr.fillSmoothCircle(cx - 10, cy - 6, 6, RGB565(35, 210, 140), furDark);
            spr.fillSmoothCircle(cx + 10, cy - 6, 6, RGB565(35, 210, 140), furDark);
            spr.fillCircle(cx - 10, cy - 6, 3, RGB565(15, 20, 28));
            spr.fillCircle(cx + 10, cy - 6, 3, RGB565(15, 20, 28));
            spr.drawPixel(cx - 11, cy - 8, C_WHITE);
            spr.drawPixel(cx + 9, cy - 8, C_WHITE);

            // Chiếc Laptop mini to rõ (Rộng 40px)
            int16_t lapX = cx - 20, lapY = cy + 12;
            spr.fillRoundRect(lapX, lapY + 8, 40, 12, 3, RGB565(160, 175, 195)); // bàn phím
            spr.fillRoundRect(lapX + 3, lapY, 34, 10, 2, RGB565(40, 50, 70));     // màn hình
            spr.fillRoundRect(lapX + 5, lapY + 1, 30, 8, 1, RGB565(75, 220, 255)); // màn hình phát sáng
            // Logo trái táo nhỏ
            spr.fillCircle(cx, lapY + 4, 1, C_WHITE);

            // Hai chân mèo múp gõ phím luân phiên theo nhịp giây
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 8, lapY + 8 - (step ? 3 : 0), 4, furWhite, furDark);
            spr.fillSmoothCircle(cx + 8, lapY + 8 - (!step ? 3 : 0), 4, furWhite, furDark);
        } else {
            // Break: Cầm ly trà sữa xả hơi
            // Mắt híp cười ( ^ ^ )
            spr.drawLine(cx - 15, cy - 3, cx - 10, cy - 8, furDark);
            spr.drawLine(cx - 10, cy - 8, cx - 5, cy - 3, furDark);
            spr.drawLine(cx + 5, cy - 3, cx + 10, cy - 8, furDark);
            spr.drawLine(cx + 10, cy - 8, cx + 15, cy - 3, furDark);

            // Ly trà sữa boba to hơn
            spr.fillRoundRect(cx - 7, cy + 12, 14, 17, 3, RGB565(240, 210, 175));
            spr.fillRect(cx - 8, cy + 10, 16, 4, RGB565(255, 120, 140)); // nắp
            spr.drawFastVLine(cx, cy + 4, 7, RGB565(100, 225, 145));    // ống hút
            // Trân châu
            spr.fillCircle(cx - 3, cy + 23, 1, RGB565(40, 25, 20));
            spr.fillCircle(cx + 3, cy + 23, 1, RGB565(40, 25, 20));
            spr.fillCircle(cx,     cy + 20, 1, RGB565(40, 25, 20));

            // Trái tim đỏ nhỏ
            spr.fillCircle(cx + 22, cy - 18, 3, RGB565(255, 60, 90));
            spr.fillCircle(cx + 27, cy - 18, 3, RGB565(255, 60, 90));
            spr.fillTriangle(cx + 19, cy - 17, cx + 30, cy - 17, cx + 24, cy - 11, RGB565(255, 60, 90));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Chó Shiba Inu Pixel (Phóng lớn ~58×70px, má phúng phính đáng yêu)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawShiba(TFT_eSprite& spr, int16_t cx, int16_t cy,
                         uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                         uint16_t bgColor) {
    uint16_t furOrange = RGB565(240, 148, 42);     // Vàng cam Shiba ấm áp
    uint16_t furWhite  = RGB565(250, 246, 238);    // Trắng kem
    uint16_t earInner  = RGB565(250, 180, 170);    // Lòng tai hồng phấn

    // 1. Bóng đổ
    spr.fillSmoothCircle(cx, cy + 28, 24, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai tam giác vểnh
    spr.fillTriangle(cx - 24, cy - 8, cx - 17, cy - 35, cx - 6, cy - 16, furOrange);
    spr.fillTriangle(cx - 21, cy - 10, cx - 17, cy - 29, cx - 9, cy - 16, earInner);

    spr.fillTriangle(cx + 6, cy - 16, cx + 17, cy - 35, cx + 24, cy - 8, furOrange);
    spr.fillTriangle(cx + 9, cy - 16, cx + 17, cy - 29, cx + 21, cy - 10, earInner);

    // 3. Đầu tròn Shiba to (Bán kính 25px)
    spr.fillSmoothCircle(cx, cy - 4, 25, furOrange, bgColor);

    // 4. Má trắng phúng phính đặc trưng Shiba
    spr.fillSmoothCircle(cx - 10, cy + 5, 13, furWhite, furOrange);
    spr.fillSmoothCircle(cx + 10, cy + 5, 13, furWhite, furOrange);
    spr.fillRoundRect(cx - 13, cy + 4, 26, 17, 7, furWhite);

    // 5. Thân & chân
    spr.fillRoundRect(cx - 19, cy + 12, 38, 20, 8, furOrange);
    spr.fillRoundRect(cx - 7, cy + 12, 14, 17, 4, furWhite);

    // Hai đốm lông mày tròn trắng trên mắt (đặc trưng Shiba Inu)
    spr.fillCircle(cx - 9, cy - 14, 3, furWhite);
    spr.fillCircle(cx + 9, cy - 14, 3, furWhite);

    // Mũi tam giác đen
    spr.fillTriangle(cx - 3, cy + 2, cx + 3, cy + 2, cx, cy + 6, RGB565(20, 20, 25));

    // Lưỡi hồng thè nhẹ siêu đáng yêu
    spr.fillSmoothCircle(cx, cy + 10, 3, RGB565(255, 110, 130), furWhite);

    // Đuôi cuộn xoắn bên phải
    spr.fillCircle(cx + 22, cy + 20, 5, furOrange);
    spr.fillCircle(cx + 22, cy + 18, 3, furWhite);

    // 6. Trạng thái Pomodoro
    if (pomoState == 0) {
        // Ngủ: Mắt nhắm tít hạnh phúc ( ˘ω˘ )
        spr.drawFastHLine(cx - 16, cy - 5, 8, RGB565(35, 25, 20));
        spr.drawFastHLine(cx + 8,  cy - 5, 8, RGB565(35, 25, 20));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 185, 45));
        spr.drawString("Z", cx + 25, cy - 25, 2);
        spr.drawString("z", cx + 33, cy - 32, 1);
    } else if (pomoState == 2) {
        // Tạm dừng: Nghiêng đầu dấu hỏi
        spr.fillSmoothCircle(cx - 10, cy - 5, 5, RGB565(35, 20, 15), furOrange);
        spr.fillSmoothCircle(cx + 10, cy - 5, 5, RGB565(35, 20, 15), furOrange);
        spr.drawPixel(cx - 11, cy - 6, C_WHITE);
        spr.drawPixel(cx + 9, cy - 6, C_WHITE);

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("?", cx + 24, cy - 26, 4);
    } else {
        // Đang chạy
        if (pomoMode == 0) {
            // Focus: Gõ laptop mini
            spr.fillSmoothCircle(cx - 10, cy - 5, 5, RGB565(35, 20, 15), furOrange);
            spr.fillSmoothCircle(cx + 10, cy - 5, 5, RGB565(35, 20, 15), furOrange);
            spr.drawPixel(cx - 11, cy - 6, C_WHITE);
            spr.drawPixel(cx + 9, cy - 6, C_WHITE);

            // Khăn quàng cổ màu đỏ (Bandana)
            spr.fillTriangle(cx - 10, cy + 9, cx + 10, cy + 9, cx, cy + 16, RGB565(240, 40, 50));

            // Laptop xanh navy rộng 40px
            int16_t lapX = cx - 20, lapY = cy + 14;
            spr.fillRoundRect(lapX, lapY + 6, 40, 11, 3, RGB565(45, 60, 85));
            spr.fillRoundRect(lapX + 3, lapY, 34, 8, 2, RGB565(25, 35, 55));
            spr.fillRoundRect(lapX + 4, lapY + 1, 32, 6, 1, RGB565(245, 185, 45)); // màn hình sáng vàng

            // Chân gõ luân phiên
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 8, lapY + 6 - (step ? 3 : 0), 4, furWhite, furOrange);
            spr.fillSmoothCircle(cx + 8, lapY + 6 - (!step ? 3 : 0), 4, furWhite, furOrange);
        } else {
            // Break: Cười toe toét
            spr.drawLine(cx - 15, cy - 2, cx - 10, cy - 7, RGB565(35, 20, 15));
            spr.drawLine(cx - 10, cy - 7, cx - 5, cy - 2, RGB565(35, 20, 15));
            spr.drawLine(cx + 5, cy - 2, cx + 10, cy - 7, RGB565(35, 20, 15));
            spr.drawLine(cx + 10, cy - 7, cx + 15, cy - 2, RGB565(35, 20, 15));

            // Cốc nước cam có lát chanh
            spr.fillRoundRect(cx - 7, cy + 12, 14, 17, 3, RGB565(255, 160, 40));
            spr.drawFastVLine(cx + 1, cy + 4, 9, RGB565(40, 210, 120));

            // Ngôi sao vàng
            spr.drawString("*", cx + 22, cy - 18, 4);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Chuột lang nước Capybara (Phóng lớn ~62×68px, quả cam to nổi bật)
// ─────────────────────────────────────────────────────────────────────────────
void DeskPet::_drawCapybara(TFT_eSprite& spr, int16_t cx, int16_t cy,
                            uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                            uint16_t bgColor) {
    uint16_t furBrown   = RGB565(165, 115, 65);       // Nâu quế Capybara
    uint16_t muzzleDark = RGB565(120, 78, 40);        // Mõm nâu sẫm
    uint16_t orangeCol  = RGB565(255, 135, 15);       // Quả cam trên đầu
    uint16_t leafGreen  = RGB565(40, 185, 55);

    // 1. Bóng đổ
    spr.fillSmoothCircle(cx, cy + 28, 26, RGB565(15, 20, 30), bgColor);

    // 2. Hai tai tròn nhỏ hai bên đầu
    spr.fillSmoothCircle(cx - 24, cy - 12, 5, muzzleDark, bgColor);
    spr.fillSmoothCircle(cx + 24, cy - 12, 5, muzzleDark, bgColor);

    // 3. Đầu & Thân hộp tròn đặc trưng Capybara (Rộng 50px, Cao 46px)
    spr.fillSmoothRoundRect(cx - 25, cy - 14, 50, 46, 15, furBrown, bgColor);

    // 4. Mõm to vuông vức
    spr.fillSmoothRoundRect(cx - 15, cy - 2, 30, 25, 8, muzzleDark, furBrown);

    // Hai lỗ mũi nhỏ
    spr.drawPixel(cx - 4, cy + 9, RGB565(35, 20, 10));
    spr.drawPixel(cx + 4, cy + 9, RGB565(35, 20, 10));

    // 5. ĐẶC TRƯNG HUYỀN THOẠI: Quả cam thăng bằng trên đỉnh đầu (Phóng lớn nổi bật)
    spr.fillSmoothCircle(cx, cy - 22, 8, orangeCol, bgColor);
    spr.fillTriangle(cx, cy - 29, cx + 7, cy - 33, cx + 4, cy - 28, leafGreen);

    // 6. Trạng thái Pomodoro
    if (pomoState == 0) {
        // Ngủ siêu chill: mắt một mí nhắm ngang
        spr.drawFastHLine(cx - 18, cy - 6, 7, RGB565(40, 25, 15));
        spr.drawFastHLine(cx + 11, cy - 6, 7, RGB565(40, 25, 15));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(orangeCol);
        spr.drawString("Z", cx + 25, cy - 22, 2);
        spr.drawString("z", cx + 33, cy - 30, 1);
    } else if (pomoState == 2) {
        // Tạm dừng: Mắt mở hé
        spr.fillRect(cx - 18, cy - 7, 7, 3, RGB565(30, 15, 10));
        spr.fillRect(cx + 11, cy - 7, 7, 3, RGB565(30, 15, 10));

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(255, 215, 40));
        spr.drawString("...", cx + 26, cy - 24, 2);
    } else {
        // Đang chạy
        if (pomoMode == 0) {
            // Focus: Ngồi gõ phím với vẻ mặt bình thản không chút âu lo
            spr.fillRect(cx - 18, cy - 7, 7, 3, RGB565(30, 15, 10));
            spr.fillRect(cx + 11, cy - 7, 7, 3, RGB565(30, 15, 10));

            // Laptop màu xám bạc rộng 42px
            int16_t lapX = cx - 21, lapY = cy + 14;
            spr.fillRoundRect(lapX, lapY + 7, 42, 12, 3, RGB565(140, 145, 155));
            spr.fillRoundRect(lapX + 3, lapY, 36, 9, 2, RGB565(60, 65, 75));
            spr.fillRoundRect(lapX + 4, lapY + 1, 34, 7, 1, RGB565(100, 230, 180)); // màn hình mint green

            // Chân trước nâu gõ phím
            bool step = (remainingSec % 2 == 0);
            spr.fillSmoothCircle(cx - 8, lapY + 7 - (step ? 3 : 0), 4, muzzleDark, furBrown);
            spr.fillSmoothCircle(cx + 8, lapY + 7 - (!step ? 3 : 0), 4, muzzleDark, furBrown);
        } else {
            // Break: Ngâm mình onsen chill thư thái
            spr.drawFastHLine(cx - 18, cy - 6, 7, RGB565(40, 25, 15));
            spr.drawFastHLine(cx + 11, cy - 6, 7, RGB565(40, 25, 15));

            // Bồn tắm gỗ viền quanh đáy rộng 56px
            spr.fillRoundRect(cx - 28, cy + 16, 56, 16, 4, RGB565(135, 90, 50));
            spr.drawFastHLine(cx - 25, cy + 19, 50, RGB565(80, 50, 25));

            // Làn khói hơi nước bốc lên nhẹ nhàng
            spr.setTextDatum(MC_DATUM);
            spr.setTextColor(RGB565(200, 225, 255));
            spr.drawString("~", cx - 20, cy - 20, 4);
            spr.drawString("~", cx + 20, cy - 20, 4);
        }
    }
}
