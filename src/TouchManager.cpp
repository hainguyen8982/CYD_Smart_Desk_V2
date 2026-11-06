#include <Wire.h>
// ═══════════════════════════════════════════════════════════════════════
//  TouchManager.cpp — Robust touch implementation for ESP32 CYD
// ═══════════════════════════════════════════════════════════════════════
#include "TouchManager.h"
#include "HardwareManager.h"
#include <Preferences.h>

TouchManager touch;

TouchManager::TouchManager()
    : _isTouching(false),
      _touchStartTime(0),
      _startX(0), _startY(0),
      _lastX(0), _lastY(0),
      _lastTapTime(0),
      _calibState(CALIB_NONE),
      _calibValid(false),
      _calibWaitRelease(false),
      _calibDoneTime(0),
      _calXis90(true),
      _calXmin(350), _calXmax(3800),
      _calYmin(350), _calYmax(3800) {}

void TouchManager::begin() {
#ifdef BOARD_NICEMCU_28IPS
    pinMode(PIN_TOUCH_INT, INPUT);
    pinMode(PIN_TOUCH_SDA, INPUT_PULLUP);
    pinMode(PIN_TOUCH_SCL, INPUT_PULLUP);

    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);

    // Disable auto-sleep on CST816D so it responds reliably
    Wire.beginTransmission(0x15);
    Wire.write(0xFE);
    Wire.write(0xFF);
    Wire.endTransmission();

    _calibValid = true;
    _calibState = CALIB_NONE;
    Serial.println("[TouchManager] NiceMCU CST816D Capacitive Touch initialized on I2C (SDA=33, SCL=32)");
#else
    pinMode(PIN_TOUCH_CS, OUTPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);
    pinMode(PIN_TOUCH_IRQ, INPUT);
    pinMode(25, OUTPUT); // CLK
    pinMode(32, OUTPUT); // MOSI
    pinMode(39, INPUT);  // MISO

    _loadCalibration();

    if (!_calibValid) {
        Serial.println("[TouchManager] No calibration found in NVS — using CYD landscape defaults");
        _setDefaultCalibration();
    }
    Serial.println("[TouchManager] Touch driver initialized");
#endif
}

uint16_t TouchManager::_xptReadChannel(uint8_t cmd) {
#ifndef BOARD_NICEMCU_28IPS
    digitalWrite(PIN_TOUCH_CS, LOW);

    for (int i = 7; i >= 0; i--) {
        digitalWrite(32, (cmd >> i) & 1);
        digitalWrite(25, HIGH);
        delayMicroseconds(2);
        digitalWrite(25, LOW);
        delayMicroseconds(2);
    }

    uint16_t result = 0;
    for (int i = 11; i >= 0; i--) {
        digitalWrite(25, HIGH);
        delayMicroseconds(2);
        if (digitalRead(39)) result |= (1 << i);
        digitalWrite(25, LOW);
        delayMicroseconds(2);
    }

    digitalWrite(PIN_TOUCH_CS, HIGH);
    return result;
#else
    return 0;
#endif
}

bool TouchManager::_readRaw(uint16_t *raw90, uint16_t *rawD0) {
#ifndef BOARD_NICEMCU_28IPS
    if (digitalRead(PIN_TOUCH_IRQ) == HIGH) return false;

    // Đọc trục Y (0x90): đọc 1 mẫu xả điện dung phiến điện trở, sau đó lấy trung bình 4 mẫu ổn định
    _xptReadChannel(0x90);
    long sum90 = 0;
    for (int i = 0; i < 4; i++) {
        sum90 += _xptReadChannel(0x90);
    }

    // Đọc trục X (0xD0): đọc 1 mẫu xả điện dung phiến điện trở, sau đó lấy trung bình 4 mẫu ổn định
    _xptReadChannel(0xD0);
    long sumD0 = 0;
    for (int i = 0; i < 4; i++) {
        sumD0 += _xptReadChannel(0xD0);
    }

    uint16_t r90 = sum90 / 4;
    uint16_t rD0 = sumD0 / 4;

    // Filter out edge noise
    if (r90 < 150 || rD0 < 150 || r90 > 3950 || rD0 > 3950) return false;

    *raw90 = r90;
    *rawD0 = rD0;
    return true;
#else
    return false;
#endif
}

