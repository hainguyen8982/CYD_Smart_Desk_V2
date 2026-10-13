// ═══════════════════════════════════════════════════════════════════════
//  HardwareManager.cpp — Hardware control implementation for CYD
// ═══════════════════════════════════════════════════════════════════════
#include "HardwareManager.h"
#include <time.h>

HardwareManager hardware;

HardwareManager::HardwareManager()
    : _currentBrightness(85),
      _autoBrightness(true),
      _soundVolume(70),
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

    // 3. Speaker / Buzzer (GPIO 26)
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);

    // 4. RGB LED (Active LOW on CYD boards)
    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    setRGB(COLOR_OFF);

    Serial.println("[HardwareManager] Initialized: Backlight, LDR, Speaker, RGB LED OK");
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

    // CYD LDR Behavior on GPIO 34:
    // Bright room: rawLdr near 0 -> 95% High Brightness
    // Dark room: rawLdr increases (300..900+) -> Dim down to 15%
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
    if (_soundVolume == 0) return;
    // Ultra-short 12ms click at 2600Hz for crisp tactile feedback
    tone(PIN_SPEAKER, 2600, 12);
}

void HardwareManager::playBeep(uint16_t freqHz, uint16_t durationMs) {
    if (_soundVolume == 0) return;
    tone(PIN_SPEAKER, freqHz, durationMs);
}

void HardwareManager::playAlarmTune() {
    static const int notes[] = { 1047, 1318, 1568, 2093, 1568, 2093 };
    for (int note : notes) {
        tone(PIN_SPEAKER, note, 100);
        delay(120);
    }
    noTone(PIN_SPEAKER);
    digitalWrite(PIN_SPEAKER, LOW);
}

void HardwareManager::stopSound() {
    noTone(PIN_SPEAKER);
    digitalWrite(PIN_SPEAKER, LOW);
}

void HardwareManager::setSoundVolume(uint8_t volPercent) {
    if (volPercent > 100) volPercent = 100;
    _soundVolume = volPercent;
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
