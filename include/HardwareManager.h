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

enum StartupSound {
    STARTUP_SND_MARIMBA = 0,  // Acoustic Marimba (64kHz DAC PCM BOOT_SOUND)
    STARTUP_SND_CRYSTAL = 1,  // Crystal Chime (Pha lê trong trẻo)
    STARTUP_SND_CYBER   = 2,  // Cyber Synth (Điện tử tương lai)
    STARTUP_SND_RETRO   = 3,  // Retro 8-Bit (Cổ điển)
    STARTUP_SND_MUTE    = 4,  // Tắt âm khởi động (Mute)
    STARTUP_SND_COUNT   = 5,
    STARTUP_SND_ENABLED = 0   // Alias tương thích ngược
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
    void playStartupJingle();                            // Plays configured boot jingle / chime
    void playStartupSound(uint8_t snd = 0xFF);           // Direct playback of startup chime
    void playSoftTone(uint16_t freqHz, uint16_t durationMs, uint8_t maxDuty = 50, bool decay = true);
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

    // Startup sound selection
    uint8_t getStartupSound() const { return _startupSound; }
    void setStartupSound(uint8_t snd);
    const char* getStartupSoundName() const;
    void nextStartupSound();
    void prevStartupSound();

    // Alarm melody selection
    uint8_t getAlarmMelody() const { return _alarmMelody; }
    void setAlarmMelody(uint8_t melody);
    const char* getAlarmMelodyName() const;
    void nextAlarmMelody();
    void prevAlarmMelody();
    void playAlarmMelody(uint8_t idx = 0xFF);
    void updateSound(); // Non-blocking alarm melody state machine (ticked in loop)
    void saveSoundSettings();

    // ── Screen Power & Sleep / Office Schedule ─────────────────
    void turnScreenOff();
    void turnScreenOn();
    void toggleScreenPower();
    bool isScreenOff() const { return _screenOff; }
    void extendManualWake(unsigned long durationMs = 3600000UL); // Giữ sáng 60 phút khi làm OT
    void clearManualWake() { _manualWakeActive = false; _manualWakeUntil = 0; }
    bool isManualWakeActive() const;

    uint8_t getPowerMode() const { return _powerMode; } // 0 = Office (Default), 1 = Home
    void setPowerMode(uint8_t mode);

    bool isSleepScheduleEnabled() const;
    void setSleepSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled);
    void getSleepSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const;

    void setOfficeSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled);
    void getOfficeSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const;
    void setHomeSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled);
    void getHomeSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const;

    void savePowerSettings();
    void loadPowerSettings();

    // ── RGB LED (Active-LOW on CYD) ───────────────────────────
    void setRGB(RGBColor color);
    void setRGBRaw(bool r, bool g, bool b);

private:
    uint8_t       _currentBrightness;
    bool          _autoBrightness;
    uint8_t       _soundVolume;
    uint8_t       _touchSound;
    uint8_t       _alarmMelody;
    uint8_t       _startupSound;
    bool          _dacAtMidpoint;
    unsigned long _lastLdrCheck;
    int           _smoothedLdr;

    // Non-blocking alarm melody state machine (zero delay() while ringing!)
    bool          _melodyActive = false;
    uint8_t       _melodyIdx = 0;
    uint8_t       _melodyStep = 0;
    bool          _melodyNoteOn = false;
    unsigned long _melodyNextMs = 0;

    // Power & Sleep Schedule State
    bool          _screenOff;
    bool          _scheduleScreenOff;     // True if screen was turned off automatically by schedule
    bool          _manualWakeActive;      // True when user manually woke screen during sleep schedule (OT override)
    unsigned long _manualWakeUntil;       // millis() timestamp when OT override expires
    uint8_t       _savedBrightness;
    uint8_t       _powerMode;            // 0 = Office (Default), 1 = Home

    // Office Schedule (Default: 18:30 - 07:45, screen off 0%)
    uint8_t       _officeStartHour;
    uint8_t       _officeStartMin;
    uint8_t       _officeEndHour;
    uint8_t       _officeEndMin;
    bool          _officeScheduleEn;

    // Home Schedule (Default: 23:00 - 06:30, dim to 5% night clock)
    uint8_t       _homeStartHour;
    uint8_t       _homeStartMin;
    uint8_t       _homeEndHour;
    uint8_t       _homeEndMin;
    bool          _homeScheduleEn;
};

extern HardwareManager hardware;

#endif // HARDWARE_MANAGER_H
