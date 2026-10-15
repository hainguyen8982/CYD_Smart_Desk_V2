// ═══════════════════════════════════════════════════════════════════════
//  Display.cpp — Premium 16-bit color rendering engine
//  CYD SmartDesk V2 — Page 0: Weather Clock (matches reference mockup)
//
//  Architecture Notes:
//  • Single full-screen TFT_eSprite (320×240 × 16-bit = 153,600 bytes)
//  • Allocated from main heap (~215KB available without DMA)
//  • All drawing is procedural (no bitmap images) to save flash/RAM
//  • Pastel blob decorations, sparkles, glass cards = premium feel
// ═══════════════════════════════════════════════════════════════════════
#include "Display.h"
#include "Language.h"
#include "WeatherService.h"
#include "FinanceService.h"
#include "TouchManager.h"
#include "vn_lunar.h"
#include "HardwareManager.h"
#include "DockIconsColor20.h"
#include "FabXLogo.h"
#include "DeskPet.h"
#include "DeskMood.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

Display display;

// ─── Display Core ───────────────────────────────────────────────────

// ─── Constructor ─────────────────────────────────────────────────────
Display::Display()
    : _tft(), _spr(&_tft), _sprReady(false), _lastRender(0),
      _currentPage(PAGE_CLOCK), _dockVisible(true), _lastDockActivity(0),
      _activePopup(POPUP_NONE), _currencyTab(0), _settingsTab(0),
      _alarmEnabled(false), _alarmHour(7), _alarmMinute(0),
      _alarmRinging(false), _alarmRingStart(0), _lastAlarmBeepTick(0),
      _alarmSnoozed(false), _alarmSnoozeUntil(0), _alarmLastTriggerDay(-1),
      _alarmLastTriggerMin(-1),
      _alarmAutoCloseTime(0),
      _calYear(0), _calMonth(0),
      _pomoMode(POMO_FOCUS), _pomoState(POMO_STOPPED),
      _pomoTotalSec(25 * 60), _pomoRemainingSec(25 * 60),
      _pomoSession(1), _pomoLastSecTick(0) {}

// ─── Begin ───────────────────────────────────────────────────────────
void Display::begin() {
    initLanguage();                // Load saved language (VI / EN) from NVS
    initTheme();                   // Load saved theme from NVS
    loadAlarmSettings();           // Load saved alarm settings from NVS
    deskPet.begin();               // Load saved Desk Pet mascot from NVS
    deskMood.begin();              // Load saved Desk Mood from NVS

    // Mount LittleFS and ensure clean storage (purge leftover wallpaper if any)
    if (LittleFS.begin(true)) {
        if (LittleFS.exists("/wallpaper.jpg")) {
            LittleFS.remove("/wallpaper.jpg");
            Serial.println("[Display] Purged legacy /wallpaper.jpg");
        }
    }

    _tft.init();
    delay(100);
    _tft.setRotation(1);           // Landscape 320×240
    _tft.invertDisplay(true);      // CYD panel requires color inversion
    _tft.fillScreen(C_BLACK);

    Serial.printf("[Display] Free heap: %u | Max alloc block: %u\n",
                  ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Allocate sprite: try 16-bit first (~153KB), fallback to 8-bit (~76KB) if RAM insufficient
    _spr.setColorDepth(16);
    void* buf = _spr.createSprite(SCREEN_W, SCREEN_H);
    if (!buf) {
        Serial.println("[Display] 16-bit sprite alloc failed (~153KB) -> Fallback to 8-bit (~76KB)...");
        _spr.setColorDepth(8);
        buf = _spr.createSprite(SCREEN_W, SCREEN_H);
    }

    if (buf) {
        _sprReady = true;
        _spr.setTextWrap(false);
        _spr.fillSprite(C_BLACK);
        _spr.pushSprite(0, 0);

        // Turn on backlight now that screen is cleanly filled with black
        hardware.setBacklight(85);

        Serial.printf("[Display] Sprite created OK! Depth: %d-bit (%d bytes)\n",
                      _spr.getColorDepth(),
                      SCREEN_W * SCREEN_H * (_spr.getColorDepth() / 8));

        // Run Particle Convergence Boot Animation!
        playBootAnimation();
    } else {
        Serial.println("[Display] ERROR: Both 16-bit and 8-bit Sprite alloc failed!");
        _sprReady = false;
        hardware.setBacklight(85);
        _tft.setTextColor(TFT_RED, C_BLACK);
        _tft.drawString("RAM Error: Sprite failed!", SCREEN_W / 2, SCREEN_H / 2 + 40, 2);
    }
}

// ─── FabX Logo & Boot Animation ──────────────────────────────────────
void Display::drawFabXLogo(int16_t x, int16_t y, uint8_t bloomAlpha) {
    if (!_sprReady) return;
    for (int16_t row = 0; row < FABX_LOGO_H; row++) {
        for (int16_t col = 0; col < FABX_LOGO_W; col++) {
            uint16_t color = pgm_read_word(&FABX_BITMAP[row * FABX_LOGO_W + col]);
            if (color != 0x0000) {
                if (bloomAlpha > 0) {
                    color = blendColor(color, C_WHITE, bloomAlpha);
                }
                _spr.drawPixel(x + col, y + row, color);
            }
        }
    }
}

void Display::playBootAnimation() {
    if (!_sprReady) return;

    struct ParticleState {
        float startX, startY;
        int16_t targetX, targetY;
        float nx, ny;
        float curve;
        uint16_t color;
        uint16_t delayMs;
        uint16_t durationMs;
    };

    ParticleState* particles = (ParticleState*)malloc(sizeof(ParticleState) * FABX_PARTICLE_COUNT);
    if (!particles) {
        Serial.println("[Display] Boot animation malloc failed -> Fallback to static splash");
        showBootStatus("Starting System...", 15);
        return;
    }

    const int16_t logoX = (SCREEN_W - FABX_LOGO_W) / 2; // (320 - 136) / 2 = 92
    const int16_t logoY = 46;

    for (int i = 0; i < FABX_PARTICLE_COUNT; i++) {
        FabXParticleTarget pt;
        memcpy_P(&pt, &FABX_TARGETS[i], sizeof(FabXParticleTarget));

        particles[i].targetX = logoX + pt.x;
        particles[i].targetY = logoY + pt.y;
        particles[i].color   = pt.color;

        // Spawn particles spread widely outside the 4 edges of the screen
        int edge = random(0, 4);
        if (edge == 0) { // Top
            particles[i].startX = (float)random(-40, SCREEN_W + 40);
            particles[i].startY = (float)random(-60, -10);
        } else if (edge == 1) { // Bottom
            particles[i].startX = (float)random(-40, SCREEN_W + 40);
            particles[i].startY = (float)random(SCREEN_H + 10, SCREEN_H + 60);
        } else if (edge == 2) { // Left
            particles[i].startX = (float)random(-60, -10);
            particles[i].startY = (float)random(-40, SCREEN_H + 40);
        } else { // Right
            particles[i].startX = (float)random(SCREEN_W + 10, SCREEN_W + 60);
            particles[i].startY = (float)random(-40, SCREEN_H + 40);
        }

        // Calculate normal vector for graceful curving/vortex trajectories
        float dx = particles[i].targetX - particles[i].startX;
        float dy = particles[i].targetY - particles[i].startY;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 1.0f) {
            particles[i].nx = -dy / dist;
            particles[i].ny =  dx / dist;
        } else {
            particles[i].nx = 0.0f;
            particles[i].ny = 0.0f;
        }

        particles[i].curve      = (float)random(-42, 43); // Arc deviation in pixels
        particles[i].delayMs    = random(0, 650);         // Staggered launch up to 650ms
        particles[i].durationMs = random(1300, 1750);     // 1.3s - 1.75s travel time
    }

    unsigned long animStart = millis();
    const unsigned long totalDuration = 2400; // 2.4s cinematic sequence

    while (millis() - animStart < totalDuration) {
        unsigned long elapsed = millis() - animStart;

        _spr.fillSprite(C_BLACK);

        for (int i = 0; i < FABX_PARTICLE_COUNT; i++) {
            if (elapsed < particles[i].delayMs) continue;

            float t = (float)(elapsed - particles[i].delayMs) / (float)particles[i].durationMs;
            if (t >= 1.0f) {
                // Particle has reached target -> draw solid pixel at final position
                _spr.drawPixel(particles[i].targetX, particles[i].targetY, particles[i].color);
                continue;
            }

            // Smooth cubic ease out: 1 - (1-t)^3
            float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            float dx = particles[i].targetX - particles[i].startX;
            float dy = particles[i].targetY - particles[i].startY;

            // Arc curvature via sine envelope: 0 at start, max at mid-flight, 0 at target
            float arc = sinf(t * 3.14159265f) * particles[i].curve;

            float curX = particles[i].startX + ease * dx + particles[i].nx * arc;
            float curY = particles[i].startY + ease * dy + particles[i].ny * arc;

            // Render subtle neon trail behind moving particle
            if (t > 0.06f) {
                float tTrail = t - 0.05f;
                float easeTrail = 1.0f - (1.0f - tTrail) * (1.0f - tTrail) * (1.0f - tTrail);
                float arcTrail = sinf(tTrail * 3.14159265f) * particles[i].curve;
                float trX = particles[i].startX + easeTrail * dx + particles[i].nx * arcTrail;
                float trY = particles[i].startY + easeTrail * dy + particles[i].ny * arcTrail;
                _spr.drawPixel((int16_t)trX, (int16_t)trY, blendColor(particles[i].color, C_BLACK, 160));
            }

            // Render particle head (2x2 glowing block during flight, 1px near target)
            if (t < 0.90f) {
                _spr.fillRect((int16_t)curX, (int16_t)curY, 2, 2, particles[i].color);
            } else {
                _spr.drawPixel((int16_t)curX, (int16_t)curY, particles[i].color);
            }
        }

        _spr.pushSprite(0, 0);
        delay(14); // ~60 FPS cap
    }

    free(particles);

    // 1. Chữ FabX bừng sáng chói lòa rồi từ từ dịu về màu logo chuẩn (Bloom Flare Decay)
    const uint8_t flareSteps[] = { 240, 180, 120, 60, 0 };
    for (int i = 0; i < 5; i++) {
        _spr.fillSprite(C_BLACK);
        drawFabXLogo(logoX, logoY, flareSteps[i]);
        _spr.pushSprite(0, 0);
        delay(35);
    }

    // 2. Tia Laser khắc ngang dưới chân logo mở ra dòng chữ "S M A R T   D E S K"
    const int16_t laserY = 98;
    const int16_t maxLaserHalfW = 85; // Quét từ 160 ra hai phía (85 -> 235)
    for (int step = 1; step <= 8; step++) {
        _spr.fillSprite(C_BLACK);
        drawFabXLogo(logoX, logoY, 0); // Logo hiển thị màu gốc 100%

        int16_t w = (maxLaserHalfW * step) / 8;
        // Đường laser xanh ngọc neon
        _spr.drawFastHLine(SCREEN_W / 2 - w, laserY, w * 2, 0x0672);
        // 2 điểm sáng laser rực rỡ ở 2 đầu vết cắt
        _spr.fillCircle(SCREEN_W / 2 - w, laserY, 1, C_WHITE);
        _spr.fillCircle(SCREEN_W / 2 + w, laserY, 1, C_WHITE);

        // Chữ SMART DESK hiện dần theo đường laser quét
        uint8_t textFade = (step * 255) / 8;
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(blendColor(0x0000, C_WHITE, textFade), C_BLACK);
        _spr.drawString("S M A R T   D E S K", SCREEN_W / 2, 114, 2);

        _spr.pushSprite(0, 0);
        delay(22);
    }

    // Khung hình hoàn thiện sạch sẽ trước khi chuyển sang boot status
    _spr.fillSprite(C_BLACK);
    drawFabXLogo(logoX, logoY, 0);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE, C_BLACK);
    _spr.drawString("S M A R T   D E S K", SCREEN_W / 2, 114, 2);
    _spr.pushSprite(0, 0);
    delay(40);

    // Initial status display
    showBootStatus("Starting System...", 15);
}

void Display::showBootStatus(const char* status, uint8_t progressPercent) {
    if (!status) return;

    if (_sprReady) {
        _spr.fillSprite(C_BLACK);

        // 1. FabX Logo
        const int16_t logoX = (SCREEN_W - FABX_LOGO_W) / 2;
        const int16_t logoY = 46;
        drawFabXLogo(logoX, logoY);

        // 2. Subtitle: S M A R T   D E S K
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE, C_BLACK);
        _spr.drawString("S M A R T   D E S K", SCREEN_W / 2, 112, 2);

        // 3. High-tech Progress Bar
        const int16_t barX = 45;
        const int16_t barY = 152;
        const int16_t barW = 230;
        const int16_t barH = 5;

        // Background track (dark slate)
        _spr.fillRoundRect(barX, barY, barW, barH, 2, 0x18E3);
        _spr.drawRoundRect(barX - 1, barY - 1, barW + 2, barH + 2, 3, 0x2965);

        // Active fill (Gradient: Royal Blue 0x13BA -> Cyan 0x063F -> Neon Green 0x0672)
        uint8_t pct = constrain(progressPercent, 5, 100);
        int16_t fillW = (barW * pct) / 100;
        if (fillW > 0) {
            for (int16_t x = 0; x < fillW; x++) {
                float r = (float)x / (float)barW;
                uint16_t gradColor;
                if (r < 0.6f) {
                    gradColor = blendColor(0x13BA, 0x063F, (uint8_t)((r / 0.6f) * 255));
                } else {
                    gradColor = blendColor(0x063F, 0x0672, (uint8_t)(((r - 0.6f) / 0.4f) * 255));
                }
                _spr.drawFastVLine(barX + x, barY, barH, gradColor);
            }
            _spr.fillCircle(barX + fillW, barY + barH / 2, 2, C_WHITE);
        }

        // 4. Status text
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_CYAN, C_BLACK);
        _spr.drawString(status, SCREEN_W / 2, 176, 2);

        // 5. Tech info & footer
        char pctBuf[32];
        snprintf(pctBuf, sizeof(pctBuf), "BOOTLOADER  •  %d%%", pct);
        _spr.setTextColor(0x8410, C_BLACK); // Muted slate gray
        _spr.drawString(pctBuf, SCREEN_W / 2, 196, 1);

        _spr.setTextColor(0x4208, C_BLACK);
        _spr.drawString("FABX SMARTDESK V2  •  COMMERCIAL EDITION", SCREEN_W / 2, 222, 1);

        _spr.pushSprite(0, 0);
    } else {
        // Fallback directly to TFT if sprite not ready
        _tft.fillRect(0, 160, SCREEN_W, 60, C_BLACK);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(C_CYAN, C_BLACK);
        _tft.drawString(status, SCREEN_W / 2, 180, 2);
    }
}

