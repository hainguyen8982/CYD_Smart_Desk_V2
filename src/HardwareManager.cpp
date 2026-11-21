#include "HardwareManager.h"
#include "Language.h"
#include "boot_sound.h"
#include "EcosystemService.h"
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <time.h>

HardwareManager hardware;

HardwareManager::HardwareManager()
    : _currentBrightness(85),
      _autoBrightness(true),
      _soundVolume(90),
      _touchSound(TOUCH_SND_CLICK),
      _alarmMelody(ALARM_MELODY_MELODIC),
      _startupSound(STARTUP_SND_ENABLED),
      _dacAtMidpoint(false),
      _lastLdrCheck(0),
      _smoothedLdr(-1),
      _screenOff(false),
      _scheduleScreenOff(false),
      _manualWakeActive(false),
      _manualWakeUntil(0),
      _savedBrightness(85),
      _powerMode(0),            // Mặc định: Chế độ Văn phòng (0 = Office, 1 = Home)
      _officeStartHour(18),     // Mặc định Văn phòng: Tan ca 18:30
      _officeStartMin(30),
      _officeEndHour(7),        // Vào ca 07:45 sáng
      _officeEndMin(45),
      _officeScheduleEn(true),
      _homeStartHour(23),       // Mặc định Ở nhà: Đi ngủ 23:00
      _homeStartMin(0),
      _homeEndHour(6),          // Thức dậy 06:30 sáng
      _homeEndMin(30),
      _homeScheduleEn(true) {}

void HardwareManager::begin() {
    // 1. Backlight - keep OFF initially to avoid flashing stale frame from previous reboot
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, 0);
    _currentBrightness = 0;

    // 2. LDR Ambient sensor (GPIO 34 ADC1)
    pinMode(PIN_LDR, INPUT);
    analogSetPinAttenuation(PIN_LDR, ADC_11db);

    // 3. Speaker / Buzzer (GPIO 26 -> Audio Amplifier IC -> Speaker)
    dacDisable(PIN_SPEAKER);
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);
    _dacAtMidpoint = false;

    // Load saved sound settings from NVS
    Preferences prefs;
    if (prefs.begin("cyd_sound", true)) {
        _touchSound = prefs.getUChar("touch", TOUCH_SND_CLICK);
        _alarmMelody = prefs.getUChar("melody", ALARM_MELODY_MELODIC);
        _startupSound = prefs.getUChar("startup", STARTUP_SND_ENABLED);
        if (_touchSound > TOUCH_SND_CHIRP) _touchSound = TOUCH_SND_CLICK;
        if (_alarmMelody > ALARM_MELODY_GENTLE) _alarmMelody = ALARM_MELODY_MELODIC;
        if (_startupSound > STARTUP_SND_MUTE) _startupSound = STARTUP_SND_ENABLED;
        prefs.end();
    }

    // Load power & sleep schedule settings from NVS
    loadPowerSettings();

    // 4. RGB LED (Active LOW on CYD boards)
    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    setRGB(COLOR_OFF);

    Serial.printf("[HardwareManager] Audio/Speaker OK! Touch: %s, Alarm: %s, Boot: %s\n",
                  getTouchSoundName(), getAlarmMelodyName(), getStartupSoundName());
    uint8_t sh, sm, eh, em;
    bool en;
    getSleepSchedule(sh, sm, eh, em, en);
    Serial.printf("[HardwareManager] Power Mode: %s, Schedule: %02d:%02d -> %02d:%02d (En: %d)\n",
                  (_powerMode == 0) ? "Office" : "Home",
                  sh, sm, eh, em, en ? 1 : 0);
}

void HardwareManager::extendManualWake(unsigned long durationMs) {
    _manualWakeActive = true;
    _manualWakeUntil = millis() + durationMs;
}

bool HardwareManager::isManualWakeActive() const {
    if (!_manualWakeActive) return false;
    return (long)(_manualWakeUntil - millis()) > 0;
}