bool TouchManager::getTouch(uint16_t *x, uint16_t *y) {
#ifdef BOARD_NICEMCU_28IPS
    static unsigned long lastPollUs = 0;
    static unsigned long lastValidTouchMs = 0;
    static bool cachedPressed = false;
    static uint16_t cachedX = 0, cachedY = 0;

    unsigned long nowUs = micros();
    unsigned long nowMs = millis();

    // Điều tiết tốc độ đọc I2C tối đa 1 lần mỗi 6ms (~166Hz) để chip CST816D không bị nghẽn NACK
    if (nowUs - lastPollUs < 6000) {
        if (cachedPressed) {
            if (x) *x = cachedX;
            if (y) *y = cachedY;
        }
        return cachedPressed;
    }
    lastPollUs = nowUs;

    // Đọc gói tin 5 bytes từ CST816D bắt đầu tại thanh ghi 0x02
    Wire.beginTransmission(0x15);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) {
        // Nếu CST816D đang bận quét điện dung (NACK tạm thời), giữ trạng thái chạm thêm tối đa 25ms
        if (cachedPressed && (nowMs - lastValidTouchMs <= 25)) {
            if (x) *x = cachedX;
            if (y) *y = cachedY;
            return true;
        }
        cachedPressed = false;
        return false;
    }

    if (Wire.requestFrom((uint8_t)0x15, (uint8_t)5) != 5) {
        if (cachedPressed && (nowMs - lastValidTouchMs <= 25)) {
            if (x) *x = cachedX;
            if (y) *y = cachedY;
            return true;
        }
        cachedPressed = false;
        return false;
    }

    uint8_t packet[5];
    for (int i = 0; i < 5; i++) {
        packet[i] = Wire.read();
    }

    uint8_t fingerNum = packet[0];
    // Chip CST816D trả về số ngón tay tại bit [3:0] (1 hoặc 2). Nếu == 0 hoặc > 2 (nhiễu I2C 0xFF) -> Không có chạm
    if ((fingerNum & 0x0F) == 0 || (fingerNum & 0x0F) > 2) {
        cachedPressed = false;
        return false;
    }

    // Tọa độ gốc CST816D: raw_x [0..239], raw_y [0..319]
    uint16_t raw_x = ((packet[1] & 0x0F) << 8) | packet[2];
    uint16_t raw_y = ((packet[3] & 0x0F) << 8) | packet[4];

    // Ánh xạ sang Landscape 320x240 (ST7789 Rotation 1):
    // raw_y [0..319] tương ứng trục ngang 0..319
    // raw_x [0..239] tương ứng trục dọc 0..239 (nghịch đảo)
    uint16_t mappedX = raw_y;
    uint16_t mappedY = (raw_x <= 239) ? (239 - raw_x) : 0;

    mappedX = constrain(mappedX, (uint16_t)0, (uint16_t)319);
    mappedY = constrain(mappedY, (uint16_t)0, (uint16_t)239);

    cachedPressed = true;
    cachedX = mappedX;
    cachedY = mappedY;
    lastValidTouchMs = nowMs;

    if (x) *x = mappedX;
    if (y) *y = mappedY;
    return true;
#else
    uint16_t raw90, rawD0;
    if (!_readRaw(&raw90, &rawD0)) return false;

    int rawX = _calXis90 ? raw90 : rawD0;
    int rawY = _calXis90 ? rawD0 : raw90;

    long calX = map((long)rawX, _calXmin, _calXmax, CALIB_TL_X, CALIB_TR_X);
    long calY = map((long)rawY, _calYmin, _calYmax, CALIB_TL_Y, CALIB_BL_Y);

    *x = constrain(calX, 0, 319);
    *y = constrain(calY, 0, 239);
    return true;