void Display::update() {
    if (!_sprReady) return;

    // Duy trì bộ đếm Pomodoro chạy ngầm liên tục
    pomoTick();

    // Duy trì kiểm tra Báo thức chạy ngầm liên tục
    checkAlarmTick();

    unsigned long now = millis();
    if (now - _lastRender < DISPLAY_FPS_INTERVAL) return;
    _lastRender = now;

    // If in touch calibration mode, render crosshair overlay
    if (touch.isCalibrating()) {
        renderCalibrationScreen();
        _spr.pushSprite(0, 0);
        return;
    }

    // NẾU BÁO THỨC ĐANG KÊU: RENDER TOÀN MÀN HÌNH BÁO THỨC (Ưu tiên tuyệt đối)
    if (_alarmRinging) {
        renderAlarmRingingScreen();
        _spr.pushSprite(0, 0);
        return;
    }

    // NẾU ĐANG MỞ POPUP BÁO THỨC: ĐÓNG BĂNG VẼ LẠI NỀN ĐỂ CHỐNG NHẤP NHÁY TUYỆT ĐỐI
    if (_activePopup == POPUP_ALARM) {
        // Tự động đóng popup sau 3 giây nếu được mở từ thao tác bật chuông
        if (_alarmAutoCloseTime > 0 && millis() > _alarmAutoCloseTime) {
            _alarmAutoCloseTime = 0;
            _activePopup = POPUP_NONE;
            _lastRender = 0;
        }
        return;
    }

    // Check dock auto-hide timeout (5 seconds of inactivity)
    if (_dockVisible && (now - _lastDockActivity > 5000)) {
        _dockVisible = false;
    }

    switch (_currentPage) {
        case PAGE_CLOCK:    renderPage0_WeatherClock(); break;
        case PAGE_CALENDAR: renderPage1_Calendar();     break;
        case PAGE_FINANCE:  renderPage2_Finance();      break;
        case PAGE_POMODORO: renderPage3_Pomodoro();     break;
        case PAGE_MOOD:     renderPage4_Mood();         break;
        case PAGE_SETTINGS: renderPage5_Settings();     break;
        default:            renderPage0_WeatherClock(); break;
    }

    // If popup modal is active, render it over page; otherwise render dock
    if (_activePopup == POPUP_WEATHER) {
        renderWeatherPopup();
    } else if (_activePopup == POPUP_LUNAR) {
        renderLunarPopup();
    } else if (_activePopup == POPUP_ALARM) {
        renderAlarmPopup();
    } else if (_activePopup == POPUP_FINANCE_GOLD) {
        renderFinanceGoldPopup();
    } else if (_activePopup == POPUP_FINANCE_CURRENCY) {
        renderFinanceCurrencyPopup();
    } else {
        renderDock();
    }

    _spr.pushSprite(0, 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 0 — WEATHER CLOCK  (reference: user mockup image)
//
//  Layout (320×240 landscape):
//  ┌──────────────────────────────────────────────────────────────────┐
//  │  [Pastel blob background with sparkles]                         │
//  │                                                                  │
//  │   08:30           ┌─── Glass Card ────┐                         │
//  │   FRIDAY, OCT 27  │ ☀️☁️ weather icon  │                         │
//  │                   │    24°C            │                         │
//  │   Waxing Gibbous  │    SUNNY           │                         │
//  │   🌕 Oct 27 75%   │ 💧65% 💨12 ☀️UV:4  │                         │
//  │                   └───────────────────┘                         │
//  │  ┌─ HOME ─┐  ┌─ BACK ─┐  ┌─ MENU ─┐                           │
//  └──────────────────────────────────────────────────────────────────┘
// ─── Mini icons for Weather Metrics (Water Drop, Wind, UV) ───────────
static void drawMiniDrop(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.fillCircle(x, y + 2, 2, color);
    spr.drawPixel(x, y - 2, color);
    spr.drawLine(x - 2, y + 2, x, y - 2, color);
    spr.drawLine(x + 2, y + 2, x, y - 2, color);
}

static void drawMiniWind(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.drawFastHLine(x - 4, y - 2, 8, color);
    spr.drawPixel(x + 4, y - 3, color);
    spr.drawFastHLine(x - 2, y + 2, 6, color);
    spr.drawPixel(x + 4, y + 3, color);
}

static void drawMiniSun(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.fillCircle(x, y, 2, color);
    spr.drawPixel(x, y - 4, color);
    spr.drawPixel(x, y + 4, color);
    spr.drawPixel(x - 4, y, color);
    spr.drawPixel(x + 4, y, color);
}

static void drawMiniMoon(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.fillCircle(x, y, 3, color);
    spr.fillCircle(x + 1, y - 1, 3, RGB565(15, 20, 32)); // crescent shadow
}

static void drawMiniStar(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.drawFastVLine(x, y - 3, 7, color);
    spr.drawFastHLine(x - 3, y, 7, color);
    spr.drawPixel(x, y, C_WHITE);
}

static void drawMiniCal(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.drawRoundRect(x - 3, y - 3, 7, 7, 1, color);
    spr.drawFastHLine(x - 3, y - 1, 7, color);
    spr.drawPixel(x - 1, y - 4, color);
    spr.drawPixel(x + 1, y - 4, color);
}

static void drawMiniHeart(TFT_eSprite& spr, int16_t x, int16_t y, uint16_t color) {
    spr.fillCircle(x - 2, y - 2, 2, color);
    spr.fillCircle(x + 2, y - 2, 2, color);
    spr.fillTriangle(x - 4, y - 1, x + 4, y - 1, x, y + 4, color);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 0 — WEATHER CLOCK (Matches user reference image)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage0_WeatherClock() {
    // ────────────────────────────────────────────────────────────────
    // 1. BACKGROUND (Custom Wallpaper OR Deep Obsidian with Ambient Aura)
    // ────────────────────────────────────────────────────────────────
    drawBackground();

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    LunarDate lDate = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1, 1900 + t.tm_year);

    // ────────────────────────────────────────────────────────────────
    // 2. GIANT HERO CLOCK — Shifted down to clockCy = 55 for perfect screen balance
    // ────────────────────────────────────────────────────────────────
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);

    int16_t clockCx = 160;
    int16_t clockCy = 55;

    _spr.setTextDatum(MC_DATUM);

    // Layer 1: Ambient deep cyan glow halo
    _spr.setTextColor(RGB565(0, 70, 150));
    _spr.drawString(timeBuf, clockCx - 2, clockCy, 8);
    _spr.drawString(timeBuf, clockCx + 2, clockCy, 8);
    _spr.drawString(timeBuf, clockCx, clockCy - 2, 8);
    _spr.drawString(timeBuf, clockCx, clockCy + 2, 8);

    // Layer 2: Bright cyan rim
    _spr.setTextColor(RGB565(0, 190, 255));
    _spr.drawString(timeBuf, clockCx - 1, clockCy - 1, 8);
    _spr.drawString(timeBuf, clockCx + 1, clockCy - 1, 8);
    _spr.drawString(timeBuf, clockCx - 1, clockCy + 1, 8);
    _spr.drawString(timeBuf, clockCx + 1, clockCy + 1, 8);

    // Layer 3: Solid Electric Neon Cyan body (matching reference image)
    _spr.setTextColor(RGB565(0, 245, 255));
    _spr.drawString(timeBuf, clockCx, clockCy, 8);

    // ── Alarm Bell Icon (Right next to clock digits, NO text time) ──
    int16_t timeW = _spr.textWidth(timeBuf, 8);
    int16_t bellX = clockCx + (timeW / 2) + 16;
    if (bellX < 268) bellX = 268;
    if (bellX > 298) bellX = 298;
    drawBellIcon(bellX, 34, _alarmEnabled);

    // ────────────────────────────────────────────────────────────────
    // 3. DATE LINE — Centered with ample space (Y = 107, Font 2, Pure White)
    // ────────────────────────────────────────────────────────────────
    char dateBuf[40];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
             getDayNameI18n(t.tm_wday),
             t.tm_mday, t.tm_mon + 1, 1900 + t.tm_year);

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(dateBuf, 160, 107, 2);

    // ────────────────────────────────────────────────────────────────
    // 4. TWO EXPANDED GLASS CARDS (Y = 129..211, H = 82, W = 146)
    //    Balanced full-screen symmetry with floating dock overlay!
    // ────────────────────────────────────────────────────────────────
    const WeatherData& w = weather.getData();
    WeatherType wType = weather.getType();
    bool night = weather.isNight();

    int16_t cardY = 129;
    int16_t cardH = 82;
    int16_t cardW = 146;

    // ── LEFT CARD: THOI TIET (X = 10..156, H = 82) ──────────────────
    int16_t leftX = 10;
    drawGlassCard(leftX, cardY, cardW, cardH, 12);

    // Header (Y = cardY + 11): GPS Icon + City Name (Font 2)
    _spr.fillCircle(leftX + 14, cardY + 11, 2, C_RED);
    _spr.fillTriangle(leftX + 12, cardY + 11, leftX + 16, cardY + 11, leftX + 14, cardY + 15, C_RED);

    const char* cityStr = (strlen(w.city) > 0) ? w.city : "TP. HCM";
    char cityUpper[20];
    size_t cj = 0;
    for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
        cityUpper[cj] = (char)toupper((unsigned char)cityStr[cj]);
    }
    cityUpper[cj] = '\0';
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cityUpper, leftX + 22, cardY + 11, 2);

    // Hint: small touch arrow ">" at top right
    _spr.setTextColor(RGB565(100, 150, 200));
    _spr.drawString(">", leftX + cardW - 12, cardY + 11, 1);

    // Divider line (Soft neutral light gray)
    _spr.drawFastHLine(leftX + 8, cardY + 22, cardW - 16, RGB565(110, 115, 125));

    // Main Row: Weather Icon (prominent 32px) + Temp (Font 4) + Condition
    // Icon placed with ample breathing room below divider line
    int16_t iconCx = leftX + 24;
    int16_t iconCy = cardY + 49;
    drawWeatherIcon(iconCx, iconCy, wType, night);

    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%.0f", w.valid ? w.temp : 29.0f);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(tempStr, leftX + 48, cardY + 31, 4);

    int16_t tNumW = _spr.textWidth(tempStr, 4);
    int16_t degX = leftX + 48 + tNumW + 2;
    _spr.drawCircle(degX + 2, cardY + 34, 2, C_GOLD);
    _spr.setTextColor(C_GOLD);
    _spr.drawString("C", degX + 7, cardY + 33, 2);

    // Condition text: spaced 6px cleanly below 29°C for relaxed readability
    const char* condStr = getWeatherConditionI18n(wType, night, false);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(condStr, leftX + 50, cardY + 60, 1);

    // ── RIGHT CARD: AM LICH (X = 164..310, H = 82) ──────────────────
    int16_t rightX = 164;
    drawGlassCard(rightX, cardY, cardW, cardH, 12);

    uint8_t lunarDay = lDate.day;
    const char* phaseName = getMoonPhaseNameI18n(lunarDay);

    // Header (Y = cardY + 11): Phase Name (Font 2)
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(phaseName, rightX + 14, cardY + 11, 2);

    // Hint: small touch arrow ">" at top right
    _spr.setTextColor(RGB565(100, 150, 200));
    _spr.drawString(">", rightX + cardW - 12, cardY + 11, 1);

    // Divider line (Soft neutral light gray)
    _spr.drawFastHLine(rightX + 8, cardY + 22, cardW - 16, RGB565(110, 115, 125));

    // Main Row: Moon Icon (28px) + Lunar Date (Font 4) + Can Chi Ngay on separate line
    // Centered at cardY + 49 to align horizontally with weather icon
    int16_t moonCx = rightX + 24;
    int16_t moonCy = cardY + 49;
    drawMoonIcon(moonCx, moonCy, 14, lunarDay);

    char alDayStr[16];
    snprintf(alDayStr, sizeof(alDayStr), "%02d/%02d", lDate.day, lDate.month);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(alDayStr, rightX + 46, cardY + 31, 4);

    _spr.setTextColor(RGB565(140, 205, 255));
    _spr.drawString((getLanguage() == LANG_VI ? "AL" : "LUN"), rightX + 46 + _spr.textWidth(alDayStr, 4) + 3, cardY + 33, 2);

    // Can Chi day placed 6px cleanly below 04/09 AL — NEVER overlaps!
    char dayCanChiBuf[28];
    snprintf(dayCanChiBuf, sizeof(dayCanChiBuf), (getLanguage() == LANG_VI ? "Ngay %s" : "Day %s"), lDate.dayCanChi);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(dayCanChiBuf, rightX + 50, cardY + 60, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP COMMON UTILITIES & WINDOWS 11 STYLE CLOSE BUTTON
// ═══════════════════════════════════════════════════════════════════════
#define POPUP_DIVIDER_COLOR RGB565(75, 75, 85) // Unified neutral cool slate gray for all popups

static void toVNFloat(char* str) {
    if (!str || getLanguage() != LANG_VI) return;
    for (char* p = str; *p; p++) {
        if (*p == '.') *p = ',';
    }
}

void Display::drawPopupCloseButton(int16_t x, int16_t y) {
    // Nút Close Windows 11: 4 góc bo tròn mềm mại thật sự (R=8), không vát cạnh
    uint16_t bg = _spr.readPixel(x - 1, y + 13);
    if (bg == 0) bg = RGB565(8, 14, 24);
    _spr.fillSmoothRoundRect(x, y, 26, 26, 8, RGB565(45, 110, 205), bg);
    _spr.fillSmoothRoundRect(x + 1, y + 1, 24, 24, 7, RGB565(18, 60, 125), RGB565(45, 110, 205));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("X", x + 13, y + 13, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 1: CHI TIET THOI TIET (Option B: Fullscreen Sheet + Header Gradient)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderWeatherPopup() {
    // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không dải gradient
    _spr.fillRect(0, 0, 320, 240, RGB565(8, 14, 24));

    const WeatherData& w = weather.getData();
    WeatherType wType = weather.getType();
    bool night = weather.isNight();

    // 1. Header (Y = 10..38): GPS Pin + Tên thành phố + Nút đóng tròn [X]
    _spr.fillCircle(18, 23, 3, C_RED);
    _spr.fillTriangle(16, 23, 20, 23, 18, 28, C_RED);
    const char* cityStr = (strlen(w.city) > 0) ? w.city : "TP. HO CHI MINH";
    char cityUpper[28];
    size_t cj = 0;
    for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
        cityUpper[cj] = (char)toupper((unsigned char)cityStr[cj]);
    }
    cityUpper[cj] = '\0';
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cityUpper, 28, 23, 2);

    // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc mềm mại
    drawPopupCloseButton();

    // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

    // ══════════════════════════════════════════════════════════════════
    // TẦNG 1: Thời tiết hiện tại (X: 10..310, Y: 46..106)
    // ══════════════════════════════════════════════════════════════════
    // Cột trái: Icon thời tiết lớn (tâm cx = 38, cy = 76)
    drawWeatherIcon(38, 76, wType, night);

    // Cột phải: infoX = 74
    int16_t infoX = 74;

    // Dòng 1 (Y = 56): Nhiệt độ to + Trạng thái
    char tempNum[8];
    snprintf(tempNum, sizeof(tempNum), "%.0f", w.valid ? w.temp : 29.0f);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(tempNum, infoX, 56, 4);

    int16_t numW = _spr.textWidth(tempNum, 4);
    int16_t degX = infoX + numW + 2;
    _spr.drawCircle(degX + 2, 59, 2, C_GOLD);
    _spr.setTextColor(C_GOLD);
    _spr.drawString("C", degX + 7, 58, 2);

    // Dấu chấm phân cách •
    _spr.setTextColor(RGB565(90, 130, 180));
    _spr.drawString("•", degX + 23, 58, 2);

    const char* condStr = getWeatherConditionI18n(wType, night, true);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(condStr, degX + 33, 58, 2);

    // Dòng 2 (Y = 88): 3 thông số Độ ẩm • Gió • UV rộng rãi
    _spr.setTextDatum(ML_DATUM);

    char humStr[16];
    snprintf(humStr, sizeof(humStr), "%d%%", w.valid ? w.humidity : 78);
    char windStr[20];
    snprintf(windStr, sizeof(windStr), "%.1f km/h", w.valid ? w.windSpeed : 7.2f);
    toVNFloat(windStr);
    char uvStr[16];
    float uv = w.valid ? w.uvIndex : 6.0f;
    snprintf(uvStr, sizeof(uvStr), "UV %.0f", uv);

    const int16_t gap = 12;
    int16_t currX = infoX;

    // 1. Độ ẩm (Giọt nước)
    drawMiniDrop(_spr, currX + 2, 87, C_CYAN);
    _spr.setTextColor(RGB565(170, 205, 235));
    _spr.drawString(humStr, currX + 11, 88, 1);
    currX += 11 + _spr.textWidth(humStr, 1) + gap;

    // Dấu •
    _spr.setTextColor(RGB565(80, 110, 150));
    _spr.drawString("•", currX, 88, 1);
    currX += _spr.textWidth("•", 1) + gap;

    // 2. Gió (Cánh gió)
    drawMiniWind(_spr, currX + 2, 88, RGB565(120, 210, 255));
    _spr.setTextColor(RGB565(170, 205, 235));
    _spr.drawString(windStr, currX + 12, 88, 1);
    currX += 12 + _spr.textWidth(windStr, 1) + gap;

    // Dấu •
    _spr.setTextColor(RGB565(80, 110, 150));
    _spr.drawString("•", currX, 88, 1);
    currX += _spr.textWidth("•", 1) + gap;

    // 3. UV (Mặt trời mini)
    drawMiniSun(_spr, currX + 2, 88, C_GOLD);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(uvStr, currX + 12, 88, 1);

    // Vạch kẻ phân cách tầng 1 và tầng 2 (Y = 110) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 110, 300, POPUP_DIVIDER_COLOR);

    // ══════════════════════════════════════════════════════════════════
    // TẦNG 2: 3 Cột Dự Báo Thời Tiết Rộng Rãi (W = 300 / 3 = 100px)
    // ══════════════════════════════════════════════════════════════════
    int16_t colW = 100;
    int16_t startX = 10;

    // 2 vạch phân cách dọc nhạt màu tinh tế giữa 3 ngày dự báo (Không bị ám xanh)
    _spr.drawFastVLine(startX + colW, 118, 110, RGB565(50, 50, 45));
    _spr.drawFastVLine(startX + colW * 2, 118, 110, RGB565(50, 50, 45));

    for (int d = 0; d < 3; d++) {
        int16_t colCenterX = startX + d * colW + colW / 2;

        time_t fNow = time(NULL);
        struct tm fTm;
        localtime_r(&fNow, &fTm);
        int fWday = (fTm.tm_wday + d + 1) % 7;

        // 1. Tên Thứ (Font 2)
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString(getShortDayNameI18n(fWday), colCenterX, 128, 2);

        // 2. Weather Icon mini
        drawForecastIcon(colCenterX, 156, w.forecast[d].type);

        // 3. Trạng thái (Font 1, Gold)
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(255, 215, 0));
        _spr.drawString(getWeatherConditionI18n(w.forecast[d].type, false, false), colCenterX, 182, 1);

        // 4. Nhiệt độ Min - Max
        char tRange[16];
        snprintf(tRange, sizeof(tRange), "%.0f - %.0f C", w.forecast[d].tempMin, w.forecast[d].tempMax);
        _spr.setTextColor(RGB565(200, 225, 245));
        _spr.drawString(tRange, colCenterX, 206, 1);
        _spr.drawCircle(colCenterX + _spr.textWidth(tRange, 1) / 2 - 6, 203, 1, C_GOLD);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 2: CHI TIET THIEN VAN & AM LICH (Option B: Fullscreen Sheet + Header Gradient)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderLunarPopup() {
    // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không dải gradient
    _spr.fillRect(0, 0, 320, 240, RGB565(10, 14, 22));

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    LunarDate ld = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1, 1900 + t.tm_year);

    uint8_t lunarDay = ld.day;
    float moonPct = 0;
    if (lunarDay <= 1)       { moonPct = 0; }
    else if (lunarDay <= 6)  { moonPct = lunarDay * 6.7f; }
    else if (lunarDay <= 8)  { moonPct = 50; }
    else if (lunarDay <= 13) { moonPct = 50 + (lunarDay - 8) * 6.0f; }
    else if (lunarDay <= 16) { moonPct = 100; }
    else if (lunarDay <= 21) { moonPct = 100 - (lunarDay - 16) * 6.0f; }
    else if (lunarDay <= 23) { moonPct = 50; }
    else                     { moonPct = 50 - (lunarDay - 23) * 7.0f; }
    const char* phaseName = getMoonPhaseNameI18n(lunarDay);

    // 1. Header (Y = 10..38): Tiêu đề + Nút đóng tròn [X]
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0));
    _spr.drawString(L(TXT_ASTRONOMY_LUNAR), 16, 23, 2);

    // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
    drawPopupCloseButton();

    // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

    // 2. Mặt trăng lớn (R = 24, đường kính 48px) + Tên tuần trăng + Độ sáng
    int16_t moonCy = 80;
    drawMoonIcon(48, moonCy, 24, lunarDay, RGB565(10, 14, 22));

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(phaseName, 88, 68, 4);

    char illuBuf[32];
    snprintf(illuBuf, sizeof(illuBuf), (getLanguage() == LANG_VI ? "Do sang mat trang: %.0f%%" : "Moon Illumination: %.0f%%"), moonPct);
    _spr.setTextColor(RGB565(140, 205, 255));
    _spr.drawString(illuBuf, 88, 96, 2);

    // Vạch kẻ phân cách tầng 1 và tầng 2 (Y = 118) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 118, 300, POPUP_DIVIDER_COLOR);

    // 3. Danh sách thông số Âm lịch & Can Chi (Rộng rãi 320px)
    // Row 1: Ngày âm lịch
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString(L(TXT_LUNAR_DATE), 18, 138, 2);
    char lDateBuf[24];
    snprintf(lDateBuf, sizeof(lDateBuf), (getLanguage() == LANG_VI ? "%02d thang %02d" : "Day %02d, Month %02d"), ld.day, ld.month);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(lDateBuf, 302, 138, 2);

    // Row 2: Ngày can chi
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString(L(TXT_DAY_CANCHI), 18, 164, 2);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(ld.dayCanChi, 302, 164, 2);

    // Row 3: Năm can chi
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString(L(TXT_YEAR_CANCHI), 18, 190, 2);
    char yrBuf[32];
    snprintf(yrBuf, sizeof(yrBuf), "%s (2026)", ld.yearCanChi);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(yrBuf, 302, 190, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  ALARM ENGINE & POPUP MODAL (100% Front-screen touch operated)
// ═══════════════════════════════════════════════════════════════════════
void Display::loadAlarmSettings() {
    Preferences prefs;
    prefs.begin("cyd_alarm", true);
    _alarmEnabled = prefs.getBool("enabled", false);
    _alarmHour    = prefs.getUChar("hour", 7);
    _alarmMinute  = prefs.getUChar("min", 0);
    String msg    = prefs.getString("msg", "Uong nuoc & Thu gian");
    strlcpy(_alarmMessage, msg.c_str(), sizeof(_alarmMessage));
    prefs.end();
    if (_alarmHour > 23) _alarmHour = 7;
    if (_alarmMinute > 59) _alarmMinute = 0;
    Serial.printf("[Alarm] Loaded: %s %02d:%02d, Msg: %s\n",
                  _alarmEnabled ? "BAT" : "TAT", _alarmHour, _alarmMinute, _alarmMessage);
}

void Display::saveAlarmSettings() {
    Preferences prefs;
    prefs.begin("cyd_alarm", false);
    prefs.putBool("enabled", _alarmEnabled);
    prefs.putUChar("hour", _alarmHour);
    prefs.putUChar("min", _alarmMinute);
    prefs.putString("msg", _alarmMessage);
    prefs.end();
    Serial.printf("[Alarm] Saved: %s %02d:%02d, Msg: %s\n",
                  _alarmEnabled ? "BAT" : "TAT", _alarmHour, _alarmMinute, _alarmMessage);
}

void Display::setAlarm(uint8_t h, uint8_t m, bool enabled, const char* msg) {
    _alarmHour = (h <= 23) ? h : 7;
    _alarmMinute = (m <= 59) ? m : 0;
    _alarmEnabled = enabled;
    _alarmLastTriggerDay = -1;
    _alarmLastTriggerMin = -1;
    if (msg != nullptr) {
        strlcpy(_alarmMessage, msg, sizeof(_alarmMessage));
    }
    saveAlarmSettings();
    _lastRender = 0;
}

void Display::checkAlarmTick() {
    unsigned long now = millis();

    // 1. Khi báo thức đang reo
    if (_alarmRinging) {
        // Tự tắt sau 3 phút nếu không có ai chạm màn hình (tránh nóng loa)
        if (now - _alarmRingStart > 180000UL) {
            dismissAlarm();
            return;
        }

        // Tạo chuỗi âm báo thức cách quãng 900ms (non-blocking)
        if (now - _lastAlarmBeepTick >= 900) {
            _lastAlarmBeepTick = now;
            hardware.playBeep(2400, 100);
            delay(35);
            hardware.playBeep(2800, 120);
        }
        return;
    }

    // 2. Kiểm tra kích hoạt báo thức
    if (!_alarmEnabled) return;

    time_t nowSec = time(NULL);
    struct tm t;
    localtime_r(&nowSec, &t);

    // Xử lý snooze
    if (_alarmSnoozed) {
        if (nowSec >= _alarmSnoozeUntil) {
            _alarmSnoozed = false;
            triggerAlarm();
        }
        return;
    }

    // Kích hoạt theo giờ cài đặt (chỉ kích hoạt 1 lần cho cùng 1 phút đã reo)
    if (t.tm_hour == _alarmHour && t.tm_min == _alarmMinute) {
        int currentMin = _alarmHour * 60 + _alarmMinute;
        if (_alarmLastTriggerMin != currentMin || _alarmLastTriggerDay != t.tm_yday) {
            _alarmLastTriggerDay = t.tm_yday;
            _alarmLastTriggerMin = currentMin;
            triggerAlarm();
        }
    }
}

void Display::triggerAlarm() {
    _alarmRinging = true;
    _alarmRingStart = millis();
    _lastAlarmBeepTick = 0;
    _alarmSnoozed = false;
    _activePopup = POPUP_NONE;
    wakeDock();
    _lastRender = 0;
    Serial.println("[Alarm] >>> TRIGGERED! Ringing started <<<");
}

void Display::dismissAlarm() {
    _alarmRinging = false;
    _alarmSnoozed = false;
    hardware.playBeep(1800, 150);
    _lastRender = 0;
    Serial.println("[Alarm] Dismissed by user touch");
}

void Display::snoozeAlarm() {
    _alarmRinging = false;
    _alarmSnoozed = true;
    _alarmSnoozeUntil = time(NULL) + 300; // Snooze 5 phút
    hardware.playBeep(2200, 120);
    _lastRender = 0;
    Serial.println("[Alarm] Snoozed for 5 minutes");
}

void Display::drawBellIcon(int16_t cx, int16_t cy, bool isEnabled, uint16_t customColor) {
    const uint16_t* bmp = isEnabled ? bell_icon_gold_20x20 : bell_icon_off_20x20;
    int16_t startX = cx - 10;
    int16_t startY = cy - 10;
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint16_t color = pgm_read_word(&bmp[r * 20 + c]);
            if (color != 0x0000) {
                if (customColor != 0) {
                    _spr.drawPixel(startX + c, startY + r, customColor);
                } else {
                    _spr.drawPixel(startX + c, startY + r, color);
                }
            }
        }
    }
}