void HardwareManager::turnScreenOff() {
    if (_screenOff) return;
    if (_currentBrightness > 0) {
        _savedBrightness = _currentBrightness;
    }
    _screenOff = true;
    _scheduleScreenOff = false;
    clearManualWake();
    analogWrite(PIN_TFT_BL, 0);
    // Hạ xung nhịp CPU từ 240MHz xuống 80MHz khi màn hình tắt để tiết kiệm điện & máy mát lạnh
    setCpuFrequencyMhz(80);
    savePowerSettings();
    ecosystem.sendEvent(EVT_NIGHT_SLEEP, 0);
    Serial.printf("[HardwareManager] Screen turned OFF (0W Standby, CPU @ %d MHz)\n", getCpuFrequencyMhz());
}

void HardwareManager::turnScreenOn() {
    if (!_screenOff) return;
    _screenOff = false;
    _scheduleScreenOff = false;
    // Giữ sáng 60 phút (OT Override) để tránh bị lịch tắt văn phòng tắt lại ngay lập tức
    extendManualWake();
    // Khôi phục xung nhịp tối đa 240MHz để đồ họa đạt 30 FPS mượt mà
    setCpuFrequencyMhz(240);
    uint8_t restoreB = (_savedBrightness >= 15) ? _savedBrightness : 85;
    setBacklight(restoreB);
    savePowerSettings();
    ecosystem.sendEvent(EVT_MORNING_WAKE, restoreB);
    Serial.printf("[HardwareManager] Screen woke UP (Restored to %d%%, CPU @ %d MHz, OT override 60m)\n", restoreB, getCpuFrequencyMhz());
}

void HardwareManager::toggleScreenPower() {
    if (_screenOff) turnScreenOn();
    else turnScreenOff();
}

void HardwareManager::setPowerMode(uint8_t mode) {
    _powerMode = (mode > 1) ? 0 : mode;
    savePowerSettings();
}

bool HardwareManager::isSleepScheduleEnabled() const {
    return (_powerMode == 0) ? _officeScheduleEn : _homeScheduleEn;
}

void HardwareManager::setSleepSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled) {
    if (_powerMode == 0) {
        setOfficeSchedule(startH, startM, endH, endM, enabled);
    } else {
        setHomeSchedule(startH, startM, endH, endM, enabled);
    }
}

void HardwareManager::getSleepSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const {
    if (_powerMode == 0) {
        getOfficeSchedule(startH, startM, endH, endM, enabled);
    } else {
        getHomeSchedule(startH, startM, endH, endM, enabled);
    }
}

void HardwareManager::setOfficeSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled) {
    _officeStartHour  = (startH < 24) ? startH : 18;
    _officeStartMin   = (startM < 60) ? startM : 30;
    _officeEndHour    = (endH < 24) ? endH : 7;
    _officeEndMin     = (endM < 60) ? endM : 45;
    _officeScheduleEn = enabled;
    savePowerSettings();
}

void HardwareManager::getOfficeSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const {
    startH  = _officeStartHour;
    startM  = _officeStartMin;
    endH    = _officeEndHour;
    endM    = _officeEndMin;
    enabled = _officeScheduleEn;
}

void HardwareManager::setHomeSchedule(uint8_t startH, uint8_t startM, uint8_t endH, uint8_t endM, bool enabled) {
    _homeStartHour  = (startH < 24) ? startH : 23;
    _homeStartMin   = (startM < 60) ? startM : 0;
    _homeEndHour    = (endH < 24) ? endH : 6;
    _homeEndMin     = (endM < 60) ? endM : 30;
    _homeScheduleEn = enabled;
    savePowerSettings();
}

void HardwareManager::getHomeSchedule(uint8_t& startH, uint8_t& startM, uint8_t& endH, uint8_t& endM, bool& enabled) const {
    startH  = _homeStartHour;
    startM  = _homeStartMin;
    endH    = _homeEndHour;
    endM    = _homeEndMin;
    enabled = _homeScheduleEn;
}

