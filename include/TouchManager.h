// ═══════════════════════════════════════════════════════════════════════
//  TouchManager.h — Touch controller for ESP32 CYD (XPT2046 Resistive)
//  Features: Auto-calibration state machine, NVS persistence, noise filter
// ═══════════════════════════════════════════════════════════════════════
#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>
#include "Config.h"

// Calibration state machine
enum CalibState {
    CALIB_NONE,       // Normal operation
    CALIB_WAIT_TL,    // Waiting for Top-Left touch (30, 30)
    CALIB_WAIT_TR,    // Waiting for Top-Right touch (290, 30)
    CALIB_WAIT_BL,    // Waiting for Bottom-Left touch (30, 210)
    CALIB_DONE        // Calibration complete
};

#define CALIB_TL_X  30
#define CALIB_TL_Y  30
#define CALIB_TR_X  290
#define CALIB_TR_Y  30
#define CALIB_BL_X  30
#define CALIB_BL_Y  210

class TouchManager {
public:
    TouchManager();
    void begin();

    // ── Touch Polling ─────────────────────────────────────────
    bool getTouch(uint16_t *x, uint16_t *y);
    bool checkTap(uint16_t *x, uint16_t *y);   // Clean debounced tap event
    bool isDown() const { return _isTouching; }

    // ── Calibration ───────────────────────────────────────────
    void startCalibration();
    bool isCalibrating() const { return _calibState != CALIB_NONE; }
    CalibState getCalibState() const { return _calibState; }
    bool isCalibrated() const { return _calibValid; }
    void processCalibration();

    // ── Hitbox Helper ─────────────────────────────────────────
    static bool inBox(int16_t tx, int16_t ty, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
        return (tx >= bx && tx < (bx + bw) && ty >= by && ty < (by + bh));
    }

private:
    uint16_t _xptReadChannel(uint8_t cmd);
    bool     _readRaw(uint16_t *raw90, uint16_t *rawD0);

    // State
    bool          _isTouching;
    unsigned long _touchStartTime;
    uint16_t      _startX, _startY;
    uint16_t      _lastX, _lastY;
    unsigned long _lastTapTime;

    // Calibration
    CalibState    _calibState;
    bool          _calibValid;
    bool          _calibWaitRelease;
    unsigned long _calibDoneTime;

    uint16_t _cal90_tl, _calD0_tl;
    uint16_t _cal90_tr, _calD0_tr;
    uint16_t _cal90_bl, _calD0_bl;

    bool     _calXis90;
    int16_t  _calXmin, _calXmax;
    int16_t  _calYmin, _calYmax;

    void _loadCalibration();
    void _saveCalibration();
    void _computeCalibration();
    void _setDefaultCalibration();
};

extern TouchManager touch;

#endif // TOUCH_MANAGER_H