void Display::renderAlarmPopup() {
    // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không dải gradient
    _spr.fillRect(0, 0, 320, 240, RGB565(10, 15, 26));

    // ── Header (Y = 10..38): Icon Chuông + Tiêu đề + Nút Đóng tròn [ X ] ──────────────────
    drawBellIcon(18, 23, _alarmEnabled);
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0));
    _spr.drawString((getLanguage() == LANG_VI ? "CAI DAT BAO THUC" : "ALARM SETTINGS"), 36, 23, 2);

    // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
    drawPopupCloseButton();

    // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

    // ── 1. KHU VỰC THỜI GIAN GIỜ : PHÚT ──
    int16_t hx = 60, hw = 84;
    int16_t mx2 = 176, mw2 = 84;

    // A. Mũi tên LÊN ▲
    int16_t upY = 64;
    _spr.fillTriangle(hx + hw / 2 - 12, upY + 4, hx + hw / 2 + 12, upY + 4, hx + hw / 2, upY - 8, RGB565(0, 225, 255));
    _spr.fillTriangle(mx2 + mw2 / 2 - 12, upY + 4, mx2 + mw2 / 2 + 12, upY + 4, mx2 + mw2 / 2, upY - 8, RGB565(0, 225, 255));

    // B. Khung số Giờ & Phút (Bo góc mềm mại R = 12, số Font 4 cao sang trọng)
    int16_t boxY = 80, boxH = 58;
    // Khung Giờ
    _spr.fillRoundRect(hx, boxY, hw, boxH, 12, RGB565(12, 18, 30));
    _spr.drawRoundRect(hx, boxY, hw, boxH, 12, RGB565(0, 180, 240));
    char hBuf[8];
    snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(hBuf, hx + hw / 2, boxY + boxH / 2, 4);

    // Dấu hai chấm ":" phát sáng màu cyan ở giữa
    _spr.fillCircle(160, 103, 3, RGB565(0, 230, 255));
    _spr.fillCircle(160, 117, 3, RGB565(0, 230, 255));

    // Khung Phút
    _spr.fillRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(12, 18, 30));
    _spr.drawRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(0, 180, 240));
    char mBuf[8];
    snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(mBuf, mx2 + mw2 / 2, boxY + boxH / 2, 4);

    // C. Mũi tên XUỐNG ▼
    int16_t downY = 154;
    _spr.fillTriangle(hx + hw / 2 - 12, downY - 4, hx + hw / 2 + 12, downY - 4, hx + hw / 2, downY + 8, RGB565(0, 225, 255));
    _spr.fillTriangle(mx2 + mw2 / 2 - 12, downY - 4, mx2 + mw2 / 2 + 12, downY - 4, mx2 + mw2 / 2, downY + 8, RGB565(0, 225, 255));

    // D. Nút BẬT / TẮT BÁO THỨC ở đáy (Y = 178, H = 36, W = 230, R = 18)
    int16_t btnX = 45, btnY = 178, btnW = 230, btnH = 36;
    if (_alarmEnabled) {
        _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(35, 190, 110), RGB565(10, 15, 26));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(10, 30, 18));
        _spr.drawString((getLanguage() == LANG_VI ? "DANG BAT BAO THUC" : "ALARM ENABLED"), 160, btnY + btnH / 2, 2);
    } else {
        _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(25, 32, 45), RGB565(10, 15, 26));
        _spr.drawRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(55, 70, 95));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(150, 165, 185));
        _spr.drawString((getLanguage() == LANG_VI ? "DANG TAT BAO THUC" : "ALARM DISABLED"), 160, btnY + btnH / 2, 2);
    }

    if (strlen(_alarmMessage) > 0) {
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(140, 175, 220));
        char noteBuf[64];
        snprintf(noteBuf, sizeof(noteBuf), (getLanguage() == LANG_VI ? "Loi nhan: %s" : "Note: %s"), _alarmMessage);
        _spr.drawString(noteBuf, 160, 226, 1);
    }
}

void Display::renderAlarmTimeOnly() {
    int16_t hx = 60, hw = 84;
    int16_t mx2 = 176, mw2 = 84;
    int16_t boxY = 80, boxH = 58;

    // 1. Ô Giờ (X: 60, W: 84)
    _spr.fillRoundRect(hx, boxY, hw, boxH, 12, RGB565(12, 18, 30));
    _spr.drawRoundRect(hx, boxY, hw, boxH, 12, RGB565(0, 180, 240));
    char hBuf[8];
    snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(hBuf, hx + hw / 2, boxY + boxH / 2, 4);

    // 2. Hai chấm phát sáng ở giữa
    _spr.fillCircle(160, 103, 3, RGB565(0, 230, 255));
    _spr.fillCircle(160, 117, 3, RGB565(0, 230, 255));

    // 3. Ô Phút (X: 176, W: 84)
    _spr.fillRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(12, 18, 30));
    _spr.drawRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(0, 180, 240));
    char mBuf[8];
    snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(mBuf, mx2 + mw2 / 2, boxY + boxH / 2, 4);

    _spr.pushSprite(0, 0);
}

void Display::renderAlarmToggleBtnOnly() {
    int16_t btnX = 45, btnY = 178, btnW = 230, btnH = 36;
    if (_alarmEnabled) {
        _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(35, 190, 110), RGB565(10, 15, 26));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(10, 30, 18));
        _spr.drawString((getLanguage() == LANG_VI ? "DANG BAT BAO THUC" : "ALARM ENABLED"), 160, btnY + btnH / 2, 2);
    } else {
        _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(25, 32, 45), RGB565(10, 15, 26));
        _spr.drawRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(55, 70, 95));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(150, 165, 185));
        _spr.drawString((getLanguage() == LANG_VI ? "DANG TAT BAO THUC" : "ALARM DISABLED"), 160, btnY + btnH / 2, 2);
    }
    // Cập nhật cả icon chuông trên header
    drawBellIcon(18, 23, _alarmEnabled);
    _spr.pushSprite(0, 0);
}

void Display::renderAlarmRingingScreen() {
    uint32_t nowMs = millis();
    bool flash = (nowMs / 450) % 2;

    // Phông nền đen thẫm viền đỏ nhấp nháy tạo sự chú ý mạnh mẽ
    uint16_t bg = flash ? RGB565(48, 10, 18) : RGB565(12, 8, 16);
    _spr.fillSprite(bg);

    // Viền sáng chuyển động
    uint16_t borderColor = flash ? C_GOLD : RGB565(240, 45, 60);
    _spr.drawRoundRect(4, 4, 312, 232, 12, borderColor);
    _spr.drawRoundRect(5, 5, 310, 230, 11, borderColor);

    // 1. Icon Chuông to ở giữa đỉnh
    int16_t bellY = 30;
    drawBellIcon(160, bellY, true, C_GOLD);

    // Hiệu ứng sóng âm lan tỏa từ chuông
    if (flash) {
        _spr.drawCircle(160, bellY, 18, C_GOLD);
        _spr.drawCircle(160, bellY, 24, RGB565(255, 180, 0));
    }

    // 2. Dòng thông báo nổi bật
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(flash ? C_WHITE : C_GOLD);
    _spr.drawString((getLanguage() == LANG_VI ? "BAO THUC!" : "ALARM ALERT!"), 160, 60, 4);

    // 3. Hiển thị giờ hiện tại to rõ Font 6
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char curTime[16];
    snprintf(curTime, sizeof(curTime), "%02d:%02d", t.tm_hour, t.tm_min);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(0, 240, 255));
    _spr.drawString(curTime, 160, 96, 6);

    // Thông điệp nhắc nhở SmartDesk
    if (strlen(_alarmMessage) > 0) {
        int16_t msgW = _spr.textWidth(_alarmMessage, 2) + 20;
        if (msgW > 290) msgW = 290;
        _spr.fillSmoothRoundRect(160 - msgW / 2, 124, msgW, 24, 12, RGB565(35, 25, 40), bg);
        _spr.drawRoundRect(160 - msgW / 2, 124, msgW, 24, 12, C_GOLD);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_GOLD);
        _spr.drawString(_alarmMessage, 160, 136, 2);
    }

    // 4. Hai Nút Bấm Khổng Lồ Trên Màn Hình (100% On-Screen Touch, Không dùng nút BOOT / LED RGB phía sau)
    // Nút Trái: BÁO LẠI (+5 PHÚT)
    int16_t b1X = 14, b1Y = 160, b1W = 140, b1H = 64;
    _spr.fillSmoothRoundRect(b1X, b1Y, b1W, b1H, 16, RGB565(245, 155, 20), bg);
    _spr.drawRoundRect(b1X, b1Y, b1W, b1H, 16, C_WHITE);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(20, 20, 20));
    _spr.drawString((getLanguage() == LANG_VI ? "BAO LAI" : "SNOOZE"), b1X + b1W / 2, b1Y + 22, 2);
    _spr.setTextColor(RGB565(50, 40, 0));
    _spr.drawString((getLanguage() == LANG_VI ? "+5 PHUT" : "+5 MINS"), b1X + b1W / 2, b1Y + 44, 2);

    // Nút Phải: TẮT CHUÔNG
    int16_t b2X = 166, b2Y = 160, b2W = 140, b2H = 64;
    _spr.fillSmoothRoundRect(b2X, b2Y, b2W, b2H, 16, RGB565(225, 40, 55), bg);
    _spr.drawRoundRect(b2X, b2Y, b2W, b2H, 16, C_WHITE);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString((getLanguage() == LANG_VI ? "TAT CHUONG" : "DISMISS"), b2X + b2W / 2, b2Y + 22, 2);
    _spr.setTextColor(RGB565(255, 200, 200));
    _spr.drawString((getLanguage() == LANG_VI ? "DUNG HAN" : "STOP"), b2X + b2W / 2, b2Y + 44, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  BACKGROUND — Deep Obsidian Cosmic Sky + Dynamic Twinkling Stars
// ═══════════════════════════════════════════════════════════════════════
void Display::drawBackground() {
    // 1. Deep Obsidian Cosmic Sky (Pure velvety background matched to active Theme)
    _spr.fillSprite(theme.bg);

    // Subtle atmospheric vertical night gradient (softly transitioning from deep midnight to dark horizon)
    for (int y = 0; y < 112; y += 4) {
        uint8_t shade = map(y, 0, 112, 3, 10);
        _spr.drawFastHLine(0, y, SCREEN_W, RGB565(1, 3, shade));
        _spr.drawFastHLine(0, y + 1, SCREEN_W, RGB565(1, 3, shade));
    }

    // Twinkling delicate diamond stars and celestial stardust dots (No wireframe circles!)
    uint32_t tick = millis() / 100;
    uint8_t flickerPhase = (tick / 3) & 0x07;
    drawSparkle(24,  20, 2, (flickerPhase == 0) ? C_SPARKLE_CYAN : C_CYAN_DIM);
    drawSparkle(296, 20, 2, (flickerPhase == 1) ? C_SPARKLE_CYAN : C_CYAN_DIM);
    drawSparkle(26,  82, 2, (flickerPhase == 2) ? C_SPARKLE_W : C_CYAN_DIM);
    drawSparkle(294, 82, 2, (flickerPhase == 3) ? C_SPARKLE_W : C_CYAN_DIM);
    drawSparkle(160, 115, 1, (flickerPhase == 4) ? C_SPARKLE_W : C_CYAN_DIM);

    // Micro celestial stardust
    drawStarDot(18,  48, C_WHITE);
    drawStarDot(302, 48, C_CYAN_GLOW);
    drawStarDot(82,  24, RGB565(180, 220, 255));
    drawStarDot(238, 24, RGB565(255, 240, 200));
    drawStarDot(52,  88, C_CYAN_GLOW);
    drawStarDot(268, 88, C_WHITE);
}


// ─── Blob: soft organic shape via overlapping translucent circles ────
void Display::drawBlob(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    // Inner bright core
    _spr.fillCircle(cx, cy, r * 2 / 3, color);
    // Outer halo (dimmer, blend with background)
    // Create dimmed version by blending with dark bg
    uint8_t cr = ((color >> 11) & 0x1F) << 3;
    uint8_t cg = ((color >> 5)  & 0x3F) << 2;
    uint8_t cb = (color & 0x1F) << 3;
    uint16_t dimColor = RGB565(cr / 3 + 6, cg / 3 + 10, cb / 3 + 20);
    _spr.fillCircle(cx, cy, r, dimColor);
    _spr.fillCircle(cx, cy, r * 2 / 3, color);  // re-draw core over halo
}

// ─── Sparkle: 4-point star ───────────────────────────────────────────
void Display::drawSparkle(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    // Vertical line
    _spr.drawFastVLine(cx, cy - size, size * 2 + 1, color);
    // Horizontal line
    _spr.drawFastHLine(cx - size, cy, size * 2 + 1, color);
    // Center pixel brighter
    _spr.drawPixel(cx, cy, C_WHITE);
}

// ─── StarDot: tiny 1px twinkle ───────────────────────────────────────
void Display::drawStarDot(int16_t cx, int16_t cy, uint16_t color) {
    _spr.drawPixel(cx, cy, color);
}

// ═══════════════════════════════════════════════════════════════════════
//  WEATHER ICONS — procedural vector-style icons
// ═══════════════════════════════════════════════════════════════════════
void Display::drawWeatherIcon(int16_t cx, int16_t cy, uint8_t wType, bool isNight) {
    switch (wType) {
        case WEATHER_CLEAR:
            if (isNight) {
                // Night sky: artistic golden crescent moon + sparkling stars (diameter 30px)
                _spr.fillCircle(cx - 3, cy - 2, 13, C_GOLD);
                _spr.fillCircle(cx + 4, cy - 6, 13, theme.card); // clean crescent
                drawSparkle(cx - 9, cy + 7, 2, C_WHITE);
                drawStarDot(cx + 11, cy - 7, C_CYAN_GLOW);
            } else {
                drawSunIcon(cx, cy, 10);
            }
            break;
        case WEATHER_CLOUDY:
            drawPartlyCloudyIcon(cx, cy);
            break;
        case WEATHER_RAIN:
            drawRainIcon(cx, cy);
            break;
        case WEATHER_STORM:
            drawStormIcon(cx, cy);
            break;
        case WEATHER_SNOW:
            drawCloudIcon(cx, cy - 4, 26, 14);
            for (int i = 0; i < 4; i++) {
                int16_t sx = cx - 9 + i * 6;
                int16_t sy = cy + 10 + (i & 1) * 3;
                drawSparkle(sx, sy, 1, C_WHITE);
            }
            break;
        case WEATHER_MIST:
            for (int i = 0; i < 4; i++) {
                int16_t lw = 26 - i * 3;
                _spr.drawFastHLine(cx - lw / 2, cy - 6 + i * 4, lw, C_TEXT_DIM);
            }
            break;
        default:
            drawPartlyCloudyIcon(cx, cy);
            break;
    }
}

void Display::drawForecastIcon(int16_t cx, int16_t cy, uint8_t wType) {
    switch (wType) {
        case WEATHER_CLEAR:
            // Compact 14px warm glowing sun
            _spr.fillCircle(cx, cy, 6, C_SUN_ORANGE);
            _spr.fillCircle(cx, cy, 5, C_SUN_YELLOW);
            _spr.fillCircle(cx - 1, cy - 1, 2, RGB565(255, 255, 180));
            _spr.drawPixel(cx, cy - 7, C_SUN_YELLOW);
            _spr.drawPixel(cx, cy + 7, C_SUN_YELLOW);
            _spr.drawPixel(cx - 7, cy, C_SUN_YELLOW);
            _spr.drawPixel(cx + 7, cy, C_SUN_YELLOW);
            break;
        case WEATHER_CLOUDY:
            // Mini cute cloud with warm sun peek
            _spr.fillCircle(cx + 4, cy - 3, 4, C_SUN_YELLOW);
            _spr.fillCircle(cx - 4, cy + 1, 4, RGB565(140, 165, 190));
            _spr.fillCircle(cx + 3, cy + 1, 4, RGB565(140, 165, 190));
            _spr.fillCircle(cx - 1, cy - 1, 5, RGB565(205, 225, 245));
            _spr.fillRect(cx - 4, cy, 8, 4, RGB565(170, 195, 220));
            break;
        case WEATHER_RAIN:
            // Mini rain cloud with cyan raindrops
            _spr.fillCircle(cx - 4, cy - 1, 4, RGB565(115, 135, 160));
            _spr.fillCircle(cx + 3, cy - 1, 4, RGB565(115, 135, 160));
            _spr.fillCircle(cx - 1, cy - 3, 5, RGB565(165, 185, 210));
            _spr.fillRect(cx - 4, cy - 1, 8, 3, RGB565(145, 165, 190));
            _spr.drawLine(cx - 4, cy + 5, cx - 5, cy + 8, RGB565(0, 220, 255));
            _spr.drawLine(cx, cy + 5, cx - 1, cy + 8, RGB565(0, 220, 255));
            _spr.drawLine(cx + 4, cy + 5, cx + 3, cy + 8, RGB565(0, 220, 255));
            break;
        case WEATHER_STORM:
            // Darker storm cloud with golden lightning bolt
            _spr.fillCircle(cx - 4, cy - 2, 4, RGB565(85, 100, 120));
            _spr.fillCircle(cx + 3, cy - 2, 4, RGB565(85, 100, 120));
            _spr.fillCircle(cx - 1, cy - 4, 5, RGB565(115, 130, 150));
            _spr.fillRect(cx - 4, cy - 2, 8, 3, RGB565(95, 110, 130));
            _spr.drawLine(cx, cy + 3, cx - 2, cy + 6, RGB565(255, 230, 0));
            _spr.drawLine(cx - 2, cy + 6, cx + 1, cy + 6, RGB565(255, 230, 0));
            _spr.drawLine(cx + 1, cy + 6, cx - 1, cy + 9, RGB565(255, 230, 0));
            break;
        case WEATHER_SNOW:
            _spr.fillCircle(cx - 3, cy - 2, 4, RGB565(155, 180, 205));
            _spr.fillCircle(cx + 3, cy - 2, 4, RGB565(155, 180, 205));
            _spr.fillCircle(cx, cy - 4, 5, RGB565(195, 215, 235));
            _spr.drawPixel(cx - 3, cy + 5, C_WHITE);
            _spr.drawPixel(cx + 3, cy + 5, C_WHITE);
            _spr.drawPixel(cx, cy + 8, C_WHITE);
            break;
        case WEATHER_MIST:
            _spr.drawFastHLine(cx - 7, cy - 3, 14, RGB565(155, 180, 205));
            _spr.drawFastHLine(cx - 9, cy, 18, RGB565(135, 160, 190));
            _spr.drawFastHLine(cx - 6, cy + 3, 12, RGB565(115, 145, 175));
            break;
        default:
            _spr.fillCircle(cx, cy, 5, C_SUN_YELLOW);
            break;
    }
}

void Display::drawSunIcon(int16_t cx, int16_t cy, int16_t r) {
    // 8 circular ray points around the sun (diameter ~30px)
    for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int16_t rx = cx + (int16_t)(cos(rad) * (r + 5));
        int16_t ry = cy + (int16_t)(sin(rad) * (r + 5));
        _spr.drawPixel(rx, ry, C_SUN_YELLOW);
        _spr.drawPixel(rx + (a % 90 == 0 ? 1 : 0), ry, RGB565(255, 245, 180));
    }

    // 3D Sun body with warm radial depth
    _spr.fillCircle(cx, cy, r + 2, C_SUN_ORANGE);
    _spr.fillCircle(cx, cy, r, C_SUN_YELLOW);
    _spr.fillCircle(cx - 2, cy - 2, r - 2, RGB565(255, 245, 160));
    _spr.drawPixel(cx - 2, cy - 2, C_WHITE);
}