void HardwareManager::loadPowerSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_power", true)) {
        _powerMode = prefs.getUChar("mode", 0);

        // Office schedule
        _officeStartHour  = prefs.getUChar("off_sh", prefs.getUChar("s_sh", 18));
        _officeStartMin   = prefs.getUChar("off_sm", prefs.getUChar("s_sm", 30));
        _officeEndHour    = prefs.getUChar("off_eh", prefs.getUChar("s_eh", 7));
        _officeEndMin     = prefs.getUChar("off_em", prefs.getUChar("s_em", 45));
        _officeScheduleEn = prefs.getBool("off_en", prefs.getBool("s_en", true));

        // Home schedule
        _homeStartHour    = prefs.getUChar("hom_sh", 23);
        _homeStartMin     = prefs.getUChar("hom_sm", 0);
        _homeEndHour      = prefs.getUChar("hom_eh", 6);
        _homeEndMin       = prefs.getUChar("hom_em", 30);
        _homeScheduleEn   = prefs.getBool("hom_en", true);

        uint8_t savedB = prefs.getUChar("saved_b", 85);
        if (savedB >= 15 && savedB <= 100) _savedBrightness = savedB;
        prefs.end();
    }
}

void HardwareManager::savePowerSettings() {
    Preferences prefs;
    if (prefs.begin("cyd_power", false)) {
        prefs.putUChar("mode", _powerMode);

        prefs.putUChar("off_sh", _officeStartHour);
        prefs.putUChar("off_sm", _officeStartMin);
        prefs.putUChar("off_eh", _officeEndHour);
        prefs.putUChar("off_em", _officeEndMin);
        prefs.putBool("off_en", _officeScheduleEn);

        prefs.putUChar("hom_sh", _homeStartHour);
        prefs.putUChar("hom_sm", _homeStartMin);
        prefs.putUChar("hom_eh", _homeEndHour);
        prefs.putUChar("hom_em", _homeEndMin);
        prefs.putBool("hom_en", _homeScheduleEn);

        prefs.putUChar("saved_b", _savedBrightness);
        prefs.end();
    }
}

void HardwareManager::setBacklight(uint8_t percentage) {
    if (percentage > 100) percentage = 100;
    if (percentage > 0) {
        _savedBrightness = percentage;
        _screenOff = false;
    }
    _currentBrightness = percentage;
    uint32_t duty = (percentage * 255) / 100;
    analogWrite(PIN_TFT_BL, duty);
}

