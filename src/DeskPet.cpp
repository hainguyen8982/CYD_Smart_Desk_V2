// ═══════════════════════════════════════════════════════════════════════
//  DeskPet.cpp — Virtual Desk Pet Mascot for Pomodoro Focus Engine
//  CYD SmartDesk V2 — Multi-Character Companion System (HD Chibi Bitmaps)
// ═══════════════════════════════════════════════════════════════════════
#include "DeskPet.h"
#include "PetBitmaps.h"
#include "Colors.h"
#include <Preferences.h>

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

    int16_t startX = cx - PET_BITMAP_W / 2;
    int16_t startY = cy - PET_BITMAP_H / 2;

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
}
