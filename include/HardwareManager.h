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

    // ── Buzzer & Sound Feedback ───────────────────────────────
    void playClick();                                    // 15ms tactile audio click
    void playBeep(uint16_t freqHz = 2400, uint16_t durationMs = 50);
    void playAlarmTune();
    void stopSound();
    void setSoundVolume(uint8_t volPercent);
    uint8_t getSoundVolume() const { return _soundVolume; }

    // ── RGB LED (Active-LOW on CYD) ───────────────────────────
    void setRGB(RGBColor color);
    void setRGBRaw(bool r, bool g, bool b);

private:
    uint8_t       _currentBrightness;
    bool          _autoBrightness;
    uint8_t       _soundVolume;
    unsigned long _lastLdrCheck;
    int           _smoothedLdr;
};

extern HardwareManager hardware;

#endif // HARDWARE_MANAGER_H