#endif
}

bool TouchManager::checkTap(uint16_t *x, uint16_t *y) {
    if (isCalibrating()) {
        processCalibration();
        return false;
    }

    static unsigned long lastPressedTime = 0;
    uint16_t currX = 0, currY = 0;
    bool pressed = getTouch(&currX, &currY);
    unsigned long now = millis();

    if (pressed) {
        lastPressedTime = now;
        if (!_isTouching) {
            // Ngay khi vừa chạm ngón tay vào màn hình -> NHẬN LỆNH NGAY LẬP TỨC (Press event)
            if (now - _lastTapTime >= 75) {
                _isTouching = true;
                _touchStartTime = now;
                _startX = currX;
                _startY = currY;
                _lastX = currX;
                _lastY = currY;
                _lastTapTime = now;
                *x = currX;
                *y = currY;
                Serial.printf("[Touch] ✓ Tap registered on press at (%d, %d)\n", *x, *y);
                return true;
            }
        } else {
            // Đang giữ ngón tay trên màn hình (Long press / Auto-repeat hoặc Kéo trượt Slider)
            int dx = abs((int)currX - (int)_startX);
            int dy = abs((int)currY - (int)_startY);

            // 1. Hỗ trợ kéo trượt (Drag / Slide) siêu nhạy theo trục ngang CHỈ trong vùng Thanh Độ Sáng (Y <= 52)
            //    Giới hạn vùng Y <= 52 giúp các nút bấm bên dưới không bị nhảy đúp khi ngón tay rung nhẹ 2px!
            if (_startY <= 52 && currY <= 58 && abs((int)currX - (int)_lastX) >= 2 && (now - _lastTapTime >= 25)) {
                _lastTapTime = now;
                _lastX = currX;
                _lastY = currY;
                *x = currX;
                *y = currY;
                return true;
            }

            // 2. Giữ cố định tại chỗ quá 320ms -> Tự động lặp lại mỗi 140ms (Tăng/giảm độ sáng hoặc giờ báo thức)
            //    Không lặp lại ở vùng đáy màn hình (_startY >= 166) để tránh tự mở Dock rồi bấm nhầm nút Power!
            unsigned long heldDuration = now - _touchStartTime;
            if (_startY < 166 && heldDuration > 320 && (now - _lastTapTime >= 140)) {
                if (dx < 35 && dy < 35) {
                    _lastTapTime = now;
                    *x = _startX;
                    *y = _startY;
                    return true;
                }
            }
        }
    } else {
        // Nhấc ngón tay khỏi màn hình đủ 25ms mới reset trạng thái (chống đứt quãng khi kéo trượt)
        if (_isTouching && (now - lastPressedTime >= 25)) {
            _isTouching = false;
        }
    }
    return false;
}

// ── Calibration implementation ───────────────────────────────────────
void TouchManager::startCalibration() {
#ifndef BOARD_NICEMCU_28IPS
    _calibState = CALIB_WAIT_TL;
    _calibWaitRelease = true;
    Serial.println("[TouchManager] Calibration started — Touch the crosshairs");
#else
    _calibState = CALIB_NONE;
    Serial.println("[TouchManager] NiceMCU capacitive touch is factory calibrated, skipping calibration.");
#endif
}

