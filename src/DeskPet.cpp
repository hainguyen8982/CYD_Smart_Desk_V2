// ═══════════════════════════════════════════════════════════════════════
//  DeskPet.cpp — Virtual Desk Pet Mascot for Pomodoro Focus Engine
//  CYD SmartDesk V2 — Multi-Character Companion System (HD Chibi Bitmaps)
// ═══════════════════════════════════════════════════════════════════════
#include "DeskPet.h"
#include "PetBitmaps.h"
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
    const uint16_t* bmp = getPetBitmap(_type);
    if (!bmp) return;

    // ── 1. MICRO-BREATHING ANIMATION (Nhịp thở tĩnh lặng, siêu êm) ──
    // pomoState: 0=Stopped/Idle, 1=Running, 2=Paused
    // pomoMode: 0=Focus, 1=Short Break, 2=Long Break
    int16_t offsetY = 0;
    if (pomoState == 1) { // Đang chạy (Running)
        if (pomoMode == 0) {
            // Focus: Nhịp thở nhấp nhô 1.5px theo chu kỳ 2.2 giây
            float phase = (float)(millis() % 2200) / 2200.0f * 6.2831853f;
            offsetY = (int16_t)roundf(sinf(phase) * 1.5f);
        } else {
            // Break time: Nhịp thở chậm rãi, thư thái (chu kỳ 3.0s)
            float phase = (float)(millis() % 3000) / 3000.0f * 6.2831853f;
            offsetY = (int16_t)roundf(sinf(phase) * 2.0f);
        }
    } else if (pomoState == 2) {
        // Paused: Đóng băng bất động như đang chờ lệnh
        offsetY = 0;
    } else {
        // Idle: Nhịp thở nghỉ ngơi rất chậm (3.5s)
        float phase = (float)(millis() % 3500) / 3500.0f * 6.2831853f;
        offsetY = (int16_t)roundf(sinf(phase) * 1.0f);
    }

    int16_t startX = cx - PET_BITMAP_W / 2;
    int16_t startY = cy - PET_BITMAP_H / 2 + offsetY;

    for (int r = 0; r < PET_BITMAP_H; r++) {
        int16_t py = startY + r;
        if (py < 0 || py >= 240) continue;
        int rowIdx = r * PET_BITMAP_W;
        for (int c = 0; c < PET_BITMAP_W; c++) {
            int16_t px = startX + c;
            if (px < 0 || px >= 320) continue;
            uint16_t color = pgm_read_word(&bmp[rowIdx + c]);
            if (color != PET_TRANSPARENT_COLOR) {
                spr.drawPixel(px, py, color);
            }
        }
    }

    // ── 2. STATE EMOTE: Bong bóng Zzz trôi nhẹ khi đang nghỉ ngơi (Break mode) ──
    if (pomoMode != 0 && pomoState == 1) {
        uint32_t t = millis() % 2500;
        int16_t zY1 = (cy - 20) - (int16_t)(t * 20 / 2500);
        int16_t zX1 = cx + 24 + (int16_t)(sinf(t * 0.005f) * 2.0f);
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(RGB565(170, 205, 255), bgColor);
        spr.drawString("z", zX1, zY1, 2);

        if (t > 900) {
            uint32_t t2 = t - 900;
            int16_t zY2 = (cy - 16) - (int16_t)(t2 * 22 / 2500);
            int16_t zX2 = cx + 32 + (int16_t)(cosf(t2 * 0.006f) * 2.0f);
            spr.drawString("z", zX2, zY2, 1);
        }
    }
}