void HardwareManager::updateAutoBrightness() {
    // Nếu màn hình bị tắt chủ động bằng tay (Nút Power / Web) -> Giữ nguyên 0W cho tới khi chạm màn hình
    if (_screenOff && !_scheduleScreenOff) return;

    if (!_autoBrightness && !_scheduleScreenOff) return;
    unsigned long now = millis();
    if (now - _lastLdrCheck < LDR_CHECK_INTERVAL) return;
    _lastLdrCheck = now;

    int rawLdr = analogRead(PIN_LDR);

    // Kiểm tra thời gian thực
    time_t tNow = time(NULL);
    struct tm ti;
    bool timeValid = (tNow >= 1600000000 && localtime_r(&tNow, &ti));
    int currentMinOfDay = timeValid ? (ti.tm_hour * 60 + ti.tm_min) : -1;

    // 1. CHẾ ĐỘ VĂN PHÒNG (Office Mode - MẶC ĐỊNH):
    if (_powerMode == 0) {
        bool inOfficeSleep = false;
        if (timeValid && _officeScheduleEn) {
            int startMin = _officeStartHour * 60 + _officeStartMin;
            int endMin   = _officeEndHour * 60 + _officeEndMin;
            if (startMin > endMin) inOfficeSleep = (currentMinOfDay >= startMin || currentMinOfDay < endMin);
            else inOfficeSleep = (currentMinOfDay >= startMin && currentMinOfDay < endMin);
        }

        if (inOfficeSleep) {
            // Nếu người dùng vừa chủ động đánh thức (làm OT) -> Giữ màn hình sáng 60 phút, không tắt lại ngay!
            if (isManualWakeActive()) {
                if (_screenOff) return;
                if (_autoBrightness) {
                    int target = map(rawLdr, 0, 800, 90, 45);
                    target = constrain(target, 40, 95);
                    if (_smoothedLdr == -1) _smoothedLdr = target;
                    else _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
                    setBacklight(_smoothedLdr);
                }
                return;
            } else if (_manualWakeActive) {
                // Hết 60 phút không tương tác -> Xóa cờ OT override để tự động tắt khi phòng tối
                clearManualWake();
            }

            // Ngoài giờ làm việc:
            // Phòng tối (đã tắt đèn ra về, rawLdr >= 350) -> TẮT ĐEN MÀN HÌNH HOÀN TOÀN (0%)
            // Phòng vẫn bật đèn (làm thêm OT, rawLdr < 350) -> Giữ sáng làm việc bình thường (50%-90%)
            if (rawLdr >= 350) {
                if (!_screenOff) {
                    turnScreenOff();
                    _scheduleScreenOff = true;
                }
                return;
            } else {
                if (_screenOff && _scheduleScreenOff && rawLdr < 250) {
                    turnScreenOn();
                    clearManualWake();
                }
                if (_screenOff) return;
                int target = map(rawLdr, 0, 350, 90, 50);
                target = constrain(target, 40, 95);
                if (_smoothedLdr == -1) _smoothedLdr = target;
                else _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
                setBacklight(_smoothedLdr);
                return;
            }
        } else {
            clearManualWake();
            if (_screenOff && _scheduleScreenOff) {
                // Đã đến giờ vào ca sáng (!inOfficeSleep) -> Tự động bật sáng màn hình đón ngày mới!
                turnScreenOn();
                clearManualWake();
                if (!_autoBrightness) return;
            }
        }
    } 
    // 2. CHẾ ĐỘ Ở NHÀ (Home Mode - Giảm mờ xem giờ ban đêm 5%):
    else {
        if (_screenOff && _scheduleScreenOff) {
            turnScreenOn();
            clearManualWake();
            if (!_autoBrightness) return;
        }
        bool inHomeSleep = false;
        if (timeValid && _homeScheduleEn) {
            int startMin = _homeStartHour * 60 + _homeStartMin;
            int endMin   = _homeEndHour * 60 + _homeEndMin;
            if (startMin > endMin) inHomeSleep = (currentMinOfDay >= startMin || currentMinOfDay < endMin);
            else inHomeSleep = (currentMinOfDay >= startMin && currentMinOfDay < endMin);
        }

        // Logic bổ trợ thông minh:
        // A. Trong khung giờ ngủ (vd: 23:00 - 06:30):
        //    - Phòng tắt đèn (rawLdr >= 350) -> Mờ dịu 5% làm đồng hồ đêm không chói mắt.
        //    - Phòng còn bật đèn cày muộn (rawLdr < 350) -> Giữ sáng dịu 45%-70% để xem.
        // B. Ngủ sớm ngoài khung giờ (vd: 21:30 mà phòng đã tắt đèn tối, rawLdr >= 450) -> Tự động giảm mờ 5% luôn!
        if (inHomeSleep) {
            if (rawLdr >= 350) {
                int target = 5;
                if (_smoothedLdr == -1) _smoothedLdr = target;
                else _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
                setBacklight(_smoothedLdr);
                return;
            } else {
                int target = map(rawLdr, 0, 350, 70, 45);
                target = constrain(target, 40, 75);
                if (_smoothedLdr == -1) _smoothedLdr = target;
                else _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
                setBacklight(_smoothedLdr);
                return;
            }
        } else {
            // Chưa đến giờ ngủ nhưng tắt đèn phòng đi ngủ sớm
            if (rawLdr >= 450) {
                int target = 5;
                if (_smoothedLdr == -1) _smoothedLdr = target;
                else _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
                setBacklight(_smoothedLdr);
                return;
            }
        }
    }

    // Ban ngày hoặc phòng đang bật đèn: Tự động điều chỉnh mượt mà theo cảm biến LDR
    int target = map(rawLdr, 0, 800, 95, 25);
    target = constrain(target, 25, 100);

    if (_smoothedLdr == -1) {
        _smoothedLdr = target;
    } else {
        _smoothedLdr = (_smoothedLdr * 3 + target) / 4;
    }

    setBacklight(_smoothedLdr);
}

void HardwareManager::playClick() {
    playTouchSound();
}

