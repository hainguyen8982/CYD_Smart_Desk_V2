#include "HardwareManager.h"
#include "Language.h"
#include <Preferences.h>
#include <time.h>

HardwareManager hardware;

HardwareManager::HardwareManager()
    : _currentBrightness(85),
      _autoBrightness(true),
      _soundVolume(70),
      _touchSound(TOUCH_SND_CLICK),
      _alarmMelody(ALARM_MELODY_MELODIC),
      _lastLdrCheck(0),
      _smoothedLdr(-1) {}

void HardwareManager::begin() {
    // 1. Backlight - keep OFF initially to avoid flashing stale frame from previous reboot
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, 0);
    _currentBrightness = 0;

    // 2. LDR Ambient sensor (GPIO 34 ADC1)
    pinMode(PIN_LDR, INPUT);
    analogSetPinAttenuation(PIN_LDR, ADC_11db);

    // 3. Speaker / Buzzer (GPIO 26 -> Audio Amplifier IC -> Speaker)
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);

    // Load saved sound settings from NVS
    Preferences prefs;
    if (prefs.begin("cyd_sound", true)) {
        _touchSound = prefs.getUChar("touch", TOUCH_SND_CLICK);
        _alarmMelody = prefs.getUChar("melody", ALARM_MELODY_MELODIC);
        if (_touchSound > TOUCH_SND_CHIRP) _touchSound = TOUCH_SND_CLICK;
        if (_alarmMelody > ALARM_MELODY_GENTLE) _alarmMelody = ALARM_MELODY_MELODIC;
        prefs.end();
    }

    // 4. RGB LED (Active LOW on CYD boards)
    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    setRGB(COLOR_OFF);

    Serial.printf("[HardwareManager] Audio/Speaker OK! Touch: %s, Alarm: %s\n",
                  getTouchSoundName(), getAlarmMelodyName());
}

void HardwareManager::setBacklight(uint8_t percentage) {
    if (percentage > 100) percentage = 100;
    _currentBrightness = percentage;
    uint32_t duty = (percentage * 255) / 100;
    analogWrite(PIN_TFT_BL, duty);
}

void HardwareManager::updateAutoBrightness() {
    if (!_autoBrightness) return;
    unsigned long now = millis();
    if (now - _lastLdrCheck < LDR_CHECK_INTERVAL) return;
    _lastLdrCheck = now;

    int rawLdr = analogRead(PIN_LDR);

    int target = map(rawLdr, 0, 800, 95, 15);
    target = constrain(target, 15, 100);

    // Night sleep schedule (23:00 - 06:00): Auto-dim to 5%
    time_t tNow = time(NULL);
    struct tm ti;
    if (tNow >= 1600000000 && localtime_r(&tNow, &ti)) {
        if (ti.tm_hour >= 23 || ti.tm_hour < 6) {
            target = 5;
        }
    }

    if (_smoothedLdr == -1) {
        _smoothedLdr = target;
    } else {
        _smoothedLdr = (_smoothedLdr * 3 + target) / 4; // Smooth transition
    }

    setBacklight(_smoothedLdr);
}

void HardwareManager::playClick() {
    playTouchSound();
}

void HardwareManager::playTouchSound() {
    if (_soundVolume == 0 || _touchSound == TOUCH_SND_MUTE) return;

    switch (_touchSound) {
        case TOUCH_SND_CLICK:
            // 1. Phím cơ (Mechanical Click): Tiếng "tách" đanh, dứt khoát với 2 micro-snap
            tone(PIN_SPEAKER, 1400, 4);
            delay(5);
            tone(PIN_SPEAKER, 2100, 6);
            delay(7);
            break;

        case TOUCH_SND_WATER:
            // 2. Giọt nước (Water Drop): Quét tần số tăng dần từ 1100Hz -> 2400Hz tạo hiệu ứng "bõm/tõm" bong bóng nước
            for (int f = 1100; f <= 2300; f += 200) {
                tone(PIN_SPEAKER, f, 5);
                delay(5);
            }
            break;

        case TOUCH_SND_BEEP:
            // 3. Bíp nhẹ (Soft Chime): Tiếng chuông "ting" êm ái ngân 32ms ở nốt Đô cao (C6 - 1047Hz)
            tone(PIN_SPEAKER, 1047, 32);
            delay(34);
            break;

        case TOUCH_SND_CHIRP:
            // 4. Chíp chíp (Dual Chirp): 2 tiếng chim hót liên tiếp có khoảng ngắt rõ rệt ("chíp - chíp")
            tone(PIN_SPEAKER, 1760, 16); // Nốt A6
            delay(18);
            noTone(PIN_SPEAKER);
            delay(18);                    // Khoảng nghỉ giữa 2 tiếng
            tone(PIN_SPEAKER, 2349, 22); // Nốt D7 cao
            delay(24);
            break;

        default:
            tone(PIN_SPEAKER, 1400, 5);
            delay(6);
            break;
    }
    stopSound();
}

void HardwareManager::playBeep(uint16_t freqHz, uint16_t durationMs) {
    if (_soundVolume == 0) return;
    tone(PIN_SPEAKER, freqHz, durationMs);
    delay(durationMs + 2);
    stopSound();
}