void Display::drawCloudIcon(int16_t cx, int16_t cy, int16_t w, int16_t h) {
    // Volumetric 3D Fluffy Cloud with depth shading
    int16_t baseR = h / 3;

    // Bottom soft shadow layer
    _spr.fillRoundRect(cx - w / 2, cy - baseR + 2, w, baseR * 2 + 2, baseR, RGB565(150, 170, 195));
    _spr.fillCircle(cx - w / 4, cy - baseR, baseR + 2, RGB565(160, 180, 205));
    _spr.fillCircle(cx + 2, cy - baseR - 2, baseR + 4, RGB565(170, 190, 215));
    _spr.fillCircle(cx + w / 4, cy - baseR + 1, baseR + 1, RGB565(160, 180, 205));

    // Pure white cloud body
    _spr.fillRoundRect(cx - w / 2, cy - baseR, w, baseR * 2, baseR, C_WHITE);
    _spr.fillCircle(cx - w / 4, cy - baseR - 2, baseR + 2, C_WHITE);
    _spr.fillCircle(cx + 2, cy - baseR - 4, baseR + 4, C_WHITE);
    _spr.fillCircle(cx + w / 4, cy - baseR - 1, baseR + 1, C_WHITE);

    // Specular top highlight
    _spr.fillCircle(cx + 1, cy - baseR - 4, baseR - 1, RGB565(255, 255, 255));
}

void Display::drawPartlyCloudyIcon(int16_t cx, int16_t cy) {
    // 1. 3D Sun behind top-left (sr = 8, rays at 12)
    int16_t sx = cx - 5;
    int16_t sy = cy - 5;
    int16_t sr = 8;

    for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int16_t rx = sx + (int16_t)(cos(rad) * (sr + 4));
        int16_t ry = sy + (int16_t)(sin(rad) * (sr + 4));
        _spr.drawPixel(rx, ry, C_SUN_YELLOW);
    }
    _spr.fillCircle(sx, sy, sr + 1, C_SUN_ORANGE);
    _spr.fillCircle(sx, sy, sr, C_SUN_YELLOW);
    _spr.fillCircle(sx - 1, sy - 1, sr - 2, RGB565(255, 245, 170));

    // 2. 3D Fluffy Cloud in foreground (w = 26, h = 14)
    int16_t clx = cx + 3;
    int16_t cly = cy + 4;
    drawCloudIcon(clx, cly, 26, 14);
}

void Display::drawRainIcon(int16_t cx, int16_t cy) {
    drawCloudIcon(cx, cy - 4, 26, 14);
    // Rain drops
    for (int i = 0; i < 4; i++) {
        int16_t dx = cx - 7 + i * 5;
        int16_t dy = cy + 7 + (i & 1) * 2;
        _spr.drawLine(dx, dy, dx - 1, dy + 5, C_RAIN_BLUE);
    }
}

void Display::drawStormIcon(int16_t cx, int16_t cy) {
    drawCloudIcon(cx, cy - 4, 26, 14);
    // Lightning bolt
    _spr.drawLine(cx,     cy + 4, cx - 2, cy + 9, C_GOLD);
    _spr.drawLine(cx - 2, cy + 9, cx + 1, cy + 9, C_GOLD);
    _spr.drawLine(cx + 1, cy + 9, cx - 1, cy + 15, C_GOLD);
}

void Display::drawMoonIcon(int16_t cx, int16_t cy, int16_t r, uint8_t lunarDay, uint16_t bgTint) {
    if (bgTint == 0) bgTint = theme.card;

    // Palette: Luminous moon gold & pearlescent highlights
    uint16_t moonGold = RGB565(255, 230, 140);      // Warm celestial gold
    uint16_t moonHighlight = RGB565(255, 248, 210); // Luminous inner glow
    uint16_t craterColor = RGB565(225, 200, 120);   // Soft crater shading

    if (lunarDay <= 1) {
        // Trăng non (New Moon): Celestial outline with tiny star
        _spr.drawCircle(cx, cy, r, RGB565(70, 90, 120));
        _spr.drawCircle(cx, cy, r - 1, RGB565(40, 50, 75));
        drawSparkle(cx, cy, 2, C_WHITE);
        return;
    }

    if (lunarDay >= 14 && lunarDay <= 16) {
        // Trăng tròn / Rằm (Full Moon): Centered exactly at geometric cy
        _spr.fillCircle(cx, cy, r, moonGold);
        _spr.fillCircle(cx - r / 4, cy - r / 4, r * 2 / 3, moonHighlight);
        _spr.fillCircle(cx - r / 3, cy - r / 5, r / 5, craterColor);
        _spr.fillCircle(cx + r / 4, cy + r / 3, r / 6, craterColor);
        _spr.fillCircle(cx + r / 3, cy - r / 3, r / 7, craterColor);
        _spr.fillCircle(cx - r / 5, cy + r / 3, r / 7, craterColor);
        _spr.drawCircle(cx, cy, r + 1, RGB565(255, 242, 190));
        return;
    }

    bool isWaxing = (lunarDay < 15);

    if ((isWaxing && lunarDay <= 6) || (!isWaxing && lunarDay >= 24)) {
        // ── CRESCENT MOON (Trăng lưỡi liềm nghệ thuật: 2 góc nhọn chếch vút lên trên) ──
        // CÂN CHỈNH QUANG HỌC (Optical Centering):
        // Nâng trục vẽ lên (r * 2 / 10) để trọng tâm thị giác của phần trăng phát sáng
        // nằm ngay chính giữa 2 đường kẻ, sừng trên vút ngang số ngày AL, đáy trăng không bị chìm
        int16_t optCy = cy - (r * 2 / 10);

        // 1. Draw radiant moon body
        _spr.fillCircle(cx, optCy, r, moonGold);
        _spr.fillCircle(cx, optCy, r - 1, moonHighlight);

        // 2. Cutout shadow circle shifted UPWARDS (dy < 0) and slightly sideways
        int16_t dy = -r * 4 / 10;
        int16_t dx = 0;

        if (isWaxing) {
            // Waxing (Mùng 2..6): crescent cradled on bottom-right, horns point up
            dx = map(lunarDay, 2, 6, -r * 2 / 10, -r * 6 / 10);
        } else {
            // Waning (24..29): crescent cradled on bottom-left, horns point up
            dx = map(lunarDay, 24, 29, r * 6 / 10, r * 2 / 10);
        }

        _spr.fillCircle(cx + dx, optCy + dy, r, bgTint);

        // 3. Delicate celestial sparkle star near the upper horn
        if (isWaxing) {
            drawSparkle(cx - r * 4 / 10, optCy - r * 4 / 10, 1, C_WHITE);
        } else {
            drawSparkle(cx + r * 4 / 10, optCy - r * 4 / 10, 1, C_WHITE);
        }
    } else {
        // ── QUARTER / GIBBOUS (Bán nguyệt & Trăng trương dần) ──
        int16_t optCy = cy - (r * 1 / 10);
        _spr.fillCircle(cx, optCy, r, moonGold);
        _spr.fillCircle(cx - r / 3, optCy - r / 4, r / 5, craterColor);
        _spr.fillCircle(cx + r / 4, optCy + r / 3, r / 6, craterColor);

        int16_t dy = -r * 2 / 10;
        int16_t offset = 0;
        if (isWaxing) {
            offset = map(lunarDay, 7, 13, -r * 7 / 10, -r * 13 / 10);
        } else {
            offset = map(lunarDay, 17, 23, r * 13 / 10, r * 7 / 10);
        }
        _spr.fillCircle(cx + offset, optCy + dy, r, bgTint);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  GLASS CARD — frosted translucent panel with 50% alpha blending
// ═══════════════════════════════════════════════════════════════════════
void Display::drawGlassCard(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius) {
    if (_spr.getColorDepth() == 16) {
        uint16_t* fb = (uint16_t*)_spr.getPointer();
        if (fb) {
            uint16_t tint = theme.card;
            int16_t x2 = x + w;
            int16_t y2 = y + h;
            int32_t r2 = (int32_t)radius * radius;

            for (int16_t j = y; j < y2; j++) {
                if (j < 0 || j >= SCREEN_H) continue;
                int32_t rowIdx = (int32_t)j * SCREEN_W;
                for (int16_t i = x; i < x2; i++) {
                    if (i < 0 || i >= SCREEN_W) continue;

                    // Rounded corner clipping
                    if (i < x + radius && j < y + radius) {
                        int16_t dx = (x + radius) - i;
                        int16_t dy = (y + radius) - j;
                        if ((int32_t)dx * dx + (int32_t)dy * dy > r2) continue;
                    } else if (i >= x2 - radius && j < y + radius) {
                        int16_t dx = i - (x2 - radius - 1);
                        int16_t dy = (y + radius) - j;
                        if ((int32_t)dx * dx + (int32_t)dy * dy > r2) continue;
                    } else if (i < x + radius && j >= y2 - radius) {
                        int16_t dx = (x + radius) - i;
                        int16_t dy = j - (y2 - radius - 1);
                        if ((int32_t)dx * dx + (int32_t)dy * dy > r2) continue;
                    } else if (i >= x2 - radius && j >= y2 - radius) {
                        int16_t dx = i - (x2 - radius - 1);
                        int16_t dy = j - (y2 - radius - 1);
                        if ((int32_t)dx * dx + (int32_t)dy * dy > r2) continue;
                    }

                    // 50% Alpha Blend: background pixel + glass tint
                    uint16_t bgP = fb[rowIdx + i];
                    fb[rowIdx + i] = ((bgP & 0xF7DE) >> 1) + ((tint & 0xF7DE) >> 1);
                }
            }
        } else {
            _spr.fillRoundRect(x, y, w, h, radius, theme.card);
        }
    } else {
        _spr.fillRoundRect(x, y, w, h, radius, theme.card);
    }

    // Clean, elegant frosted glowing glass border (no harsh top specular line)
    _spr.drawRoundRect(x, y, w, h, radius, theme.cardBorder);
}

void Display::drawGlassCardAccent(int16_t x, int16_t y, int16_t w, int16_t h,
                                   int16_t radius, uint16_t accentColor) {
    drawGlassCard(x, y, w, h, radius);
    _spr.drawFastHLine(x + radius, y + h - 2, w - radius * 2, accentColor);
}

// ═══════════════════════════════════════════════════════════════════════
//  NAVIGATION & SMART DOCK (Auto-hide after 5s)
// ═══════════════════════════════════════════════════════════════════════
void Display::setPage(uint8_t p) {
    if (p <= PAGE_SETTINGS && _currentPage != p) {
        _currentPage = p;
        wakeDock();
    }
}

void Display::wakeDock() {
    _dockVisible = true;
    _lastDockActivity = millis();
}

// ─── Calendar Navigation Methods ──────────────────────────────────────
void Display::prevCalMonth() {
    if (_calYear == 0) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        _calYear = t.tm_year + 1900;
        _calMonth = t.tm_mon + 1;
    }
    _calMonth--;
    if (_calMonth < 1) {
        _calMonth = 12;
        _calYear--;
    }
}

void Display::nextCalMonth() {
    if (_calYear == 0) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        _calYear = t.tm_year + 1900;
        _calMonth = t.tm_mon + 1;
    }
    _calMonth++;
    if (_calMonth > 12) {
        _calMonth = 1;
        _calYear++;
    }
}

void Display::resetCalMonth() {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    _calYear = t.tm_year + 1900;
    _calMonth = t.tm_mon + 1;
}

// ─── Pomodoro Focus Engine ───────────────────────────────────────────
void Display::pomoSetMode(PomoMode mode) {
    _pomoMode = mode;
    _pomoState = POMO_STOPPED;
    if (_pomoMode == POMO_FOCUS) {
        _pomoTotalSec = 25 * 60;
    } else if (_pomoMode == POMO_SHORT_BREAK) {
        _pomoTotalSec = 5 * 60;
    } else { // POMO_LONG_BREAK
        _pomoTotalSec = 15 * 60;
    }
    _pomoRemainingSec = _pomoTotalSec;
}

void Display::pomoStart() {
    _pomoState = POMO_RUNNING;
    _pomoLastSecTick = millis();
}

void Display::pomoPause() {
    _pomoState = POMO_PAUSED;
}

void Display::pomoReset() {
    _pomoState = POMO_STOPPED;
    _pomoRemainingSec = _pomoTotalSec;
}

void Display::pomoTick() {
    if (_pomoState != POMO_RUNNING) return;

    unsigned long now = millis();
    if (now - _pomoLastSecTick >= 1000) {
        _pomoLastSecTick = now;
        if (_pomoRemainingSec > 0) {
            _pomoRemainingSec--;
        }
        if (_pomoRemainingSec == 0) {
            _pomoState = POMO_STOPPED;
            // Chuông báo hiệu kết thúc phiên (2 tiếng beep dứt khoát)
            hardware.playBeep(2600, 150);
            delay(80);
            hardware.playBeep(3300, 300);

            if (_pomoMode == POMO_FOCUS) {
                if (_pomoSession < 4) {
                    _pomoSession++;
                    pomoSetMode(POMO_SHORT_BREAK);
                } else {
                    _pomoSession = 1;
                    pomoSetMode(POMO_LONG_BREAK);
                }
            } else {
                pomoSetMode(POMO_FOCUS);
            }
        }
    }
}