void HardwareManager::playTouchSound() {
    if (_soundVolume == 0 || _touchSound == TOUCH_SND_MUTE) return;

    // Chống lặp tiếng (Debounce): Loại bỏ triệt để xung đột kích hoạt 2 tiếng liên tiếp trong 60ms
    unsigned long now = millis();
    static unsigned long lastTouchSoundTime = 0;
    if (now - lastTouchSoundTime < 60) {
        return;
    }
    lastTouchSoundTime = now;

    if (_dacAtMidpoint) {
        dacDisable(PIN_SPEAKER);
        _dacAtMidpoint = false;
    }

    switch (_touchSound) {
        case TOUCH_SND_CLICK:
            // 1. Phím cơ (Mechanical Click): Tiếng "tách" đanh, dứt khoát với 2 micro-snap (1600Hz -> 2400Hz)
            tone(PIN_SPEAKER, 1600, 4);
            delay(5);
            tone(PIN_SPEAKER, 2400, 6);
            delay(7);
            break;

        case TOUCH_SND_WATER:
            // 2. Giọt nước (Water Drop): Tiếng "bõm" giọt nước nảy nhanh (1300Hz -> 2800Hz trong ~25ms)
            tone(PIN_SPEAKER, 1300, 4);
            delay(5);
            tone(PIN_SPEAKER, 1750, 5);
            delay(6);
            tone(PIN_SPEAKER, 2200, 6);
            delay(7);
            tone(PIN_SPEAKER, 2800, 8);
            delay(9);
            break;

        case TOUCH_SND_BEEP:
            // 3. Bíp nhẹ (Soft Beep): Tiếng "ting" dứt khoát, thanh thoát ở 1175Hz (nốt Re cao D6) trong 26ms
            tone(PIN_SPEAKER, 1175, 26);
            delay(28);
            break;

        case TOUCH_SND_CHIRP:
            // 4. Chíp chíp (Dual Chirp): 2 tiếng chim non lảnh lót A6 (1760Hz) và D7 (2349Hz)
            tone(PIN_SPEAKER, 1760, 12);
            delay(14);
            noTone(PIN_SPEAKER);
            delay(14);
            tone(PIN_SPEAKER, 2349, 16);
            delay(18);
            break;

        default:
            tone(PIN_SPEAKER, 1600, 5);
            delay(6);
            break;
    }
    stopSound();
}

void HardwareManager::playBeep(uint16_t freqHz, uint16_t durationMs) {
    if (_soundVolume == 0) return;
    if (_dacAtMidpoint) {
        dacDisable(PIN_SPEAKER);
        _dacAtMidpoint = false;
    }
    tone(PIN_SPEAKER, freqHz, durationMs);
    delay(durationMs + 2);
    stopSound();
}

void HardwareManager::playSoftTone(uint16_t freqHz, uint16_t durationMs, uint8_t maxDuty, bool decay) {
    if (_soundVolume == 0 || freqHz == 0 || durationMs == 0) return;
    if (_dacAtMidpoint) {
        dacDisable(PIN_SPEAKER);
        _dacAtMidpoint = false;
    }

    // Scale duty safely: 11% - 15% max duty ensures SC8002B amplifier stays in clean linear range
    uint8_t baseDuty = (uint8_t)(((uint16_t)maxDuty * _soundVolume) / 100);
    if (baseDuty < 2) baseDuty = 2;

    const uint8_t LEDC_CH = 7;
    ledcSetup(LEDC_CH, freqHz, 8);
    ledcAttachPin(PIN_SPEAKER, LEDC_CH);

    if (!decay) {
        ledcWrite(LEDC_CH, baseDuty);
        delay(durationMs);
    } else {
        // Natural exponential decay: duty diminishes smoothly over time into total silence
        int steps = 24;
        int stepMs = durationMs / steps;
        if (stepMs < 4) stepMs = 4;
        steps = durationMs / stepMs;
        for (int i = 0; i < steps; i++) {
            float f = (float)(steps - i) / (float)steps;
            uint8_t d = (uint8_t)(baseDuty * (0.25f * f + 0.75f * f * f));
            ledcWrite(LEDC_CH, d);
            delay(stepMs);
        }
    }

    ledcWrite(LEDC_CH, 0);
    ledcDetachPin(PIN_SPEAKER);
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);
}

void HardwareManager::playStartupJingle() {
    playStartupSound(_startupSound);
}