void HardwareManager::playStartupJingle() {
    if (_soundVolume == 0 || _touchSound == TOUCH_SND_MUTE) return;

    // Classic Warm Chime Startup (Rich, smooth legato Westminster-style chime)
    // Low-mid frequencies for full warmth without harsh buzzer shrillness:
    // G4 (392Hz) -> C5 (523Hz) -> E5 (659Hz) -> G5 (784Hz) -> C6 (1047Hz sustained)
    struct Note {
        uint16_t freq;
        uint16_t durationMs;
        uint16_t pauseMs;
    };

    static const Note jingle[] = {
        { 392,  120, 125 },  // G4: Nốt trầm mở đầu êm dịu
        { 523,  130, 135 },  // C5: Thăng hoa ấm áp
        { 659,  140, 145 },  // E5: Trong trẻo
        { 784,  155, 160 },  // G5: Vang sáng
        { 1047, 450, 470 }   // C6: Ngân dài du dương kết thúc
    };

    for (const auto& n : jingle) {
        tone(PIN_SPEAKER, n.freq, n.durationMs);
        delay(n.pauseMs);
    }
    stopSound();
}

void HardwareManager::playAlarmTune() {
    playAlarmMelody(_alarmMelody);
}

void HardwareManager::playAlarmMelody(uint8_t idx) {
    uint8_t m = (idx <= ALARM_MELODY_GENTLE) ? idx : _alarmMelody;
    switch (m) {
        case ALARM_MELODY_CLASSIC: {
            // Rapid energetic triple-beep
            for (int i = 0; i < 3; i++) {
                tone(PIN_SPEAKER, 2200, 65);
                delay(85);
            }
            break;
        }
        case ALARM_MELODY_MELODIC: {
            // Joyful ascending chime: C5 (1047), E5 (1319), G5 (1568), C6 (2093)
            static const int notes[] = { 1047, 1319, 1568, 2093, 1568, 2093 };
            for (int n : notes) {
                tone(PIN_SPEAKER, n, 85);
                delay(105);
            }
            break;
        }
        case ALARM_MELODY_CYBER: {
            // Futuristic synth arpeggio: A5 (1760), C6 (2093), E6 (2637), G6 (3136)
            static const int notes[] = { 1760, 2093, 2637, 3136 };
            for (int n : notes) {
                tone(PIN_SPEAKER, n, 70);
                delay(85);
            }
            break;
        }
        case ALARM_MELODY_GENTLE: {
            // Warm morning chime: G4 (392), C5 (523), E5 (659), G5 (784)
            static const int notes[] = { 523, 659, 784, 1047 };
            for (int n : notes) {
                tone(PIN_SPEAKER, n, 120);
                delay(140);
            }
            break;
        }
    }
    stopSound();
}

void HardwareManager::stopSound() {
    noTone(PIN_SPEAKER);
    digitalWrite(PIN_SPEAKER, LOW);
}

void HardwareManager::setSoundVolume(uint8_t volPercent) {
    if (volPercent > 100) volPercent = 100;
    _soundVolume = volPercent;
}

void HardwareManager::setTouchSound(uint8_t snd) {
    _touchSound = (snd <= TOUCH_SND_CHIRP) ? snd : TOUCH_SND_CLICK;
    saveSoundSettings();
}

const char* HardwareManager::getTouchSoundName() const {
    return getTouchSoundNameI18n(_touchSound);
}

void HardwareManager::nextTouchSound() {
    _touchSound = (_touchSound + 1) % 5;
    saveSoundSettings();
    playTouchSound();
}

void HardwareManager::prevTouchSound() {
    _touchSound = (_touchSound + 4) % 5;
    saveSoundSettings();
    playTouchSound();
}

void HardwareManager::setAlarmMelody(uint8_t melody) {
    _alarmMelody = (melody <= ALARM_MELODY_GENTLE) ? melody : ALARM_MELODY_MELODIC;
    saveSoundSettings();
}

const char* HardwareManager::getAlarmMelodyName() const {
    return getAlarmMelodyNameI18n(_alarmMelody);
}

void HardwareManager::nextAlarmMelody() {
    _alarmMelody = (_alarmMelody + 1) % 4;
    saveSoundSettings();
}

void HardwareManager::prevAlarmMelody() {
    _alarmMelody = (_alarmMelody + 3) % 4;
    saveSoundSettings();
}

void HardwareManager::saveSoundSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_sound", false)) {
        prefs.putUChar("touch", _touchSound);
        prefs.putUChar("melody", _alarmMelody);
        prefs.end();
    }
}

void HardwareManager::setRGBRaw(bool r, bool g, bool b) {
    // CYD RGB pins are Active-LOW (LOW = ON, HIGH = OFF)
    digitalWrite(PIN_RGB_RED, r ? LOW : HIGH);
    digitalWrite(PIN_RGB_GREEN, g ? LOW : HIGH);
    digitalWrite(PIN_RGB_BLUE, b ? LOW : HIGH);
}

void HardwareManager::setRGB(RGBColor color) {
    switch (color) {
        case COLOR_RED:     setRGBRaw(true, false, false); break;
        case COLOR_GREEN:   setRGBRaw(false, true, false); break;
        case COLOR_BLUE:    setRGBRaw(false, false, true); break;
        case COLOR_YELLOW:  setRGBRaw(true, true, false); break;
        case COLOR_CYAN:    setRGBRaw(false, true, true); break;
        case COLOR_MAGENTA: setRGBRaw(true, false, true); break;
        case COLOR_WHITE:   setRGBRaw(true, true, true); break;
        case COLOR_OFF:
        default:            setRGBRaw(false, false, false); break;
    }
}