bool Display::handleTouch(uint16_t x, uint16_t y) {
    // 0. NẾU BÁO THỨC ĐANG KÊU: XỬ LÝ 2 NÚT BẤM TO TRÊN MÀN HÌNH (100% Cảm ứng mặt trước, Không dùng nút BOOT / LED)
    if (_alarmRinging) {
        // Nút Trái: BÁO LẠI (+5 PHÚT) (x: 10..155, y: 150..235)
        if (x >= 10 && x <= 155 && y >= 150) {
            snoozeAlarm();
            return true;
        }
        // Nút Phải: TẮT CHUÔNG (x: 160..310, y: 150..235)
        if (x >= 160 && x <= 310 && y >= 150) {
            dismissAlarm();
            return true;
        }
        // Chạm vào bất kỳ vị trí nào khác trên màn hình khi đang reo cũng tắt chuông để an toàn
        dismissAlarm();
        return true;
    }

    // 1. POPUP BÁO THỨC (POPUP_ALARM): CÁC NÚT TƯƠNG TÁC BÊN TRONG MODAL
    if (_activePopup == POPUP_ALARM) {
        // A. Nút [X] đóng góc trên phải: (x >= 260 && y <= 50)
        if (x >= 260 && y <= 50) {
            saveAlarmSettings();
            _activePopup = POPUP_NONE;
            _alarmAutoCloseTime = 0;
            hardware.playClick();
            _lastRender = 0;
            return true;
        }

        // B. Nút BẬT/TẮT Báo thức ở đáy: (x >= 40 && x <= 280 && y >= 175 && y <= 230)
        if (x >= 40 && x <= 280 && y >= 175 && y <= 230) {
            _alarmEnabled = !_alarmEnabled;
            _alarmLastTriggerDay = -1;
            _alarmLastTriggerMin = -1;
            saveAlarmSettings();
            hardware.playClick();
            renderAlarmToggleBtnOnly();
            return true;
        }

        // C. Cột GIỜ: Mũi tên LÊN ▲ / Nửa trên ô Giờ (x: 45..150, y: 48..110)
        if (x >= 45 && x <= 150 && y >= 48 && y <= 110) {
            _alarmAutoCloseTime = 0;
            _alarmHour = (_alarmHour + 1) % 24;
            _alarmLastTriggerDay = -1;
            _alarmLastTriggerMin = -1;
            hardware.playClick();
            renderAlarmTimeOnly();
            return true;
        }

        // D. Cột GIỜ: Mũi tên XUỐNG ▼ / Nửa dưới ô Giờ (x: 45..150, y: 111..174)
        if (x >= 45 && x <= 150 && y >= 111 && y <= 174) {
            _alarmAutoCloseTime = 0;
            _alarmHour = (_alarmHour + 23) % 24;
            _alarmLastTriggerDay = -1;
            _alarmLastTriggerMin = -1;
            hardware.playClick();
            renderAlarmTimeOnly();
            return true;
        }

        // E. Cột PHÚT: Mũi tên LÊN ▲ / Nửa trên ô Phút (x: 170..275, y: 48..110)
        if (x >= 170 && x <= 275 && y >= 48 && y <= 110) {
            _alarmAutoCloseTime = 0;
            _alarmMinute = (_alarmMinute + 1) % 60;
            _alarmLastTriggerDay = -1;
            _alarmLastTriggerMin = -1;
            hardware.playClick();
            renderAlarmTimeOnly();
            return true;
        }

        // F. Cột PHÚT: Mũi tên XUỐNG ▼ / Nửa dưới ô Phút (x: 170..275, y: 111..174)
        if (x >= 170 && x <= 275 && y >= 111 && y <= 174) {
            _alarmAutoCloseTime = 0;
            _alarmMinute = (_alarmMinute + 59) % 60;
            _alarmLastTriggerDay = -1;
            _alarmLastTriggerMin = -1;
            hardware.playClick();
            renderAlarmTimeOnly();
            return true;
        }

        return true; // Chạm vào các khoảng đệm bên trong popup không đóng vô cớ
    }

    // 2. POPUP TỶ GIÁ NGOẠI TỆ (POPUP_FINANCE_CURRENCY)
    if (_activePopup == POPUP_FINANCE_CURRENCY) {
        // Nút [X] đóng góc trên phải: (x >= 265 && y <= 48)
        if (x >= 265 && y <= 48) {
            _activePopup = POPUP_NONE;
            hardware.playClick();
            _lastRender = 0;
            return true;
        }

        // Tab Ngoại tệ 1: (x >= 10 && x <= 132 && y <= 48)
        if (x >= 10 && x <= 132 && y <= 48) {
            if (_currencyTab != 0) {
                _currencyTab = 0;
                hardware.playClick();
                _lastRender = 0;
            }
            return true;
        }

        // Tab Ngoại tệ 2: (x >= 134 && x <= 256 && y <= 48)
        if (x >= 134 && x <= 256 && y <= 48) {
            if (_currencyTab != 1) {
                _currencyTab = 1;
                hardware.playClick();
                _lastRender = 0;
            }
            return true;
        }

        return true; // Chạm trong popup không đóng vô cớ
    }

    // 3. POPUP VÀNG SJC (POPUP_FINANCE_GOLD)
    if (_activePopup == POPUP_FINANCE_GOLD) {
        // Nút [X] đóng góc trên phải: (x >= 265 && y <= 48)
        if (x >= 265 && y <= 48) {
            _activePopup = POPUP_NONE;
            hardware.playClick();
            _lastRender = 0;
            return true;
        }
        return true;
    }

    // 4. CÁC POPUP KHÁC (Weather, Lunar): Chạm bất kỳ đâu để đóng
    if (_activePopup != POPUP_NONE) {
        _activePopup = POPUP_NONE;
        hardware.playClick();
        _lastRender = 0; // Trigger instant redraw
        return true;
    }

    // 3. ƯU TIÊN TUYỆT ĐỐI CHO THANH DOCK:
    // Khi thanh Dock đang hiển thị:
    if (_dockVisible) {
        if (y >= 185) {
            // Chạm vào vùng Dock -> Xử lý chức năng chuyển trang / đổi theme
            wakeDock(); // Reset 5s auto-hide timer
            if (x < 80)                   { setPage(PAGE_CLOCK);    return true; }
            else if (x >= 80 && x < 120)  { setPage(PAGE_CALENDAR); return true; }
            else if (x >= 120 && x < 160) { setPage(PAGE_FINANCE);  return true; }
            else if (x >= 160 && x < 200) { setPage(PAGE_POMODORO); return true; }
            else if (x >= 200 && x < 240) { setPage(PAGE_MOOD);     return true; }
            else if (x >= 240)            { setPage(PAGE_SETTINGS); return true; }
            return true;
        } else {
            // Nhấn ra ngoài vùng Dock (Y < 185) -> TẮT THANH DOCK NGAY
            // Cảm ứng của các chức năng màn hình lúc này CHƯA được kích hoạt
            _dockVisible = false;
            _lastRender = 0; // Vẽ lại màn hình với Dock đã ẩn
            return true;
        }
    }

    // 4. If on Calendar page and tapping in the top Title Card (Y <= 50)
    if (_currentPage == PAGE_CALENDAR && y <= 50) {
        if (x <= 66) {
            prevCalMonth();
            hardware.playClick();
            _lastRender = 0; // Trigger instant redraw
            return true;
        } else if (x >= 250) {
            nextCalMonth();
            hardware.playClick();
            _lastRender = 0; // Trigger instant redraw
            return true;
        } else if (x >= 75 && x <= 245) {
            resetCalMonth();
            hardware.playClick();
            _lastRender = 0; // Trigger instant redraw
            return true;
        }
    }

    // 5. If on Clock page (PAGE_CLOCK):
    if (_currentPage == PAGE_CLOCK) {
        // 5a. Chạm vào BIỂU TƯỢNG CHUÔNG (X: 245..315, Y: 15..70) -> Bật / Tắt nhanh chuông báo
        if (x >= 245 && x <= 315 && y >= 15 && y <= 70) {
            if (!_alarmEnabled) {
                // Đang TẮT -> BẬT lên và hiện popup báo giờ báo thức trong 3s để user biết
                _alarmEnabled = true;
                saveAlarmSettings();
                _alarmAutoCloseTime = millis() + 3000;
                _activePopup = POPUP_ALARM;
                renderPage0_WeatherClock();
                renderAlarmPopup();
                _spr.pushSprite(0, 0);
                return true;
            } else {
                // Đang BẬT -> TẮT đi ngay lập tức
                _alarmEnabled = false;
                saveAlarmSettings();
                _alarmAutoCloseTime = 0;
                _activePopup = POPUP_NONE;
                _lastRender = 0;
                return true;
            }
        }

        // 5b. Chạm vào VÙNG GIỮA ĐỒNG HỒ (Quanh dấu : giữa giờ và phút, X: 115..205, Y: 15..95) -> Mở Cài Đặt Giờ
        if (x >= 115 && x <= 205 && y >= 15 && y <= 95) {
            loadAlarmSettings();
            _alarmAutoCloseTime = 0; // Mở thủ công thì không tự đóng sau 3s
            _activePopup = POPUP_ALARM;
            renderPage0_WeatherClock();
            renderAlarmPopup();
            _spr.pushSprite(0, 0);
            return true;
        }

        // 5b. Chạm vào 2 Thẻ Kính Thời Tiết & Âm Lịch (Y: 126..194)
        if (y >= 126 && y <= 194) {
            if (x >= 10 && x <= 156) {
                _activePopup = POPUP_WEATHER;
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else if (x >= 164 && x <= 310) {
                _activePopup = POPUP_LUNAR;
                hardware.playClick();
                _lastRender = 0;
                return true;
            }
        }
    }

    // 5. If on Pomodoro page (PAGE_POMODORO), interactive controls
    if (_currentPage == PAGE_POMODORO) {
        // A. Top Mode Capsule Tabs (Y: 6..32)
        if (y >= 6 && y <= 32) {
            if (x >= 40 && x <= 120) {
                pomoSetMode(POMO_FOCUS);
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else if (x > 120 && x <= 200) {
                pomoSetMode(POMO_SHORT_BREAK);
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else if (x > 200 && x <= 280) {
                pomoSetMode(POMO_LONG_BREAK);
                hardware.playClick();
                _lastRender = 0;
                return true;
            }
        }

        // B1. Chạm trực tiếp vào Pet ở tâm vòng tròn (X: 52..124, Y: 88..164) -> Đổi sang Pet kế tiếp!
        if (x >= 52 && x <= 124 && y >= 88 && y <= 164) {
            deskPet.nextPet();
            hardware.playTouchSound();
            _lastRender = 0;
            return true;
        }

        // B2. Chạm vào Vòng tròn bên ngoài (X: 15..156, Y: 45..194) -> Đổi Chạy / Tạm dừng
        if (x >= 15 && x <= 156 && y >= 45 && y <= 194) {
            if (_pomoState == POMO_RUNNING) {
                pomoPause();
            } else {
                pomoStart();
            }
            hardware.playClick();
            _lastRender = 0;
            return true;
        }

        // C. Cụm nút điều khiển bên phải (X: 156..315, Y: 148..194)
        // Kết thúc tại Y=194, cách ly 100% với vùng mở Dock (Y >= 196)
        if (x >= 156 && x <= 315 && y >= 148 && y <= 194) {
            if (_pomoState == POMO_STOPPED) {
                pomoStart();
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else if (_pomoState == POMO_RUNNING) {
                // Trái: TẠM DỪNG (X: 156..234)
                if (x <= 234) {
                    pomoPause();
                }
                // Phải: ĐẶT LẠI (X: 235..315)
                else {
                    pomoReset();
                }
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else if (_pomoState == POMO_PAUSED) {
                // Trái: TIẾP TỤC (X: 156..234)
                if (x <= 234) {
                    pomoStart();
                }
                // Phải: ĐẶT LẠI (X: 235..315)
                else {
                    pomoReset();
                }
                hardware.playClick();
                _lastRender = 0;
                return true;
            }
        }
    }

    // 6. If on Finance page (PAGE_FINANCE):
    if (_currentPage == PAGE_FINANCE) {
        if (y < 210) {
            if (x <= 158) {
                // Chạm Cột trái -> Mở Popup Vàng SJC & Thế Giới
                _activePopup = POPUP_FINANCE_GOLD;
                hardware.playClick();
                _lastRender = 0;
                return true;
            } else {
                // Chạm Cột phải -> Mở Popup Tỷ Giá Ngoại Tệ
                _activePopup = POPUP_FINANCE_CURRENCY;
                _currencyTab = 0;
                hardware.playClick();
                _lastRender = 0;
                return true;
            }
        }
    }

    // 7. If on Mood page (PAGE_MOOD):
    if (_currentPage == PAGE_MOOD) {
        if (deskMood.handleTouch(x, y)) {
            _lastRender = 0;
            return true;
        }
    }

    // 8. If on Settings page (PAGE_SETTINGS):
    if (_currentPage == PAGE_SETTINGS) {
        if (handleTouchSettings(x, y)) {
            return true;
        }
    }

    // 8. Khi Dock đang ẩn, chạm vào vùng dưới màn hình (Y >= 210) sẽ đánh thức Dock
    if (!_dockVisible) {
        if (y >= 210) {
            wakeDock();
            return true;
        }
        return false;
    }

    return false;
}

void Display::renderCalibrationScreen() {
    _spr.fillSprite(TFT_BLACK);
    _spr.setTextColor(TFT_WHITE, TFT_BLACK);
    _spr.setTextDatum(TC_DATUM);
    _spr.drawString((getLanguage() == LANG_VI ? "HIEU CHUAN CAM UNG (TOUCH)" : "TOUCH CALIBRATION"), 160, 10, 2);

    CalibState state = touch.getCalibState();
    if (state == CALIB_DONE) {
        _spr.setTextColor(TFT_GREEN, TFT_BLACK);
        _spr.setTextDatum(MC_DATUM);
        _spr.drawString((getLanguage() == LANG_VI ? "HIEU CHUAN THANH CONG!" : "CALIBRATION SUCCESSFUL!"), 160, 100, 4);
        _spr.setTextColor(TFT_WHITE, TFT_BLACK);
        _spr.drawString((getLanguage() == LANG_VI ? "Dang luu vao NVS Flash..." : "Saving to NVS Flash..."), 160, 135, 2);
        return;
    }

    int tx = 160, ty = 120;
    const char* hint = "";
    const char* step = "";

    switch (state) {
        case CALIB_WAIT_TL:
            tx = CALIB_TL_X; ty = CALIB_TL_Y;
            hint = (getLanguage() == LANG_VI ? "Cham tam do GOC TREN - TRAI" : "Tap target TOP - LEFT");
            step = (getLanguage() == LANG_VI ? "Buoc 1/3" : "Step 1/3");
            break;
        case CALIB_WAIT_TR:
            tx = CALIB_TR_X; ty = CALIB_TR_Y;
            hint = (getLanguage() == LANG_VI ? "Cham tam do GOC TREN - PHAI" : "Tap target TOP - RIGHT");
            step = (getLanguage() == LANG_VI ? "Buoc 2/3" : "Step 2/3");
            break;
        case CALIB_WAIT_BL:
            tx = CALIB_BL_X; ty = CALIB_BL_Y;
            hint = (getLanguage() == LANG_VI ? "Cham tam do GOC DUOI - TRAI" : "Tap target BOTTOM - LEFT");
            step = (getLanguage() == LANG_VI ? "Buoc 3/3" : "Step 3/3");
            break;
        default:
            break;
    }

    // Draw crosshair target
    _spr.drawLine(tx - 18, ty, tx + 18, ty, TFT_YELLOW);
    _spr.drawLine(tx, ty - 18, tx, ty + 18, TFT_YELLOW);
    _spr.drawCircle(tx, ty, 10, TFT_YELLOW);
    _spr.drawCircle(tx, ty, 3, TFT_RED);

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(TFT_CYAN, TFT_BLACK);
    _spr.drawString(step, 160, 95, 4);
    _spr.setTextColor(TFT_WHITE, TFT_BLACK);
    _spr.drawString(hint, 160, 130, 2);
}

void Display::renderDock() {
    if (!_dockVisible) {
        // Subtle indicator pill bar when auto-hidden (H=2px at very bottom)
        _spr.fillRoundRect(140, 237, 40, 2, 1, theme.accent);
        return;
    }

    // Centered Floating Solid Obsidian Dock Capsule (Seamless, no split line)
    int16_t dockX = 38;
    int16_t dockY = 196;
    int16_t dockW = 244;
    int16_t dockH = 36;
    int16_t dockR = 18;

    // 1. Soft outer shadow (floating 3D lift)
    _spr.fillRoundRect(dockX - 1, dockY + 1, dockW + 2, dockH + 2, dockR + 1, RGB565(2, 4, 8));

    // 2. Uniform Deep Obsidian Glass Base (100% solid, NO horizontal color split)
    _spr.fillRoundRect(dockX, dockY, dockW, dockH, dockR, RGB565(12, 16, 26));

    // 3. Crisp outer border
    _spr.drawRoundRect(dockX, dockY, dockW, dockH, dockR, RGB565(40, 58, 86));

    // 4. Subtle specular highlight on top edge
    _spr.drawFastHLine(dockX + dockR, dockY, dockW - dockR * 2, RGB565(140, 190, 245));

    // 6 Vector Icon Slots (spaced 40px apart)
    for (int i = 0; i < 6; i++) {
        int16_t cx = dockX + 22 + i * 40;
        int16_t cy = dockY + dockH / 2;
        bool isActive = (_currentPage == i);

        if (isActive) {
            // Nền tối nhẹ cho icon đang được chọn (Bo góc tròn thực sự r=10)
            _spr.fillRoundRect(cx - 16, dockY + 4, 32, 28, 10, RGB565(18, 24, 36));
            // Đường bao khi chọn icon 1px, màu xanh lá, 4 góc bo tròn thực sự (r=10, không bị vát)
            _spr.drawRoundRect(cx - 16, dockY + 4, 32, 28, 10, RGB565(0, 230, 118));
        }

        switch (i) {
            case 0: drawDockIconHome(cx, cy, isActive); break;
            case 1: drawDockIconCalendar(cx, cy, isActive); break;
            case 2: drawDockIconFinance(cx, cy, isActive); break;
            case 3: drawDockIconPomodoro(cx, cy, isActive); break;
            case 4: drawDockIconMood(cx, cy, isActive); break;
            case 5: drawDockIconTheme(cx, cy, isActive); break;
        }
    }
}

// ─── Professional 20x20 Color Glyph Renderer (Vibrant & Crisp) ────────
static void drawDockBitmap20(TFT_eSprite& spr, int16_t cx, int16_t cy, const uint16_t* bmp) {
    int16_t startX = cx - 10;
    int16_t startY = cy - 10;
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            uint16_t color = pgm_read_word(&bmp[r * 20 + c]);
            if (color != 0x0000) {
                spr.drawPixel(startX + c, startY + r, color);
            }
        }
    }
}

void Display::drawDockIconHome(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_home_20x20);
}

void Display::drawDockIconCalendar(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_calendar_20x20);
}

void Display::drawDockIconFinance(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_finance_20x20);
}

void Display::drawDockIconPomodoro(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_pomodoro_20x20);
}

void Display::drawDockIconMood(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_mood_20x20);
}