void HardwareManager::playStartupSound(uint8_t snd) {
    uint8_t s = (snd <= STARTUP_SND_MUTE) ? snd : _startupSound;
    if (_soundVolume == 0 || s == STARTUP_SND_MUTE || BOOT_SOUND_LEN < 2) return;

    // Dừng hoàn toàn mọi kênh PWM / Tone đang bám trên chân loa (tránh xung đột tần số gây tiếng rè "è è")
    stopSound();

    // Tạm nhả xung nhịp cho các tác vụ nền trước khi phát âm thanh liên tục
    delay(5);

    if (s == STARTUP_SND_CRYSTAL) {
        // 1. Pha lê (Crystal Harmonic Chime): C5 -> E5 -> G5 -> C6 êm dịu, ngân vang
        static const uint16_t notes[] = { 523, 659, 784, 1047 };
        static const uint16_t durs[]  = { 90,  90,  90,  220 };
        for (int i = 0; i < 4; i++) {
            playSoftTone(notes[i], durs[i], 45, true);
            delay(15);
        }
        stopSound();
        return;
    } else if (s == STARTUP_SND_CYBER) {
        // 2. Cyber Synth: Hợp âm điện tử tương lai A4 -> E5 -> A5 -> C#6 -> E6
        static const uint16_t notes[] = { 440, 659, 880, 1109, 1319 };
        static const uint16_t durs[]  = { 55,  55,  55,  55,   180 };
        for (int i = 0; i < 5; i++) {
            playSoftTone(notes[i], durs[i], 42, true);
            delay(10);
        }
        stopSound();
        return;
    } else if (s == STARTUP_SND_RETRO) {
        // 3. Retro 8-Bit: Tiếng boot cổ điển kiểu GameBoy / Coin-up (B5 -> E6)
        playSoftTone(988, 75, 45, false);
        delay(10);
        playSoftTone(1319, 240, 45, true);
        stopSound();
        return;
    }

    // 0. Mặc định (STARTUP_SND_MARIMBA): Phát mẫu PCM 64kHz 4x Oversampling
    // Ultrasonic soft ramp to DC midpoint 128 if not already at midpoint (20us per step = 50 kHz)
    // Silently charges AC coupling capacitor without any audible chirp or initial click
    if (!_dacAtMidpoint) {
        for (int v = 0; v <= 128; v++) {
            dacWrite(PIN_SPEAKER, v);
            delayMicroseconds(20);
        }
        _dacAtMidpoint = true;
    }

    // 4x Oversampled 64,000 Hz Monotonic Linear Interpolation:
    // Nội suy bậc tuyến tính 4x siêu mịn (64kHz) đẩy tần số bậc thang vượt xa ngưỡng khuếch đại,
    // triệt tiêu hoàn toàn tiếng rè "è è" / "xè xè" trên mạch khuếch đại Class-AB (SC8002B / NS4150).
    // Biên độ cân bằng chuẩn (peak ±16 steps tại 70% volume) giữ IC khuếch đại trong vùng tuyến tính sạch 100%.
    uint64_t startUs = esp_timer_get_time();

    for (uint32_t i = 0; i < BOOT_SOUND_LEN - 1; i++) {
        if ((i & 0x1FF) == 0) {
            esp_task_wdt_reset();
        }

        int16_t c0 = (int16_t)pgm_read_byte(&BOOT_SOUND[i]) - 128;
        int16_t c1 = (int16_t)pgm_read_byte(&BOOT_SOUND[i + 1]) - 128;

        // Scale by volume: BOOT_SOUND has peak ±110 around 128; at 90% volume -> ±99 DAC steps (loud, clear, zero clipping)
        int16_t y0 = (int16_t)(((int32_t)c0 * _soundVolume) / 100);
        int16_t y1 = (int16_t)(((int32_t)c1 * _soundVolume) / 100);

        // 4 sub-steps per 16kHz sample (62.5us / 4 = 15.625us per sub-step = 64kHz update rate)
        for (uint8_t sub = 0; sub < 4; sub++) {
            int16_t yInterp = constrain(128 + y0 + ((y1 - y0) * sub) / 4, 4, 252);
            dacWrite(PIN_SPEAKER, (uint8_t)yInterp);
            uint64_t targetUs = startUs + ((uint64_t)(i * 4 + sub + 1) * 125) / 8;
            while ((uint64_t)esp_timer_get_time() < targetUs) {}
        }
    }

    // Smooth, sub-audible ultrasonic fade-down to 0V (6.4ms, 128 steps * 50us)
    // Completely eliminates clicks/pops while cutting off DAC power and residual noise
    for (int v = 128; v >= 0; v--) {
        dacWrite(PIN_SPEAKER, v);
        delayMicroseconds(50);
    }

    // Fully disable DAC and clamp pin hard to GND (0V)
    dacDisable(PIN_SPEAKER);
    pinMode(PIN_SPEAKER, OUTPUT);
    digitalWrite(PIN_SPEAKER, LOW);
    _dacAtMidpoint = false;
}

