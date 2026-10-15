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
    MOOD_BUG         = 2,   // ⚡ Bực mình / Đang săn bug (Bug hunting / Angry)
    MOOD_LOW_BATTERY = 3,   // 🟡 Cạn năng lượng 0% (Low battery 0%)
    MOOD_CHILL       = 4,   // 🟣 Đang chán / Siêu chill (Bored / Chill)
    MOOD_TGIF        = 5,   // 🎉 Cuối tuần rồi / Quẩy thôi (TGIF / Weekend)
    MOOD_COUNT       = 6
};

enum MoodPack : uint8_t {
    PACK_MOCHI = 0,  // Bé Đầu Trọc Mochi (Mochi Guy)
    PACK_STAR  = 1,  // Ngôi Sao Vàng Cute (Cute Star)
    PACK_BUNNY = 2,  // Bé Thỏ Trắng Dễ Thương (Cute Bunny)
    PACK_COUNT = 3
};

class DeskMood {
public:
    DeskMood();

    void begin();
    MoodType getMood() const { return _mood; }
    void setMood(MoodType m);
    void nextMood();
    void prevMood();

    MoodPack getPack() const { return _pack; }
    void setPack(MoodPack p);
    void nextPack();
    const char* getPackName(MoodPack p) const;

    const char* getName(MoodType m) const;
    const char* getIcon(MoodType m) const;
    const char* getTag(MoodType m, uint8_t lang) const;
    const char* getMessage(MoodType m, uint8_t lang) const;
    uint16_t    getAccentColor(MoodType m) const;

    // Custom Status Management (Tự đặt câu trạng thái riêng CHO TỪNG TÂM TRẠNG)
    bool isCustom(MoodType m) const { return (m < MOOD_COUNT) ? _customEnabled[m] : false; }
    bool isCurrentCustom() const { return isCustom(_mood); }
    const char* getCustomTag(MoodType m) const { return (m < MOOD_COUNT) ? _customTags[m] : ""; }
    const char* getCustomMessage(MoodType m) const { return (m < MOOD_COUNT) ? _customMsgs[m] : ""; }
    void setCustomForMood(MoodType m, bool enabled, const char* tag = nullptr, const char* msg = nullptr);
    void resetMoodToDefault(MoodType m);

    void renderPage(TFT_eSprite& spr, uint8_t lang, uint16_t themeBg);
    bool handleTouch(int16_t x, int16_t y);

private:
    void _loadSettings();
    void _saveSettings();
    void _syncHardwareLed();

    MoodType _mood;
    MoodPack _pack;
    bool     _customEnabled[MOOD_COUNT];
    char     _customTags[MOOD_COUNT][24];      // Max 20 chars + null for each mood
    char     _customMsgs[MOOD_COUNT][40];      // Max 36 chars + null for each mood
};

extern DeskMood deskMood;

#endif // DESK_MOOD_H