void Display::drawDockIconTheme(int16_t cx, int16_t cy, bool isActive) {
    drawDockBitmap20(_spr, cx, cy, dock_pro_theme_20x20);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 1 — LUNAR MONTH CALENDAR (Interactive Month Navigation)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage1_Calendar() {
    // 1. Tông xuyệt tông 100% với Page 0 (Nền vũ trụ Obsidian sâu thẳm)
    drawBackground();

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    int curYear = t.tm_year + 1900;
    int curMonth = t.tm_mon + 1;
    int curDay = (t.tm_mday > 0) ? t.tm_mday : 1;

    // Initialize calendar view to current month on first display
    if (_calYear == 0 || _calMonth == 0) {
        _calYear = curYear;
        _calMonth = curMonth;
    }

    // ── 2. QUYỂN LỊCH ĐỂ BÀN 3 TỜ XẾP CHỒNG (Hợp nhất mép phải) ───────────
    int16_t calX = 14, calY = 8, calW = 292, calH = 216, calR = 10;
    int16_t calRight = calX + calW; // 306 — Mép phải hợp nhất 1 đường duy nhất

    // Tờ thứ 3 (Dưới cùng): Nhô ra 4px bên trái, 6px ở đáy, mép phải đồng phẳng
    int16_t x3 = calX - 4, y3 = calY + 6, w3 = calRight - x3;
    _spr.fillRoundRect(x3, y3, w3, calH, calR, RGB565(8, 12, 20));
    _spr.drawRoundRect(x3, y3, w3, calH, calR, RGB565(28, 40, 58));

    // Tờ thứ 2 (Ở giữa): Nhô ra 2px bên trái, 3px ở đáy, mép phải đồng phẳng
    int16_t x2 = calX - 2, y2 = calY + 3, w2 = calRight - x2;
    _spr.fillRoundRect(x2, y2, w2, calH, calR, RGB565(13, 17, 28));
    _spr.drawRoundRect(x2, y2, w2, calH, calR, RGB565(36, 52, 76));

    // Tờ thứ 1 (Trang trên cùng): Obsidian Paper sâu thẳm
    _spr.fillRoundRect(calX, calY, calW, calH, calR, RGB565(17, 22, 35));
    _spr.drawRoundRect(calX, calY, calW, calH, calR, RGB565(48, 68, 102));

    // Gáy sổ lịch bọc da Đỏ Đô (Kéo dài xuống Y=41, cắt ngang chính giữa các thanh lò xo Y=37..46)
    _spr.fillRoundRect(calX, calY, calW, 34, calR, RGB565(120, 22, 36));
    _spr.fillRect(calX, calY + 18, calW, 16, RGB565(120, 22, 36)); // Phẳng mép dưới tại Y=41 (chia đôi gáy lò xo)

    // 6 Khoen lò xo kim loại (Spiral Rings) — Có khoảng thở trên/dưới rõ ràng
    for (int k = 0; k < 6; k++) {
        int16_t kx = 64 + k * 38;
        // Lỗ đục trên giấy
        _spr.fillCircle(kx + 2, 42, 3, RGB565(8, 10, 16));
        // Khoen kim loại bạc (H=10px, từ Y=37 đến Y=46)
        _spr.fillRoundRect(kx, 37, 4, 10, 2, RGB565(180, 195, 215));
        // Ánh sáng specular highlight trắng
        _spr.drawFastVLine(kx + 1, 38, 8, C_WHITE);
    }

    // ── 3. NÚT ĐIỀU HƯỚNG & TIÊU ĐỀ TRÊN GÁY LỊCH (Y=10..34) ──────────
    // Nút [ < ] Prev Month: Tâm (30, 23), Mũi tên tam giác có trọng tâm chính xác (30.0, 23.0)
    _spr.fillCircle(30, 23, 11, RGB565(80, 15, 26));
    _spr.drawCircle(30, 23, 11, RGB565(170, 45, 65));
    _spr.fillTriangle(26, 23, 32, 18, 32, 28, RGB565(255, 225, 235));

    // Nút [ > ] Next Month: Tâm (290, 23), Mũi tên tam giác có trọng tâm chính xác (290.0, 23.0)
    _spr.fillCircle(290, 23, 11, RGB565(80, 15, 26));
    _spr.drawCircle(290, 23, 11, RGB565(170, 45, 65));
    _spr.fillTriangle(294, 23, 288, 18, 288, 28, RGB565(255, 225, 235));

    // Tiêu đề Tháng • Năm (Chữ vàng ánh kim dập nổi)
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(255, 245, 220));
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), (getLanguage() == LANG_VI ? "THANG %02d  •  %d" : "MONTH %02d  •  %d"), _calMonth, _calYear);
    _spr.drawString(titleBuf, 160, 17, 2);

    // Dòng phụ: Thông tin Âm lịch chi tiết hôm nay (cách đỉnh gáy lò xo 4px)
    bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);
    char subBuf[64];
    if (isCurrentMonth) {
        LunarDate todayLunar = LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
        snprintf(subBuf, sizeof(subBuf), (getLanguage() == LANG_VI ? "Hom nay: %02d/%02d AL  -  Nam %s" : "Today: %02d/%02d Lunar  -  Year %s"), 
                 todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
        _spr.setTextColor(RGB565(245, 215, 150)); // Gold dịu
    } else {
        LunarDate midLunar = LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
        snprintf(subBuf, sizeof(subBuf), (getLanguage() == LANG_VI ? "Nam %s  •  [Cham ve hom nay]" : "Year %s  •  [Tap for today]"), midLunar.yearCanChi);
        _spr.setTextColor(RGB565(255, 200, 100));
    }
    _spr.drawString(subBuf, 160, 29, 1);

    // ── 4. THANH THỨ (T2 to CN, cách đáy gáy lò xo 4px) ───────────────
    const char* daysOfWeekVI[] = { "T2", "T3", "T4", "T5", "T6", "T7", "CN" };
    const char* daysOfWeekEN[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
    const char* const* daysOfWeek = (getLanguage() == LANG_VI) ? daysOfWeekVI : daysOfWeekEN;
    for (int col = 0; col < 7; col++) {
        int16_t hx = 34 + col * 42;
        if (col == 6)      _spr.setTextColor(RGB565(255, 110, 110)); // CN (Sunday) Đỏ San Hô
        else if (col == 5) _spr.setTextColor(RGB565(130, 215, 255)); // T7 Cyan nhạt
        else               _spr.setTextColor(RGB565(140, 170, 205)); // Ngày trong tuần màu Slate
        _spr.drawString(daysOfWeek[col], hx, 58, 2);
    }
    // Đường chỉ mỏng tinh tế ngăn cách thanh thứ
    _spr.drawFastHLine(24, 69, 272, RGB565(32, 46, 68));

    // ── 5. CALENDAR MATRIX CALCULATION ────────────────────────────────
    int daysInMonth = 31;
    if (_calMonth == 4 || _calMonth == 6 || _calMonth == 9 || _calMonth == 11) {
        daysInMonth = 30;
    } else if (_calMonth == 2) {
        bool isLeapYear = (_calYear % 4 == 0 && _calYear % 100 != 0) || (_calYear % 400 == 0);
        daysInMonth = isLeapYear ? 29 : 28;
    }

    static const int tMap[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int yCalc = _calYear;
    if (_calMonth < 3) yCalc -= 1;
    int wday1 = (yCalc + yCalc / 4 - yCalc / 100 + yCalc / 400 + tMap[_calMonth - 1] + 1) % 7;
    int startCol = (wday1 + 6) % 7; // Monday = 0, ..., Sunday = 6

    int totalSlots = startCol + daysInMonth;
    int numRows = (totalSlots + 6) / 7; // 4, 5, hoặc 6 hàng

    // Dynamic row spacing
    int16_t rowH = (numRows <= 5) ? 29 : 24;
    int16_t gridStartY = (numRows <= 5) ? 82 : 79;

    // ── 6. LƯỚI NGÀY (Căn đối xứng chuẩn xác toán học) ───────────────
    for (int d = 1; d <= daysInMonth; d++) {
        int slot = startCol + (d - 1);
        int col = slot % 7;
        int row = slot / 7;
        int16_t cx = 34 + col * 42;
        int16_t cy = gridStartY + row * rowH;

        LunarDate ld = LunarCalendar::convertSolar2Lunar(d, _calMonth, _calYear);
        bool isToday = (d == curDay && isCurrentMonth);

        // Chuỗi ngày âm lịch
        char lStr[8];
        if (ld.day == 1) snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
        else snprintf(lStr, sizeof(lStr), "%d", ld.day);

        // Đo đạc kích thước chính xác theo pixel của từng con số
        char sBuf[8];
        snprintf(sBuf, sizeof(sBuf), "%d", d);
        int16_t sW = _spr.textWidth(sBuf, 2);
        int16_t lW = _spr.textWidth(lStr, 1);
        int16_t gap = 2;
        int16_t totalW = sW + gap + lW;

        // Căn giữa tuyệt đối cụm (Dương lịch + Âm lịch) quanh trục tâm cx
        int16_t startX = cx - totalW / 2;
        int16_t solarX = startX;
        int16_t solarY = cy - 8;
        int16_t lunarX = startX + sW + gap;
        int16_t lunarY = cy;

        if (isToday) {
            // Hôm nay: Ô sáng bo cong tròn mềm mại, đệm đối xứng 100% cả 4 phía
            int16_t boxW = totalW + 10;
            int16_t boxH = (numRows <= 5) ? 24 : 20;
            int16_t boxX = cx - boxW / 2;
            int16_t boxY = cy - boxH / 2;
            int16_t cornerR = (boxH >= 24) ? 8 : 7;

            // Bo 4 góc tròn mịn (anti-aliased smooth rounded corners)
            _spr.fillSmoothRoundRect(boxX, boxY, boxW, boxH, cornerR, theme.green, RGB565(17, 22, 35));

            // Số Dương Lịch (Font 2, TL_DATUM)
            _spr.setTextDatum(TL_DATUM);
            _spr.setTextColor(RGB565(12, 24, 18));
            _spr.drawString(sBuf, solarX, solarY, 2);

            // Số Âm Lịch (Font 1, TL_DATUM — góc dưới phải)
            _spr.setTextColor(RGB565(18, 48, 28));
            _spr.drawString(lStr, lunarX, lunarY, 1);
        } else {
            // Ngày thường
            _spr.setTextDatum(TL_DATUM);
            if (col == 6)      _spr.setTextColor(RGB565(255, 120, 120)); // CN Đỏ San Hô
            else if (col == 5) _spr.setTextColor(RGB565(140, 220, 255)); // T7 Cyan nhạt
            else               _spr.setTextColor(C_WHITE);               // Ngày trong tuần trắng sáng
            _spr.drawString(sBuf, solarX, solarY, 2);

            // Số Âm Lịch (Font 1, TL_DATUM)
            if (ld.day == 1 || ld.day == 15) {
                _spr.setTextColor(RGB565(255, 215, 0)); // Gold rực rỡ
            } else {
                _spr.setTextColor(RGB565(110, 140, 175)); // Xám Slate
            }
            _spr.drawString(lStr, lunarX, lunarY, 1);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 2 — FINANCE BENTO GRID & DUAL-TAB POPUPS (Option 1)
//  Left Column: Vàng SJC 9999 + Thế Giới + Biểu đồ 7 ngày vàng
//  Right Column: Tỷ giá USD, EUR, BTC + Sparkline nội dòng + Popup 2 Tab lớn
// ═══════════════════════════════════════════════════════════════════════

void Display::drawFinanceBadge(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, bool isPositive) {
    if (h % 2 != 0) h++; // Đảm bảo chiều cao luôn chẵn để bán kính r = h/2 tạo thành hình oval tròn hoàn hảo
    int16_t textW = _spr.textWidth(text, 1);
    int16_t badgeW = textW + 8; // Tự động co giãn theo độ dài text chống tràn tuyệt đối
    if (badgeW < w) badgeW = w;
    int16_t r = h / 2;
    uint16_t bgTint = isPositive ? RGB565(15, 45, 25) : RGB565(50, 15, 20);
    uint16_t fgCol = isPositive ? theme.green : RGB565(255, 110, 110);
    _spr.fillSmoothRoundRect(x, y, badgeW, h, r, bgTint, theme.bg);
    _spr.drawRoundRect(x, y, badgeW, h, r, fgCol);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(fgCol);
    _spr.drawString(text, x + badgeW / 2, y + h / 2, 1);
    _spr.setTextDatum(TL_DATUM); // Bảo toàn Datum mặc định cho các hàm vẽ tiếp theo
}

// ── Biểu đồ cột thanh mảnh (Micro Bar Chart) biểu thị biến động tỷ giá các ngày trong tuần ──
void Display::drawFinanceMicroBars(int16_t x, int16_t y, int16_t w, int16_t h, const float* deltas, uint8_t count) {
    if (!deltas || count == 0) return;
    int16_t barW = 4;
    int16_t gap = 3;
    int16_t baseY = y + h / 2;

    // Đường trục giữa mờ (Baseline)
    _spr.drawFastHLine(x, baseY, count * (barW + gap) - gap, RGB565(45, 55, 70));

    for (uint8_t i = 0; i < count; i++) {
        int16_t bx = x + i * (barW + gap);
        float d = deltas[i];
        if (d >= 0) {
            int16_t bh = (int16_t)(d * 40.0f);
            if (bh < 3) bh = 3;
            if (bh > h / 2 - 1) bh = h / 2 - 1;
            _spr.fillRoundRect(bx, baseY - bh, barW, bh + 1, 1, C_GREEN);
        } else {
            int16_t bh = (int16_t)(-d * 40.0f);
            if (bh < 3) bh = 3;
            if (bh > h / 2 - 1) bh = h / 2 - 1;
            _spr.fillRoundRect(bx, baseY, barW, bh + 1, 1, RGB565(255, 95, 95));
        }
    }
}

// ── Biểu đồ Sparkline Vector cao cấp (Vector Line, không banding 8-bit, không nòng nọc) ──
void Display::drawFinanceSparkline(int16_t x, int16_t y, int16_t w, int16_t h,
                                    const float* pts, uint8_t count, uint16_t color, bool areaFill) {
    if (!pts || count < 2) return;

    float minVal = pts[0];
    float maxVal = pts[0];
    for (uint8_t i = 1; i < count; i++) {
        if (pts[i] < minVal) minVal = pts[i];
        if (pts[i] > maxVal) maxVal = pts[i];
    }
    float range = maxVal - minVal;
    if (range < 0.0001f) range = 1.0f;

    // Tọa độ các điểm trên màn hình (đệm 6px trên và dưới để tránh chạm viền)
    int16_t px[16];
    int16_t py[16];
    uint8_t n = (count > 16) ? 16 : count;

    for (uint8_t i = 0; i < n; i++) {
        px[i] = x + (int16_t)((i * (w - 1)) / (n - 1));
        py[i] = (y + h - 6) - (int16_t)(((pts[i] - minVal) / range) * (h - 12));
    }

    // Vẽ 2 đường lưới ngang tham chiếu rất mờ, tinh tế
    _spr.drawFastHLine(x, y + h / 3, w, RGB565(32, 40, 52));
    _spr.drawFastHLine(x, y + (h * 2) / 3, w, RGB565(32, 40, 52));

    // Vẽ đường biểu đồ vector 2px liền mạch, sắc sảo (100% chuẩn màu 16-bit rực rỡ)
    for (uint8_t i = 0; i < n - 1; i++) {
        _spr.drawLine(px[i], py[i], px[i + 1], py[i + 1], color);
        _spr.drawLine(px[i], py[i] + 1, px[i + 1], py[i + 1] + 1, color);
        // Chấm tròn nhỏ tại các mốc dữ liệu
        _spr.fillCircle(px[i], py[i], 1, blendColor(color, C_WHITE, 180));
    }

    // Chốt điểm dữ liệu cuối cùng: chấm tròn nhỏ tinh tế, không bóng nòng nọc
    int16_t lastX = px[n - 1];
    int16_t lastY = py[n - 1];
    _spr.fillCircle(lastX, lastY, 2, color);
    _spr.drawPixel(lastX, lastY, C_WHITE);
}

void Display::renderPage2_Finance() {
    drawBackground();
    const FinanceData& fd = finance.getData();

    // ═══════════════════════════════════════════════════════════════════
    //  CỘT TRÁI: VÀNG SJC 9999 & THẾ GIỚI (X = 8, W = 148, Y = 8, H = 224)
    // ═══════════════════════════════════════════════════════════════════
    int16_t leftX = 8;
    int16_t cardW = 148;
    int16_t cardH = 224;
    drawGlassCard(leftX, 8, cardW, cardH, 10); // Viền kính đồng nhất, không lệch màu đáy

    // Header Vàng
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.yellow);
    _spr.drawString((getLanguage() == LANG_VI ? "VANG SJC" : "SJC GOLD"), leftX + 10, 14, 2);

    char sjcBadge[16];
    bool sjcPos = (fd.sjcChangePercent >= 0);
    snprintf(sjcBadge, sizeof(sjcBadge), "%+.1f%%", fd.sjcChangePercent);
    toVNFloat(sjcBadge);
    drawFinanceBadge(leftX + cardW - 46, 14, 38, 14, sjcBadge, sjcPos);

    // Mua & Bán SJC — Bố cục 2 hàng đối xứng, chống tràn tuyệt đối cho số 5 chữ số (140,3 tr)
    char buyStr[16], sellStr[16];
    snprintf(buyStr, sizeof(buyStr), "%.1f", fd.sjcBuy);
    toVNFloat(buyStr);
    snprintf(sellStr, sizeof(sellStr), "%.1f", fd.sjcSell);
    toVNFloat(sellStr);

    // Hàng 1: MUA
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString((getLanguage() == LANG_VI ? "MUA" : "BUY"), leftX + 10, 37, 1);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(buyStr, leftX + 42, 33, 4);
    _spr.setTextColor(theme.textVDim);
    _spr.drawString((getLanguage() == LANG_VI ? "tr" : "M"), leftX + 42 + _spr.textWidth(buyStr, 4) + 2, 43, 1);

    // Hàng 2: BÁN
    _spr.setTextColor(theme.textDim);
    _spr.drawString((getLanguage() == LANG_VI ? "BAN" : "SELL"), leftX + 10, 58, 1);
    _spr.setTextColor(theme.yellow);
    _spr.drawString(sellStr, leftX + 42, 54, 4);
    _spr.setTextColor(theme.textVDim);
    _spr.drawString((getLanguage() == LANG_VI ? "tr" : "M"), leftX + 42 + _spr.textWidth(sellStr, 4) + 2, 64, 1);

    // Đường phân cách mờ 1 (đồng bộ thẳng hàng với cột tỉ giá tại Y = 84)
    _spr.drawFastHLine(leftX + 8, 84, cardW - 16, blendColor(theme.cardBorder, theme.bg, 110));

    // Giá Vàng Thế Giới (Kitco)
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString((getLanguage() == LANG_VI ? "THE GIOI (KITCO)" : "WORLD (KITCO)"), leftX + 10, 92, 1);

    char kitcoStr[24];
    char kNum[16];
    long kw = (long)roundf(fd.worldGoldUsd);
    FinanceService::formatVND(kw, kNum, sizeof(kNum));
    snprintf(kitcoStr, sizeof(kitcoStr), "$%s", kNum);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(kitcoStr, leftX + 10, 105, 4);
    _spr.setTextColor(theme.textVDim);
    _spr.drawString("/oz", leftX + 10 + _spr.textWidth(kitcoStr, 4) + 3, 114, 1);

    // Đường phân cách mờ 2 (đồng bộ thẳng hàng với cột tỉ giá tại Y = 140)
    _spr.drawFastHLine(leftX + 8, 140, cardW - 16, blendColor(theme.cardBorder, theme.bg, 110));

    // Biểu đồ 7 ngày Vàng SJC (Vector sắc nét)
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.yellow);
    _spr.drawString((getLanguage() == LANG_VI ? "XU HUONG 7 NGAY" : "7-DAY TREND"), leftX + 10, 146, 1);

    drawFinanceSparkline(leftX + 10, 156, cardW - 20, 36, fd.sjc7d, 7, C_GOLD, false);

    // Tính toán nhãn 7 ngày gần nhất động (tính lùi từ Hôm nay)
    time_t rawNow = time(NULL);
    struct tm tmNow;
    localtime_r(&rawNow, &tmNow);
    int curWday = tmNow.tm_wday; // 0 = CN, 1 = T2.. 6 = T7

    _spr.setTextDatum(MC_DATUM);
    for (int i = 0; i < 7; i++) {
        int16_t dx = (leftX + 10) + (int16_t)((i * (cardW - 20 - 1)) / 6);
        int w = (curWday - (6 - i) + 14) % 7;
        if (i == 6) {
            _spr.setTextColor(theme.yellow); // Làm nổi bật ngày hôm nay (Tùy chọn 1)
        } else {
            _spr.setTextColor(theme.textVDim);
        }
        _spr.drawString(getShortDayNameI18n(w), dx, 196, 1);
    }

    // Call-to-action chân thẻ
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(blendColor(theme.textDim, C_WHITE, 120));
    _spr.drawString((getLanguage() == LANG_VI ? "CHAM XEM CHI TIET >" : "TAP FOR DETAILS >"), leftX + cardW / 2, 215, 1);

    // ═══════════════════════════════════════════════════════════════════
    //  CỘT PHẢI: TỶ GIÁ NGOẠI TỆ & VĨ MÔ (X = 164, W = 148, Y = 8, H = 224)
    // ═══════════════════════════════════════════════════════════════════
    int16_t rightX = 164;
    drawGlassCard(rightX, 8, cardW, cardH, 10); // Viền kính đồng nhất, không lệch màu đáy

    // Header Tỷ giá
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.accent);
    _spr.drawString((getLanguage() == LANG_VI ? "TI GIA (VND)" : "RATES (VND)"), rightX + 10, 14, 2);
    _spr.setTextDatum(TR_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("VCB", rightX + cardW - 10, 16, 1);

    // Mục 1: Ngoại tệ 1 (Mặc định: USD)
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(fd.cur1Code, rightX + 10, 38, 2);

    char cur1Badge[16];
    bool cur1Pos = (fd.cur1ChangePercent >= 0);
    snprintf(cur1Badge, sizeof(cur1Badge), "%+.2f%%", fd.cur1ChangePercent);
    toVNFloat(cur1Badge);
    drawFinanceBadge(rightX + 44, 36, 42, 14, cur1Badge, cur1Pos);

    char cur1RateStr[16];
    FinanceService::formatVND((long)roundf(fd.cur1Rate), cur1RateStr, sizeof(cur1RateStr));
    _spr.setTextDatum(TL_DATUM); // Đảm bảo căn trái tuyệt đối
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cur1RateStr, rightX + 10, 55, 4);

    // Biểu đồ cột thanh mảnh 5 ngày (+/-)
    drawFinanceMicroBars(rightX + 96, 55, 40, 22, fd.cur1Bars, 5);

    // Đường phân cách mờ 1
    _spr.drawFastHLine(rightX + 8, 84, cardW - 16, blendColor(theme.cardBorder, theme.bg, 110));

    // Mục 2: Ngoại tệ 2 (Mặc định: EUR hoặc CAD...)
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(fd.cur2Code, rightX + 10, 94, 2);

    char cur2Badge[16];
    bool cur2Pos = (fd.cur2ChangePercent >= 0);
    snprintf(cur2Badge, sizeof(cur2Badge), "%+.2f%%", fd.cur2ChangePercent);
    toVNFloat(cur2Badge);
    drawFinanceBadge(rightX + 44, 92, 42, 14, cur2Badge, cur2Pos);

    char cur2RateStr[16];
    FinanceService::formatVND((long)roundf(fd.cur2Rate), cur2RateStr, sizeof(cur2RateStr));
    _spr.setTextDatum(TL_DATUM); // Đảm bảo căn trái tuyệt đối
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cur2RateStr, rightX + 10, 111, 4);

    // Biểu đồ cột thanh mảnh 5 ngày (+/-)
    drawFinanceMicroBars(rightX + 96, 111, 40, 22, fd.cur2Bars, 5);

    // Đường phân cách mờ 2
    _spr.drawFastHLine(rightX + 8, 140, cardW - 16, blendColor(theme.cardBorder, theme.bg, 110));

    // Mục 3: Macro / BTC
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("BTC / USDT", rightX + 10, 149, 1);

    char btcBadge[16];
    bool btcPos = (fd.btcChangePercent >= 0);
    snprintf(btcBadge, sizeof(btcBadge), "%+.1f%%", fd.btcChangePercent);
    toVNFloat(btcBadge);
    drawFinanceBadge(rightX + 76, 147, 40, 14, btcBadge, btcPos);

    char btcRateStr[16];
    if (fd.btcRate >= 1000.0f) {
        char btcNum[16];
        FinanceService::formatVND((long)roundf(fd.btcRate), btcNum, sizeof(btcNum));
        snprintf(btcRateStr, sizeof(btcRateStr), "$%s", btcNum);
    } else {
        snprintf(btcRateStr, sizeof(btcRateStr), "$%.0f", fd.btcRate);
    }
    _spr.setTextDatum(TL_DATUM); // Đảm bảo căn trái tuyệt đối
    _spr.setTextColor(C_WHITE);
    _spr.drawString(btcRateStr, rightX + 10, 165, 4);

    // Call-to-action chân thẻ: THỐNG NHẤT 100% CÂU CHỮ VỚI CỘT VÀNG
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(blendColor(theme.textDim, C_WHITE, 120));
    _spr.drawString((getLanguage() == LANG_VI ? "CHAM XEM CHI TIET >" : "TAP FOR DETAILS >"), rightX + cardW / 2, 215, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP TỶ GIÁ NGOẠI TỆ — FULL-SCREEN DUAL-TAB (USD / EUR)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderFinanceCurrencyPopup() {
    const FinanceData& fd = finance.getData();

    // Nền tối sâu chuyên nghiệp tràn viền Full Screen (320x240) — Đơn sắc sang trọng, không dải gradient
    _spr.fillRect(0, 0, 320, 240, RGB565(8, 12, 20));

    // ── 2 TAB CHUYỂN NGOẠI TỆ & NÚT [X] ĐÓNG POPUP ──
    char tab1Label[16], tab2Label[16];
    snprintf(tab1Label, sizeof(tab1Label), "%s / VND", fd.cur1Code);
    snprintf(tab2Label, sizeof(tab2Label), "%s / VND", fd.cur2Code);

    // Tab 1 (X: 14..128, Y: 10..36)
    if (_currencyTab == 0) {
        _spr.fillSmoothRoundRect(14, 10, 116, 26, 13, theme.accent, RGB565(8, 12, 20));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(10, 20, 30));
        _spr.drawString(tab1Label, 14 + 58, 10 + 13, 2);
    } else {
        _spr.fillSmoothRoundRect(14, 10, 116, 26, 13, RGB565(18, 26, 38), RGB565(8, 12, 20));
        _spr.drawRoundRect(14, 10, 116, 26, 13, RGB565(42, 60, 85));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(theme.textDim);
        _spr.drawString(tab1Label, 14 + 58, 10 + 13, 2);
    }

    // Tab 2 (X: 136..250, Y: 10..36)
    if (_currencyTab == 1) {
        _spr.fillSmoothRoundRect(136, 10, 116, 26, 13, theme.accent, RGB565(8, 12, 20));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(10, 20, 30));
        _spr.drawString(tab2Label, 136 + 58, 10 + 13, 2);
    } else {
        _spr.fillSmoothRoundRect(136, 10, 116, 26, 13, RGB565(18, 26, 38), RGB565(8, 12, 20));
        _spr.drawRoundRect(136, 10, 116, 26, 13, RGB565(42, 60, 85));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(theme.textDim);
        _spr.drawString(tab2Label, 136 + 58, 10 + 13, 2);
    }

    // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
    drawPopupCloseButton();

    // Đường kẻ phân cách sau hàng Tab (Y = 44) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 44, 300, POPUP_DIVIDER_COLOR);

    // ── NỘI DUNG TỶ GIÁ CHI TIẾT & BIỂU ĐỒ FULL SCREEN ──
    int16_t midX = 160;
    int16_t chartX = 14;
    int16_t chartY = 124;
    int16_t chartW = 292;
    int16_t chartH = 68;

    _spr.setTextDatum(TL_DATUM);

    if (_currencyTab == 0) {
        // Tab Ngoại tệ 1
        char cur1RateStr[16];
        FinanceService::formatVND((long)roundf(fd.cur1Rate), cur1RateStr, sizeof(cur1RateStr));
        _spr.setTextColor(C_WHITE);
        _spr.drawString(cur1RateStr, 16, 52, 4);

        _spr.setTextColor(theme.accent);
        _spr.drawString("VND", 16 + _spr.textWidth(cur1RateStr, 4) + 6, 58, 2);

        char cur1Badge[16];
        bool cur1Pos = (fd.cur1ChangePercent >= 0);
        snprintf(cur1Badge, sizeof(cur1Badge), "%+.2f%%", fd.cur1ChangePercent);
        toVNFloat(cur1Badge);
        drawFinanceBadge(190, 54, 58, 18, cur1Badge, cur1Pos);

        // Dòng biên độ 7 ngày
        char min1[16], max1[16];
        FinanceService::formatVND((long)roundf(fd.cur1Min7d), min1, sizeof(min1));
        FinanceService::formatVND((long)roundf(fd.cur1Max7d), max1, sizeof(max1));
        char range7d[64];
        snprintf(range7d, sizeof(range7d), (getLanguage() == LANG_VI ? "Bien do 7d:   Cao: %s   •   Thap: %s" : "7d Range:   High: %s   •   Low: %s"), max1, min1);

        _spr.setTextDatum(TL_DATUM);
        _spr.setTextColor(RGB565(170, 200, 235));
        _spr.drawString(range7d, 16, 80, 1);

        // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
        _spr.fillSmoothRoundRect(midX - 125, 96, 250, 20, 10, RGB565(16, 24, 36), RGB565(8, 12, 20));
        _spr.drawRoundRect(midX - 125, 96, 250, 20, 10, RGB565(36, 52, 75));

        char dayStr[32], dinhStr[32];
        snprintf(dayStr, sizeof(dayStr), (getLanguage() == LANG_VI ? "Day: %s" : "Low: %s"), min1);
        snprintf(dinhStr, sizeof(dinhStr), (getLanguage() == LANG_VI ? "Dinh: %s" : "High: %s"), max1);

        _spr.setTextDatum(MR_DATUM);
        _spr.setTextColor(RGB565(160, 205, 255));
        _spr.drawString(dayStr, midX - 10, 106, 1);

        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(90, 115, 150));
        _spr.drawString("•", midX, 106, 1);

        _spr.setTextDatum(ML_DATUM);
        _spr.setTextColor(C_GREEN);
        _spr.drawString(dinhStr, midX + 10, 106, 1);

        // Biểu đồ vector 7 ngày khổ rộng 292px
        drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.cur17d, 7, C_GREEN, false);
    } else {
        // Tab Ngoại tệ 2
        char cur2RateStr[16];
        FinanceService::formatVND((long)roundf(fd.cur2Rate), cur2RateStr, sizeof(cur2RateStr));
        _spr.setTextColor(C_WHITE);
        _spr.drawString(cur2RateStr, 16, 52, 4);

        _spr.setTextColor(theme.accent);
        _spr.drawString("VND", 16 + _spr.textWidth(cur2RateStr, 4) + 6, 58, 2);

        char cur2Badge[16];
        bool cur2Pos = (fd.cur2ChangePercent >= 0);
        snprintf(cur2Badge, sizeof(cur2Badge), "%+.2f%%", fd.cur2ChangePercent);
        toVNFloat(cur2Badge);
        drawFinanceBadge(190, 54, 58, 18, cur2Badge, cur2Pos);

        // Dòng biên độ 7 ngày
        char min2[16], max2[16];
        FinanceService::formatVND((long)roundf(fd.cur2Min7d), min2, sizeof(min2));
        FinanceService::formatVND((long)roundf(fd.cur2Max7d), max2, sizeof(max2));
        char range7d[64];
        snprintf(range7d, sizeof(range7d), (getLanguage() == LANG_VI ? "Bien do 7d:   Cao: %s   •   Thap: %s" : "7d Range:   High: %s   •   Low: %s"), max2, min2);

        _spr.setTextDatum(TL_DATUM);
        _spr.setTextColor(RGB565(170, 200, 235));
        _spr.drawString(range7d, 16, 80, 1);

        // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
        _spr.fillSmoothRoundRect(midX - 125, 96, 250, 20, 10, RGB565(16, 24, 36), RGB565(8, 12, 20));
        _spr.drawRoundRect(midX - 125, 96, 250, 20, 10, RGB565(36, 52, 75));

        char dayStr[32], dinhStr[32];
        snprintf(dayStr, sizeof(dayStr), (getLanguage() == LANG_VI ? "Day: %s" : "Low: %s"), min2);
        snprintf(dinhStr, sizeof(dinhStr), (getLanguage() == LANG_VI ? "Dinh: %s" : "High: %s"), max2);

        _spr.setTextDatum(MR_DATUM);
        _spr.setTextColor(RGB565(160, 205, 255));
        _spr.drawString(dayStr, midX - 10, 106, 1);

        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(90, 115, 150));
        _spr.drawString("•", midX, 106, 1);

        _spr.setTextDatum(ML_DATUM);
        _spr.setTextColor(RGB565(255, 120, 120));
        _spr.drawString(dinhStr, midX + 10, 106, 1);

        // Biểu đồ vector 7 ngày khổ rộng 292px
        drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.cur27d, 7, RGB565(255, 95, 95), false);
    }

    // Nhãn 7 ngày gần nhất động (tính lùi từ Hôm nay)
    time_t rawPopTime = time(NULL);
    struct tm tmPop;
    localtime_r(&rawPopTime, &tmPop);
    int curWdayPop = tmPop.tm_wday; // 0 = CN, 1 = T2.. 6 = T7

    _spr.setTextDatum(MC_DATUM);
    for (int i = 0; i < 7; i++) {
        int16_t dx = chartX + (int16_t)((i * (chartW - 1)) / 6);
        int w = (curWdayPop - (6 - i) + 14) % 7;
        if (i == 6) {
            _spr.setTextColor(theme.accent); // Làm nổi bật ngày hôm nay bằng màu nhấn
        } else {
            _spr.setTextColor(RGB565(160, 175, 200));
        }
        _spr.drawString(getShortDayNameI18n(w), dx, chartY + chartH + 9, 1);
    }

    // Footer nguồn dữ liệu
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textVDim);
    _spr.drawString((getLanguage() == LANG_VI ? "Nguon: Vietcombank & ExchangeRate-API" : "Source: Vietcombank & ExchangeRate-API"), 160, 224, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP VÀNG SJC 9999 & THẾ GIỚI — FULL-SCREEN POPUP (320x240)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderFinanceGoldPopup() {
    const FinanceData& fd = finance.getData();

    // Nền tối sâu sang trọng tràn viền Full Screen (320x240) — Đơn sắc sang trọng, không dải gradient
    _spr.fillRect(0, 0, 320, 240, RGB565(12, 11, 16));

    // Tiêu đề Popup
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_GOLD);
    _spr.drawString((getLanguage() == LANG_VI ? "VANG SJC 9999 & THE GIOI" : "SJC GOLD 9999 & WORLD"), 16, 23, 2);

    // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
    drawPopupCloseButton();

    // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
    _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

    // Bảng giá SJC & Thế Giới (Bố cục thông thoáng, không đè chữ)
    char sjcBuyStr[32], sjcSellStr[32];
    snprintf(sjcBuyStr, sizeof(sjcBuyStr), (getLanguage() == LANG_VI ? "SJC MUA: %.1f tr" : "SJC BUY: %.1f M"), fd.sjcBuy);
    toVNFloat(sjcBuyStr);
    snprintf(sjcSellStr, sizeof(sjcSellStr), (getLanguage() == LANG_VI ? "BAN: %.1f tr" : "SELL: %.1f M"), fd.sjcSell);
    toVNFloat(sjcSellStr);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(sjcBuyStr, 16, 48, 2);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(sjcSellStr, 150, 48, 2);

    bool sjcPos = (fd.sjcChangePercent >= 0);
    drawFinanceBadge(252, 47, 52, 18, fd.sjcChangeStr, sjcPos);

    char kitcoStr[48];
    char kNum[16];
    long kw = (long)fd.worldGoldUsd;
    int kdec = (int)(roundf((fd.worldGoldUsd - kw) * 10.0f)) % 10;
    FinanceService::formatVND(kw, kNum, sizeof(kNum));
    snprintf(kitcoStr, sizeof(kitcoStr), (getLanguage() == LANG_VI ? "THE GIOI (KITCO):  $%s,%d / oz" : "WORLD (KITCO):  $%s,%d / oz"), kNum, kdec);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(170, 215, 255));
    _spr.drawString(kitcoStr, 16, 70, 2);

    char diffStr[48];
    snprintf(diffStr, sizeof(diffStr), (getLanguage() == LANG_VI ? "Chenh lech trong nuoc: ~%.1f tr/luong" : "Domestic spread: ~%.1f M/tael"), fd.domesticDiffTr);
    toVNFloat(diffStr);
    _spr.setTextColor(RGB565(200, 210, 225));
    _spr.drawString(diffStr, 16, 90, 1);

    // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
    int16_t midX = 160;
    _spr.fillSmoothRoundRect(midX - 125, 104, 250, 20, 10, RGB565(24, 22, 18), RGB565(12, 11, 16));
    _spr.drawRoundRect(midX - 125, 104, 250, 20, 10, RGB565(70, 58, 35));

    char dayGStr[32], dinhGStr[32];
    snprintf(dayGStr, sizeof(dayGStr), (getLanguage() == LANG_VI ? "Day: %.1f tr" : "Low: %.1f M"), fd.sjcMin7d);
    toVNFloat(dayGStr);
    snprintf(dinhGStr, sizeof(dinhGStr), (getLanguage() == LANG_VI ? "Dinh: %.1f tr" : "High: %.1f M"), fd.sjcMax7d);
    toVNFloat(dinhGStr);

    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(160, 205, 255));
    _spr.drawString(dayGStr, midX - 10, 114, 1);

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(120, 105, 70));
    _spr.drawString("•", midX, 114, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(dinhGStr, midX + 10, 114, 1);

    // Biểu đồ vàng 7 ngày khổ rộng 292px
    int16_t chartX = 14;
    int16_t chartY = 130;
    int16_t chartW = 292;
    int16_t chartH = 64;

    drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.sjc7d, 7, C_GOLD, false);

    // Trục 7 ngày gần nhất động (tính lùi từ Hôm nay)
    time_t rawGoldTime = time(NULL);
    struct tm tmGold;
    localtime_r(&rawGoldTime, &tmGold);
    int curWdayGold = tmGold.tm_wday; // 0 = CN, 1 = T2.. 6 = T7

    _spr.setTextDatum(MC_DATUM);
    for (int i = 0; i < 7; i++) {
        int16_t dx = chartX + (int16_t)((i * (chartW - 1)) / 6);
        int w = (curWdayGold - (6 - i) + 14) % 7;
        if (i == 6) {
            _spr.setTextColor(C_GOLD); // Làm nổi bật ngày hôm nay bằng màu vàng kim
        } else {
            _spr.setTextColor(RGB565(180, 185, 195));
        }
        _spr.drawString(getShortDayNameI18n(w), dx, chartY + chartH + 9, 1);
    }

    // Footer nguồn
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textVDim);
    _spr.drawString((getLanguage() == LANG_VI ? "Nguon: SJC & Kitco  •  Don vi: Trieu VND/Luong" : "Source: SJC & Kitco  •  Unit: Million VND/Tael"), 160, 224, 1);
}

