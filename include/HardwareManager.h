// ═══════════════════════════════════════════════════════════════════════
//  HardwareManager.h — Hardware control for ESP32 CYD board
//  Controls: Backlight (PWM), LDR Sensor, Buzzer (Click/Alarm), RGB LED
// ═══════════════════════════════════════════════════════════════════════
#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>
#include "Config.h"

enum RGBColor {
    COLOR_OFF,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_WHITE
};

enum TouchSound {
    TOUCH_SND_MUTE = 0,
    TOUCH_SND_CLICK = 1,
    TOUCH_SND_WATER = 2,
    TOUCH_SND_BEEP = 3,
    TOUCH_SND_CHIRP = 4
};

enum AlarmMelody {
    ALARM_MELODY_CLASSIC = 0,
    ALARM_MELODY_MELODIC = 1,
    ALARM_MELODY_CYBER = 2,
    ALARM_MELODY_GENTLE = 3
};

class HardwareManager {
public:
    HardwareManager();
    void begin();

    // ── Backlight & Ambient Sensor ────────────────────────────
    void setBacklight(uint8_t percentage);
    uint8_t getBacklight() const { return _currentBrightness; }
    void setAutoBrightness(bool enable) { _autoBrightness = enable; }
    bool isAutoBrightness() const { return _autoBrightness; }
    void updateAutoBrightness();

    // ── Buzzer & Sound Feedback (GPIO 26 -> Audio Amp -> Speaker) ──
    void playClick();                                    // Tactile audio feedback (respects _touchSound)
    void playBeep(uint16_t freqHz = 2400, uint16_t durationMs = 50);
    void playAlarmTune();
    void stopSound();
    void setSoundVolume(uint8_t volPercent);
    uint8_t getSoundVolume() const { return _soundVolume; }

    // Touch feedback selection
    uint8_t getTouchSound() const { return _touchSound; }
    void setTouchSound(uint8_t snd);
    const char* getTouchSoundName() const;
    void nextTouchSound();
    void prevTouchSound();
    void playTouchSound();

    // Alarm melody selection
    uint8_t getAlarmMelody() const { return _alarmMelody; }
    void setAlarmMelody(uint8_t melody);
    const char* getAlarmMelodyName() const;
    void nextAlarmMelody();
    void prevAlarmMelody();
    void playAlarmMelody(uint8_t idx = 0xFF);
    void saveSoundSettings();

    // ── RGB LED (Active-LOW on CYD) ───────────────────────────
    void setRGB(RGBColor color);
    void setRGBRaw(bool r, bool g, bool b);

private:
    uint8_t       _currentBrightness;
    bool          _autoBrightness;
    uint8_t       _soundVolume;
    uint8_t       _touchSound;
    uint8_t       _alarmMelody;
    unsigned long _lastLdrCheck;
    int           _smoothedLdr;
};

extern HardwareManager hardware;

#endif // HARDWARE_MANAGER_H
