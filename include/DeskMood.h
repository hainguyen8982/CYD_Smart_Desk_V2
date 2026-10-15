// ═══════════════════════════════════════════════════════════════════════
//  DeskMood.h — Office Status & Mood Signboard Engine
//  CYD SmartDesk V2 — Social Vibe Billboard for Modern Desks
// ═══════════════════════════════════════════════════════════════════════
#ifndef DESK_MOOD_H
#define DESK_MOOD_H

#include <Arduino.h>
#include <TFT_eSPI.h>

enum MoodType : uint8_t {
    MOOD_HAPPY       = 0,   // 🟢 Vui vẻ / Sẵn sàng chém gió (Ready to chat)
    MOOD_FOCUS       = 1,   // 🔴 Tập trung cao độ / Đừng làm phiền (Deep work / DND)
    MOOD_BUG         = 2,   // ⚡ Bực mình vì bug (Bug hunting)
    MOOD_LOW_BATTERY = 3,   // 🟡 Cạn pin 1% (Low battery / AFK)
    MOOD_CHILL       = 4,   // 🟣 Siêu chill (Keep calm & chill)
    MOOD_TGIF        = 5,   // 🎉 Thứ Sáu rồi (TGIF / Weekend vibe)
    MOOD_COUNT       = 6
};

class DeskMood {
public:
    DeskMood();

    void begin();
    MoodType getMood() const { return _mood; }
    void setMood(MoodType m);
    void nextMood();
    void prevMood();

    const char* getName(MoodType m) const;
    const char* getIcon(MoodType m) const;
    const char* getTag(MoodType m, uint8_t lang) const;
    const char* getMessage(MoodType m, uint8_t lang) const;
    uint16_t    getAccentColor(MoodType m) const;

    void renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg);
    bool handleTouch(int16_t x, int16_t y);

private:
    void _loadSettings();
    void _saveSettings();
    void _syncHardwareLed();

    void _drawAvatar(TFT_eSprite& spr, int16_t cx, int16_t cy, MoodType m, uint16_t cardBg);

    MoodType _mood;
};

extern DeskMood deskMood;

#endif // DESK_MOOD_H