// ═══════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════
//  POMODORO MASCOT & PAGE 3 (Landscape Split Layout — Option 3)
//  Trái: Vòng tròn lớn (R=60) ôm chú Cà chua Mascot sinh động
//  Phải: Đồng hồ số đếm ngược to đậm (Font 6) + Nút bấm công thái học
// ═══════════════════════════════════════════════════════════════════════

void Display::drawTomatoMascot(int16_t cx, int16_t cy, bool isFocus) {
    // 1. Bóng đổ mềm mại dưới chân chú cà chua
    _spr.fillSmoothCircle(cx - 9, cy + 27, 7, RGB565(15, 20, 30), theme.bg);
    _spr.fillSmoothCircle(cx + 9, cy + 27, 7, RGB565(15, 20, 30), theme.bg);
    _spr.fillRect(cx - 9, cy + 22, 18, 9, RGB565(15, 20, 30));

    // 2. Thân quả cà chua mập mạp (ĐỎ TƯƠI NGUYÊN BẢN, B=0 TUYỆT ĐỐI)
    uint16_t bodyColor = RGB565(255, 30, 0);  // Đỏ cờ / Đỏ cà chua chín rực rỡ
    _spr.fillSmoothCircle(cx - 12, cy + 1, 26, bodyColor, theme.bg);
    _spr.fillSmoothCircle(cx + 12, cy + 1, 26, bodyColor, theme.bg);
    _spr.fillSmoothCircle(cx, cy + 5, 25, bodyColor, theme.bg);
    _spr.fillRect(cx - 12, cy - 18, 24, 40, bodyColor);

    // 3. Vệt sáng phản chiếu (Specular Highlight) màu cam đỏ ấm
    _spr.fillSmoothCircle(cx - 15, cy - 8, 5, RGB565(255, 120, 80), bodyColor);
    _spr.fillSmoothCircle(cx - 10, cy - 13, 3, RGB565(255, 160, 120), bodyColor);

    // 4. Cuống và đài lá xanh biếc trên đầu (Green Leaves & Stem)
    uint16_t stemColor = RGB565(20, 110, 40);  // Cuống xanh đậm
    uint16_t leafColor = RGB565(35, 190, 60);  // Lá xanh tươi
    _spr.fillRoundRect(cx - 3, cy - 29, 6, 11, 2, stemColor);
    // 3 cánh lá sao xòe ra
    _spr.fillTriangle(cx - 1, cy - 18, cx - 18, cy - 25, cx - 4, cy - 18, leafColor);
    _spr.fillTriangle(cx + 1, cy - 18, cx + 18, cy - 25, cx + 4, cy - 18, leafColor);
    _spr.fillTriangle(cx - 8, cy - 18, cx + 8, cy - 18, cx, cy - 10, RGB565(50, 215, 85));

    // 5. Khuôn mặt anime dễ thương (Cute Anime Face)
    uint16_t eyeColor = RGB565(20, 20, 28);
    if (isFocus) {
        // Đang tập trung: Mắt to tròn long lanh, miệng cười tươi
        // Mắt trái
        _spr.fillSmoothCircle(cx - 13, cy + 3, 5, eyeColor, bodyColor);
        _spr.drawPixel(cx - 14, cy + 2, C_WHITE);
        _spr.drawPixel(cx - 13, cy + 2, C_WHITE);
        // Mắt phải
        _spr.fillSmoothCircle(cx + 13, cy + 3, 5, eyeColor, bodyColor);
        _spr.drawPixel(cx + 12, cy + 2, C_WHITE);
        _spr.drawPixel(cx + 13, cy + 2, C_WHITE);

        // Miệng cười ngọt ngào :)
        _spr.drawPixel(cx - 4, cy + 12, RGB565(55, 15, 15));
        _spr.drawPixel(cx - 3, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx - 2, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx - 1, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx,     cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx + 1, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx + 2, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx + 3, cy + 13, RGB565(55, 15, 15));
        _spr.drawPixel(cx + 4, cy + 12, RGB565(55, 15, 15));
    } else {
        // Đang giải lao: Mắt híp cười thư giãn (^ ^)
        _spr.drawLine(cx - 16, cy + 5, cx - 11, cy, RGB565(45, 15, 15));
        _spr.drawLine(cx - 11, cy, cx - 6, cy + 5, RGB565(45, 15, 15));
        _spr.drawLine(cx + 6, cy + 5, cx + 11, cy, RGB565(45, 15, 15));
        _spr.drawLine(cx + 11, cy, cx + 16, cy + 5, RGB565(45, 15, 15));

        // Miệng cười nhẹ
        _spr.drawFastHLine(cx - 4, cy + 13, 9, RGB565(55, 15, 15));
    }

    // 6. Đôi má ửng cam hồng (Warm Blush Cheeks)
    _spr.fillSmoothCircle(cx - 22, cy + 8, 4, RGB565(255, 95, 60), bodyColor);
    _spr.fillSmoothCircle(cx + 22, cy + 8, 4, RGB565(255, 95, 60), bodyColor);
}