void HardwareManager::playAlarmTune() {
    playAlarmMelody(_alarmMelody);
}

void HardwareManager::playAlarmMelody(uint8_t idx) {
    if (_soundVolume == 0) return;
    if (_dacAtMidpoint) {
        dacDisable(PIN_SPEAKER);
        _dacAtMidpoint = false;
    }
    uint8_t m = (idx <= ALARM_MELODY_GENTLE) ? idx : _alarmMelody;
    switch (m) {
        case ALARM_MELODY_CLASSIC: {
            // Rapid energetic triple-beep (êm ấm qua bộ điều chế LEDC playSoftTone)
            for (int i = 0; i < 3; i++) {
                playSoftTone(2200, 65, 48, false);
                delay(25);
            }
            break;
        }
        case ALARM_MELODY_MELODIC: {
            // Joyful ascending chime: C6 (1047), E6 (1319), G6 (1568), C7 (2093)
            static const uint16_t notes[] = { 1047, 1319, 1568, 2093, 1568, 2093 };
            static const uint16_t durs[]  = { 80,   80,   80,   110,  75,   160  };
            for (int i = 0; i < 6; i++) {
                playSoftTone(notes[i], durs[i], 46, true);
                delay(18);
            }
            break;
        }
        case ALARM_MELODY_CYBER: {
            // Futuristic synth arpeggio: A5 (880), C#6 (1109), E6 (1319), A6 (1760)
            static const uint16_t notes[] = { 880, 1109, 1319, 1760 };
            for (int i = 0; i < 4; i++) {
                playSoftTone(notes[i], 70, 45, true);
                delay(15);
            }
            break;
        }
        case ALARM_MELODY_GENTLE: {
            // Warm morning chime: C5 (523), E5 (659), G5 (784), C6 (1047)
            static const uint16_t notes[] = { 523, 659, 784, 1047 };
            static const uint16_t durs[]  = { 110, 110, 110, 200  };
            for (int i = 0; i < 4; i++) {
                playSoftTone(notes[i], durs[i], 44, true);
                delay(20);
            }
            break;
        }
    }
    stopSound();
}

void HardwareManager::stopSound() {
    noTone(PIN_SPEAKER);
    ledcWrite(7, 0);
    ledcDetachPin(PIN_SPEAKER);
    if (_dacAtMidpoint) {
        for (int v = 128; v >= 0; v -= 4) {
            dacWrite(PIN_SPEAKER, v);
            delayMicroseconds(50);
        }
        dacDisable(PIN_SPEAKER);
        _dacAtMidpoint = false;
    }
    pinMode(PIN_SPEAKER, OUTPUT);
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

void HardwareManager::setStartupSound(uint8_t snd) {
    _startupSound = (snd <= STARTUP_SND_MUTE) ? snd : STARTUP_SND_MARIMBA;
    saveSoundSettings();
}

const char* HardwareManager::getStartupSoundName() const {
    return getStartupSoundNameI18n(_startupSound);
}

void HardwareManager::nextStartupSound() {
    _startupSound = (_startupSound + 1) % STARTUP_SND_COUNT;
    saveSoundSettings();
}

void HardwareManager::prevStartupSound() {
    _startupSound = (_startupSound + STARTUP_SND_COUNT - 1) % STARTUP_SND_COUNT;
    saveSoundSettings();
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
        prefs.putUChar("startup", _startupSound);
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