void TouchManager::processCalibration() {
#ifdef BOARD_NICEMCU_28IPS
    return;
#endif
    if (_calibState == CALIB_DONE) {
        if (millis() - _calibDoneTime > 1500) {
            _calibState = CALIB_NONE;
            Serial.println("[TouchManager] Calibration finished. Resuming normal UI");
        }
        return;
    }

    uint16_t r90, rD0;
    bool pressed = _readRaw(&r90, &rD0);

    if (_calibWaitRelease) {
        if (!pressed) {
            _calibWaitRelease = false;
            delay(100);
        }
        return;
    }

    if (!pressed) return;

    hardware.playBeep(2400, 40);

    switch (_calibState) {
        case CALIB_WAIT_TL:
            _cal90_tl = r90; _calD0_tl = rD0;
            Serial.printf("[Calib] Point 1/3 (TL): 90=%d, D0=%d\n", r90, rD0);
            _calibState = CALIB_WAIT_TR;
            _calibWaitRelease = true;
            break;

        case CALIB_WAIT_TR:
            _cal90_tr = r90; _calD0_tr = rD0;
            Serial.printf("[Calib] Point 2/3 (TR): 90=%d, D0=%d\n", r90, rD0);
            _calibState = CALIB_WAIT_BL;
            _calibWaitRelease = true;
            break;

        case CALIB_WAIT_BL:
            _cal90_bl = r90; _calD0_bl = rD0;
            Serial.printf("[Calib] Point 3/3 (BL): 90=%d, D0=%d\n", r90, rD0);
            _computeCalibration();
            _saveCalibration();
            _calibState = CALIB_DONE;
            _calibDoneTime = millis();
            hardware.playBeep(2000, 100);
            delay(100);
            hardware.playBeep(2600, 100);
            break;

        default:
            break;
    }
}

void TouchManager::_computeCalibration() {
    int delta90 = abs((int)_cal90_tr - (int)_cal90_tl);
    int deltaD0 = abs((int)_calD0_tr - (int)_calD0_tl);

    if (delta90 > deltaD0) {
        _calXis90 = true;
        _calXmin  = _cal90_tl;
        _calXmax  = _cal90_tr;
        _calYmin  = _calD0_tl;
        _calYmax  = _calD0_bl;
    } else {
        _calXis90 = false;
        _calXmin  = _calD0_tl;
        _calXmax  = _calD0_tr;
        _calYmin  = _cal90_tl;
        _calYmax  = _cal90_bl;
    }

    Serial.printf("[Calib] Computed: XChannel=0x%02X, X[%d->%d], Y[%d->%d]\n",
                  _calXis90 ? 0x90 : 0xD0, _calXmin, _calXmax, _calYmin, _calYmax);
}

void TouchManager::_loadCalibration() {
    Preferences prefs;
    prefs.begin("touch_cal", true);
    _calibValid = prefs.getBool("valid", false);
    if (_calibValid) {
        _calXis90 = prefs.getBool("xIs90", true);
        _calXmin  = prefs.getShort("xMin", 350);
        _calXmax  = prefs.getShort("xMax", 3800);
        _calYmin  = prefs.getShort("yMin", 350);
        _calYmax  = prefs.getShort("yMax", 3800);
        Serial.printf("[TouchManager] Loaded from NVS 'touch_cal': XCh=0x%02X, X[%d->%d], Y[%d->%d]\n",
                      _calXis90 ? 0x90 : 0xD0, _calXmin, _calXmax, _calYmin, _calYmax);
    }
    prefs.end();
}

void TouchManager::_saveCalibration() {
    Preferences prefs;
    prefs.begin("touch_cal", false);
    prefs.putBool("valid", true);
    prefs.putBool("xIs90", _calXis90);
    prefs.putShort("xMin", _calXmin);
    prefs.putShort("xMax", _calXmax);
    prefs.putShort("yMin", _calYmin);
    prefs.putShort("yMax", _calYmax);
    prefs.end();
    _calibValid = true;
    Serial.println("[TouchManager] Calibration saved to NVS 'touch_cal'");
}

void TouchManager::_setDefaultCalibration() {
    // Default for CYD ESP32-2432S028 landscape orientation
    _calXis90 = false;
    _calXmin  = 3800; // Inverted X in landscape
    _calXmax  = 350;
    _calYmin  = 350;
    _calYmax  = 3800;
}
