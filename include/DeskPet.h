// ═══════════════════════════════════════════════════════════════════════
//  DeskPet.h — Virtual Desk Pet Mascot for Pomodoro Focus Engine
//  CYD SmartDesk V2 — Multi-Character Companion System
// ═══════════════════════════════════════════════════════════════════════
#ifndef DESK_PET_H
#define DESK_PET_H

#include <Arduino.h>
#include <TFT_eSPI.h>

enum DeskPetType : uint8_t {
    PET_TOMATO   = 0,   // Quả Cà Chua Pomodoro truyền thống
    PET_CAT      = 1,   // Mèo Tuxedo Pixel gõ phím
    PET_SHIBA    = 2,   // Chó Shiba Inu năng động
    PET_CAPYBARA = 3,   // Chuột lang Capybara siêu chill
    PET_COUNT    = 4
};

class DeskPet {
public:
    DeskPet();

    void begin();
    DeskPetType getType() const { return _type; }
    void setType(DeskPetType type);
    void nextPet();

    const char* getName(DeskPetType type) const;
    const char* getNameCurrent() const { return getName(_type); }
    const char* getIcon(DeskPetType type) const;

    void render(TFT_eSprite& spr, int16_t cx, int16_t cy,
                uint8_t pomoState, uint8_t pomoMode, uint16_t remainingSec,
                uint16_t bgColor);

private:
    void _loadSettings();
    void _saveSettings();

    DeskPetType _type;
};

extern DeskPet deskPet;

#endif // DESK_PET_H