void Display::renderPage3_Pomodoro() {
    drawBackground();

    // ── 1. THANH CHỌN CHẾ ĐỘ DẠNG VIÊN THUỐC HỢP NHẤT (Top Segmented Capsule) ──
    // Y = 8..28 (H=20px), Capsule nền tối bo tròn X = 45..275 (W=230px)
    int16_t capX = 45, capY = 8, capW = 230, capH = 20, capR = 10;
    _spr.fillSmoothRoundRect(capX, capY, capW, capH, capR, RGB565(20, 26, 38), theme.bg);
    _spr.drawRoundRect(capX, capY, capW, capH, capR, RGB565(36, 48, 68));

    const char* tabNamesVI[] = { "TAP TRUNG", "NGHI 5P", "NGHI 15P" };
    const char* tabNamesEN[] = { "FOCUS", "BREAK 5M", "BREAK 15M" };
    const char* const* tabNames = (getLanguage() == LANG_VI) ? tabNamesVI : tabNamesEN;
    int16_t segX[] = { 47, 123, 199 };
    int16_t segW[] = { 75, 75, 74 };

    uint16_t modeColors[] = {
        RGB565(255, 30, 0),    // Focus: Đỏ Cà Chua nguyên bản
        RGB565(35, 210, 140),  // Short Break: Mint Green
        RGB565(55, 175, 255)   // Long Break: Sky Blue
    };
    uint16_t activeColor = modeColors[_pomoMode];

    for (int i = 0; i < 3; i++) {
        bool isActive = (_pomoMode == i);
        if (isActive) {
            _spr.fillSmoothRoundRect(segX[i], capY + 1, segW[i], capH - 2, 8, activeColor, RGB565(20, 26, 38));
            _spr.setTextDatum(MC_DATUM);
            _spr.setTextColor(C_WHITE);
            _spr.drawString(tabNames[i], segX[i] + segW[i] / 2, capY + capH / 2, 1);
        } else {
            _spr.setTextDatum(MC_DATUM);
            _spr.setTextColor(RGB565(120, 140, 170));
            _spr.drawString(tabNames[i], segX[i] + segW[i] / 2, capY + capH / 2, 1);
        }
    }

    // ── 2. NỬA TRÁI: VÒNG TRÒN TIẾN ĐỘ LỚN & CHÚ CÀ CHUA (Hero Left Dial) ──────
    // Tâm (cx=88, cy=126), Bán kính ngoài R=68, R trong=61 (X: 20..156, Y: 58..194)
    int16_t cx = 88, ringCy = 126;
    int16_t outerR = 68, innerR = 61;

    // Vòng ray nền MÀU XÁM TRUNG TÍNH (Neutral Slate Gray, TUYỆT ĐỐI KHÔNG XANH NAVY)
    _spr.drawArc(cx, ringCy, outerR, innerR, 0, 360, RGB565(75, 75, 75), theme.bg, true);

    // Vẽ Desk Pet Mascot (Cà chua / Mèo / Shiba / Capybara) ở trung tâm vòng tròn
    deskPet.render(_spr, cx, ringCy + 1, _pomoState, _pomoMode, _pomoRemainingSec, theme.bg);

    // Tính % tiến độ
    float progress = 0.0f;
    if (_pomoTotalSec > 0) {
        progress = (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    uint32_t sweepDeg = (uint32_t)(progress * 360.0f);
    if (sweepDeg >= 360) {
        _spr.drawArc(cx, ringCy, outerR, innerR, 0, 360, activeColor, theme.bg, true);
    } else if (sweepDeg > 0) {
        uint32_t startAng = 180; // 12 o'clock in TFT_eSPI
        uint32_t endAng = (180 + sweepDeg) % 360;
        _spr.drawArc(cx, ringCy, outerR, innerR, startAng, endAng, activeColor, theme.bg, true);
    }

    // Chấm bi Knob ở đầu mút cung tròn
    float angRad = (180.0f + (float)sweepDeg) * 0.0174532925f; // deg to rad
    float midR = (outerR + innerR) * 0.5f;
    int16_t knobX = cx - (int16_t)(sinf(angRad) * midR);
    int16_t knobY = ringCy + (int16_t)(cosf(angRad) * midR);

    _spr.fillSmoothCircle(knobX, knobY, 5, activeColor, theme.bg);
    _spr.fillSmoothCircle(knobX, knobY, 2, C_WHITE, activeColor);

    // ── 3. NỬA PHẢI: ĐỒNG HỒ SỐ ĐẾM NGƯỢC SIÊU TO & BẢNG NÚT BẤM (Right Panel) ─
    int16_t cx2 = 236;

    // Dòng trạng thái nhỏ trang nhã (Status tag)
    const char* statusDesc = (getLanguage() == LANG_VI) ? "DANG TAP TRUNG" : "FOCUSING...";
    if (_pomoState == POMO_STOPPED) {
        statusDesc = (_pomoMode == POMO_FOCUS)
                     ? ((getLanguage() == LANG_VI) ? "SAN SANG TAP TRUNG" : "READY TO FOCUS")
                     : ((getLanguage() == LANG_VI) ? "SAN SANG NGHI" : "READY FOR BREAK");
    } else if (_pomoState == POMO_PAUSED) {
        statusDesc = (getLanguage() == LANG_VI) ? "DANG TAM DUNG" : "PAUSED";
    } else {
        if (_pomoMode == POMO_SHORT_BREAK) statusDesc = (getLanguage() == LANG_VI) ? "NGHI NGAN 5 PHUT" : "SHORT BREAK 5M";
        else if (_pomoMode == POMO_LONG_BREAK) statusDesc = (getLanguage() == LANG_VI) ? "NGHI DAI 15 PHUT" : "LONG BREAK 15M";
    }
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(activeColor);
    _spr.drawString(statusDesc, cx2, 48, 2);

    // Đồng hồ số đếm ngược Font 6 cực to, sắc nét, nổi bật hoàn toàn (Y=104)
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _pomoRemainingSec / 60, _pomoRemainingSec % 60);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(timeBuf, cx2, 104, 6);

    // ── 4. CỤM NÚT ĐIỀU KHIỂN CÔNG THÁI HỌC (Y = 152..194, H = 42px — Rộng rãi, kết thúc tại Y=194) ──
    int16_t btnY = 152, btnH = 42;

    if (_pomoState == POMO_STOPPED) {
        // Nút Start lớn bo tròn viên thuốc (Radius 21)
        int16_t bX = 162, bW = 148;
        _spr.fillSmoothRoundRect(bX, btnY, bW, btnH, 21, activeColor, theme.bg);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString(L(TXT_POMO_START), bX + bW / 2, btnY + btnH / 2, 2);
    } else if (_pomoState == POMO_RUNNING) {
        // Nút Trái: TAM DUNG / PAUSE
        int16_t b1X = 158, b1W = 74;
        _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, RGB565(245, 180, 45), theme.bg);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(20, 20, 25));
        _spr.drawString(L(TXT_POMO_PAUSE), b1X + b1W / 2, btnY + btnH / 2, 2);

        // Nút Phải: DAT LAI / RESET
        int16_t b2X = 236, b2W = 74;
        _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(35, 42, 52), theme.bg);
        _spr.drawRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(65, 78, 95));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(220, 230, 245));
        _spr.drawString(L(TXT_POMO_RESET), b2X + b2W / 2, btnY + btnH / 2, 2);
    } else if (_pomoState == POMO_PAUSED) {
        // Nút Trái: TIEP TUC / RESUME
        int16_t b1X = 158, b1W = 74;
        _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, RGB565(35, 215, 140), theme.bg);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(15, 25, 20));
        _spr.drawString((getLanguage() == LANG_VI) ? "TIEP TUC" : "RESUME", b1X + b1W / 2, btnY + btnH / 2, 2);

        // Nút Phải: DAT LAI / RESET
        int16_t b2X = 236, b2W = 74;
        _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(180, 35, 50), theme.bg);
        _spr.drawRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(220, 60, 80));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString(L(TXT_POMO_RESET), b2X + b2W / 2, btnY + btnH / 2, 2);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 4 — DESK MOOD / VIBE SIGNBOARD
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage4_Mood() {
    deskMood.renderPage(_spr, (uint8_t)getLanguage(), theme.bg);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 5 — SETTINGS (Pure Seamless Android Material 3 List — ZERO CARDS)
//  6 Rows: Language, Theme, Touch Sound, Alarm Melody, Brightness, System
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage5_Settings() {
    // Nền trơn tĩnh (Solid Velvet) chuẩn màu Theme, loại bỏ toàn bộ sao lốm đốm để text rõ nét
    _spr.fillSprite(theme.bg);

    // ── 1. Header Bar Phẳng Liền Mạch (Y = 6..24) ────────────────────
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.accent);
    _spr.drawString(L(TXT_SETTINGS), 20, 8, 2);

    _spr.setTextDatum(TR_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("SmartDesk V2.1", 300, 10, 1);

    // Đường hairline ngăn cách mỏng 1px
    _spr.drawFastHLine(20, 25, 280, RGB565(32, 42, 58));

    // ── 2. Hàng 1: Ngôn ngữ (Language) (Y = 26..53, Tâm Y = 39) ───────
    // Icon tròn màu Xanh ngọc lục bảo
    _spr.fillCircle(30, 39, 8, RGB565(30, 180, 90));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("L", 30, 39, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_ROW_LANGUAGE), 46, 39, 2);

    // Giá trị hiện tại + Mũi tên ›
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(theme.accent);
    char langStr[32];
    snprintf(langStr, sizeof(langStr), "%s  >", getLanguageName());
    _spr.drawString(langStr, 300, 39, 2);

    // Hairline
    _spr.drawFastHLine(20, 53, 280, RGB565(24, 32, 46));

    // ── 3. Hàng 2: Giao diện (Theme) (Y = 54..81, Tâm Y = 67) ────────
    // Icon tròn màu Tím
    _spr.fillCircle(30, 67, 8, RGB565(130, 65, 210));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("T", 30, 67, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_ROW_THEME), 46, 67, 2);

    // Giá trị hiện tại + Mũi tên ›
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(theme.accent);
    char thStr[32];
    snprintf(thStr, sizeof(thStr), "%s  >", getThemeName());
    _spr.drawString(thStr, 300, 67, 2);

    // Hairline
    _spr.drawFastHLine(20, 81, 280, RGB565(24, 32, 46));

    // ── 4. Hàng 3: Âm chạm cảm ứng (Touch Sound) (Y = 82..109, Tâm Y = 95) ──
    // Icon tròn màu Xanh ngọc
    _spr.fillCircle(30, 95, 8, RGB565(0, 170, 220));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("S", 30, 95, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_ROW_TOUCH_SND), 46, 95, 2);

    // Giá trị hiện tại + Mũi tên ›
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(theme.accent);
    char sndStr[32];
    snprintf(sndStr, sizeof(sndStr), "%s  >", hardware.getTouchSoundName());
    _spr.drawString(sndStr, 300, 95, 2);

    // Hairline
    _spr.drawFastHLine(20, 109, 280, RGB565(24, 32, 46));

    // ── 5. Hàng 4: Chuông báo thức (Alarm Melody) (Y = 110..137, Tâm Y = 123) ──
    // Icon tròn màu Cam san hô
    _spr.fillCircle(30, 123, 8, RGB565(240, 95, 45));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("A", 30, 123, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_ROW_ALARM_MEL), 46, 123, 2);

    // Giá trị chuông
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(theme.accent);
    char melStr[32];
    snprintf(melStr, sizeof(melStr), "%s >", hardware.getAlarmMelodyName());
    _spr.drawString(melStr, 242, 123, 2);

    // Nút [THỬ] / [TEST] phát loa
    _spr.fillRoundRect(250, 113, 50, 20, 10, theme.green);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_BLACK);
    _spr.drawString(L(TXT_TEST), 275, 123, 2);

    // Hairline
    _spr.drawFastHLine(20, 137, 280, RGB565(24, 32, 46));

    // ── 6. Hàng 5: Độ sáng màn hình (Display Brightness) (Y = 138..165, Tâm Y = 151) ──
    // Icon tròn màu Vàng nắng
    _spr.fillCircle(30, 151, 8, RGB565(245, 175, 25));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_BLACK);
    _spr.drawString("B", 30, 151, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_ROW_BRIGHTNESS), 46, 151, 2);

    // Thanh tiến trình độ sáng liền mạch (X = 112, Y = 147, W = 88, H = 8)
    _spr.fillRoundRect(112, 147, 88, 8, 4, RGB565(25, 34, 48));
    bool isAuto = hardware.isAutoBrightness();
    int fillW = map(hardware.getBacklight(), 0, 100, 0, 88);
    if (fillW > 0) {
        _spr.fillRoundRect(112, 147, fillW, 8, 4, isAuto ? theme.green : theme.accent);
    }

    // Phần trăm
    char bStr[8];
    snprintf(bStr, sizeof(bStr), "%d%%", hardware.getBacklight());
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(bStr, 204, 151, 1);

    // Nút [AUTO]
    _spr.fillRoundRect(246, 140, 54, 22, 11, isAuto ? RGB565(15, 60, 40) : RGB565(20, 28, 42));
    _spr.drawRoundRect(246, 140, 54, 22, 11, isAuto ? theme.green : RGB565(44, 58, 82));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(isAuto ? theme.green : theme.textDim);
    _spr.drawString(L(TXT_AUTO), 273, 151, 1);

    // Hairline
    _spr.drawFastHLine(20, 165, 280, RGB565(24, 32, 46));

    // ── 7. Hàng 6: Hệ thống & WiFi (Y = 166..196, Tâm Y = 181) ───────
    // Icon tròn màu Xanh dương
    _spr.fillCircle(30, 181, 8, RGB565(45, 125, 245));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("i", 30, 181, 1);

    // Tiêu đề
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_SYSTEM_IP), 46, 181, 2);

    // IP Address
    String ipStr = "IP: " + WiFi.localIP().toString();
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(ipStr.c_str(), 300, 181, 1);
}

// ─── Touch Handler for Page 4 Settings (Full-Row Taps) ────────────────
bool Display::handleTouchSettings(int16_t x, int16_t y) {
    // ── Hàng 1: Đổi Ngôn ngữ (Language) (Y: 26..53) ──
    if (y >= 26 && y <= 53) {
        toggleLanguage();
        hardware.playTouchSound();
        _lastRender = 0;
        return true;
    }

    // ── Hàng 2: Đổi Giao diện (Theme) (Y: 54..81) ──
    if (y >= 54 && y <= 81) {
        nextTheme();
        hardware.playTouchSound();
        _lastRender = 0;
        return true;
    }

    // ── Hàng 3: Đổi Âm chạm cảm ứng (Touch Sound) (Y: 82..109) ──
    if (y >= 82 && y <= 109) {
        hardware.nextTouchSound();
        hardware.playTouchSound();
        _lastRender = 0;
        return true;
    }

    // ── Hàng 4: Chuông báo thức (Alarm Melody) (Y: 110..137) ──
    if (y >= 110 && y <= 137) {
        if (x >= 246) {
            // Nút [THỬ]: Phát chuông trên loa ngoài GPIO 26
            hardware.playAlarmMelody();
            _lastRender = 0;
            return true;
        } else {
            // Chạm vào thân hàng: Chuyển điệu chuông tiếp theo
            hardware.nextAlarmMelody();
            hardware.playTouchSound();
            _lastRender = 0;
            return true;
        }
    }

    // ── Hàng 5: Độ sáng màn hình (Y: 138..165) ──
    if (y >= 138 && y <= 165) {
        if (x >= 240) {
            // Nút [AUTO]: Bật / tắt cảm biến quang LDR
            hardware.setAutoBrightness(!hardware.isAutoBrightness());
            hardware.playTouchSound();
            _lastRender = 0;
            return true;
        } else {
            // Chạm vào thanh trượt hoặc nửa trái hàng: Tăng/chỉnh độ sáng tuần hoàn
            hardware.setAutoBrightness(false);
            int curB = hardware.getBacklight();
            curB += 20;
            if (curB > 100) curB = 20;
            hardware.setBacklight(curB);
            hardware.playTouchSound();
            _lastRender = 0;
            return true;
        }
    }

    // ── Hàng 6: Thông tin hệ thống (Y: 166..196) ──
    if (y >= 166 && y <= 196) {
        hardware.playTouchSound();
        return true;
    }

    return false;
}




