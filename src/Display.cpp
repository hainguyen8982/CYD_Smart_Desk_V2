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
#include "AnniversaryManager.h"
#include "DeskMood.h"
#include "DeskPet.h"
#include "DockIconsColor20.h"
#include "EcosystemService.h"
#include "FabXLogo.h"
#include "FinanceService.h"
#include "HardwareManager.h"
#include "Language.h"
#include "TouchManager.h"
#include "WeatherService.h"
#include "WebDashboard.h"
#include "ZenFonts.h"
#include "vn_lunar.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <qrcode.h>
#include <time.h>

Display display;

// ─── Display Core ───────────────────────────────────────────────────

// ─── Constructor ─────────────────────────────────────────────────────
Display::Display()
    : _tft(), _spr(&_tft), _sprReady(false), _lastRender(0),
      _currentPage(PAGE_CLOCK), _dockVisible(true), _lastDockActivity(0),
      _activePopup(POPUP_NONE), _currencyTab(0), _settingsTab(0),
      _settingsSoundIsStartup(true), _alarmEnabled(false), _alarmHour(7),
      _alarmMinute(0), _alarmRinging(false), _alarmRingStart(0),
      _lastAlarmBeepTick(0), _alarmSnoozed(false), _alarmSnoozeUntil(0),
      _alarmLastTriggerDay(-1), _alarmLastTriggerMin(-1),
      _alarmAutoCloseTime(0), _calYear(0), _calMonth(0), _pomoMode(POMO_FOCUS),
      _pomoState(POMO_STOPPED), _pomoTotalSec(25 * 60),
      _pomoRemainingSec(25 * 60), _pomoSession(1), _pomoLastSecTick(0),
      _tickerOffset(0), _lastTickerTick(0), _tickerCachedDay(0) {}

// ─── Begin ───────────────────────────────────────────────────────────
void Display::begin() {
  initLanguage();      // Load saved language (VI / EN) from NVS
  initTheme();         // Load saved theme from NVS
  loadAlarmSettings(); // Load saved alarm settings from NVS
  deskPet.begin();     // Load saved Desk Pet mascot from NVS
  deskMood.begin();    // Load saved Desk Mood from NVS

  // Mount LittleFS and ensure clean storage (purge leftover wallpaper if any)
  if (LittleFS.begin(true)) {
    if (LittleFS.exists("/wallpaper.jpg")) {
      LittleFS.remove("/wallpaper.jpg");
      Serial.println("[Display] Purged legacy /wallpaper.jpg");
    }
  }

#ifdef BOARD_NICEMCU_28IPS
  // Hardware reset shared between ST7789 Display and CST816D Touch (GPIO 2)
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(30);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(50);
#endif

  _tft.init();
  delay(100);
  _tft.setRotation(3);      // Landscape 320×240 (Xoay 180° theo chiều lắp case)
  _tft.invertDisplay(true); // CYD panel requires color inversion
  _tft.fillScreen(C_BLACK);

  Serial.printf("[Display] Free heap: %u | Max alloc block: %u\n",
                ESP.getFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  // Allocate sprite: try 16-bit first (~153KB), fallback to 8-bit (~76KB) if
  // RAM insufficient
  _spr.setColorDepth(16);
  void *buf = _spr.createSprite(SCREEN_W, SCREEN_H);
  if (!buf) {
    Serial.println("[Display] 16-bit sprite alloc failed (~153KB) -> Fallback "
                   "to 8-bit (~76KB)...");
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
    Serial.println(
        "[Display] ERROR: Both 16-bit and 8-bit Sprite alloc failed!");
    _sprReady = false;
    hardware.setBacklight(85);
    _tft.setTextColor(TFT_RED, C_BLACK);
    _tft.drawString("RAM Error: Sprite failed!", SCREEN_W / 2,
                    SCREEN_H / 2 + 40, 2);
  }
}

// ─── FabX Logo & Boot Animation ──────────────────────────────────────
void Display::drawFabXLogo(int16_t x, int16_t y, uint8_t bloomAlpha) {
  if (!_sprReady)
    return;
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
  if (!_sprReady)
    return;

  struct ParticleState {
    float startX, startY;
    int16_t targetX, targetY;
    float nx, ny;
    float curve;
    uint16_t color;
    uint16_t delayMs;
    uint16_t durationMs;
  };

  ParticleState *particles =
      (ParticleState *)malloc(sizeof(ParticleState) * FABX_PARTICLE_COUNT);
  if (!particles) {
    Serial.println(
        "[Display] Boot animation malloc failed -> Fallback to static splash");
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
    particles[i].color = pt.color;

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
      particles[i].ny = dx / dist;
    } else {
      particles[i].nx = 0.0f;
      particles[i].ny = 0.0f;
    }

    particles[i].curve = (float)random(-42, 43); // Arc deviation in pixels
    particles[i].delayMs = random(0, 650);       // Staggered launch up to 650ms
    particles[i].durationMs = random(1300, 1750); // 1.3s - 1.75s travel time
  }

  unsigned long animStart = millis();
  const unsigned long totalDuration = 2400; // 2.4s cinematic sequence

  while (millis() - animStart < totalDuration) {
    unsigned long elapsed = millis() - animStart;

    _spr.fillSprite(C_BLACK);

    for (int i = 0; i < FABX_PARTICLE_COUNT; i++) {
      if (elapsed < particles[i].delayMs)
        continue;

      float t = (float)(elapsed - particles[i].delayMs) /
                (float)particles[i].durationMs;
      if (t >= 1.0f) {
        // Particle has reached target -> draw solid pixel at final position
        _spr.drawPixel(particles[i].targetX, particles[i].targetY,
                       particles[i].color);
        continue;
      }

      // Smooth cubic ease out: 1 - (1-t)^3
      float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
      float dx = particles[i].targetX - particles[i].startX;
      float dy = particles[i].targetY - particles[i].startY;

      // Arc curvature via sine envelope: 0 at start, max at mid-flight, 0 at
      // target
      float arc = sinf(t * 3.14159265f) * particles[i].curve;

      float curX = particles[i].startX + ease * dx + particles[i].nx * arc;
      float curY = particles[i].startY + ease * dy + particles[i].ny * arc;

      // Render subtle neon trail behind moving particle
      if (t > 0.06f) {
        float tTrail = t - 0.05f;
        float easeTrail =
            1.0f - (1.0f - tTrail) * (1.0f - tTrail) * (1.0f - tTrail);
        float arcTrail = sinf(tTrail * 3.14159265f) * particles[i].curve;
        float trX =
            particles[i].startX + easeTrail * dx + particles[i].nx * arcTrail;
        float trY =
            particles[i].startY + easeTrail * dy + particles[i].ny * arcTrail;
        _spr.drawPixel((int16_t)trX, (int16_t)trY,
                       blendColor(particles[i].color, C_BLACK, 160));
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

  // 1. Chữ FabX bừng sáng chói lòa rồi từ từ dịu về màu logo chuẩn (Bloom Flare
  // Decay)
  const uint8_t flareSteps[] = {240, 180, 120, 60, 0};
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

void Display::showBootStatus(const char *status, uint8_t progressPercent) {
  if (!status)
    return;

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

    // Active fill (Gradient: Royal Blue 0x13BA -> Cyan 0x063F -> Neon Green
    // 0x0672)
    uint8_t pct = constrain(progressPercent, 5, 100);
    int16_t fillW = (barW * pct) / 100;
    if (fillW > 0) {
      for (int16_t x = 0; x < fillW; x++) {
        float r = (float)x / (float)barW;
        uint16_t gradColor;
        if (r < 0.6f) {
          gradColor = blendColor(0x13BA, 0x063F, (uint8_t)((r / 0.6f) * 255));
        } else {
          gradColor =
              blendColor(0x063F, 0x0672, (uint8_t)(((r - 0.6f) / 0.4f) * 255));
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
    _spr.drawString("FABX SMARTDESK V2  •  COMMERCIAL EDITION", SCREEN_W / 2,
                    222, 1);

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
  if (!_sprReady)
    return;

  // Duy trì bộ đếm Pomodoro chạy ngầm liên tục
  pomoTick();

  // Duy trì kiểm tra Báo thức chạy ngầm liên tục
  checkAlarmTick();

  unsigned long now = millis();
  if (now - _lastRender < DISPLAY_FPS_INTERVAL)
    return;
  _lastRender = now;

  // If in touch calibration mode, render crosshair overlay
  if (touch.isCalibrating()) {
    _zen16ScreenValid = false;
    renderCalibrationScreen();
    _spr.pushSprite(0, 0);
    return;
  }

  // NẾU BÁO THỨC ĐANG KÊU: RENDER TOÀN MÀN HÌNH BÁO THỨC (Ưu tiên tuyệt đối)
  if (_alarmRinging) {
    _zen16ScreenValid = false;
    if (isMinimalUiMode()) {
      renderAlarmRingingScreenZen16();
    } else {
      renderAlarmRingingScreen();
      _spr.pushSprite(0, 0);
    }
    return;
  }

  // NẾU ĐANG MỞ POPUP BÁO THỨC: ĐÓNG BĂNG VẼ LẠI NỀN ĐỂ CHỐNG NHẤP NHÁY TUYỆT
  // ĐỐI
  if (_activePopup == POPUP_ALARM) {
    _zen16ScreenValid = false;
    _zen16LastPopup = (uint8_t)POPUP_ALARM;
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
    _zen16ScreenValid = false;
  }

  // ── TRUE 16-BIT RGB565 FAST PATH FOR MINIMAL ZEN SCREENS ───────────
  // Bypasses 8-bit RGB332 sprite quantization for silky-smooth 16-level
  // Be Vietnam Pro anti-aliasing (zero jaggies, zero blotchy text, zero
  // flicker!)
  if (isMinimalUiMode() && _activePopup == POPUP_WEATHER) {
    if (!_zen16ScreenValid || _zen16LastPopup != (uint8_t)POPUP_WEATHER) {
      renderWeatherPopupZen16();
      _zen16ScreenValid = true;
      _zen16LastPopup = (uint8_t)POPUP_WEATHER;
    }
    return;
  }

  if (isMinimalUiMode() && _activePopup == POPUP_LUNAR) {
    if (!_zen16ScreenValid || _zen16LastPopup != (uint8_t)POPUP_LUNAR) {
      renderLunarPopupZen16();
      _zen16ScreenValid = true;
      _zen16LastPopup = (uint8_t)POPUP_LUNAR;
    }
    return;
  }

  if (isMinimalUiMode() && _activePopup == POPUP_FINANCE_GOLD) {
    if (!_zen16ScreenValid || _zen16LastPopup != (uint8_t)POPUP_FINANCE_GOLD) {
      renderFinanceGoldPopupZen16();
      _zen16ScreenValid = true;
      _zen16LastPopup = (uint8_t)POPUP_FINANCE_GOLD;
    }
    return;
  }

  if (isMinimalUiMode() && _activePopup == POPUP_FINANCE_CURRENCY) {
    if (!_zen16ScreenValid ||
        _zen16LastPopup != (uint8_t)POPUP_FINANCE_CURRENCY ||
        _zen16LastCurrencyTab != _currencyTab) {
      renderFinanceCurrencyPopupZen16();
      _zen16ScreenValid = true;
      _zen16LastPopup = (uint8_t)POPUP_FINANCE_CURRENCY;
      _zen16LastCurrencyTab = _currencyTab;
    }
    return;
  }

  if (isMinimalUiMode() && _activePopup == POPUP_NONE &&
      (_currentPage == PAGE_CLOCK || _currentPage == PAGE_CALENDAR ||
       _currentPage == PAGE_FINANCE || _currentPage == PAGE_POMODORO ||
       _currentPage == PAGE_MOOD || _currentPage == PAGE_SETTINGS)) {
    bool pageSame = (_zen16ScreenValid && _zen16LastPage == _currentPage &&
                     _zen16LastPopup == (uint8_t)POPUP_NONE);
    if (_dockVisible && pageSame) {
      // Overlay ONLY the bottom Dock strip (y=194..239) on top of the live
      // 16-bit Zen screen — ZERO full-screen redraw, ZERO font shift!
      if (!_zen16LastDock) {
        _spr.fillRect(0, 194, 320, 46, C_BLACK);
        renderDock();
        _spr.pushSprite(0, 194, 0, 194, 320, 46);
        _zen16LastDock = true;
      }
      return;
    }

    bool forceFull = (!pageSame || _zen16LastDock != _dockVisible);
    if (_currentPage == PAGE_CLOCK) {
      renderPage0_MinimalZen16(forceFull);
    } else if (_currentPage == PAGE_CALENDAR) {
      if (forceFull || _calYear != _zen16LastCalYear ||
          _calMonth != _zen16LastCalMonth) {
        renderPage1_CalendarZen16();
      }
    } else if (_currentPage == PAGE_FINANCE) {
      if (forceFull)
        renderPage2_FinanceZen16();
    } else if (_currentPage == PAGE_POMODORO) {
      renderPage3_PomodoroZen16(forceFull);
    } else if (_currentPage == PAGE_MOOD) {
      if (forceFull)
        renderPage4_MoodZen16();
    } else if (_currentPage == PAGE_SETTINGS) {
      renderPage5_SettingsZen16(forceFull);
    }
    if (_dockVisible) {
      _spr.fillRect(0, 194, 320, 46, C_BLACK);
      renderDock();
      _spr.pushSprite(0, 194, 0, 194, 320, 46);
    }
    _zen16ScreenValid = true;
    _zen16LastPage = _currentPage;
    _zen16LastPopup = (uint8_t)POPUP_NONE;
    _zen16LastDock = _dockVisible;
    return;
  }
  _zen16ScreenValid = false;
  _zen16LastPopup = 255;
  _zen16LastCurrencyTab = 255;
  _zen16LastDock = false;

  switch (_currentPage) {
  case PAGE_CLOCK:
    renderPage0_WeatherClock();
    break;
  case PAGE_CALENDAR:
    renderPage1_Calendar();
    break;
  case PAGE_FINANCE:
    renderPage2_Finance();
    break;
  case PAGE_POMODORO:
    renderPage3_Pomodoro();
    break;
  case PAGE_MOOD:
    renderPage4_Mood();
    break;
  case PAGE_SETTINGS:
    renderPage5_Settings();
    break;
  default:
    renderPage0_WeatherClock();
    break;
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
static void drawMiniDrop(TFT_eSprite &spr, int16_t x, int16_t y,
                         uint16_t color) {
  spr.fillCircle(x, y + 2, 2, color);
  spr.drawPixel(x, y - 2, color);
  spr.drawLine(x - 2, y + 2, x, y - 2, color);
  spr.drawLine(x + 2, y + 2, x, y - 2, color);
}

static void drawMiniWind(TFT_eSprite &spr, int16_t x, int16_t y,
                         uint16_t color) {
  spr.drawFastHLine(x - 4, y - 2, 8, color);
  spr.drawPixel(x + 4, y - 3, color);
  spr.drawFastHLine(x - 2, y + 2, 6, color);
  spr.drawPixel(x + 4, y + 3, color);
}

static void drawMiniSun(TFT_eSprite &spr, int16_t x, int16_t y,
                        uint16_t color) {
  spr.fillCircle(x, y, 2, color);
  spr.drawPixel(x, y - 4, color);
  spr.drawPixel(x, y + 4, color);
  spr.drawPixel(x - 4, y, color);
  spr.drawPixel(x + 4, y, color);
}

static void drawMiniMoon(TFT_eSprite &spr, int16_t x, int16_t y,
                         uint16_t color) {
  spr.fillCircle(x, y, 3, color);
  spr.fillCircle(x + 1, y - 1, 3, RGB565(15, 20, 32)); // crescent shadow
}

static void drawMiniStar(TFT_eSprite &spr, int16_t x, int16_t y,
                         uint16_t color) {
  spr.drawFastVLine(x, y - 3, 7, color);
  spr.drawFastHLine(x - 3, y, 7, color);
  spr.drawPixel(x, y, C_WHITE);
}

static void drawMiniCal(TFT_eSprite &spr, int16_t x, int16_t y,
                        uint16_t color) {
  spr.drawRoundRect(x - 3, y - 3, 7, 7, 1, color);
  spr.drawFastHLine(x - 3, y - 1, 7, color);
  spr.drawPixel(x - 1, y - 4, color);
  spr.drawPixel(x + 1, y - 4, color);
}

static void drawMiniHeart(TFT_eSprite &spr, int16_t x, int16_t y,
                          uint16_t color) {
  spr.fillCircle(x - 2, y - 2, 2, color);
  spr.fillCircle(x + 2, y - 2, 2, color);
  spr.fillTriangle(x - 4, y - 1, x + 4, y - 1, x, y + 4, color);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 0 — WEATHER CLOCK (Matches user reference image)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage0_WeatherClock() {
  if (isMinimalUiMode()) {
    renderPage0_MinimalZen();
    return;
  }
  // ────────────────────────────────────────────────────────────────
  // 1. BACKGROUND (Custom Wallpaper OR Deep Obsidian with Ambient Aura)
  // ────────────────────────────────────────────────────────────────
  drawBackground();

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);

  LunarDate lDate = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1,
                                                      1900 + t.tm_year);

  // ────────────────────────────────────────────────────────────────
  // 2. GIANT HERO CLOCK — Shifted down to clockCy = 55 for perfect screen
  // balance ────────────────────────────────────────────────────────────────
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);

  int16_t clockCx = 160;
  int16_t clockCy = 55;

  _spr.setTextDatum(MC_DATUM);

  // Đồng bộ màu hào quang và chữ số đồng hồ lớn theo Theme đang chọn
  uint16_t clockHalo = blendColor(theme.accent, theme.bg, 85);
  uint16_t clockRim = blendColor(theme.accent, C_WHITE, 175);
  uint16_t clockCore = blendColor(theme.accent, C_WHITE, 225);

  // Layer 1: Ambient deep theme glow halo
  _spr.setTextColor(clockHalo);
  _spr.drawString(timeBuf, clockCx - 2, clockCy, 8);
  _spr.drawString(timeBuf, clockCx + 2, clockCy, 8);
  _spr.drawString(timeBuf, clockCx, clockCy - 2, 8);
  _spr.drawString(timeBuf, clockCx, clockCy + 2, 8);

  // Layer 2: Bright theme rim
  _spr.setTextColor(clockRim);
  _spr.drawString(timeBuf, clockCx - 1, clockCy - 1, 8);
  _spr.drawString(timeBuf, clockCx + 1, clockCy - 1, 8);
  _spr.drawString(timeBuf, clockCx - 1, clockCy + 1, 8);
  _spr.drawString(timeBuf, clockCx + 1, clockCy + 1, 8);

  // Layer 3: Solid Electric Neon body matched to active Theme
  _spr.setTextColor(clockCore);
  _spr.drawString(timeBuf, clockCx, clockCy, 8);

  // ── Alarm Bell Icon (Right next to clock digits, NO text time) ──
  int16_t timeW = _spr.textWidth(timeBuf, 8);
  int16_t bellX = clockCx + (timeW / 2) + 16;
  if (bellX < 268)
    bellX = 268;
  if (bellX > 298)
    bellX = 298;
  drawBellIcon(bellX, 34, _alarmEnabled);

  // ────────────────────────────────────────────────────────────────
  // 3. DATE LINE — Centered with ample space (Y = 107, Font 2, Pure White)
  // ────────────────────────────────────────────────────────────────
  char dateBuf[40];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
           getDayNameI18n(t.tm_wday), t.tm_mday, t.tm_mon + 1,
           1900 + t.tm_year);

  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(dateBuf, 160, 107, 2);

  // ────────────────────────────────────────────────────────────────
  // 4. TWO EXPANDED GLASS CARDS (Y = 129..211, H = 82, W = 146)
  //    Balanced full-screen symmetry with floating dock overlay!
  // ────────────────────────────────────────────────────────────────
  const WeatherData &w = weather.getData();
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
  _spr.fillTriangle(leftX + 12, cardY + 11, leftX + 16, cardY + 11, leftX + 14,
                    cardY + 15, C_RED);

  const char *cityStr = (strlen(w.city) > 0) ? w.city : "TP. HCM";
  char cityUpper[28];
  size_t cj = 0;
  for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
    unsigned char uch = (unsigned char)cityStr[cj];
    cityUpper[cj] = (uch < 0x80) ? (char)toupper(uch) : (char)uch;
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

  char tempStr[12];
  snprintf(tempStr, sizeof(tempStr), "%.1f", w.valid ? w.temp : 29.0f);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(tempStr, leftX + 48, cardY + 31, 4);

  int16_t tNumW = _spr.textWidth(tempStr, 4);
  int16_t degX = leftX + 48 + tNumW + 2;
  _spr.drawCircle(degX + 1, cardY + 35, 2, C_GOLD);
  _spr.setTextColor(C_GOLD);
  _spr.drawString("C", degX + 6, cardY + 31, 4);

  // Condition text: spaced 6px cleanly below 29°C for relaxed readability
  const char *condStr = getWeatherConditionI18n(wType, night, false);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(RGB565(255, 215, 0)); // Gold
  _spr.drawString(condStr, leftX + 50, cardY + 60, 1);

  // ── RIGHT CARD: AM LICH (X = 164..310, H = 82) ──────────────────
  int16_t rightX = 164;
  drawGlassCard(rightX, cardY, cardW, cardH, 12);

  uint8_t lunarDay = lDate.day;
  const char *phaseName = getMoonPhaseNameI18n(lunarDay);

  // Header (Y = cardY + 11): Phase Name (Font 2)
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(phaseName, rightX + 14, cardY + 11, 2);

  // Hint: small touch arrow ">" at top right
  _spr.setTextColor(RGB565(100, 150, 200));
  _spr.drawString(">", rightX + cardW - 12, cardY + 11, 1);

  // Divider line (Soft neutral light gray)
  _spr.drawFastHLine(rightX + 8, cardY + 22, cardW - 16, RGB565(110, 115, 125));

  // Main Row: Moon Icon (28px) + Lunar Date (Font 4) + Can Chi Ngay on separate
  // line Centered at cardY + 49 to align horizontally with weather icon
  int16_t moonCx = rightX + 24;
  int16_t moonCy = cardY + 49;
  drawMoonIcon(moonCx, moonCy, 14, lunarDay);

  char alDayStr[16];
  snprintf(alDayStr, sizeof(alDayStr), "%02d/%02d", lDate.day, lDate.month);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(alDayStr, rightX + 46, cardY + 31, 4);

  _spr.setTextColor(RGB565(140, 205, 255));
  _spr.drawString((getLanguage() == LANG_VI ? "ÂL" : "LUN"),
                  rightX + 46 + _spr.textWidth(alDayStr, 4) + 3, cardY + 33, 2);

  // Can Chi day placed 6px cleanly below 04/09 AL — NEVER overlaps!
  char dayCanChiBuf[36];
  snprintf(dayCanChiBuf, sizeof(dayCanChiBuf),
           (getLanguage() == LANG_VI ? "Ngày %s" : "Day %s"), lDate.dayCanChi);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(RGB565(255, 215, 0)); // Gold
  _spr.drawString(dayCanChiBuf, rightX + 50, cardY + 60, 1);

  // ── BOTTOM TICKER: ANNIVERSARY & PERPETUAL CALENDAR ─────────────
  renderPage0Ticker();
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 0 (MINIMAL ZEN) — Signature Apple StandBy Luxury Minimalist Clock
//  • Locked 1 Signature Apple Palette: Pure Black + Ice Cyan + Crisp White +
//  Champagne Gold • 4-bit Anti-Aliased Segoe UI Semilight/Regular (uniform
//  brightness, zero jaggies!)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage0_MinimalZen() {
  // Locked Signature Apple StandBy Palette (clean in BOTH 16-bit RGB565 & 8-bit
  // RGB332)
  const uint16_t zenBg = C_BLACK; // #000000 pure black (melts into glass bezel)
  const uint16_t zenText = C_WHITE;                 // #FFFFFF crisp pure white
  const uint16_t zenMuted = RGB565(170, 178, 190);  // Neutral cool silver-gray
  const uint16_t zenDim = RGB565(112, 118, 128);    // Subtle muted slate
  const uint16_t zenAccent = RGB565(100, 210, 255); // Apple Ice Cyan (#64D2FF)
  const uint16_t zenGold =
      RGB565(235, 190, 95);                     // Warm Champagne Gold (#EBBE5F)
  const uint16_t zenTrack = RGB565(36, 40, 46); // Clean dark track

  _spr.fillSprite(zenBg);

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  LunarDate lDate = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1,
                                                      1900 + t.tm_year);

  const WeatherData &w = weather.getData();
  WeatherType wType = weather.getType();
  bool night = weather.isNight();
  bool isVI = (getLanguage() == LANG_VI);

  // ── 1. TOP SUBTLE HEADER (Y = 18, Slim Semilight) ─────────────────
  char dateBuf[36];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
           getDayNameI18n(t.tm_wday), t.tm_mday, t.tm_mon + 1,
           1900 + t.tm_year);
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(zenMuted);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, dateBuf, 18, 18);

  // Top-right Alarm status (Bell + slim alarm time if enabled)
  drawBellIcon(296, 18, _alarmEnabled);
  if (_alarmEnabled) {
    char alStr[10];
    snprintf(alStr, sizeof(alStr), "%02d:%02d", _alarmHour, _alarmMinute);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(zenAccent);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, alStr, 280, 18);
  }

  // ── 2. HERO ROW: HUGE MOCKUP-SCALE CLOCK (96px) + OUTLINE WEATHER ─
  // Exact mockup aesthetic: Hours ("14") in Theme Accent, ":35" in crisp pure
  // white!
  char hhStr[4], mmStr[6];
  snprintf(hhStr, sizeof(hhStr), "%02d", t.tm_hour);
  snprintf(mmStr, sizeof(mmStr), ":%02d", t.tm_min);

  int16_t hhW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, hhStr);
  int16_t mmW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, mmStr);
  int16_t totalW = hhW + mmW;

  int16_t clockX0 = 114 - totalW / 2;
  if (clockX0 < 10)
    clockX0 = 10;
  int16_t clockCy = 86;

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(zenAccent);
  _spr.drawSmoothFace(&ZEN_FONT_CLOCK, hhStr, clockX0, clockCy);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_CLOCK, mmStr, clockX0 + hhW, clockCy);

  // Right minimalist line-art weather cluster (ONLY sun has warm gold color!)
  int16_t wCx = 268;
  drawMinimalWeatherIcon(wCx, 72, wType, night, zenBg);

  char tempNum[12];
  snprintf(tempNum, sizeof(tempNum), "%.0f°C", w.valid ? w.temp : 29.0f);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_NUM, tempNum, wCx, 114);

  // ── 3. SIGNATURE THIN CHAMPAGNE GOLD BAR (Y = 152, H = 3px) ───────
  const int16_t barX = 18, barY = 152, barW = 284, barH = 3;
  _spr.fillSmoothRoundRect(barX, barY, barW, barH, 1, zenTrack, zenBg);

  float ratio = 0.0f;
  uint16_t barColor = zenGold;
  if (_pomoState != POMO_STOPPED && _pomoTotalSec > 0) {
    ratio = (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec;
    barColor = (_pomoMode == POMO_FOCUS) ? zenAccent : RGB565(52, 211, 153);
  } else {
    ratio = (float)(t.tm_hour * 60 + t.tm_min) / 1440.0f;
  }
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  int16_t fillW = (int16_t)(barW * ratio);
  if (fillW < 6 && ratio > 0.0f)
    fillW = 6;
  if (fillW > 0) {
    _spr.fillSmoothRoundRect(barX, barY, fillW, barH, 1, barColor, zenTrack);
  }

  // ── 4. CLEAN SLIM METRICS FOOTER (Y = 176) ────────────────────────
  char leftMeta[44];
  if (_pomoState != POMO_STOPPED) {
    snprintf(leftMeta, sizeof(leftMeta), "%s %02d:%02d",
             (_pomoMode == POMO_FOCUS) ? (isVI ? "TẬP TRUNG" : "FOCUS")
                                       : (isVI ? "NGHỈ" : "BREAK"),
             _pomoRemainingSec / 60, _pomoRemainingSec % 60);
    _spr.fillSmoothCircle(22, 176, 3, barColor, zenBg);
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(barColor);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, leftMeta, 31, 176);
  } else {
    snprintf(leftMeta, sizeof(leftMeta), "%02d/%02d %s • %s", lDate.day,
             lDate.month, isVI ? "ÂL" : "LUN", lDate.dayCanChi);
    _spr.fillSmoothCircle(22, 176, 3, zenAccent, zenBg);
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(zenMuted);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, leftMeta, 31, 176);
  }

  char rightMeta[28];
  if (w.valid && w.humidity > 0) {
    snprintf(rightMeta, sizeof(rightMeta), "RH %d%%", w.humidity);
  } else {
    snprintf(rightMeta, sizeof(rightMeta), "%s",
             (strlen(w.city) > 0) ? w.city : "TP.HCM");
  }
  _spr.setTextDatum(MR_DATUM);
  _spr.setTextColor(zenDim);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, rightMeta, 302, 176);

  // ── 5. FLOATING MINIMALIST TICKER (Y = 206, SLIM FONT, NO BOX) ────
  if (!_dockVisible) {
    static Language lastZenLang = LANG_VI;
    if (_tickerCachedDay != t.tm_mday || _cachedTickerMsg.length() == 0 ||
        lastZenLang != getLanguage()) {
      _cachedTickerMsg =
          anniversaryMgr.getTodayTickerMessage(getLanguage() == LANG_EN);
      _tickerCachedDay = t.tm_mday;
      lastZenLang = getLanguage();
      _tickerOffset = 0;
    }
    if (_cachedTickerMsg.length() > 0) {
      uint16_t tCol = zenDim;
      if (_cachedTickerMsg.startsWith("[★"))
        tCol = zenGold;
      else if (_cachedTickerMsg.startsWith("[✿"))
        tCol = theme.accentSec;
      else
        tCol = zenDim;

      _spr.setTextColor(tCol, zenBg);
      int16_t strW =
          _spr.measureSmoothFace(&ZEN_FONT_LIGHT, _cachedTickerMsg.c_str());
      const int16_t maxW = 284;
      if (strW <= maxW) {
        _spr.setTextDatum(MC_DATUM);
        _spr.drawSmoothFace(&ZEN_FONT_LIGHT, _cachedTickerMsg.c_str(), 160,
                            206);
      } else {
        unsigned long nowMs = millis();
        if (nowMs - _lastTickerTick >= 35) {
          _lastTickerTick = nowMs;
          _tickerOffset++;
          if (_tickerOffset >= strW + 45)
            _tickerOffset = 0;
        }
        _spr.setVnClip(18, 194, maxW, 24);
        _spr.setTextDatum(ML_DATUM);
        int16_t drawX = 18 - _tickerOffset;
        _spr.drawSmoothFace(&ZEN_FONT_LIGHT, _cachedTickerMsg.c_str(), drawX,
                            206);
        if (drawX + strW + 45 < 302) {
          _spr.drawSmoothFace(&ZEN_FONT_LIGHT, _cachedTickerMsg.c_str(),
                              drawX + strW + 45, 206);
        }
        _spr.resetVnClip();
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  MINIMALIST LINE-ART WEATHER ICON (Mockup style: crisp 2px white outline,
//  warm gold sun/rays ONLY for sunny/partly-cloudy accents!)
// ═══════════════════════════════════════════════════════════════════════
void Display::drawMinimalWeatherIcon(int16_t cx, int16_t cy,
                                     uint8_t weatherType, bool isNight,
                                     uint16_t bgCol) {
  const uint16_t lineCol = C_WHITE;
  const uint16_t sunCol = RGB565(250, 200, 75); // Warm minimalist gold sun

  // Helper lambda to draw a crisp hollow 2px outline cloud centered at (ox, oy)
  auto drawOutlineCloud = [&](int16_t ox, int16_t oy) {
    // Outer white silhouette of cloud bumps + flat bottom
    _spr.fillSmoothCircle(ox - 10, oy + 4, 7, lineCol, bgCol);
    _spr.fillSmoothCircle(ox - 2, oy - 2, 9, lineCol, bgCol);
    _spr.fillSmoothCircle(ox + 9, oy + 3, 8, lineCol, bgCol);
    _spr.fillRect(ox - 10, oy + 3, 20, 8, lineCol);
    // Carve hollow interior (2px stroke!) with bgCol
    _spr.fillSmoothCircle(ox - 10, oy + 4, 5, bgCol, lineCol);
    _spr.fillSmoothCircle(ox - 2, oy - 2, 7, bgCol, lineCol);
    _spr.fillSmoothCircle(ox + 9, oy + 3, 6, bgCol, lineCol);
    _spr.fillRect(ox - 10, oy + 2, 20, 7, bgCol);
  };

  // Helper lambda to draw minimalist gold sun ring + 8 short radial rays
  auto drawOutlineSun = [&](int16_t sx, int16_t sy, int16_t r) {
    _spr.fillSmoothCircle(sx, sy, r, sunCol, bgCol);
    _spr.fillSmoothCircle(sx, sy, r - 2, bgCol, sunCol);
    for (int i = 0; i < 8; i++) {
      float a = i * 0.785398f; // 45 deg
      int16_t x1 = sx + (int16_t)roundf(cosf(a) * (r + 3));
      int16_t y1 = sy + (int16_t)roundf(sinf(a) * (r + 3));
      int16_t x2 = sx + (int16_t)roundf(cosf(a) * (r + 6));
      int16_t y2 = sy + (int16_t)roundf(sinf(a) * (r + 6));
      _spr.drawWideLine(x1, y1, x2, y2, 1.5f, sunCol, bgCol);
    }
  };

  if (weatherType == WEATHER_CLEAR) {
    if (isNight) {
      // Crisp minimalist crescent moon outline
      _spr.fillSmoothCircle(cx, cy, 11, lineCol, bgCol);
      _spr.fillSmoothCircle(cx, cy, 9, bgCol, lineCol);
      _spr.fillSmoothCircle(cx + 5, cy - 3, 9, bgCol, bgCol);
      _spr.drawArc(cx + 5, cy - 3, 9, 7, 130, 290, lineCol, bgCol, true);
    } else {
      drawOutlineSun(cx, cy, 8);
    }
  } else if (weatherType == WEATHER_CLOUDY) {
    // Exact mockup icon: crisp white outline cloud + warm gold sun peeking
    // top-right!
    if (!isNight) {
      drawOutlineSun(cx + 8, cy - 6, 6);
    } else {
      _spr.fillSmoothCircle(cx + 9, cy - 6, 6, lineCol, bgCol);
      _spr.fillSmoothCircle(cx + 12, cy - 8, 5, bgCol, bgCol);
    }
    drawOutlineCloud(cx - 2, cy + 3);
  } else if (weatherType == WEATHER_RAIN) {
    drawOutlineCloud(cx, cy - 2);
    for (int i = -1; i <= 1; i++) {
      int16_t rx = cx + i * 8;
      _spr.drawWideLine(rx, cy + 12, rx - 3, cy + 17, 1.6f, lineCol, bgCol);
    }
  } else if (weatherType == WEATHER_STORM) {
    drawOutlineCloud(cx, cy - 3);
    // Minimalist gold lightning bolt
    _spr.drawWideLine(cx + 1, cy + 9, cx - 3, cy + 14, 1.8f, sunCol, bgCol);
    _spr.drawWideLine(cx - 3, cy + 14, cx + 2, cy + 14, 1.8f, sunCol, bgCol);
    _spr.drawWideLine(cx + 2, cy + 14, cx - 2, cy + 19, 1.8f, sunCol, bgCol);
  } else {
    // CLOUDY / FOG
    drawOutlineCloud(cx, cy + 1);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  TRUE 16-BIT RGB565 MINIMAL ZEN RENDERERS (65,536 colors, 16-level AA!)
//  • Uses Google Be Vietnam Pro (Light 82px + Regular 25px + Regular 14px)
//  • Zero 8-bit RGB332 quantization -> 0% jaggies, 0% blotchy text!
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage0_MinimalZen16(bool forceFull) {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(112, 118, 128);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenTrack = RGB565(36, 40, 46);

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  int curMinKey = t.tm_hour * 60 + t.tm_min;
  bool isVI = (getLanguage() == LANG_VI);

  bool needBody =
      forceFull || (curMinKey != _zen16LastMin) ||
      (_alarmEnabled != _zen16LastAlarmEn) ||
      (_pomoState != _zen16LastPomoState) ||
      (_pomoState != POMO_STOPPED && _pomoRemainingSec != _zen16LastPomoSec);

  if (forceFull) {
    // Draw static vector graphics (bell, outline weather icon, dock pill) once
    // via sprite -> push
    _spr.fillSprite(zenBg);
    drawBellIcon(296, 18, _alarmEnabled);
    drawMinimalWeatherIcon(268, 72, weather.getType(), weather.isNight(),
                           zenBg);
    renderDock();
    _spr.pushSprite(0, 0);
  }

  if (needBody) {
    _zen16LastMin = curMinKey;
    _zen16LastAlarmEn = _alarmEnabled;
    _zen16LastPomoState = (uint8_t)_pomoState;
    _zen16LastPomoSec = _pomoRemainingSec;

    LunarDate lDate = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1,
                                                        1900 + t.tm_year);
    const WeatherData &w = weather.getData();

    // 1. Top Date (16-bit Be Vietnam Pro 14px)
    char dateBuf[36];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
             getDayNameI18n(t.tm_wday), t.tm_mday, t.tm_mon + 1,
             1900 + t.tm_year);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dateBuf, 18, 18, ML_DATUM,
                               zenMuted, zenBg, 14, 6, 200, 24);

    // Alarm time next to bell
    if (_alarmEnabled) {
      char alStr[10];
      snprintf(alStr, sizeof(alStr), "%02d:%02d", _alarmHour, _alarmMinute);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, alStr, 280, 18,
                                 MR_DATUM, zenAccent, zenBg, 224, 6, 60, 24);
    } else {
      _tft.fillRect(224, 6, 60, 24, zenBg);
    }

    // 2. Giant Hero Clock (16-bit Be Vietnam Pro Light 82px — ZERO jaggies!)
    char hhStr[4], mmStr[6];
    snprintf(hhStr, sizeof(hhStr), "%02d", t.tm_hour);
    snprintf(mmStr, sizeof(mmStr), ":%02d", t.tm_min);
    int16_t hhW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, hhStr);
    int16_t mmW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, mmStr);
    int16_t clockX0 = 114 - (hhW + mmW) / 2;
    if (clockX0 < 10)
      clockX0 = 10;

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, hhStr, clockX0, 88,
                               ML_DATUM, zenAccent, zenBg, 8, 38,
                               hhW + (clockX0 - 8) + 2, 98);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, mmStr, clockX0 + hhW, 88,
                               ML_DATUM, zenText, zenBg, clockX0 + hhW, 38,
                               226 - (clockX0 + hhW), 98);

    // 3. Temperature (16-bit Be Vietnam Pro Regular 25px)
    char tempNum[12];
    snprintf(tempNum, sizeof(tempNum), "%.0f°C", w.valid ? w.temp : 29.0f);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, tempNum, 268, 114, MC_DATUM,
                               zenText, zenBg, 228, 96, 84, 38);

    // 4. Thin 3px Champagne Gold Progress Bar (direct 16-bit!)
    const int16_t barX = 18, barY = 152, barW = 284, barH = 3;
    _tft.fillRoundRect(barX, barY, barW, barH, 1, zenTrack);
    float ratio = 0.0f;
    uint16_t barColor = zenGold;
    if (_pomoState != POMO_STOPPED && _pomoTotalSec > 0) {
      ratio = (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec;
      barColor = (_pomoMode == POMO_FOCUS) ? zenAccent : RGB565(52, 211, 153);
    } else {
      ratio = (float)(t.tm_hour * 60 + t.tm_min) / 1440.0f;
    }
    if (ratio < 0.0f)
      ratio = 0.0f;
    if (ratio > 1.0f)
      ratio = 1.0f;
    int16_t fillW = (int16_t)(barW * ratio);
    if (fillW < 6 && ratio > 0.0f)
      fillW = 6;
    if (fillW > 0)
      _tft.fillRoundRect(barX, barY, fillW, barH, 1, barColor);

    // 5. Clean 16-bit Metrics Footer
    char leftMeta[44];
    if (_pomoState != POMO_STOPPED) {
      snprintf(leftMeta, sizeof(leftMeta), "%s %02d:%02d",
               (_pomoMode == POMO_FOCUS) ? (isVI ? "TẬP TRUNG" : "FOCUS")
                                         : (isVI ? "NGHỈ" : "BREAK"),
               _pomoRemainingSec / 60, _pomoRemainingSec % 60);
      _tft.fillCircle(22, 176, 3, barColor);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, leftMeta, 31, 176,
                                 ML_DATUM, barColor, zenBg, 30, 165, 190, 22);
    } else {
      snprintf(leftMeta, sizeof(leftMeta), "%02d/%02d %s • %s", lDate.day,
               lDate.month, isVI ? "ÂL" : "LUN", lDate.dayCanChi);
      _tft.fillCircle(22, 176, 3, zenGold);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, leftMeta, 31, 176,
                                 ML_DATUM, zenMuted, zenBg, 30, 165, 195, 22);
    }

    char rightMeta[28];
    if (w.valid && w.humidity > 0)
      snprintf(rightMeta, sizeof(rightMeta), "RH %d%%", w.humidity);
    else
      snprintf(rightMeta, sizeof(rightMeta), "%s",
               (strlen(w.city) > 0) ? w.city : "TP.HCM");
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, rightMeta, 302, 176,
                               MR_DATUM, zenDim, zenBg, 226, 165, 80, 22);
  }

  // 6. Floating 16-bit Vietnamese Ticker (smooth marquee, 0% flicker!)
  static Language lastZen16Lang = LANG_VI;
  if (_tickerCachedDay != t.tm_mday || _cachedTickerMsg.length() == 0 ||
      lastZen16Lang != getLanguage()) {
    _cachedTickerMsg =
        anniversaryMgr.getTodayTickerMessage(getLanguage() == LANG_EN);
    _tickerCachedDay = t.tm_mday;
    lastZen16Lang = getLanguage();
    _tickerOffset = 0;
    forceFull = true;
  }
  if (!_dockVisible && _cachedTickerMsg.length() > 0) {
    uint16_t tCol = zenDim;
    if (_cachedTickerMsg.startsWith("[★"))
      tCol = zenGold;
    else if (_cachedTickerMsg.startsWith("[✿"))
      tCol = RGB565(240, 150, 180);

    int16_t strW =
        _spr.measureSmoothFace(&ZEN_FONT_LIGHT, _cachedTickerMsg.c_str());
    const int16_t maxW = 284;
    if (strW <= maxW) {
      if (forceFull || needBody) {
        VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                                   _cachedTickerMsg.c_str(), 160, 206, MC_DATUM,
                                   tCol, zenBg, 18, 195, maxW, 22);
      }
    } else {
      unsigned long nowMs = millis();
      if (forceFull || (nowMs - _lastTickerTick >= 40)) {
        _lastTickerTick = nowMs;
        if (!forceFull) {
          _tickerOffset++;
          // Once the tail fully exits the left edge, re-enter smoothly from
          // the right edge (zero flash / zero snap!)
          if (_tickerOffset > strW + 12)
            _tickerOffset = -maxW;
        }
        VnSprite::drawSmoothFace16(
            _tft, &ZEN_FONT_LIGHT, _cachedTickerMsg.c_str(), 18 - _tickerOffset,
            206, ML_DATUM, tCol, zenBg, 18, 195, maxW, 22, 18, 18 + maxW);
      }
    }
  }
}

static void toVNFloat(char *str);

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 1 (MINIMAL ZEN SPRITE) — Used when Dock/Popup overlays Calendar
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage1_CalendarZen() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(138, 146, 158);
  const uint16_t zenDim = RGB565(96, 104, 116);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenCoral = RGB565(255, 95, 87);
  const uint16_t zenLine = RGB565(32, 36, 42);

  _spr.fillSprite(zenBg);

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  int curYear = t.tm_year + 1900;
  int curMonth = t.tm_mon + 1;
  int curDay = (t.tm_mday > 0) ? t.tm_mday : 1;
  if (_calYear == 0 || _calMonth == 0) {
    _calYear = curYear;
    _calMonth = curMonth;
  }

  bool isVI = (getLanguage() == LANG_VI);
  bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);

  _spr.drawWideLine(28, 16, 21, 22, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(21, 22, 28, 28, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(292, 16, 299, 22, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(299, 22, 292, 28, 1.5f, zenMuted, zenBg);
  _spr.drawFastHLine(16, 65, 288, zenLine);

  char titleBuf[40];
  snprintf(titleBuf, sizeof(titleBuf),
           isVI ? "THÁNG %02d  •  %d" : "MONTH %02d  •  %d", _calMonth,
           _calYear);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, titleBuf, 160, 16);

  char subBuf[64];
  if (isCurrentMonth) {
    LunarDate todayLunar =
        LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
    snprintf(subBuf, sizeof(subBuf),
             isVI ? "Hôm nay: %02d/%02d ÂL • %s"
                  : "Today: %02d/%02d Lunar • %s",
             todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
    _spr.setTextColor(zenGold);
  } else {
    LunarDate midLunar =
        LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
    snprintf(subBuf, sizeof(subBuf),
             isVI ? "Năm %s • Chạm về hôm nay" : "Year %s • Tap for today",
             midLunar.yearCanChi);
    _spr.setTextColor(zenAccent);
  }
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, subBuf, 160, 34);

  const char *dowVI[] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
  const char *dowEN[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  const char *const *dow = isVI ? dowVI : dowEN;
  for (int col = 0; col < 7; col++) {
    int16_t hx = 28 + col * 44;
    _spr.setTextColor((col == 6) ? zenCoral
                                 : ((col == 5) ? zenAccent : zenMuted));
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, dow[col], hx, 53);
  }

  int daysInMonth = 31;
  if (_calMonth == 4 || _calMonth == 6 || _calMonth == 9 || _calMonth == 11)
    daysInMonth = 30;
  else if (_calMonth == 2)
    daysInMonth =
        ((_calYear % 4 == 0 && _calYear % 100 != 0) || (_calYear % 400 == 0))
            ? 29
            : 28;
  static const int tMap[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int yCalc = (_calMonth < 3) ? (_calYear - 1) : _calYear;
  int startCol = ((yCalc + yCalc / 4 - yCalc / 100 + yCalc / 400 +
                   tMap[_calMonth - 1] + 1) %
                      7 +
                  6) %
                 7;
  int numRows = (startCol + daysInMonth + 6) / 7;
  int16_t rowH = (numRows <= 5) ? 30 : 25;
  int16_t gridStartY = (numRows <= 5) ? 84 : 80;

  for (int d = 1; d <= daysInMonth; d++) {
    int slot = startCol + (d - 1);
    int col = slot % 7, row = slot / 7;
    int16_t cx = 28 + col * 44, cy = gridStartY + row * rowH;
    LunarDate ld = LunarCalendar::convertSolar2Lunar(d, _calMonth, _calYear);
    bool isToday = (d == curDay && isCurrentMonth);

    char sBuf[8], lStr[8];
    snprintf(sBuf, sizeof(sBuf), "%d", d);
    if (ld.day == 1)
      snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
    else
      snprintf(lStr, sizeof(lStr), "%d", ld.day);
    int16_t sW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, sBuf);
    int16_t lW = _spr.TFT_eSprite::textWidth(lStr, 1);
    int16_t startX = cx - (sW + 2 + lW) / 2;

    if (isToday) {
      int16_t boxW = 38, boxH = (numRows <= 5) ? 24 : 21;
      _spr.fillSmoothRoundRect(cx - boxW / 2, cy - boxH / 2, boxW, boxH,
                               boxH / 2, zenAccent, zenBg);
      _spr.setTextDatum(ML_DATUM);
      _spr.setTextColor(C_BLACK);
      _spr.drawSmoothFace(&ZEN_FONT_LIGHT, sBuf, startX, cy);
      _spr.setTextDatum(TL_DATUM);
      _spr.setTextColor(RGB565(12, 34, 48));
      _spr.TFT_eSprite::drawString(lStr, startX + sW + 2, cy - 1, 1);
    } else {
      _spr.setTextDatum(ML_DATUM);
      _spr.setTextColor((col == 6) ? zenCoral
                                   : ((col == 5) ? zenAccent : zenText));
      _spr.drawSmoothFace(&ZEN_FONT_LIGHT, sBuf, startX, cy);
      _spr.setTextDatum(TL_DATUM);
      _spr.setTextColor((ld.day == 1 || ld.day == 15) ? zenGold : zenDim);
      _spr.TFT_eSprite::drawString(lStr, startX + sW + 2, cy - 1, 1);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 1 (MINIMAL ZEN 16-BIT) — Apple iOS / StandBy Minimalist Calendar
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage1_CalendarZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(138, 146, 158);
  const uint16_t zenDim = RGB565(96, 104, 116);
  const uint16_t zenAccent = RGB565(100, 210, 255); // Apple Ice Cyan
  const uint16_t zenGold = RGB565(235, 190, 95);    // Warm Champagne Gold
  const uint16_t zenCoral = RGB565(255, 95, 87);    // Apple Coral Red (CN)
  const uint16_t zenLine = RGB565(32, 36, 42);

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  int curYear = t.tm_year + 1900;
  int curMonth = t.tm_mon + 1;
  int curDay = (t.tm_mday > 0) ? t.tm_mday : 1;

  if (_calYear == 0 || _calMonth == 0) {
    _calYear = curYear;
    _calMonth = curMonth;
  }
  _zen16LastCalYear = _calYear;
  _zen16LastCalMonth = _calMonth;

  bool isVI = (getLanguage() == LANG_VI);
  bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);

  // Calendar matrix math
  int daysInMonth = 31;
  if (_calMonth == 4 || _calMonth == 6 || _calMonth == 9 || _calMonth == 11) {
    daysInMonth = 30;
  } else if (_calMonth == 2) {
    bool isLeap =
        (_calYear % 4 == 0 && _calYear % 100 != 0) || (_calYear % 400 == 0);
    daysInMonth = isLeap ? 29 : 28;
  }
  static const int tMap[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int yCalc = (_calMonth < 3) ? (_calYear - 1) : _calYear;
  int wday1 = (yCalc + yCalc / 4 - yCalc / 100 + yCalc / 400 +
               tMap[_calMonth - 1] + 1) %
              7;
  int startCol = (wday1 + 6) % 7; // Mon=0..Sun=6
  int numRows = (startCol + daysInMonth + 6) / 7;
  int16_t rowH = (numRows <= 5) ? 30 : 25;
  int16_t gridStartY = (numRows <= 5) ? 84 : 80;

  // 1. Draw minimal static elements (chevrons, hairline, today's Apple cyan
  // pill, dock indicator) via sprite once
  _spr.fillSprite(zenBg);

  // Minimalist thin < and > chevrons
  _spr.drawWideLine(28, 16, 21, 22, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(21, 22, 28, 28, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(292, 16, 299, 22, 1.5f, zenMuted, zenBg);
  _spr.drawWideLine(299, 22, 292, 28, 1.5f, zenMuted, zenBg);

  // 1px hairline under weekday row
  _spr.drawFastHLine(16, 65, 288, zenLine);

  // Pre-draw Today's smooth Apple Ice Cyan pill if viewing current month
  if (isCurrentMonth) {
    int slot = startCol + (curDay - 1);
    int col = slot % 7;
    int row = slot / 7;
    int16_t cx = 28 + col * 44;
    int16_t cy = gridStartY + row * rowH;
    int16_t boxW = 38;
    int16_t boxH = (numRows <= 5) ? 24 : 21;
    _spr.fillSmoothRoundRect(cx - boxW / 2, cy - boxH / 2, boxW, boxH, boxH / 2,
                             zenAccent, zenBg);
  }

  renderDock();
  _spr.pushSprite(0, 0);

  // 2. Crisp 16-bit Header (Month • Year + Lunar subtitle)
  char titleBuf[40];
  snprintf(titleBuf, sizeof(titleBuf),
           isVI ? "THÁNG %02d  •  %d" : "MONTH %02d  •  %d", _calMonth,
           _calYear);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, titleBuf, 160, 16, MC_DATUM,
                             zenText, zenBg, 54, 5, 212, 22);

  char subBuf[64];
  uint16_t subCol = zenGold;
  if (isCurrentMonth) {
    LunarDate todayLunar =
        LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
    snprintf(subBuf, sizeof(subBuf),
             isVI ? "Hôm nay: %02d/%02d ÂL • %s"
                  : "Today: %02d/%02d Lunar • %s",
             todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
  } else {
    LunarDate midLunar =
        LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
    snprintf(subBuf, sizeof(subBuf),
             isVI ? "Năm %s • Chạm về hôm nay" : "Year %s • Tap for today",
             midLunar.yearCanChi);
    subCol = zenAccent;
  }
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, subBuf, 160, 34, MC_DATUM,
                             subCol, zenBg, 44, 24, 232, 20);

  // 3. Crisp 16-bit Weekday Bar (T2..CN)
  const char *dowVI[] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
  const char *dowEN[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  const char *const *dow = isVI ? dowVI : dowEN;
  for (int col = 0; col < 7; col++) {
    int16_t hx = 28 + col * 44;
    uint16_t c = (col == 6) ? zenCoral : ((col == 5) ? zenAccent : zenMuted);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dow[col], hx, 53,
                               MC_DATUM, c, zenBg, hx - 18, 43, 36, 20);
  }

  // 4. Crisp 16-bit Day Grid (Solar in Be Vietnam Pro 14px + subtle Lunar right
  // next to it)
  _tft.setTextDatum(TL_DATUM);
  for (int d = 1; d <= daysInMonth; d++) {
    int slot = startCol + (d - 1);
    int col = slot % 7;
    int row = slot / 7;
    int16_t cx = 28 + col * 44;
    int16_t cy = gridStartY + row * rowH;

    LunarDate ld = LunarCalendar::convertSolar2Lunar(d, _calMonth, _calYear);
    bool isToday = (d == curDay && isCurrentMonth);

    char sBuf[8], lStr[8];
    snprintf(sBuf, sizeof(sBuf), "%d", d);
    if (ld.day == 1)
      snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
    else
      snprintf(lStr, sizeof(lStr), "%d", ld.day);

    int16_t sW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, sBuf);
    int16_t lW = _tft.textWidth(lStr, 1);
    int16_t totalW = sW + 2 + lW;
    int16_t startX = cx - totalW / 2;

    if (isToday) {
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, sBuf, startX, cy,
                                 ML_DATUM, C_BLACK, zenAccent, startX, cy - 9,
                                 sW + 1, 18);
      _tft.setTextColor(RGB565(12, 34, 48), zenAccent);
      _tft.drawString(lStr, startX + sW + 2, cy - 1, 1);
    } else {
      uint16_t sCol =
          (col == 6) ? zenCoral : ((col == 5) ? zenAccent : zenText);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, sBuf, startX, cy,
                                 ML_DATUM, sCol, zenBg, startX, cy - 9, sW + 1,
                                 18);
      uint16_t lCol = (ld.day == 1 || ld.day == 15) ? zenGold : zenDim;
      _tft.setTextColor(lCol, zenBg);
      _tft.drawString(lStr, startX + sW + 2, cy - 1, 1);
    }
  }
}

void Display::renderPage2_FinanceZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(112, 118, 128);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenLine = RGB565(36, 40, 46);
  const uint16_t zenUp = RGB565(52, 211, 153);
  const uint16_t zenDown = RGB565(248, 113, 113);

  const FinanceData &fd = finance.getData();
  bool isVI = (getLanguage() == LANG_VI);

  _spr.fillSprite(zenBg);
  _spr.drawFastVLine(160, 16, 192, zenLine);
  _spr.drawFastHLine(16, 112, 132, zenLine);
  _spr.drawFastHLine(174, 78, 132, zenLine);
  _spr.drawFastHLine(174, 148, 132, zenLine);
  drawFinanceSparkline(16, 148, 132, 46, fd.sjc7d, 7, zenGold, false);
  renderDock();
  _spr.pushSprite(0, 0);

  char sjcPct[16], sellStr[16], buyVal[16], buyStr[24], kNum[16], kitcoStr[28];
  snprintf(sjcPct, sizeof(sjcPct), "%+.1f%%", fd.sjcChangePercent);
  toVNFloat(sjcPct);
  snprintf(sellStr, sizeof(sellStr), "%.1f", fd.sjcSell);
  toVNFloat(sellStr);
  snprintf(buyVal, sizeof(buyVal), "%.1f", fd.sjcBuy);
  toVNFloat(buyVal);
  snprintf(buyStr, sizeof(buyStr), "%s %s %s", isVI ? "MUA" : "BUY", buyVal,
           isVI ? "tr" : "M");
  FinanceService::formatVND((long)roundf(fd.worldGoldUsd), kNum, sizeof(kNum));
  snprintf(kitcoStr, sizeof(kitcoStr), "KITCO  $%s", kNum);

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                             isVI ? "VÀNG SJC" : "SJC GOLD", 16, 14, TL_DATUM,
                             zenGold, zenBg, 14, 12, 82, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, sjcPct, 148, 14, TR_DATUM,
                             fd.sjcChangePercent >= 0 ? zenUp : zenDown, zenBg,
                             96, 12, 54, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, sellStr, 16, 42, TL_DATUM,
                             zenText, zenBg, 14, 40, 108, 36);
  int16_t sellW = _spr.measureSmoothFace(&ZEN_FONT_NUM, sellStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, isVI ? "tr" : "M",
                             16 + sellW + 4, 54, TL_DATUM, zenMuted, zenBg,
                             16 + sellW + 2, 50, 26, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, buyStr, 16, 86, TL_DATUM,
                             zenMuted, zenBg, 14, 84, 136, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, kitcoStr, 16, 120, TL_DATUM,
                             zenMuted, zenBg, 14, 118, 138, 22);

  char c1Pct[16], c1Rate[16], c2Pct[16], c2Rate[16], btcPct[16], btcRateStr[16];
  snprintf(c1Pct, sizeof(c1Pct), "%+.2f%%", fd.cur1ChangePercent);
  toVNFloat(c1Pct);
  FinanceService::formatVND((long)roundf(fd.cur1Rate), c1Rate, sizeof(c1Rate));
  snprintf(c2Pct, sizeof(c2Pct), "%+.2f%%", fd.cur2ChangePercent);
  toVNFloat(c2Pct);
  FinanceService::formatVND((long)roundf(fd.cur2Rate), c2Rate, sizeof(c2Rate));
  snprintf(btcPct, sizeof(btcPct), "%+.1f%%", fd.btcChangePercent);
  toVNFloat(btcPct);
  if (fd.btcRate >= 1000.0f) {
    char btcNum[16];
    FinanceService::formatVND((long)roundf(fd.btcRate), btcNum, sizeof(btcNum));
    snprintf(btcRateStr, sizeof(btcRateStr), "$%s", btcNum);
  } else {
    snprintf(btcRateStr, sizeof(btcRateStr), "$%.0f", fd.btcRate);
  }

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fd.cur1Code, 174, 14,
                             TL_DATUM, zenAccent, zenBg, 172, 12, 60, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, c1Pct, 306, 14, TR_DATUM,
                             fd.cur1ChangePercent >= 0 ? zenUp : zenDown, zenBg,
                             236, 12, 72, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, c1Rate, 174, 38, TL_DATUM,
                             zenText, zenBg, 172, 36, 136, 36);

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fd.cur2Code, 174, 84,
                             TL_DATUM, zenAccent, zenBg, 172, 82, 60, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, c2Pct, 306, 84, TR_DATUM,
                             fd.cur2ChangePercent >= 0 ? zenUp : zenDown, zenBg,
                             236, 82, 72, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, c2Rate, 174, 108, TL_DATUM,
                             zenText, zenBg, 172, 106, 136, 36);

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, "BTC", 174, 154, TL_DATUM,
                             zenMuted, zenBg, 172, 152, 50, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, btcPct, 306, 154, TR_DATUM,
                             fd.btcChangePercent >= 0 ? zenUp : zenDown, zenBg,
                             236, 152, 72, 22);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, btcRateStr, 174, 176,
                             TL_DATUM, zenText, zenBg, 172, 174, 136, 36);

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                             isVI ? "Chạm vào từng mục để xem chi tiết"
                                  : "Tap any item for details",
                             160, 218, MC_DATUM, zenDim, zenBg, 16, 208, 288,
                             20);
}

void Display::renderPage3_PomodoroZen16(bool forceFull) {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(132, 138, 148);
  const uint16_t zenTrack = RGB565(36, 40, 46);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenGreen = RGB565(52, 211, 153);

  bool stateChanged = forceFull || (_pomoState != _zen16LastPomoState) ||
                      (_pomoMode != _zen16LastPomoMode);
  bool secChanged = stateChanged || (_pomoRemainingSec != _zen16LastPomoSec);
  if (!secChanged)
    return;

  _zen16LastPomoState = (uint8_t)_pomoState;
  _zen16LastPomoMode = (uint8_t)_pomoMode;
  _zen16LastPomoSec = _pomoRemainingSec;

  bool isVI = (getLanguage() == LANG_VI);
  uint16_t modeColors[] = {zenAccent, zenGreen, zenGold};
  uint16_t activeColor = modeColors[_pomoMode];

  if (stateChanged) {
    _spr.fillSprite(zenBg);
    renderDock();
    _spr.pushSprite(0, 0);

    // Draw pills AND labels in True 16-bit RGB565 directly on _tft
    // -> 100% identical RGB565 color across pill & text background (ZERO inner rectangle!)
    const int16_t btnY = 172, btnH = 34;
    if (_pomoState == POMO_STOPPED) {
      VnSprite::drawOutlineRoundRect16(_tft, 96, btnY, 128, btnH, 17,
                                       zenBg, activeColor, zenBg);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_POMO_START), 160,
                                 btnY + btnH / 2, MC_DATUM, activeColor,
                                 zenBg, 110, btnY + 7, 100, 20);
    } else {
      uint16_t c1 = (_pomoState == POMO_RUNNING) ? zenGold : zenGreen;
      VnSprite::drawOutlineRoundRect16(_tft, 44, btnY, 104, btnH, 17,
                                       zenBg, c1, zenBg);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                                 _pomoState == POMO_RUNNING
                                     ? L(TXT_POMO_PAUSE)
                                     : (isVI ? "TIẾP TỤC" : "RESUME"),
                                 96, btnY + btnH / 2, MC_DATUM, c1, zenBg, 58,
                                 btnY + 7, 76, 20);

      uint16_t outlineCol = RGB565(90, 96, 108);
      VnSprite::drawOutlineRoundRect16(_tft, 172, btnY, 104, btnH, 17,
                                       zenBg, outlineCol, zenBg);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_POMO_RESET), 224,
                                 btnY + btnH / 2, MC_DATUM, zenText, zenBg, 186,
                                 btnY + 7, 76, 20);
    }

    const char *tabVI[] = {"TẬP TRUNG", "NGHỈ 5P", "NGHỈ 15P"};
    const char *tabEN[] = {"FOCUS", "SHORT 5M", "LONG 15M"};
    const char *const *tabs = isVI ? tabVI : tabEN;
    int16_t tabCx[] = {78, 160, 242};
    for (int i = 0; i < 3; i++) {
      bool act = (_pomoMode == i);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, tabs[i], tabCx[i], 20,
                                 MC_DATUM, act ? zenText : zenMuted, zenBg,
                                 tabCx[i] - 40, 8, 80, 22);
      if (act)
        _tft.fillRoundRect(tabCx[i] - 20, 33, 40, 2, 1, activeColor);
    }

    const char *statusTxt = isVI ? "SẴN SÀNG" : "READY";
    if (_pomoState == POMO_RUNNING) {
      statusTxt = (_pomoMode == POMO_FOCUS)
                      ? (isVI ? "ĐANG TẬP TRUNG" : "FOCUSING")
                      : (isVI ? "ĐANG NGHỈ NGƠI" : "ON BREAK");
    } else if (_pomoState == POMO_PAUSED) {
      statusTxt = isVI ? "TẠM DỪNG" : "PAUSED";
    }
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, statusTxt, 160, 54,
                               MC_DATUM, activeColor, zenBg, 60, 44, 200, 20);
  }

  // Tabular fixed-width digit slots so countdown NEVER shifts horizontally.
  // Note: ZEN_FONT_CLOCK inked digits at poY=104 (MC_DATUM) span y=73..135,
  // so boxY=71, boxH=68 (y=71..139) leaves statusTxt at y=44..63 (incl. dot
  // below 'Ậ' at y=60) 100% untouched!
  uint16_t mins = _pomoRemainingSec / 60;
  uint16_t secs = _pomoRemainingSec % 60;
  char d0[2] = {(char)('0' + (mins / 10) % 10), '\0'};
  char d1[2] = {(char)('0' + (mins % 10)), '\0'};
  char d2[2] = {(char)('0' + (secs / 10) % 10), '\0'};
  char d3[2] = {(char)('0' + (secs % 10)), '\0'};

  int16_t slotW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, "0");
  int16_t colonW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, ":");
  int16_t cHalf = colonW / 2;

  int16_t x0 = 160 - cHalf - 2 * slotW;
  int16_t x1 = 160 - cHalf - slotW;
  int16_t xC = 160 - cHalf;
  int16_t x2 = 160 + (colonW - cHalf);
  int16_t x3 = x2 + slotW;

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, d0, x0 + slotW / 2, 104,
                             MC_DATUM, activeColor, zenBg, x0, 71, slotW, 68);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, d1, x1 + slotW / 2, 104,
                             MC_DATUM, activeColor, zenBg, x1, 71, slotW, 68);
  if (stateChanged) {
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, ":", 160, 104, MC_DATUM,
                               zenText, zenBg, xC, 71, colonW, 68);
  }
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, d2, x2 + slotW / 2, 104,
                             MC_DATUM, zenText, zenBg, x2, 71, slotW, 68);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, d3, x3 + slotW / 2, 104,
                             MC_DATUM, zenText, zenBg, x3, 71, slotW, 68);

  const int16_t barX = 36, barY = 152, barW = 248, barH = 3;
  _tft.fillRoundRect(barX, barY, barW, barH, 1, zenTrack);
  float progress =
      (_pomoTotalSec > 0)
          ? (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec
          : 0.0f;
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;
  int16_t fillW = (int16_t)(barW * progress);
  if (fillW < 6 && progress > 0.0f)
    fillW = 6;
  if (fillW > 0)
    _tft.fillRoundRect(barX, barY, fillW, barH, 1, activeColor);
}

void Display::renderPage0Ticker() {
  if (_dockVisible)
    return;

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);

  // Refresh ticker message if day changed or language switched or not yet
  // cached
  static Language lastLang = LANG_VI;
  if (_tickerCachedDay != t.tm_mday || _cachedTickerMsg.length() == 0 ||
      lastLang != getLanguage()) {
    _cachedTickerMsg =
        anniversaryMgr.getTodayTickerMessage(getLanguage() == LANG_EN);
    _tickerCachedDay = t.tm_mday;
    lastLang = getLanguage();
    _tickerOffset = 0;
  }

  if (_cachedTickerMsg.length() == 0)
    return;

  // Capsule coordinates (matching the cards width X: 10..310, H: 19px at Y:
  // 215)
  const int16_t barX = 10;
  const int16_t barY = 215;
  const int16_t barW = 300;
  const int16_t barH = 19;
  const int16_t barR = 5;

  // Dark glass background + subtle border
  _spr.fillSmoothRoundRect(barX, barY, barW, barH, barR, RGB565(12, 18, 30),
                           theme.bg);
  _spr.drawRoundRect(barX, barY, barW, barH, barR, RGB565(36, 50, 72));

  // Choose text color based on event type
  uint16_t textColor;
  if (_cachedTickerMsg.startsWith("[★")) {
    textColor = RGB565(255, 215, 64); // Warm Gold for Personal Anniversary
  } else if (_cachedTickerMsg.startsWith("[✿")) {
    textColor =
        RGB565(255, 160, 190); // Soft Rose for National Holidays / Festivals
  } else {
    textColor =
        RGB565(180, 225, 255); // Ice Cyan for Perpetual Calendar & Daily Quotes
  }

  _spr.setTextColor(textColor, RGB565(12, 18, 30));

  int16_t strW = _spr.textWidth(_cachedTickerMsg, 1);
  const int16_t maxW = barW - 16; // 284px

  if (strW <= maxW) {
    // Fits comfortably -> Center statically
    _spr.setTextDatum(MC_DATUM);
    _spr.drawString(_cachedTickerMsg, barX + barW / 2, barY + barH / 2, 1);
  } else {
    // Marquee scrolling
    unsigned long currentMillis = millis();
    if (currentMillis - _lastTickerTick >= 35) {
      _lastTickerTick = currentMillis;
      _tickerOffset++;
      if (_tickerOffset >= strW + 45) {
        _tickerOffset = 0;
      }
    }

    // Set clipping viewport strictly inside capsule
    _spr.setVnClip(barX + 6, barY + 1, barW - 12, barH - 2);
    _spr.setTextDatum(ML_DATUM);

    int16_t drawX1 = (barX + 8) - _tickerOffset;
    _spr.drawString(_cachedTickerMsg, drawX1, barY + barH / 2, 1);

    int16_t drawX2 = drawX1 + strW + 45;
    if (drawX2 < barX + barW) {
      _spr.drawString(_cachedTickerMsg, drawX2, barY + barH / 2, 1);
    }

    _spr.resetVnClip();
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP COMMON UTILITIES & WINDOWS 11 STYLE CLOSE BUTTON
// ═══════════════════════════════════════════════════════════════════════
#define POPUP_DIVIDER_COLOR                                                    \
  RGB565(75, 75, 85) // Unified neutral cool slate gray for all popups

static void toVNFloat(char *str) {
  if (!str || getLanguage() != LANG_VI)
    return;
  for (char *p = str; *p; p++) {
    if (*p == '.')
      *p = ',';
  }
}

void Display::drawPopupCloseButton(int16_t x, int16_t y) {
  // Nút Close Windows 11: 4 góc bo tròn mềm mại thật sự (R=8), không vát cạnh
  uint16_t bg = _spr.readPixel(x - 1, y + 13);
  if (bg == 0)
    bg = RGB565(8, 14, 24);
  _spr.fillSmoothRoundRect(x, y, 26, 26, 8, RGB565(45, 110, 205), bg);
  _spr.fillSmoothRoundRect(x + 1, y + 1, 24, 24, 7, RGB565(18, 60, 125),
                           RGB565(45, 110, 205));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString("X", x + 13, y + 13, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 1 (MINIMAL ZEN 16-BIT): CHI TIET THOI TIET
// ═══════════════════════════════════════════════════════════════════════
void Display::renderWeatherPopupZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(96, 102, 114);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenCoral = RGB565(255, 95, 87);
  const uint16_t zenLine = RGB565(34, 38, 46);

  const WeatherData &w = weather.getData();
  WeatherType wType = weather.getType();
  bool night = weather.isNight();

  // Prepare formatted metric strings up front to measure icon positions
  char humStr[16], windStr[20], uvStr[16];
  snprintf(humStr, sizeof(humStr), "%d%%", w.valid ? w.humidity : 78);
  snprintf(windStr, sizeof(windStr), "%.1f km/h", w.valid ? w.windSpeed : 7.2f);
  toVNFloat(windStr);
  snprintf(uvStr, sizeof(uvStr), "UV %.0f", w.valid ? w.uvIndex : 6.0f);

  const int16_t infoX = 88;
  const int16_t metaY = 97;
  const int16_t gap = 9;
  int16_t humW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, humStr);
  int16_t windW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, windStr);
  int16_t dotW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, "•");

  int16_t humIconX = infoX + 2;
  int16_t humTextX = infoX + 12;
  int16_t dot1X = humTextX + humW + gap;
  int16_t windIconX = dot1X + dotW + gap + 2;
  int16_t windTextX = windIconX + 10;
  int16_t dot2X = windTextX + windW + gap;
  int16_t uvIconX = dot2X + dotW + gap + 2;
  int16_t uvTextX = uvIconX + 10;

  // ── PASS 1: Vector graphics & minimal weather icons via Sprite ────
  _spr.fillSprite(zenBg);

  // Header GPS pin (Apple Coral)
  _spr.fillCircle(18, 19, 3, zenCoral);
  _spr.fillTriangle(16, 20, 20, 20, 18, 25, zenCoral);

  // Minimal Apple close pill [×] at top-right
  _spr.fillSmoothCircle(296, 21, 11, RGB565(28, 32, 38), zenBg);
  _spr.drawWideLine(292, 17, 300, 25, 1.5f, zenMuted, RGB565(28, 32, 38));
  _spr.drawWideLine(300, 17, 292, 25, 1.5f, zenMuted, RGB565(28, 32, 38));

  // Header & section hairlines
  _spr.drawFastHLine(16, 42, 288, zenLine);
  _spr.drawFastHLine(16, 118, 288, zenLine);
  _spr.drawFastVLine(110, 128, 98, zenLine);
  _spr.drawFastVLine(210, 128, 98, zenLine);

  // Current weather minimal outline icon
  drawMinimalWeatherIcon(48, 79, wType, night, zenBg);

  // Row 2 mini metric icons
  drawMiniDrop(_spr, humIconX, metaY - 1, zenAccent);
  drawMiniWind(_spr, windIconX, metaY, zenAccent);
  drawMiniSun(_spr, uvIconX, metaY, zenGold);

  // 3-Day forecast minimal outline icons
  for (int d = 0; d < 3; d++) {
    int16_t colCenterX = 10 + d * 100 + 50;
    drawMinimalWeatherIcon(colCenterX, 164, w.forecast[d].type, false, zenBg);
  }

  _spr.pushSprite(0, 0);

  // ── PASS 2: True 16-bit RGB565 Zen Fonts directly on _tft ─────────
  const char *cityStr = (strlen(w.city) > 0) ? w.city : "TP. HỒ CHÍ MINH";
  char cityUpper[36];
  size_t cj = 0;
  for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
    unsigned char uch = (unsigned char)cityStr[cj];
    cityUpper[cj] = (uch < 0x80) ? (char)toupper(uch) : (char)uch;
  }
  cityUpper[cj] = '\0';
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, cityUpper, 29, 21, ML_DATUM,
                             zenText, zenBg, 28, 10, 248, 22);

  // Row 1: Big 25px AA Temperature + Condition
  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%.1f°C", w.valid ? w.temp : 29.0f);
  toVNFloat(tempStr);
  int16_t tempW = _spr.measureSmoothFace(&ZEN_FONT_NUM, tempStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, tempStr, infoX, 68, ML_DATUM,
                             zenText, zenBg, infoX - 2, 50, tempW + 6, 36);

  int16_t bulletX = infoX + tempW + 8;
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, "•", bulletX, 68, ML_DATUM,
                             zenDim, zenBg, bulletX - 1, 58, dotW + 4, 20);

  int16_t condX = bulletX + dotW + 7;
  const char *condStr = getWeatherConditionI18n(wType, night, true);
  if (condX < 306) {
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, condStr, condX, 68,
                               ML_DATUM, zenGold, zenBg, condX - 1, 58,
                               310 - condX, 20);
  }

  // Row 2: Humidity • Wind • UV
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, humStr, humTextX, metaY,
                             ML_DATUM, zenMuted, zenBg, humTextX - 1,
                             metaY - 10, humW + 4, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, "•", dot1X, metaY, ML_DATUM,
                             zenDim, zenBg, dot1X - 1, metaY - 10, dotW + 4,
                             20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, windStr, windTextX, metaY,
                             ML_DATUM, zenMuted, zenBg, windTextX - 1,
                             metaY - 10, windW + 4, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, "•", dot2X, metaY, ML_DATUM,
                             zenDim, zenBg, dot2X - 1, metaY - 10, dotW + 4,
                             20);
  if (uvTextX < 308) {
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, uvStr, uvTextX, metaY,
                               ML_DATUM, zenGold, zenBg, uvTextX - 1,
                               metaY - 10, 312 - uvTextX, 20);
  }

  // Section 2: 3-Day Forecast text columns
  time_t fNow = time(NULL);
  struct tm fTm;
  localtime_r(&fNow, &fTm);

  for (int d = 0; d < 3; d++) {
    int16_t colX = 10 + d * 100;
    int16_t colCenterX = colX + 50;
    int fWday = (fTm.tm_wday + d + 1) % 7;

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                               getShortDayNameI18n(fWday), colCenterX, 134,
                               MC_DATUM, zenText, zenBg, colX + 2, 124, 96, 20);

    VnSprite::drawSmoothFace16(
        _tft, &ZEN_FONT_LIGHT,
        getWeatherConditionI18n(w.forecast[d].type, false, false), colCenterX,
        195, MC_DATUM, zenGold, zenBg, colX + 2, 185, 96, 20);

    char tRange[20];
    snprintf(tRange, sizeof(tRange), "%.0f° - %.0f°", w.forecast[d].tempMin,
             w.forecast[d].tempMax);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, tRange, colCenterX, 216,
                               MC_DATUM, zenAccent, zenBg, colX + 2, 206, 96,
                               20);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 1: CHI TIET THOI TIET (Option B: Fullscreen Sheet + Header
//  Gradient)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderWeatherPopup() {
  // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không
  // dải gradient
  _spr.fillRect(0, 0, 320, 240, RGB565(8, 14, 24));

  const WeatherData &w = weather.getData();
  WeatherType wType = weather.getType();
  bool night = weather.isNight();

  // 1. Header (Y = 10..38): GPS Pin + Tên thành phố + Nút đóng tròn [X]
  _spr.fillCircle(18, 23, 3, C_RED);
  _spr.fillTriangle(16, 23, 20, 23, 18, 28, C_RED);
  const char *cityStr = (strlen(w.city) > 0) ? w.city : "TP. HỒ CHÍ MINH";
  char cityUpper[36];
  size_t cj = 0;
  for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
    unsigned char uch = (unsigned char)cityStr[cj];
    cityUpper[cj] = (uch < 0x80) ? (char)toupper(uch) : (char)uch;
  }
  cityUpper[cj] = '\0';
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(cityUpper, 28, 23, 2);

  // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc mềm
  // mại
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
  char tempNum[12];
  snprintf(tempNum, sizeof(tempNum), "%.1f", w.valid ? w.temp : 29.0f);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(tempNum, infoX, 56, 4);

  int16_t numW = _spr.textWidth(tempNum, 4);
  int16_t degX = infoX + numW + 2;
  _spr.drawCircle(degX + 1, 60, 2, C_GOLD);
  _spr.setTextColor(C_GOLD);
  _spr.drawString("C", degX + 6, 56, 4);

  int16_t cW = _spr.textWidth("C", 4);
  int16_t afterCX = degX + 6 + cW + 5;

  // Dấu chấm phân cách •
  _spr.setTextColor(RGB565(90, 130, 180));
  _spr.drawString("•", afterCX, 60, 2);

  const char *condStr = getWeatherConditionI18n(wType, night, true);
  _spr.setTextColor(RGB565(255, 215, 0)); // Gold
  _spr.drawString(condStr, afterCX + 10, 60, 2);

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

  // Vạch kẻ phân cách tầng 1 và tầng 2 (Y = 110) — Chuẩn POPUP_DIVIDER_COLOR
  // thống nhất
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
    _spr.drawString(getWeatherConditionI18n(w.forecast[d].type, false, false),
                    colCenterX, 182, 1);

    // 4. Nhiệt độ Min - Max
    char tRange[16];
    snprintf(tRange, sizeof(tRange), "%.0f - %.0f C", w.forecast[d].tempMin,
             w.forecast[d].tempMax);
    _spr.setTextColor(RGB565(200, 225, 245));
    _spr.drawString(tRange, colCenterX, 206, 1);
    _spr.drawCircle(colCenterX + _spr.textWidth(tRange, 1) / 2 - 6, 203, 1,
                    C_GOLD);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 2: CHI TIET THIEN VAN & AM LICH (Option B: Fullscreen Sheet +
//  Header Gradient)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderLunarPopupZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(104, 110, 122);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenLine = RGB565(34, 38, 46);
  const uint16_t zenSubLine = RGB565(24, 27, 34);

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  LunarDate ld = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1,
                                                   1900 + t.tm_year);

  uint8_t lunarDay = ld.day;
  float moonPct = 0.0f;
  if (lunarDay <= 1) {
    moonPct = 0.0f;
  } else if (lunarDay <= 6) {
    moonPct = lunarDay * 6.7f;
  } else if (lunarDay <= 8) {
    moonPct = 50.0f;
  } else if (lunarDay <= 13) {
    moonPct = 50.0f + (lunarDay - 8) * 6.0f;
  } else if (lunarDay <= 16) {
    moonPct = 100.0f;
  } else if (lunarDay <= 21) {
    moonPct = 100.0f - (lunarDay - 16) * 6.0f;
  } else if (lunarDay <= 23) {
    moonPct = 50.0f;
  } else {
    moonPct = 50.0f - (lunarDay - 23) * 7.0f;
  }
  if (moonPct < 0.0f)
    moonPct = 0.0f;
  if (moonPct > 100.0f)
    moonPct = 100.0f;
  const char *phaseName = getMoonPhaseNameI18n(lunarDay);
  bool isVI = (getLanguage() == LANG_VI);

  // Pass 1: Pure black background, moon graphic, hairlines & [×] via Sprite
  _spr.fillSprite(zenBg);

  // Mini crescent badge at top-left
  _spr.fillSmoothCircle(20, 21, 5, zenGold, zenBg);
  _spr.fillSmoothCircle(22, 20, 4, zenBg, zenGold);

  // Minimal Apple close pill [×] at top-right
  _spr.fillSmoothCircle(296, 21, 11, RGB565(28, 32, 38), zenBg);
  _spr.drawWideLine(292, 17, 300, 25, 1.5f, zenMuted, RGB565(28, 32, 38));
  _spr.drawWideLine(300, 17, 292, 25, 1.5f, zenMuted, RGB565(28, 32, 38));

  // Header & section hairlines
  _spr.drawFastHLine(16, 42, 288, zenLine);
  _spr.drawFastHLine(16, 118, 288, zenLine);
  _spr.drawFastHLine(16, 156, 288, zenSubLine);
  _spr.drawFastHLine(16, 192, 288, zenSubLine);

  // Large celestial moon icon (R = 24)
  drawMoonIcon(50, 80, 24, lunarDay, zenBg);

  _spr.pushSprite(0, 0);

  // Pass 2: True 16-bit RGB565 Zen Fonts directly on pure black _tft
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_ASTRONOMY_LUNAR), 34,
                             21, ML_DATUM, zenGold, zenBg, 32, 10, 240, 22);

  // Hero center-left: Moon Phase Name + Subtitle
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, phaseName, 88, 68, ML_DATUM,
                             zenText, zenBg, 86, 56, 150, 24);
  VnSprite::drawSmoothFace16(
      _tft, &ZEN_FONT_LIGHT,
      isVI ? "Độ sáng mặt trăng" : "Moon Illumination", 88, 92, ML_DATUM,
      zenDim, zenBg, 86, 82, 150, 20);

  // Hero right: Crisp 25px AA Illumination Percentage
  char pctBuf[12];
  snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", moonPct);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, pctBuf, 304, 80, MR_DATUM,
                             zenAccent, zenBg, 238, 64, 68, 32);

  // 3 Clean Key-Value Rows
  char lDateBuf[32];
  snprintf(lDateBuf, sizeof(lDateBuf),
           isVI ? "%02d tháng %02d" : "Day %02d, Month %02d", ld.day,
           ld.month);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_LUNAR_DATE), 18, 138,
                             ML_DATUM, zenMuted, zenBg, 16, 128, 146, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, lDateBuf, 302, 138,
                             MR_DATUM, zenText, zenBg, 166, 128, 138, 20);

  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_DAY_CANCHI), 18, 174,
                             ML_DATUM, zenMuted, zenBg, 16, 164, 146, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ld.dayCanChi, 302, 174,
                             MR_DATUM, zenGold, zenBg, 166, 164, 138, 20);

  char yrBuf[32];
  snprintf(yrBuf, sizeof(yrBuf), "%s (%d)", ld.yearCanChi, 1900 + t.tm_year);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_YEAR_CANCHI), 18, 210,
                             ML_DATUM, zenMuted, zenBg, 16, 200, 146, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, yrBuf, 302, 210, MR_DATUM,
                             zenGold, zenBg, 166, 200, 138, 20);
}

void Display::renderLunarPopup() {
  // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không
  // dải gradient
  _spr.fillRect(0, 0, 320, 240, RGB565(10, 14, 22));

  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  LunarDate ld = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1,
                                                   1900 + t.tm_year);

  uint8_t lunarDay = ld.day;
  float moonPct = 0;
  if (lunarDay <= 1) {
    moonPct = 0;
  } else if (lunarDay <= 6) {
    moonPct = lunarDay * 6.7f;
  } else if (lunarDay <= 8) {
    moonPct = 50;
  } else if (lunarDay <= 13) {
    moonPct = 50 + (lunarDay - 8) * 6.0f;
  } else if (lunarDay <= 16) {
    moonPct = 100;
  } else if (lunarDay <= 21) {
    moonPct = 100 - (lunarDay - 16) * 6.0f;
  } else if (lunarDay <= 23) {
    moonPct = 50;
  } else {
    moonPct = 50 - (lunarDay - 23) * 7.0f;
  }
  const char *phaseName = getMoonPhaseNameI18n(lunarDay);

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

  char illuBuf[40];
  snprintf(illuBuf, sizeof(illuBuf),
           (getLanguage() == LANG_VI ? "Độ sáng mặt trăng: %.0f%%"
                                     : "Moon Illumination: %.0f%%"),
           moonPct);
  _spr.setTextColor(RGB565(140, 205, 255));
  _spr.drawString(illuBuf, 88, 96, 2);

  // Vạch kẻ phân cách tầng 1 và tầng 2 (Y = 118) — Chuẩn POPUP_DIVIDER_COLOR
  // thống nhất
  _spr.drawFastHLine(10, 118, 300, POPUP_DIVIDER_COLOR);

  // 3. Danh sách thông số Âm lịch & Can Chi (Rộng rãi 320px)
  // Row 1: Ngày âm lịch
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(RGB565(170, 200, 230));
  _spr.drawString(L(TXT_LUNAR_DATE), 18, 138, 2);
  char lDateBuf[32];
  snprintf(
      lDateBuf, sizeof(lDateBuf),
      (getLanguage() == LANG_VI ? "%02d tháng %02d" : "Day %02d, Month %02d"),
      ld.day, ld.month);
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
  _alarmHour = prefs.getUChar("hour", 7);
  _alarmMinute = prefs.getUChar("min", 0);
  String msg = prefs.getString("msg", "Uống nước & Thư giãn");
  if (msg == "Uong nuoc & Thu gian")
    msg = "Uống nước & Thư giãn";
  strlcpy(_alarmMessage, msg.c_str(), sizeof(_alarmMessage));
  prefs.end();
  if (_alarmHour > 23)
    _alarmHour = 7;
  if (_alarmMinute > 59)
    _alarmMinute = 0;
  Serial.printf("[Alarm] Loaded: %s %02d:%02d, Msg: %s\n",
                _alarmEnabled ? "BAT" : "TAT", _alarmHour, _alarmMinute,
                _alarmMessage);
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
                _alarmEnabled ? "BAT" : "TAT", _alarmHour, _alarmMinute,
                _alarmMessage);
}

void Display::setAlarm(uint8_t h, uint8_t m, bool enabled, const char *msg) {
  _alarmHour = (h <= 23) ? h : 7;
  _alarmMinute = (m <= 59) ? m : 0;
  _alarmEnabled = enabled;
  _alarmLastTriggerDay = -1;
  _alarmLastTriggerMin = -1;
  if (msg != nullptr) {
    strlcpy(_alarmMessage, msg, sizeof(_alarmMessage));
  }
  saveAlarmSettings();
  if (_activePopup == POPUP_ALARM && _sprReady) {
    renderAlarmPopup();
    if (!isMinimalUiMode())
      _spr.pushSprite(0, 0);
  }
  _zen16ScreenValid = false;
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

    // Phát đúng giai điệu báo thức người dùng đã cài đặt (cách quãng 1100ms)
    if (now - _lastAlarmBeepTick >= 1100) {
      _lastAlarmBeepTick = now;
      hardware.playAlarmTune();
    }
    return;
  }

  // 2. Kiểm tra kích hoạt báo thức
  if (!_alarmEnabled)
    return;

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
    if (_alarmLastTriggerMin != currentMin ||
        _alarmLastTriggerDay != t.tm_yday) {
      _alarmLastTriggerDay = t.tm_yday;
      _alarmLastTriggerMin = currentMin;
      triggerAlarm();
    }
  }
}

void Display::triggerAlarm() {
  // 1. Tự động bật sáng màn hình nếu đang tắt (do hẹn giờ hoặc bấm nút Power)
  if (hardware.isScreenOff()) {
    hardware.turnScreenOn();
  }
  _alarmRinging = true;
  _alarmRingStart = millis();
  _lastAlarmBeepTick = 0;
  _alarmSnoozed = false;
  _activePopup = POPUP_NONE;
  _zen16ScreenValid = false;
  _zen16LastPopup = 255;
  wakeDock();
  _lastRender = 0;
  ecosystem.sendEvent(EVT_ALARM_RING,
                      (uint16_t)(_alarmHour * 60 + _alarmMinute));
  Serial.println("[Alarm] >>> TRIGGERED! Ringing started <<<");
}

void Display::dismissAlarm() {
  _alarmRinging = false;
  _alarmSnoozed = false;
  _dockVisible = false;
  _zen16ScreenValid = false;
  _zen16LastPopup = 255;
  hardware.playBeep(1800, 150);
  _lastRender = 0;
  ecosystem.sendEvent(EVT_ALARM_DISMISS, 0);
  Serial.println("[Alarm] Dismissed by user touch");
}

void Display::snoozeAlarm() {
  _alarmRinging = false;
  _alarmSnoozed = true;
  _dockVisible = false;
  _zen16ScreenValid = false;
  _zen16LastPopup = 255;
  _alarmSnoozeUntil = time(NULL) + 300; // Snooze 5 phút
  hardware.playBeep(2200, 120);
  _lastRender = 0;
  ecosystem.sendEvent(EVT_ALARM_DISMISS, 0);
  Serial.println("[Alarm] Snoozed for 5 minutes");
}

void Display::drawBellIcon(int16_t cx, int16_t cy, bool isEnabled,
                           uint16_t customColor) {
  const uint16_t *bmp = isEnabled ? bell_icon_gold_20x20 : bell_icon_off_20x20;
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
  if (isMinimalUiMode()) {
    const uint16_t zenBg = C_BLACK;
    const uint16_t zenText = C_WHITE;
    const uint16_t zenMuted = RGB565(150, 158, 170);
    const uint16_t zenDim = RGB565(104, 110, 122);
    const uint16_t zenAccent = RGB565(100, 210, 255);
    const uint16_t zenGold = RGB565(235, 190, 95);
    const uint16_t zenGreen = RGB565(52, 211, 153);
    const uint16_t zenTrackOff = RGB565(46, 52, 62);
    const uint16_t zenLine = RGB565(32, 36, 44);

    const int16_t hCx = 94, mCx = 226;
    const int16_t swX = 250, swY = 176, swW = 46, swH = 24;

    // Pass 1: Minimal vector graphics via Sprite -> push once
    _spr.fillSprite(zenBg);
    drawBellIcon(20, 19, _alarmEnabled);

    // Minimal Apple close pill [×]
    _spr.fillSmoothCircle(296, 19, 10, RGB565(28, 32, 38), zenBg);
    _spr.drawWideLine(292, 15, 300, 23, 1.5f, zenMuted, RGB565(28, 32, 38));
    _spr.drawWideLine(300, 15, 292, 23, 1.5f, zenMuted, RGB565(28, 32, 38));

    // Hairline dividers
    _spr.drawFastHLine(16, 36, 288, zenLine);
    _spr.drawFastHLine(16, 166, 288, zenLine);

    // Subtle colon dots centered between giant 82px digits
    _spr.fillSmoothCircle(160, 91, 3, zenDim, zenBg);
    _spr.fillSmoothCircle(160, 113, 3, zenDim, zenBg);

    // iOS Toggle Switch (right side of row Y=188)
    uint16_t trackCol = _alarmEnabled ? zenGreen : zenTrackOff;
    _spr.fillSmoothRoundRect(swX, swY, swW, swH, swH / 2, trackCol, zenBg);
    int16_t thumbCx = _alarmEnabled ? (swX + swW - 12) : (swX + 12);
    uint16_t thumbCol = _alarmEnabled ? C_WHITE : RGB565(188, 196, 208);
    _spr.fillSmoothCircle(thumbCx, swY + swH / 2, 9, thumbCol, trackCol);

    _spr.pushSprite(0, 0);

    // Pass 2: True 16-bit Zen Fonts directly on pure black _tft
    VnSprite::drawSmoothFace16(
        _tft, &ZEN_FONT_LIGHT,
        (getLanguage() == LANG_VI ? "CÀI ĐẶT BÁO THỨC" : "ALARM SETTINGS"), 36,
        19, ML_DATUM, zenGold, zenBg, 34, 8, 224, 22);

    // Giant 82px borderless 2-tone digits (Cyan Hour : White Minute)
    char hBuf[8], mBuf[8];
    snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
    snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, hBuf, hCx, 102, MC_DATUM,
                               zenAccent, zenBg, 34, 60, 118, 84);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, mBuf, mCx, 102, MC_DATUM,
                               zenText, zenBg, 168, 60, 118, 84);

    // Status label on pure black bg (left of iOS switch)
    const char *stText =
        _alarmEnabled
            ? (getLanguage() == LANG_VI ? "Đang bật báo thức" : "Alarm On")
            : (getLanguage() == LANG_VI ? "Đang tắt báo thức" : "Alarm Off");
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, stText, 24, 188, ML_DATUM,
                               _alarmEnabled ? zenGreen : zenMuted, zenBg, 20,
                               177, 220, 22);

    // Breathing room + subtle Note at bottom
    if (strlen(_alarmMessage) > 0) {
      char noteBuf[96];
      snprintf(noteBuf, sizeof(noteBuf),
               (getLanguage() == LANG_VI ? "Lời nhắn: %s" : "Note: %s"),
               _alarmMessage);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, noteBuf, 160, 220,
                                 MC_DATUM, zenDim, zenBg, 16, 210, 288, 20);
    }
    return;
  }

  // Nền tối sâu chuyên nghiệp Full Screen (320x240) — Đơn sắc sang trọng, không
  // dải gradient
  _spr.fillRect(0, 0, 320, 240, RGB565(10, 15, 26));

  // ── Header (Y = 10..38): Icon Chuông + Tiêu đề + Nút Đóng tròn [ X ]
  // ──────────────────
  drawBellIcon(18, 23, _alarmEnabled);
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(RGB565(255, 215, 0));
  _spr.drawString(
      (getLanguage() == LANG_VI ? "CÀI ĐẶT BÁO THỨC" : "ALARM SETTINGS"), 36,
      23, 2);

  // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
  drawPopupCloseButton();

  // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
  _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

  // ── 1. KHU VỰC THỜI GIAN GIỜ : PHÚT ──
  int16_t hx = 60, hw = 84;
  int16_t mx2 = 176, mw2 = 84;

  // A. Mũi tên LÊN ▲
  int16_t upY = 64;
  _spr.fillTriangle(hx + hw / 2 - 12, upY + 4, hx + hw / 2 + 12, upY + 4,
                    hx + hw / 2, upY - 8, RGB565(0, 225, 255));
  _spr.fillTriangle(mx2 + mw2 / 2 - 12, upY + 4, mx2 + mw2 / 2 + 12, upY + 4,
                    mx2 + mw2 / 2, upY - 8, RGB565(0, 225, 255));

  // B. Khung số Giờ & Phút (Bo góc mềm mại đồng tâm R = 12, số Font 4 cao sang
  // trọng)
  int16_t boxY = 80, boxH = 58;
  // Khung Giờ
  _spr.fillSmoothRoundRect(hx, boxY, hw, boxH, 12, RGB565(0, 180, 240),
                           RGB565(10, 15, 26));
  _spr.fillSmoothRoundRect(hx + 1, boxY + 1, hw - 2, boxH - 2, 11,
                           RGB565(12, 18, 30), RGB565(0, 180, 240));
  char hBuf[8];
  snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(hBuf, hx + hw / 2, boxY + boxH / 2, 4);

  // Dấu hai chấm ":" phát sáng màu cyan ở giữa
  _spr.fillSmoothCircle(160, 103, 3, RGB565(0, 230, 255), RGB565(10, 15, 26));
  _spr.fillSmoothCircle(160, 117, 3, RGB565(0, 230, 255), RGB565(10, 15, 26));

  // Khung Phút
  _spr.fillSmoothRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(0, 180, 240),
                           RGB565(10, 15, 26));
  _spr.fillSmoothRoundRect(mx2 + 1, boxY + 1, mw2 - 2, boxH - 2, 11,
                           RGB565(12, 18, 30), RGB565(0, 180, 240));
  char mBuf[8];
  snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(mBuf, mx2 + mw2 / 2, boxY + boxH / 2, 4);

  // C. Mũi tên XUỐNG ▼
  int16_t downY = 154;
  _spr.fillTriangle(hx + hw / 2 - 12, downY - 4, hx + hw / 2 + 12, downY - 4,
                    hx + hw / 2, downY + 8, RGB565(0, 225, 255));
  _spr.fillTriangle(mx2 + mw2 / 2 - 12, downY - 4, mx2 + mw2 / 2 + 12,
                    downY - 4, mx2 + mw2 / 2, downY + 8, RGB565(0, 225, 255));

  // D. Nút BẬT / TẮT BÁO THỨC ở đáy (Y = 178, H = 36, W = 230, R = 18)
  int16_t btnX = 45, btnY = 178, btnW = 230, btnH = 36;
  if (_alarmEnabled) {
    _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(35, 190, 110),
                             RGB565(10, 15, 26));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(10, 30, 18));
    _spr.drawString(
        (getLanguage() == LANG_VI ? "ĐANG BẬT BÁO THỨC" : "ALARM ENABLED"), 160,
        btnY + btnH / 2, 2);
  } else {
    _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(55, 70, 95),
                             RGB565(10, 15, 26));
    _spr.fillSmoothRoundRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, 17,
                             RGB565(25, 32, 45), RGB565(55, 70, 95));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(150, 165, 185));
    _spr.drawString(
        (getLanguage() == LANG_VI ? "ĐANG TẮT BÁO THỨC" : "ALARM DISABLED"),
        160, btnY + btnH / 2, 2);
  }

  if (strlen(_alarmMessage) > 0) {
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(140, 175, 220));
    char noteBuf[96];
    snprintf(noteBuf, sizeof(noteBuf),
             (getLanguage() == LANG_VI ? "Lời nhắn: %s" : "Note: %s"),
             _alarmMessage);
    _spr.drawString(noteBuf, 160, 226, 1);
  }
}

void Display::renderAlarmTimeOnly() {
  int16_t hx = 60, hw = 84;
  int16_t mx2 = 176, mw2 = 84;
  int16_t boxY = 80, boxH = 58;

  if (isMinimalUiMode()) {
    const uint16_t zenBg = C_BLACK;
    const uint16_t zenAccent = RGB565(100, 210, 255);
    char hBuf[8], mBuf[8];
    snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
    snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, hBuf, 94, 102, MC_DATUM,
                               zenAccent, zenBg, 34, 60, 118, 84);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, mBuf, 226, 102, MC_DATUM,
                               C_WHITE, zenBg, 168, 60, 118, 84);
    return;
  }

  // 1. Ô Giờ (X: 60, W: 84)
  _spr.fillSmoothRoundRect(hx, boxY, hw, boxH, 12, RGB565(0, 180, 240),
                           RGB565(10, 15, 26));
  _spr.fillSmoothRoundRect(hx + 1, boxY + 1, hw - 2, boxH - 2, 11,
                           RGB565(12, 18, 30), RGB565(0, 180, 240));
  char hBuf[8];
  snprintf(hBuf, sizeof(hBuf), "%02d", _alarmHour);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(hBuf, hx + hw / 2, boxY + boxH / 2, 4);

  // 2. Hai chấm phát sáng ở giữa
  _spr.fillSmoothCircle(160, 103, 3, RGB565(0, 230, 255), RGB565(10, 15, 26));
  _spr.fillSmoothCircle(160, 117, 3, RGB565(0, 230, 255), RGB565(10, 15, 26));

  // 3. Ô Phút (X: 176, W: 84)
  _spr.fillSmoothRoundRect(mx2, boxY, mw2, boxH, 12, RGB565(0, 180, 240),
                           RGB565(10, 15, 26));
  _spr.fillSmoothRoundRect(mx2 + 1, boxY + 1, mw2 - 2, boxH - 2, 11,
                           RGB565(12, 18, 30), RGB565(0, 180, 240));
  char mBuf[8];
  snprintf(mBuf, sizeof(mBuf), "%02d", _alarmMinute);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(mBuf, mx2 + mw2 / 2, boxY + boxH / 2, 4);

  _spr.pushSprite(0, 0);
}

void Display::renderAlarmToggleBtnOnly() {
  int16_t btnX = 45, btnY = 178, btnW = 230, btnH = 36;
  if (isMinimalUiMode()) {
    const uint16_t zenBg = C_BLACK;
    const uint16_t zenMuted = RGB565(150, 158, 170);
    const uint16_t zenGreen = RGB565(52, 211, 153);
    const uint16_t zenTrackOff = RGB565(46, 52, 62);
    const int16_t swX = 250, swY = 176, swW = 46, swH = 24;

    // Update top-left bell icon
    _spr.fillRect(0, 0, 34, 34, zenBg);
    drawBellIcon(20, 19, _alarmEnabled);
    _spr.pushSprite(0, 0, 0, 0, 34, 34);

    // Update iOS Toggle Switch on right
    _spr.fillRect(swX - 2, swY - 2, swW + 4, swH + 4, zenBg);
    uint16_t trackCol = _alarmEnabled ? zenGreen : zenTrackOff;
    _spr.fillSmoothRoundRect(swX, swY, swW, swH, swH / 2, trackCol, zenBg);
    int16_t thumbCx = _alarmEnabled ? (swX + swW - 12) : (swX + 12);
    uint16_t thumbCol = _alarmEnabled ? C_WHITE : RGB565(188, 196, 208);
    _spr.fillSmoothCircle(thumbCx, swY + swH / 2, 9, thumbCol, trackCol);
    _spr.pushSprite(swX - 2, swY - 2, swX - 2, swY - 2, swW + 4, swH + 4);

    // Update 16-bit status label on pure black bg
    const char *stText =
        _alarmEnabled
            ? (getLanguage() == LANG_VI ? "Đang bật báo thức" : "Alarm On")
            : (getLanguage() == LANG_VI ? "Đang tắt báo thức" : "Alarm Off");
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, stText, 24, 188, ML_DATUM,
                               _alarmEnabled ? zenGreen : zenMuted, zenBg, 20,
                               177, 220, 22);
    return;
  }
  if (_alarmEnabled) {
    _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(35, 190, 110),
                             RGB565(10, 15, 26));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(10, 30, 18));
    _spr.drawString(
        (getLanguage() == LANG_VI ? "ĐANG BẬT BÁO THỨC" : "ALARM ENABLED"), 160,
        btnY + btnH / 2, 2);
  } else {
    _spr.fillSmoothRoundRect(btnX, btnY, btnW, btnH, 18, RGB565(55, 70, 95),
                             RGB565(10, 15, 26));
    _spr.fillSmoothRoundRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, 17,
                             RGB565(25, 32, 45), RGB565(55, 70, 95));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(150, 165, 185));
    _spr.drawString(
        (getLanguage() == LANG_VI ? "ĐANG TẮT BÁO THỨC" : "ALARM DISABLED"),
        160, btnY + btnH / 2, 2);
  }
  // Cập nhật cả icon chuông trên header
  drawBellIcon(18, 23, _alarmEnabled);
  _spr.pushSprite(0, 0);
}

void Display::renderAlarmRingingScreenZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(150, 158, 170);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenCoral = RGB565(255, 95, 87);
  const uint16_t zenWarm = RGB565(245, 222, 175);

  bool firstFrame = (_zen16LastPopup != 250);
  static int lastRingMin = -1;
  static bool lastRingFlash = false;

  bool flash = ((millis() / 450) % 2) != 0;
  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);

  if (!firstFrame && flash == lastRingFlash && t.tm_min == lastRingMin) {
    return;
  }

  bool isVI = (getLanguage() == LANG_VI);
  const char *title = isVI ? "BÁO THỨC" : "ALARM";
  int16_t tw = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, title);
  int16_t totalW = 28 + tw;
  int16_t bellCx = 160 - totalW / 2 + 10;
  int16_t txtX = bellCx + 18;

  if (firstFrame) {
    _tft.fillScreen(zenBg);

    const char *msg = (strlen(_alarmMessage) > 0)
                          ? _alarmMessage
                          : (isVI ? "Đã đến giờ!" : "Time's up!");
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, msg, 160, 138, MC_DATUM,
                               zenWarm, zenBg, 16, 127, 288, 22);

    // Snooze button (Left: x=16..154, y=166..218)
    VnSprite::drawOutlineRoundRect16(_tft, 16, 166, 138, 52, 16, zenBg, zenGold,
                                     zenBg);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                               isVI ? "BÁO LẠI" : "SNOOZE", 85, 183, MC_DATUM,
                               zenGold, zenBg, 28, 173, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                               isVI ? "+5 phút" : "+5 mins", 85, 202, MC_DATUM,
                               zenMuted, zenBg, 28, 193, 114, 18);

    // Dismiss button (Right: x=166..304, y=166..218)
    VnSprite::drawOutlineRoundRect16(_tft, 166, 166, 138, 52, 16, zenBg,
                                     zenCoral, zenBg);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                               isVI ? "TẮT CHUÔNG" : "DISMISS", 235, 183,
                               MC_DATUM, zenCoral, zenBg, 178, 173, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT,
                               isVI ? "Dừng hẳn" : "Stop", 235, 202, MC_DATUM,
                               zenMuted, zenBg, 178, 193, 114, 18);
  }

  if (firstFrame || flash != lastRingFlash) {
    int16_t boxX = bellCx - 18;
    _spr.fillRect(boxX, 6, 36, 36, zenBg);
    drawBellIcon(bellCx, 24, true, flash ? zenGold : zenCoral);
    if (flash) {
      _spr.drawCircle(bellCx, 24, 15, zenCoral);
    }
    _spr.pushSprite(boxX, 6, boxX, 6, 36, 36);

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, title, txtX, 24, ML_DATUM,
                               flash ? zenGold : zenCoral, zenBg, txtX - 2, 13,
                               tw + 6, 22);
  }

  // Tabular 82px AA clock digits (Hour in Gold, Minute in White, pulsing colon)
  if (firstFrame || t.tm_min != lastRingMin || flash != lastRingFlash) {
    char h0[2] = {(char)('0' + (t.tm_hour / 10) % 10), '\0'};
    char h1[2] = {(char)('0' + (t.tm_hour % 10)), '\0'};
    char m0[2] = {(char)('0' + (t.tm_min / 10) % 10), '\0'};
    char m1[2] = {(char)('0' + (t.tm_min % 10)), '\0'};

    int16_t slotW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, "0");
    int16_t colonW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, ":");
    int16_t cHalf = colonW / 2;

    int16_t x0 = 160 - cHalf - 2 * slotW;
    int16_t x1 = 160 - cHalf - slotW;
    int16_t xC = 160 - cHalf;
    int16_t x2 = 160 + (colonW - cHalf);
    int16_t x3 = x2 + slotW;

    if (firstFrame || t.tm_min != lastRingMin) {
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, h0, x0 + slotW / 2, 86,
                                 MC_DATUM, zenGold, zenBg, x0, 53, slotW, 68);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, h1, x1 + slotW / 2, 86,
                                 MC_DATUM, zenGold, zenBg, x1, 53, slotW, 68);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, m0, x2 + slotW / 2, 86,
                                 MC_DATUM, zenText, zenBg, x2, 53, slotW, 68);
      VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, m1, x3 + slotW / 2, 86,
                                 MC_DATUM, zenText, zenBg, x3, 53, slotW, 68);
    }

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_CLOCK, ":", 160, 86, MC_DATUM,
                               flash ? zenGold : RGB565(64, 52, 28), zenBg, xC,
                               53, colonW, 68);
  }

  _zen16LastPopup = 250;
  lastRingMin = t.tm_min;
  lastRingFlash = flash;
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
  _spr.drawString((getLanguage() == LANG_VI ? "BÁO THỨC!" : "ALARM ALERT!"),
                  160, 60, 4);

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
    int16_t msgW = _spr.textWidth(_alarmMessage, 2) + 24;
    if (msgW > 290)
      msgW = 290;
    _spr.fillSmoothRoundRect(160 - msgW / 2, 124, msgW, 24, 12, C_GOLD, bg);
    _spr.fillSmoothRoundRect(160 - msgW / 2 + 1, 125, msgW - 2, 22, 11,
                             RGB565(35, 25, 40), C_GOLD);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(_alarmMessage, 160, 136, 2);
  }

  // 4. Hai Nút Bấm Khổng Lồ Trên Màn Hình (100% On-Screen Touch, Không dùng nút
  // BOOT / LED RGB phía sau) Nút Trái: BÁO LẠI (+5 PHÚT)
  int16_t b1X = 14, b1Y = 160, b1W = 140, b1H = 64;
  _spr.fillSmoothRoundRect(b1X, b1Y, b1W, b1H, 16, C_WHITE, bg);
  _spr.fillSmoothRoundRect(b1X + 1, b1Y + 1, b1W - 2, b1H - 2, 15,
                           RGB565(245, 155, 20), C_WHITE);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(20, 20, 20));
  _spr.drawString((getLanguage() == LANG_VI ? "BÁO LẠI" : "SNOOZE"),
                  b1X + b1W / 2, b1Y + 22, 2);
  _spr.setTextColor(RGB565(50, 40, 0));
  _spr.drawString((getLanguage() == LANG_VI ? "+5 PHÚT" : "+5 MINS"),
                  b1X + b1W / 2, b1Y + 44, 2);

  // Nút Phải: TẮT CHUÔNG
  int16_t b2X = 166, b2Y = 160, b2W = 140, b2H = 64;
  _spr.fillSmoothRoundRect(b2X, b2Y, b2W, b2H, 16, C_WHITE, bg);
  _spr.fillSmoothRoundRect(b2X + 1, b2Y + 1, b2W - 2, b2H - 2, 15,
                           RGB565(225, 40, 55), C_WHITE);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString((getLanguage() == LANG_VI ? "TẮT CHUÔNG" : "DISMISS"),
                  b2X + b2W / 2, b2Y + 22, 2);
  _spr.setTextColor(RGB565(255, 200, 200));
  _spr.drawString((getLanguage() == LANG_VI ? "DỪNG HẲN" : "STOP"),
                  b2X + b2W / 2, b2Y + 44, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  BACKGROUND — Deep Obsidian Cosmic Sky + Dynamic Twinkling Stars
// ═══════════════════════════════════════════════════════════════════════
void Display::drawBackground() {
  // 1. Deep Obsidian Cosmic Sky (Pure velvety background matched to active
  // Theme)
  _spr.fillSprite(theme.bg);

  // Subtle atmospheric vertical night gradient (softly transitioning from deep
  // midnight to dark horizon)
  for (int y = 0; y < 112; y += 4) {
    uint8_t shade = map(y, 0, 112, 3, 10);
    _spr.drawFastHLine(0, y, SCREEN_W, RGB565(1, 3, shade));
    _spr.drawFastHLine(0, y + 1, SCREEN_W, RGB565(1, 3, shade));
  }

  // Twinkling delicate diamond stars and celestial stardust dots (No wireframe
  // circles!)
  uint32_t tick = millis() / 100;
  uint8_t flickerPhase = (tick / 3) & 0x07;
  drawSparkle(24, 20, 2, (flickerPhase == 0) ? C_SPARKLE_CYAN : C_CYAN_DIM);
  drawSparkle(296, 20, 2, (flickerPhase == 1) ? C_SPARKLE_CYAN : C_CYAN_DIM);
  drawSparkle(26, 82, 2, (flickerPhase == 2) ? C_SPARKLE_W : C_CYAN_DIM);
  drawSparkle(294, 82, 2, (flickerPhase == 3) ? C_SPARKLE_W : C_CYAN_DIM);
  drawSparkle(160, 115, 1, (flickerPhase == 4) ? C_SPARKLE_W : C_CYAN_DIM);

  // Micro celestial stardust
  drawStarDot(18, 48, C_WHITE);
  drawStarDot(302, 48, C_CYAN_GLOW);
  drawStarDot(82, 24, RGB565(180, 220, 255));
  drawStarDot(238, 24, RGB565(255, 240, 200));
  drawStarDot(52, 88, C_CYAN_GLOW);
  drawStarDot(268, 88, C_WHITE);
}

// ─── Blob: soft organic shape via overlapping translucent circles ────
void Display::drawBlob(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  // Inner bright core
  _spr.fillCircle(cx, cy, r * 2 / 3, color);
  // Outer halo (dimmer, blend with background)
  // Create dimmed version by blending with dark bg
  uint8_t cr = ((color >> 11) & 0x1F) << 3;
  uint8_t cg = ((color >> 5) & 0x3F) << 2;
  uint8_t cb = (color & 0x1F) << 3;
  uint16_t dimColor = RGB565(cr / 3 + 6, cg / 3 + 10, cb / 3 + 20);
  _spr.fillCircle(cx, cy, r, dimColor);
  _spr.fillCircle(cx, cy, r * 2 / 3, color); // re-draw core over halo
}

// ─── Sparkle: 4-point star ───────────────────────────────────────────
void Display::drawSparkle(int16_t cx, int16_t cy, int16_t size,
                          uint16_t color) {
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
void Display::drawWeatherIcon(int16_t cx, int16_t cy, uint8_t wType,
                              bool isNight) {
  switch (wType) {
  case WEATHER_CLEAR:
    if (isNight) {
      // Night sky: crisp sub-pixel SDF golden crescent moon + sparkling stars
      drawMoonIcon(cx, cy, 13, 4);
      drawSparkle(cx - 10, cy + 7, 2, C_WHITE);
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
  // 8 anti-aliased glowing sun rays around the sun
  for (int a = 0; a < 360; a += 45) {
    float rad = a * PI / 180.0f;
    float x1 = cx + cosf(rad) * (r + 3.0f);
    float y1 = cy + sinf(rad) * (r + 3.0f);
    float x2 = cx + cosf(rad) * (r + 5.5f);
    float y2 = cy + sinf(rad) * (r + 5.5f);
    _spr.drawWedgeLine(x1, y1, x2, y2, 1.1f, 0.7f, C_SUN_YELLOW);
  }

  // 3D Sun body with smooth anti-aliased radial depth
  _spr.fillSmoothCircle(cx, cy, r + 2, C_SUN_ORANGE);
  _spr.fillSmoothCircle(cx, cy, r, C_SUN_YELLOW, C_SUN_ORANGE);
  _spr.fillSmoothCircle(cx - 2, cy - 2, r - 2, RGB565(255, 245, 160),
                        C_SUN_YELLOW);
  _spr.drawPixel(cx - 2, cy - 2, C_WHITE);
}

void Display::drawCloudIcon(int16_t cx, int16_t cy, int16_t w, int16_t h) {
  // Volumetric 3D Fluffy Cloud with smooth anti-aliased shading
  int16_t baseR = h / 3;

  // Bottom soft shadow layer
  _spr.fillSmoothRoundRect(cx - w / 2, cy - baseR + 2, w, baseR * 2 + 2, baseR,
                           RGB565(150, 170, 195));
  _spr.fillSmoothCircle(cx - w / 4, cy - baseR, baseR + 2,
                        RGB565(160, 180, 205));
  _spr.fillSmoothCircle(cx + 2, cy - baseR - 2, baseR + 4,
                        RGB565(170, 190, 215));
  _spr.fillSmoothCircle(cx + w / 4, cy - baseR + 1, baseR + 1,
                        RGB565(160, 180, 205));

  // Pure white cloud body
  _spr.fillSmoothRoundRect(cx - w / 2, cy - baseR, w, baseR * 2, baseR,
                           C_WHITE);
  _spr.fillSmoothCircle(cx - w / 4, cy - baseR - 2, baseR + 2, C_WHITE);
  _spr.fillSmoothCircle(cx + 2, cy - baseR - 4, baseR + 4, C_WHITE);
  _spr.fillSmoothCircle(cx + w / 4, cy - baseR - 1, baseR + 1, C_WHITE);
}

void Display::drawPartlyCloudyIcon(int16_t cx, int16_t cy) {
  // 1. 3D Sun behind top-left (sr = 8, rays at 12)
  int16_t sx = cx - 5;
  int16_t sy = cy - 5;
  int16_t sr = 8;

  for (int a = 0; a < 360; a += 45) {
    float rad = a * PI / 180.0f;
    int16_t rx = sx + (int16_t)(cosf(rad) * (sr + 4));
    int16_t ry = sy + (int16_t)(sinf(rad) * (sr + 4));
    _spr.drawPixel(rx, ry, C_SUN_YELLOW);
  }
  _spr.fillSmoothCircle(sx, sy, sr + 1, C_SUN_ORANGE);
  _spr.fillSmoothCircle(sx, sy, sr, C_SUN_YELLOW, C_SUN_ORANGE);
  _spr.fillSmoothCircle(sx - 1, sy - 1, sr - 2, RGB565(255, 245, 170),
                        C_SUN_YELLOW);

  // 2. 3D Fluffy Cloud in foreground (w = 26, h = 14)
  int16_t clx = cx + 3;
  int16_t cly = cy + 4;
  drawCloudIcon(clx, cly, 26, 14);
}

void Display::drawRainIcon(int16_t cx, int16_t cy) {
  drawCloudIcon(cx, cy - 4, 26, 14);
  // Smooth anti-aliased rain drops
  for (int i = 0; i < 4; i++) {
    float dx = cx - 7 + i * 5;
    float dy = cy + 7 + (i & 1) * 2;
    _spr.drawWedgeLine(dx, dy, dx - 1.5f, dy + 5.0f, 0.9f, 0.9f, C_RAIN_BLUE);
  }
}

void Display::drawStormIcon(int16_t cx, int16_t cy) {
  drawCloudIcon(cx, cy - 4, 26, 14);
  // Smooth anti-aliased lightning bolt
  _spr.drawWedgeLine(cx, cy + 4, cx - 2, cy + 9, 1.1f, 1.0f, C_GOLD);
  _spr.drawWedgeLine(cx - 2, cy + 9, cx + 1, cy + 9, 1.0f, 1.0f, C_GOLD);
  _spr.drawWedgeLine(cx + 1, cy + 9, cx - 1, cy + 15, 1.0f, 0.7f, C_GOLD);
}

void Display::drawMoonIcon(int16_t cx, int16_t cy, int16_t r, uint8_t lunarDay,
                           uint16_t bgTint) {
  (void)bgTint;
  if (r <= 0)
    return;

  // Palette: Luminous celestial moon gold, pearlescent rim highlight, and soft
  // mare craters
  uint16_t moonGold = RGB565(255, 226, 128);
  uint16_t moonHighlight = RGB565(255, 248, 212);
  uint16_t craterColor = RGB565(222, 194, 108);
  uint16_t earthshineCol = RGB565(42, 60, 92);

  // 1. Trăng non (New Moon: Day 1 or Day 30)
  //    Delicate waxing sliver crescent on the right limb + mysterious
  //    earthshine dark body (No cross / sparkle)
  if (lunarDay <= 1 || lunarDay >= 30) {
    float R = (float)r;
    float shiftX = fmaxf(1.8f, R * 0.16f); // 2.2px for r=14, 3.8px for r=24
    float sx = -shiftX;
    float Rs = R;

    for (int16_t dy = -r - 1; dy <= r + 1; dy++) {
      int16_t py = cy + dy;
      if (py < 0 || py >= SCREEN_H)
        continue;
      for (int16_t dx = -r - 1; dx <= r + 1; dx++) {
        int16_t px = cx + dx;
        if (px < 0 || px >= SCREEN_W)
          continue;

        float dOut = sqrtf((float)(dx * dx + dy * dy));
        if (dOut >= R + 0.65f)
          continue;

        float aOut = (dOut <= R - 0.35f) ? 1.0f : (R + 0.65f - dOut);
        uint16_t bg = _spr.readPixel(px, py);

        // Right sliver crescent calculation (tapers to sharp tips at top &
        // bottom)
        float cdx = (float)dx - sx;
        float dCut = sqrtf(cdx * cdx + (float)(dy * dy));
        float aSliver =
            (dCut <= Rs - 0.50f)
                ? 0.0f
                : ((dCut >= Rs + 0.50f) ? 1.0f : (dCut - (Rs - 0.50f)));
        float lit = aOut * aSliver;

        if (lit > 0.04f && dx > -1) {
          // Golden-cream luminous sliver of new moon
          uint16_t col = (dOut >= R - 1.8f) ? moonHighlight : moonGold;
          uint8_t a = (uint8_t)(lit * 255.0f);
          _spr.drawPixel(px, py,
                         (a == 255) ? col : _spr.alphaBlend(a, col, bg));
        } else {
          // Earthshine dark celestial disc with soft celestial rim
          if (dOut <= R - 1.15f) {
            _spr.drawPixel(px, py, _spr.alphaBlend(60, earthshineCol, bg));
          } else {
            float aRing = (dOut <= R) ? 1.0f : (R + 0.65f - dOut) / 0.65f;
            uint8_t a = (uint8_t)(aRing * 110.0f);
            _spr.drawPixel(px, py,
                           _spr.alphaBlend(a, RGB565(90, 130, 180), bg));
          }
        }
      }
    }
    return;
  }

  // 2. Trăng tròn / Rằm (Full Moon: Day 14..16)
  if (lunarDay >= 14 && lunarDay <= 16) {
    _spr.fillSmoothCircle(cx, cy, r, moonGold);
    _spr.fillSmoothCircle(cx - r / 5, cy - r / 5, (r * 3) / 4, moonHighlight);
    _spr.fillSmoothCircle(cx - r / 3, cy - r / 5, fmax(2, r / 5), craterColor);
    _spr.fillSmoothCircle(cx + r / 4, cy + r / 3, fmax(2, r / 6), craterColor);
    _spr.fillSmoothCircle(cx + r / 3, cy - r / 4, fmax(1, r / 7), craterColor);
    return;
  }

  bool isWaxing = (lunarDay < 15);
  float R = (float)r;

  // 3. Trăng lưỡi liềm (Crescent Moon: Day 2..6 Waxing, Day 24..29 Waning)
  //    Single-pass 2-circle SDF subtraction — never paints bgTint over Glass
  //    Card!
  if ((isWaxing && lunarDay <= 6) || (!isWaxing && lunarDay >= 24)) {
    float t = isWaxing ? ((float)(lunarDay - 2) / 4.0f)
                       : ((float)(29 - lunarDay) / 5.0f);
    float shift = R * (0.42f + t * 0.28f);
    float sx = isWaxing ? (-shift * 0.88f) : (shift * 0.88f);
    float sy = -shift * 0.42f;
    float Rs = R * 0.92f;

    for (int16_t dy = -r - 1; dy <= r + 1; dy++) {
      int16_t py = cy + dy;
      if (py < 0 || py >= SCREEN_H)
        continue;
      for (int16_t dx = -r - 1; dx <= r + 1; dx++) {
        int16_t px = cx + dx;
        if (px < 0 || px >= SCREEN_W)
          continue;

        float dOut = sqrtf((float)(dx * dx + dy * dy));
        if (dOut >= R + 0.65f)
          continue;

        float aOut = (dOut <= R - 0.35f) ? 1.0f : ((R + 0.65f - dOut));
        float cdx = (float)dx - sx;
        float cdy = (float)dy - sy;
        float dCut = sqrtf(cdx * cdx + cdy * cdy);
        float aCut =
            (dCut <= Rs - 0.55f)
                ? 0.0f
                : ((dCut >= Rs + 0.45f) ? 1.0f : (dCut - (Rs - 0.55f)));

        float aLit = aOut * aCut;
        if (aLit <= 0.02f)
          continue;

        // Subtle 3D rim glow along outer arc
        uint16_t col = (dOut >= R - 2.2f) ? moonHighlight : moonGold;
        uint8_t alpha = (uint8_t)(aLit * 255.0f);
        uint16_t bg = _spr.readPixel(px, py);
        _spr.drawPixel(px, py,
                       (alpha == 255) ? col : _spr.alphaBlend(alpha, col, bg));
      }
    }

    // Delicate sparkle star nestled inside the crescent curve
    int16_t starX = isWaxing ? (cx - r * 3 / 10) : (cx + r * 3 / 10);
    int16_t starY = cy - r * 3 / 10;
    drawSparkle(starX, starY, 1, C_WHITE);
    return;
  }

  // 4. Bán nguyệt & Trăng khuyết lồi (Quarter & Gibbous: Day 7..13 Waxing,
  // Day 17..23 Waning)
  //    True 3D Spherical Elliptical Terminator SDF (Gibbous is convex, never
  //    concave/warped!)
  float k = 0.0f;
  if (isWaxing) {
    // Day 7 -> +0.10, Day 8 -> 0.00 (Half Moon), Day 9..13 -> -0.16..-0.76
    // (Convex Gibbous)
    if (lunarDay <= 8) {
      k = (float)(8 - lunarDay) * 0.10f;
    } else {
      k = -0.16f - (float)(lunarDay - 9) * 0.15f;
    }
  } else {
    // Day 17..21 -> +0.76..+0.16 (Convex Gibbous), Day 22 -> 0.00 (Half Moon),
    // Day 23 -> -0.10
    if (lunarDay <= 21) {
      k = 0.76f - (float)(lunarDay - 17) * 0.15f;
    } else {
      k = -(float)(lunarDay - 22) * 0.10f;
    }
  }

  // Subtle 14-degree celestial axis tilt
  const float cosTilt = 0.970f;
  const float sinTilt = 0.242f;

  // Crater centers for subtle surface texture inside lit region
  float cr1X = isWaxing ? (R * 0.32f) : (-R * 0.32f);
  float cr1Y = -R * 0.22f;
  float cr1R = fmaxf(1.8f, R * 0.20f);

  float cr2X = isWaxing ? (R * 0.24f) : (-R * 0.24f);
  float cr2Y = R * 0.30f;
  float cr2R = fmaxf(1.5f, R * 0.16f);

  for (int16_t dy = -r - 1; dy <= r + 1; dy++) {
    int16_t py = cy + dy;
    if (py < 0 || py >= SCREEN_H)
      continue;
    for (int16_t dx = -r - 1; dx <= r + 1; dx++) {
      int16_t px = cx + dx;
      if (px < 0 || px >= SCREEN_W)
        continue;

      float fdx = (float)dx;
      float fdy = (float)dy;
      float dOut = sqrtf(fdx * fdx + fdy * fdy);
      if (dOut >= R + 0.65f)
        continue;

      float aOut = (dOut <= R - 0.35f) ? 1.0f : (R + 0.65f - dOut);

      // Rotate into tilted moon coordinates
      float rx = fdx * cosTilt + fdy * sinTilt;
      float ry = -fdx * sinTilt + fdy * cosTilt;

      float chordSq = R * R - ry * ry;
      float wy = (chordSq > 0.0f) ? sqrtf(chordSq) : 0.0f;
      float xTerm = k * wy;

      float signedDist = isWaxing ? (rx - xTerm) : (xTerm - rx);
      float aTerm =
          (signedDist <= -0.55f)
              ? 0.0f
              : ((signedDist >= 0.55f) ? 1.0f : ((signedDist + 0.55f) / 1.10f));

      float aLit = aOut * aTerm;
      if (aLit <= 0.02f)
        continue;

      uint16_t col = (dOut >= R - 2.0f) ? moonHighlight : moonGold;
      float dCr1 =
          sqrtf((fdx - cr1X) * (fdx - cr1X) + (fdy - cr1Y) * (fdy - cr1Y));
      float dCr2 =
          sqrtf((fdx - cr2X) * (fdx - cr2X) + (fdy - cr2Y) * (fdy - cr2Y));
      if (dCr1 <= cr1R || dCr2 <= cr2R) {
        col = craterColor;
      }

      uint8_t alpha = (uint8_t)(aLit * 255.0f);
      uint16_t bg = _spr.readPixel(px, py);
      _spr.drawPixel(px, py,
                     (alpha == 255) ? col : _spr.alphaBlend(alpha, col, bg));
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  GLASS CARD — frosted translucent panel with 50% alpha blending
// ═══════════════════════════════════════════════════════════════════════
void Display::drawGlassCard(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t radius) {
  if (w <= 0 || h <= 0)
    return;
  int16_t maxR = (w < h ? w : h) / 2;
  if (radius > maxR)
    radius = maxR;
  if (radius < 1)
    radius = 1;

  uint16_t tint = theme.card;
  uint16_t borderCol = theme.cardBorder;

  int16_t cxL = x + radius;
  int16_t cxR = x + w - 1 - radius;
  int16_t cyT = y + radius;
  int16_t cyB = y + h - 1 - radius;

  float rInnerEnd = (float)radius - 1.15f;
  float rInnerSolid = (float)radius - 0.25f;
  float rOuterSolid = (float)radius + 0.25f;
  float rOuterEnd = (float)radius + 1.15f;

  int16_t x2 = x + w;
  int16_t y2 = y + h;

  for (int16_t py = y; py < y2; py++) {
    if (py < 0 || py >= SCREEN_H)
      continue;
    int16_t dy = 0;
    if (py < cyT)
      dy = cyT - py;
    else if (py > cyB)
      dy = py - cyB;

    for (int16_t px = x; px < x2; px++) {
      if (px < 0 || px >= SCREEN_W)
        continue;
      int16_t dx = 0;
      if (px < cxL)
        dx = cxL - px;
      else if (px > cxR)
        dx = px - cxR;

      if (dx == 0 && dy == 0) {
        uint16_t bg = _spr.readPixel(px, py);
        _spr.drawPixel(px, py, _spr.alphaBlend(128, tint, bg));
        continue;
      }

      if (dx == 0 || dy == 0) {
        int16_t dist = (dx > dy) ? dx : dy;
        if (dist == radius) {
          _spr.drawPixel(px, py, borderCol);
        } else if (dist < radius) {
          uint16_t bg = _spr.readPixel(px, py);
          _spr.drawPixel(px, py, _spr.alphaBlend(128, tint, bg));
        }
        continue;
      }

      // Corner quadrant: single-pass sub-pixel SDF (interior -> border ->
      // background)
      float d = sqrtf((float)(dx * dx + dy * dy));
      if (d >= rOuterEnd) {
        continue; // Outside corner — preserve background pixel untouched
      }

      uint16_t bg = _spr.readPixel(px, py);
      uint16_t glassFill = _spr.alphaBlend(128, tint, bg);

      if (d <= rInnerEnd) {
        _spr.drawPixel(px, py, glassFill);
      } else if (d < rInnerSolid) {
        uint8_t a =
            (uint8_t)(((d - rInnerEnd) / (rInnerSolid - rInnerEnd)) * 255.0f);
        _spr.drawPixel(px, py, _spr.alphaBlend(a, borderCol, glassFill));
      } else if (d <= rOuterSolid) {
        _spr.drawPixel(px, py, borderCol);
      } else {
        uint8_t a =
            (uint8_t)(((rOuterEnd - d) / (rOuterEnd - rOuterSolid)) * 255.0f);
        _spr.drawPixel(px, py, _spr.alphaBlend(a, borderCol, bg));
      }
    }
  }
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
    _dockVisible = false;
    _lastRender = 0;
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
  bool wasRunning = (_pomoState != POMO_STOPPED);
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
  if (wasRunning) {
    ecosystem.sendEvent(EVT_POMODORO_STOP, 0);
  }
}

void Display::pomoStart() {
  _pomoState = POMO_RUNNING;
  _pomoLastSecTick = millis();
  if (_pomoMode == POMO_FOCUS) {
    if (_pomoRemainingSec == _pomoTotalSec) {
      uint16_t mins = (_pomoTotalSec + 59) / 60;
      ecosystem.sendEvent(EVT_POMODORO_START, mins);
    } else {
      ecosystem.sendEvent(EVT_POMODORO_TICK, _pomoRemainingSec);
    }
  } else {
    ecosystem.sendEvent(EVT_POMODORO_BREAK, _pomoRemainingSec);
  }
}

void Display::pomoPause() {
  _pomoState = POMO_PAUSED;
  ecosystem.sendEvent(EVT_POMODORO_STOP, _pomoRemainingSec);
}

void Display::pomoReset() {
  _pomoState = POMO_STOPPED;
  _pomoRemainingSec = _pomoTotalSec;
  ecosystem.sendEvent(EVT_POMODORO_STOP, 0);
}

void Display::webPomodoroStart() {
  if (_currentPage != PAGE_POMODORO) {
    setPage(PAGE_POMODORO);
  }
  pomoStart();
  _lastRender = 0;
}

void Display::webPomodoroPause() {
  pomoPause();
  _lastRender = 0;
}

void Display::webPomodoroReset() {
  pomoReset();
  _lastRender = 0;
}

void Display::webPomodoroSetMode(uint8_t mode) {
  if (mode <= 2) {
    pomoSetMode((PomoMode)mode);
    _lastRender = 0;
  }
}

void Display::syncRemotePomodoroStart(uint16_t minutes) {
  if (minutes == 0)
    minutes = 25;
  _pomoMode = POMO_FOCUS;
  _pomoTotalSec = minutes * 60;
  _pomoRemainingSec = _pomoTotalSec;
  _pomoState = POMO_RUNNING;
  _pomoLastSecTick = millis();
  _lastRender = 0;
}

void Display::syncRemotePomodoroStop() {
  _pomoState = POMO_STOPPED;
  _pomoRemainingSec = _pomoTotalSec;
  _lastRender = 0;
}

void Display::pomoTick() {
  if (_pomoState != POMO_RUNNING)
    return;

  unsigned long now = millis();
  if (now - _pomoLastSecTick >= 1000) {
    _pomoLastSecTick = now;
    if (_pomoRemainingSec > 0) {
      _pomoRemainingSec--;
      // Đồng bộ số giây còn lại sang EMO Companion mỗi 5 giây
      if (_pomoMode == POMO_FOCUS && (_pomoRemainingSec > 0) &&
          (_pomoRemainingSec % 5 == 0)) {
        ecosystem.sendEvent(EVT_POMODORO_TICK, _pomoRemainingSec);
      }
    }
    if (_pomoRemainingSec == 0) {
      _pomoState = POMO_STOPPED;
      // Chuông báo hiệu kết thúc phiên (2 tiếng beep dứt khoát)
      hardware.playBeep(2600, 150);
      delay(80);
      hardware.playBeep(3300, 300);

      if (_pomoMode == POMO_FOCUS) {
        // Phát sự kiện hết giờ Pomodoro để EMO Companion vươn vai ăn mừng!
        ecosystem.sendEvent(EVT_POMODORO_BREAK, 0);
        if (_pomoSession < 4) {
          _pomoSession++;
          _pomoMode = POMO_SHORT_BREAK;
          _pomoTotalSec = 5 * 60;
          _pomoRemainingSec = _pomoTotalSec;
        } else {
          _pomoSession = 1;
          _pomoMode = POMO_LONG_BREAK;
          _pomoTotalSec = 15 * 60;
          _pomoRemainingSec = _pomoTotalSec;
        }
      } else {
        ecosystem.sendEvent(EVT_POMODORO_STOP, 0);
        _pomoMode = POMO_FOCUS;
        _pomoTotalSec = 25 * 60;
        _pomoRemainingSec = _pomoTotalSec;
      }
    }
  }
}

bool Display::handleTouch(uint16_t x, uint16_t y) {
  // 0. NẾU BÁO THỨC ĐANG KÊU: XỬ LÝ 2 NÚT BẤM TO TRÊN MÀN HÌNH (100% Cảm ứng
  // mặt trước, Không dùng nút BOOT / LED)
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
    // Chạm vào bất kỳ vị trí nào khác trên màn hình khi đang reo cũng tắt
    // chuông để an toàn
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
      _zen16ScreenValid = false;
      hardware.playClick();
      _lastRender = 0;
      return true;
    }

    // A2. Chạm vào biểu tượng Chuông ở góc trên trái của Popup (x <= 65 && y <=
    // 48) -> Bật / Tắt nhanh báo thức
    if (x <= 65 && y <= 48) {
      _alarmEnabled = !_alarmEnabled;
      _alarmLastTriggerDay = -1;
      _alarmLastTriggerMin = -1;
      hardware.playClick();
      renderAlarmToggleBtnOnly();
      saveAlarmSettings();
      return true;
    }

    // B. Hàng công tắc BẬT/TẮT Báo thức ở đáy: (x: 16..308, y: 168..235)
    if (x >= 16 && x <= 308 && y >= 168 && y <= 235) {
      _alarmEnabled = !_alarmEnabled;
      _alarmLastTriggerDay = -1;
      _alarmLastTriggerMin = -1;
      saveAlarmSettings();
      hardware.playClick();
      renderAlarmToggleBtnOnly();
      return true;
    }

    if (isMinimalUiMode()) {
      // Minimal Zen: Chạm hoặc giữ bất kỳ đâu trên cột GIỜ / PHÚT đều tăng +1 (quay vòng)
      if (x >= 30 && x <= 155 && y >= 40 && y <= 166) {
        _alarmAutoCloseTime = 0;
        _alarmHour = (_alarmHour + 1) % 24;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
      if (x >= 165 && x <= 290 && y >= 40 && y <= 166) {
        _alarmAutoCloseTime = 0;
        _alarmMinute = (_alarmMinute + 1) % 60;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
    } else {
      // Classic Mode: Cột GIỜ nửa trên ▲ tăng / nửa dưới ▼ giảm
      if (x >= 30 && x <= 155 && y >= 40 && y <= 102) {
        _alarmAutoCloseTime = 0;
        _alarmHour = (_alarmHour + 1) % 24;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
      if (x >= 30 && x <= 155 && y >= 103 && y <= 166) {
        _alarmAutoCloseTime = 0;
        _alarmHour = (_alarmHour + 23) % 24;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
      // Classic Mode: Cột PHÚT nửa trên ▲ tăng / nửa dưới ▼ giảm
      if (x >= 165 && x <= 290 && y >= 40 && y <= 102) {
        _alarmAutoCloseTime = 0;
        _alarmMinute = (_alarmMinute + 1) % 60;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
      if (x >= 165 && x <= 290 && y >= 103 && y <= 166) {
        _alarmAutoCloseTime = 0;
        _alarmMinute = (_alarmMinute + 59) % 60;
        _alarmLastTriggerDay = -1;
        _alarmLastTriggerMin = -1;
        hardware.playClick();
        renderAlarmTimeOnly();
        return true;
      }
    }

    return true; // Chạm vào các khoảng đệm bên trong popup không đóng vô cớ
  }

  // 2. POPUP TỶ GIÁ NGOẠI TỆ (POPUP_FINANCE_CURRENCY)
  if (_activePopup == POPUP_FINANCE_CURRENCY) {
    // Nút [X] đóng góc trên phải: (x >= 265 && y <= 48)
    if (x >= 265 && y <= 48) {
      _activePopup = POPUP_NONE;
      _zen16ScreenValid = false;
      hardware.playClick();
      _lastRender = 0;
      return true;
    }

    if (isMinimalUiMode()) {
      // 3 Tabs: cur1 (12..95), cur2 (96..177), BTC (178..258)
      if (y <= 46 && x >= 12 && x <= 258) {
        uint8_t newTab = (x < 96) ? 0 : ((x < 178) ? 1 : 2);
        if (_currencyTab != newTab) {
          _currencyTab = newTab;
          hardware.playClick();
          _lastRender = 0;
        }
        return true;
      }
    } else {
      // Classic 2 Tabs: Ngoại tệ 1 (10..132), Ngoại tệ 2 (134..256)
      if (x >= 10 && x <= 132 && y <= 48) {
        if (_currencyTab != 0) {
          _currencyTab = 0;
          hardware.playClick();
          _lastRender = 0;
        }
        return true;
      }
      if (x >= 134 && x <= 256 && y <= 48) {
        if (_currencyTab != 1) {
          _currencyTab = 1;
          hardware.playClick();
          _lastRender = 0;
        }
        return true;
      }
    }

    return true; // Chạm trong popup không đóng vô cớ
  }

  // 3. POPUP VÀNG SJC (POPUP_FINANCE_GOLD)
  if (_activePopup == POPUP_FINANCE_GOLD) {
    // Nút [X] đóng góc trên phải: (x >= 265 && y <= 48)
    if (x >= 265 && y <= 48) {
      _activePopup = POPUP_NONE;
      _zen16ScreenValid = false;
      hardware.playClick();
      _lastRender = 0;
      return true;
    }
    return true;
  }

  // 4. CÁC POPUP KHÁC (Weather, Lunar): Chạm bất kỳ đâu để đóng
  if (_activePopup != POPUP_NONE) {
    _activePopup = POPUP_NONE;
    _zen16ScreenValid = false;
    hardware.playClick();
    _lastRender = 0; // Trigger instant redraw
    return true;
  }

  // 3. ƯU TIÊN TUYỆT ĐỐI CHO THANH DOCK:
  // Khi thanh Dock đang hiển thị (Mở rộng vùng nhận cảm ứng của các icon Dock
  // từ Y >= 178..239 để bù trừ góc nghiêng ngón tay sát viền dưới màn hình):
  if (_dockVisible) {
    if (y >= 178) {
      // Chạm vào các icon trên thanh Dock -> Chuyển trang và tự ẩn Dock ngay
      // sau đó
      _lastRender = 0;
      hardware.playClick();
      if (x < 58) {
        setPage(PAGE_CLOCK);
        _dockVisible = false;
        return true;
      } else if (x >= 58 && x < 94) {
        setPage(PAGE_CALENDAR);
        _dockVisible = false;
        return true;
      } else if (x >= 94 && x < 130) {
        setPage(PAGE_FINANCE);
        _dockVisible = false;
        return true;
      } else if (x >= 130 && x < 166) {
        setPage(PAGE_POMODORO);
        _dockVisible = false;
        return true;
      } else if (x >= 166 && x < 202) {
        setPage(PAGE_MOOD);
        _dockVisible = false;
        return true;
      } else if (x >= 202 && x < 241) {
        setPage(PAGE_SETTINGS);
        _dockVisible = false;
        return true;
      } else if (x >= 241) {
        // NÚT POWER ⏻: Tắt màn hình ngay lập tức!
        _dockVisible = false;
        hardware.turnScreenOff();
        return true;
      }
      _dockVisible = false;
      return true;
    } else {
      // Nhấn ra phía trên thanh Dock (Y < 178):
      // • Ngoại lệ thông minh cho các thanh điều khiển trên đỉnh màn hình
      // (Chuông/Đồng hồ Y<=92,
      //   Gáy lịch Y<=50, Tab Pomodoro Y<=40, Nút đổi bộ Meme góc trên phải,
      //   Thanh Độ sáng Y<=55): vừa tự ẩn Dock vừa thực thi ngay lệnh để không
      //   bắt người dùng bấm 2 lần.
      // • Còn nếu chạm vào thân trang (thẻ Tài chính, giữa hình Meme, lưới
      // Lịch, thẻ Thời tiết):
      //   chỉ đóng thanh Dock an toàn mà không mở nhầm Popup hay đổi nhầm trạng
      //   thái!
      _dockVisible = false;
      _lastRender = 0;
      bool isTopHeaderControl =
          (_currentPage == PAGE_CLOCK && y <= 92 && x >= 65) ||
          (_currentPage == PAGE_CALENDAR && y <= 50) ||
          (_currentPage == PAGE_POMODORO && y <= 40) ||
          (_currentPage == PAGE_MOOD && x >= 265 && y <= 46) ||
          (_currentPage == PAGE_SETTINGS && y <= 55);
      if (!isTopHeaderControl) {
        hardware.playTouchSound();
        return true;
      }
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
    if (isMinimalUiMode()) {
      // Minimal Zen Touch Zones (100% of features preserved!):
      // a. Top-right Bell (X >= 245, Y <= 46) -> Toggle Alarm ON/OFF
      if (x >= 245 && y <= 46) {
        hardware.playClick();
        if (!_alarmEnabled) {
          _alarmEnabled = true;
          _alarmAutoCloseTime = millis() + 3000;
          _activePopup = POPUP_ALARM;
          _zen16ScreenValid = false;
          _zen16LastPopup = (uint8_t)POPUP_ALARM;
          renderAlarmPopup();
          saveAlarmSettings();
          return true;
        } else {
          _alarmEnabled = false;
          _alarmAutoCloseTime = 0;
          _activePopup = POPUP_NONE;
          _zen16ScreenValid = false;
          saveAlarmSettings();
          _lastRender = 0;
          return true;
        }
      }
      // b. Giant Hero Clock (X < 220, Y: 36..152) -> Open Alarm Popup
      if (x < 220 && y >= 36 && y <= 152) {
        hardware.playClick();
        loadAlarmSettings();
        _alarmAutoCloseTime = 0;
        _activePopup = POPUP_ALARM;
        _zen16ScreenValid = false;
        _zen16LastPopup = (uint8_t)POPUP_ALARM;
        renderAlarmPopup();
        return true;
      }
      // c. Right Weather Cluster (X >= 220, Y: 47..152) -> Open Weather Popup
      if (x >= 220 && y >= 47 && y <= 152) {
        _activePopup = POPUP_WEATHER;
        hardware.playClick();
        _lastRender = 0;
        return true;
      }
      // d. Footer Row (Y: 156..207): Left = Jump to Pomodoro if running/paused,
      //    otherwise open Lunar Popup; Right (X >= 215) = Weather Popup
      if (y >= 156 && y <= 207) {
        if (x < 215) {
          if (_pomoState != POMO_STOPPED) {
            setPage(PAGE_POMODORO);
            hardware.playClick();
            return true;
          }
          _activePopup = POPUP_LUNAR;
        } else {
          _activePopup = POPUP_WEATHER;
        }
        hardware.playClick();
        _lastRender = 0;
        return true;
      }
    } else {
      // 5a. Chạm vào BIỂU TƯỢNG CHUÔNG (Mở rộng toàn góc trên-phải X: 230..319,
      // Y: 0..92 sát cả mép viền) -> Bật / Tắt nhanh chuông báo
      if (x >= 230 && y <= 92) {
        hardware.playClick();
        if (!_alarmEnabled) {
          // Đang TẮT -> BẬT lên và hiện popup báo giờ báo thức trong 3s để user
          // biết
          _alarmEnabled = true;
          _alarmAutoCloseTime = millis() + 3000;
          _activePopup = POPUP_ALARM;
          renderPage0_WeatherClock();
          renderAlarmPopup();
          _spr.pushSprite(0, 0);
          saveAlarmSettings();
          return true;
        } else {
          // Đang BẬT -> TẮT đi ngay lập tức (vẽ lại tức thì trước khi lưu NVS)
          _alarmEnabled = false;
          _alarmAutoCloseTime = 0;
          _activePopup = POPUP_NONE;
          renderPage0_WeatherClock();
          renderDock();
          _spr.pushSprite(0, 0);
          saveAlarmSettings();
          _lastRender = 0;
          return true;
        }
      }

      // 5b. Chạm vào VÙNG GIỮA ĐỒNG HỒ (X: 65..229, Y: 5..95) -> Mở Cài Đặt Giờ
      // Báo Thức
      if (x >= 65 && x < 230 && y >= 5 && y <= 95) {
        hardware.playClick();
        loadAlarmSettings();
        _alarmAutoCloseTime = 0; // Mở thủ công thì không tự đóng sau 3s
        _activePopup = POPUP_ALARM;
        renderPage0_WeatherClock();
        renderAlarmPopup();
        _spr.pushSprite(0, 0);
        return true;
      }

      // 5c. Chạm vào 2 Thẻ Kính Thời Tiết & Âm Lịch (Thẻ vẽ ở Y: 129..211, nhận
      // cảm ứng Y: 126..207 để cách ly sạch với viên thuốc Dock Y >= 208)
      if (y >= 126 && y <= 207) {
        if (x >= 8 && x <= 156) {
          _activePopup = POPUP_WEATHER;
          hardware.playClick();
          _lastRender = 0;
          return true;
        } else if (x >= 164 && x <= 312) {
          _activePopup = POPUP_LUNAR;
          hardware.playClick();
          _lastRender = 0;
          return true;
        }
      }
    }
  }

  // 5. If on Pomodoro page (PAGE_POMODORO), interactive controls
  if (_currentPage == PAGE_POMODORO) {
    // A. Top Mode Capsule Tabs (Mở rộng Y: 0..40 sát mép trên để bấm không bao
    // giờ trượt)
    if (y <= 40) {
      if (x >= 35 && x <= 122) {
        pomoSetMode(POMO_FOCUS);
        hardware.playClick();
        _lastRender = 0;
        return true;
      } else if (x > 122 && x <= 198) {
        pomoSetMode(POMO_SHORT_BREAK);
        hardware.playClick();
        _lastRender = 0;
        return true;
      } else if (x > 198 && x <= 285) {
        pomoSetMode(POMO_LONG_BREAK);
        hardware.playClick();
        _lastRender = 0;
        return true;
      }
    }

    if (isMinimalUiMode()) {
      // Minimal Zen Pomodoro touch zones:
      // Center countdown (Y: 46..156) -> Toggle Start / Pause
      if (y >= 46 && y <= 156) {
        if (_pomoState == POMO_RUNNING)
          pomoPause();
        else
          pomoStart();
        hardware.playClick();
        _lastRender = 0;
        return true;
      }
      // Bottom minimalist controls (visual Y: 172..206, expanded touch Y: 162..220
      // so tilted finger presses on Pause / Reset NEVER accidentally trigger Dock!)
      if (y >= 162 && y <= 220) {
        if (_pomoState == POMO_STOPPED) {
          if (x >= 80 && x <= 240) {
            pomoStart();
            hardware.playClick();
            _lastRender = 0;
            return true;
          }
        } else if (x >= 32 && x <= 154) {
          if (_pomoState == POMO_RUNNING)
            pomoPause();
          else
            pomoStart();
          hardware.playClick();
          _lastRender = 0;
          return true;
        } else if (x >= 166 && x <= 288) {
          pomoReset();
          hardware.playClick();
          _lastRender = 0;
          return true;
        }
      }
    } else {
      // B1. Chạm trực tiếp vào Pet ở tâm vòng tròn (X: 52..124, Y: 88..164) ->
      // Đổi sang Pet kế tiếp!
      if (x >= 52 && x <= 124 && y >= 88 && y <= 164) {
        deskPet.nextPet();
        hardware.playTouchSound();
        _lastRender = 0;
        return true;
      }

      // B2. Chạm vào Vòng tròn bên ngoài (X: 15..155, Y: 45..204) -> Đổi Chạy /
      // Tạm dừng
      if (x >= 15 && x <= 155 && y >= 45 && y <= 204) {
        if (_pomoState == POMO_RUNNING) {
          pomoPause();
        } else {
          pomoStart();
        }
        hardware.playClick();
        _lastRender = 0;
        return true;
      }

      // C. Cụm nút điều khiển bên phải (X: 156..315, Y: 146..204)
      // Kết thúc tại Y=204, cách ly hoàn toàn với vùng mở Dock (Y >= 208)
      if (x >= 156 && x <= 315 && y >= 146 && y <= 204) {
        if (_pomoState == POMO_STOPPED) {
          pomoStart();
          hardware.playClick();
          _lastRender = 0;
          return true;
        } else if (_pomoState == POMO_RUNNING) {
          if (x <= 234)
            pomoPause();
          else
            pomoReset();
          hardware.playClick();
          _lastRender = 0;
          return true;
        } else if (_pomoState == POMO_PAUSED) {
          if (x <= 234)
            pomoStart();
          else
            pomoReset();
          hardware.playClick();
          _lastRender = 0;
          return true;
        }
      }
    }
  }

  // 6. If on Finance page (PAGE_FINANCE):
  //    Hai thẻ Vàng & Tỷ giá trải dài tới Y=232 (dòng chữ "CHẠM XEM CHI TIẾT >"
  //    nằm ở Y=215). Dành riêng vùng viên thuốc giữa đáy (X: 105..215, Y >=
  //    208) để mở Dock, còn lại toàn bộ mặt thẻ (kể cả dòng chữ Y=215 ở 2 bên)
  //    đều mở Popup nhạy bén!
  if (_currentPage == PAGE_FINANCE) {
    if (!(y >= 208 && x >= 105 && x <= 215)) {
      if (x <= 158) {
        // Chạm Cột trái -> Mở Popup Vàng SJC & Thế Giới
        _activePopup = POPUP_FINANCE_GOLD;
        hardware.playClick();
        _lastRender = 0;
        return true;
      } else if (x >= 162) {
        // Chạm Cột phải -> Mở thẳng tab tương ứng ở chế độ Minimal Zen (cur1 / cur2 / BTC)
        _activePopup = POPUP_FINANCE_CURRENCY;
        if (isMinimalUiMode()) {
          if (y < 78)
            _currencyTab = 0;
          else if (y < 148)
            _currencyTab = 1;
          else
            _currencyTab = 2;
        } else {
          _currencyTab = 0;
        }
        hardware.playClick();
        _lastRender = 0;
        return true;
      }
    }
  }

  // 7. If on Mood page (PAGE_MOOD):
  if (_currentPage == PAGE_MOOD) {
    if (deskMood.handleTouch(x, y)) {
      _zen16ScreenValid = false;
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

  // 8. Khi Dock đang ẩn, CHỈ đánh thức Dock khi chạm đúng vào vùng Viên thuốc
  // chỉ báo ở chính giữa đáy màn hình (X: 105..215, Y >= 208)
  if (!_dockVisible) {
    if (y >= 208 && x >= 105 && x <= 215) {
      wakeDock();
      hardware.playClick();
      _lastRender = 0;
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
  _spr.drawString((getLanguage() == LANG_VI ? "HIỆU CHUẨN CẢM ỨNG (TOUCH)"
                                            : "TOUCH CALIBRATION"),
                  160, 10, 2);

  CalibState state = touch.getCalibState();
  if (state == CALIB_DONE) {
    _spr.setTextColor(TFT_GREEN, TFT_BLACK);
    _spr.setTextDatum(MC_DATUM);
    _spr.drawString((getLanguage() == LANG_VI ? "HIỆU CHUẨN THÀNH CÔNG!"
                                              : "CALIBRATION SUCCESSFUL!"),
                    160, 100, 4);
    _spr.setTextColor(TFT_WHITE, TFT_BLACK);
    _spr.drawString((getLanguage() == LANG_VI ? "Đang lưu vào bộ nhớ Flash..."
                                              : "Saving to NVS Flash..."),
                    160, 135, 2);
    return;
  }

  int tx = 160, ty = 120;
  const char *hint = "";
  const char *step = "";

  switch (state) {
  case CALIB_WAIT_TL:
    tx = CALIB_TL_X;
    ty = CALIB_TL_Y;
    hint = (getLanguage() == LANG_VI ? "Chạm tâm đỏ GÓC TRÊN - TRÁI"
                                     : "Tap target TOP - LEFT");
    step = (getLanguage() == LANG_VI ? "Bước 1/3" : "Step 1/3");
    break;
  case CALIB_WAIT_TR:
    tx = CALIB_TR_X;
    ty = CALIB_TR_Y;
    hint = (getLanguage() == LANG_VI ? "Chạm tâm đỏ GÓC TRÊN - PHẢI"
                                     : "Tap target TOP - RIGHT");
    step = (getLanguage() == LANG_VI ? "Bước 2/3" : "Step 2/3");
    break;
  case CALIB_WAIT_BL:
    tx = CALIB_BL_X;
    ty = CALIB_BL_Y;
    hint = (getLanguage() == LANG_VI ? "Chạm tâm đỏ GÓC DƯỚI - TRÁI"
                                     : "Tap target BOTTOM - LEFT");
    step = (getLanguage() == LANG_VI ? "Bước 3/3" : "Step 3/3");
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

void Display::renderWiFiSetupScreen(const char *qrText, int remainingSec) {
  if (!_sprReady)
    return;

  // 1. Clean Obsidian Dark Background (No stray shining star/sparkle dots)
  _spr.fillSprite(RGB565(7, 11, 20));

  // 2. Integrated Top Header Bar
  _spr.fillRect(0, 0, 320, 34, RGB565(12, 18, 30));
  _spr.drawFastHLine(0, 34, 320, RGB565(0, 229, 255));

  // Glowing Radar status dot on left
  _spr.fillSmoothCircle(18, 17, 5, RGB565(0, 229, 255), RGB565(12, 18, 30));
  _spr.drawCircle(18, 17, 8, RGB565(0, 100, 140));

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(TFT_WHITE, RGB565(12, 18, 30));
  _spr.drawString("CYD SMARTDESK", 34, 17, 2);

  // Modern Capsule Badge on top right
  _spr.fillSmoothRoundRect(194, 6, 114, 22, 11, RGB565(0, 229, 255),
                           RGB565(12, 18, 30));
  _spr.fillSmoothRoundRect(195, 7, 112, 20, 10, RGB565(18, 30, 48),
                           RGB565(0, 229, 255));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(0, 229, 255), RGB565(18, 30, 48));
  _spr.drawString((getLanguage() == LANG_EN) ? "WIFI SETUP" : "CÀI ĐẶT WIFI",
                  251, 17, 2);

  // 3. Left Hero: Premium Optical QR Showcase Tile (High contrast, 116px
  // enlarged) Card dimensions: X=14, Y=44, W=126, H=126, R=10
  int16_t cardX = 14;
  int16_t cardY = 44;
  int16_t cardW = 126;
  int16_t cardH = 126;

  // Outer soft cyan accent border
  _spr.fillSmoothRoundRect(cardX - 1, cardY - 1, cardW + 2, cardH + 2, 11,
                           RGB565(0, 180, 230), RGB565(7, 11, 20));
  _spr.fillSmoothRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE,
                           RGB565(0, 180, 230));

  // Generate & draw QR code (Version 3: 29x29 modules, 4px per module =
  // 116x116px)
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW,
                  qrText ? qrText : "WIFI:S:CYD-SmartDesk-V2;T:nopass;;");

  const uint8_t modSize = 4;
  int16_t qrPx = qrcode.size * modSize;          // 29 * 4 = 116
  int16_t qrStartX = cardX + (cardW - qrPx) / 2; // 14 + 5 = 19
  int16_t qrStartY = cardY + (cardH - qrPx) / 2; // 44 + 5 = 49

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        _spr.fillRect(qrStartX + x * modSize, qrStartY + y * modSize, modSize,
                      modSize, TFT_BLACK);
      }
    }
  }

  // Camera icon hint under QR — shifted down to Y=181 for clear breathing room
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(130, 160, 200), RGB565(7, 11, 20));
  _spr.drawString((getLanguage() == LANG_EN) ? "Scan with Camera"
                                             : "Quét bằng Camera",
                  77, 181, 1);

  // 4. Right Side: Card & Quick Pairing Guide (Bordered card containing all
  // text neatly)
  int16_t rightCardX = 150;
  int16_t rightCardY = 44;
  int16_t rightCardW = 158;
  int16_t rightCardH = 136;

  _spr.fillSmoothRoundRect(rightCardX, rightCardY, rightCardW, rightCardH, 8,
                           RGB565(36, 54, 80), RGB565(7, 11, 20));
  _spr.fillSmoothRoundRect(rightCardX + 1, rightCardY + 1, rightCardW - 2,
                           rightCardH - 2, 7, RGB565(14, 20, 32),
                           RGB565(36, 54, 80));

  // Card Title
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(RGB565(0, 229, 255), RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "QUICK PAIRING"
                                             : "KẾT NỐI NHANH",
                  rightCardX + 10, rightCardY + 7, 2);

  _spr.drawFastHLine(rightCardX + 8, rightCardY + 26, rightCardW - 16,
                     RGB565(25, 38, 56));

  // Step 1
  _spr.fillSmoothCircle(rightCardX + 16, rightCardY + 39, 7,
                        RGB565(0, 229, 255), RGB565(14, 20, 32));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(10, 16, 26), RGB565(0, 229, 255));
  _spr.drawString("1", rightCardX + 16, rightCardY + 39, 1);

  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(TFT_WHITE, RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "Open phone camera"
                                             : "Mở Camera quét QR",
                  rightCardX + 28, rightCardY + 30, 1);
  _spr.setTextColor(RGB565(120, 145, 175), RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "Tap popup to join"
                                             : "Chạm thông báo vào mạng",
                  rightCardX + 28, rightCardY + 42, 1);

  // Step 2
  _spr.fillSmoothCircle(rightCardX + 16, rightCardY + 63, 7,
                        RGB565(0, 229, 255), RGB565(14, 20, 32));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(10, 16, 26), RGB565(0, 229, 255));
  _spr.drawString("2", rightCardX + 16, rightCardY + 63, 1);

  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(TFT_WHITE, RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "Select WiFi & save"
                                             : "Chọn WiFi phòng & lưu",
                  rightCardX + 28, rightCardY + 54, 1);
  _spr.setTextColor(RGB565(120, 145, 175), RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "Config page opens auto"
                                             : "Trang web tự động mở",
                  rightCardX + 28, rightCardY + 66, 1);

  // Manual Hotspot Info Pill inside card
  int16_t pillX = rightCardX + 8;
  int16_t pillY = rightCardY + 80;
  int16_t pillW = rightCardW - 16;
  int16_t pillH = 48;
  _spr.fillSmoothRoundRect(pillX, pillY, pillW, pillH, 5, RGB565(28, 42, 64),
                           RGB565(14, 20, 32));
  _spr.fillSmoothRoundRect(pillX + 1, pillY + 1, pillW - 2, pillH - 2, 4,
                           RGB565(9, 14, 23), RGB565(28, 42, 64));

  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(RGB565(0, 229, 255), RGB565(9, 14, 23));
  _spr.drawString("WiFi: CYD-SmartDesk-V2", pillX + 6, pillY + 4, 1);
  _spr.setTextColor(RGB565(160, 185, 215), RGB565(9, 14, 23));
  _spr.drawString((getLanguage() == LANG_EN) ? "Pass: None (Open)"
                                             : "Mật khẩu: Trống (Mở)",
                  pillX + 6, pillY + 17, 1);
  _spr.setTextColor(RGB565(100, 130, 165), RGB565(9, 14, 23));
  _spr.drawString("IP: 192.168.4.1", pillX + 6, pillY + 30, 1);

  // 5. Bottom Touch Action Bar
  // Tactile Skip Button on Left (X: 14, Y: 190, W: 126, H: 38, R=8)
  _spr.fillSmoothRoundRect(14, 190, 126, 38, 8, RGB565(52, 74, 108),
                           RGB565(7, 11, 20));
  _spr.fillSmoothRoundRect(15, 191, 124, 36, 7, RGB565(22, 30, 46),
                           RGB565(52, 74, 108));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(190, 215, 245), RGB565(22, 30, 46));
  _spr.drawString((getLanguage() == LANG_EN) ? "X  Skip (Offline)"
                                             : "X  Dùng Offline",
                  77, 209, 2);

  // Right Side: Live status indicator card (X: 150, Y: 190, W: 158, H: 38, R=8)
  _spr.fillSmoothRoundRect(150, 190, 158, 38, 8, RGB565(36, 54, 80),
                           RGB565(7, 11, 20));
  _spr.fillSmoothRoundRect(151, 191, 156, 36, 7, RGB565(14, 20, 32),
                           RGB565(36, 54, 80));

  _spr.fillSmoothCircle(162, 209, 3, RGB565(0, 229, 255), RGB565(14, 20, 32));
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(RGB565(0, 229, 255), RGB565(14, 20, 32));
  _spr.drawString((getLanguage() == LANG_EN) ? "Waiting for device..."
                                             : "Đang chờ kết nối...",
                  172, 201, 1);

  char timeoutStr[40];
  snprintf(timeoutStr, sizeof(timeoutStr),
           (getLanguage() == LANG_EN) ? "Auto exit in %ds"
                                      : "Tự động thoát sau %ds",
           remainingSec);
  _spr.setTextColor(RGB565(120, 145, 175), RGB565(14, 20, 32));
  _spr.drawString(timeoutStr, 172, 217, 1);

  _spr.pushSprite(0, 0);
}

void Display::renderWiFiConnectedScreen(const char *ssid, const char *ip) {
  if (!_sprReady)
    return;

  drawBackground();

  // Top integrated status bar
  _spr.fillRect(0, 0, 320, 34, RGB565(12, 18, 30));
  _spr.drawFastHLine(0, 34, 320, RGB565(0, 230, 118));

  _spr.fillSmoothCircle(18, 17, 5, RGB565(0, 230, 118), RGB565(12, 18, 30));
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(TFT_WHITE, RGB565(12, 18, 30));
  _spr.drawString("CYD SMARTDESK", 34, 17, 2);

  _spr.fillSmoothRoundRect(194, 6, 114, 22, 11, RGB565(0, 230, 118),
                           RGB565(12, 18, 30));
  _spr.fillSmoothRoundRect(195, 7, 112, 20, 10, RGB565(18, 38, 30),
                           RGB565(0, 230, 118));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(0, 230, 118), RGB565(18, 38, 30));
  _spr.drawString("CONNECTED", 251, 17, 2);

  // Centered Hero Success Card
  _spr.fillSmoothRoundRect(24, 46, 272, 176, 14, RGB565(0, 230, 118), theme.bg);
  _spr.fillSmoothRoundRect(25, 47, 270, 174, 13, RGB565(14, 22, 34),
                           RGB565(0, 230, 118));

  // Green Glowing Success Badge
  _spr.fillSmoothCircle(160, 84, 24, RGB565(0, 200, 100), RGB565(14, 22, 34));
  _spr.drawSmoothCircle(160, 84, 27, RGB565(100, 255, 180), RGB565(14, 22, 34));
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(TFT_WHITE, RGB565(0, 200, 100));
  _spr.drawString("OK", 160, 84, 4);

  _spr.setTextColor(RGB565(0, 230, 118), RGB565(14, 22, 34));
  _spr.drawString((getLanguage() == LANG_EN) ? "WIFI CONNECTED!"
                                             : "ĐÃ KẾT NỐI WIFI!",
                  160, 126, 4);

  char buf[64];
  _spr.setTextColor(TFT_WHITE, RGB565(14, 22, 34));
  snprintf(buf, sizeof(buf), "SSID: %s", ssid ? ssid : "Connected");
  _spr.drawString(buf, 160, 156, 2);

  _spr.setTextColor(RGB565(0, 229, 255), RGB565(14, 22, 34));
  snprintf(buf, sizeof(buf), "IP: %s", ip ? ip : "192.168.x.x");
  _spr.drawString(buf, 160, 184, 2);

  _spr.pushSprite(0, 0);
}

void Display::renderDock() {
  bool zenDock = isMinimalUiMode();
  if (!_dockVisible) {
    // Viên thuốc chỉ báo Dock ở chính giữa sát mép dưới (X: 134..186, Y: 236,
    // W=52, H=3px)
    _spr.fillRoundRect(134, 236, 52, 3, 1,
                       zenDock ? RGB565(96, 102, 114) : theme.accent);
    return;
  }

  // Centered Floating Solid Obsidian / Apple Graphite Dock Capsule
  int16_t dockX = 22;
  int16_t dockY = 196;
  int16_t dockW = 276;
  int16_t dockH = 36;
  int16_t dockR = 18;

  uint16_t shadowCol = zenDock ? C_BLACK : RGB565(2, 4, 8);
  uint16_t borderCol = zenDock ? RGB565(52, 56, 66) : RGB565(40, 58, 86);
  uint16_t fillCol = zenDock ? RGB565(18, 20, 24) : RGB565(12, 16, 26);
  uint16_t specCol = zenDock ? RGB565(96, 102, 116) : RGB565(140, 190, 245);
  uint16_t divCol = zenDock ? RGB565(48, 52, 62) : RGB565(40, 56, 80);
  uint16_t actBorder = zenDock ? RGB565(190, 198, 210) : RGB565(0, 230, 118);
  uint16_t actFill = zenDock ? RGB565(34, 38, 46) : RGB565(18, 24, 36);

  // 1. Soft outer shadow + Crisp anti-aliased outer border + Deep Base
  _spr.fillSmoothRoundRect(dockX - 1, dockY + 1, dockW + 2, dockH + 2,
                           dockR + 1, shadowCol);
  _spr.fillSmoothRoundRect(dockX, dockY, dockW, dockH, dockR, borderCol);
  _spr.fillSmoothRoundRect(dockX + 1, dockY + 1, dockW - 2, dockH - 2,
                           dockR - 1, fillCol, borderCol);

  // Subtle specular highlight on top edge
  _spr.drawFastHLine(dockX + dockR, dockY, dockW - dockR * 2, specCol);

  // 6 Navigation Icon Slots (spaced 36px apart: 40, 76, 112, 148, 184, 220)
  for (int i = 0; i < 6; i++) {
    int16_t cx = dockX + 18 + i * 36;
    int16_t cy = dockY + dockH / 2;
    bool isActive = (_currentPage == i);

    if (isActive) {
      _spr.fillSmoothRoundRect(cx - 15, dockY + 4, 30, 28, 9, actBorder,
                               fillCol);
      _spr.fillSmoothRoundRect(cx - 14, dockY + 5, 28, 26, 8, actFill,
                               actBorder);
    }

    switch (i) {
    case 0:
      drawDockIconHome(cx, cy, isActive);
      break;
    case 1:
      drawDockIconCalendar(cx, cy, isActive);
      break;
    case 2:
      drawDockIconFinance(cx, cy, isActive);
      break;
    case 3:
      drawDockIconPomodoro(cx, cy, isActive);
      break;
    case 4:
      drawDockIconMood(cx, cy, isActive);
      break;
    case 5:
      drawDockIconTheme(cx, cy, isActive);
      break;
    }
  }

  // Hairline divider separating Navigation from System Power Button
  _spr.drawFastVLine(241, dockY + 7, 22, divCol);

  // 7th Slot: Red Power Button (⏻)
  int16_t powerCx = 269;
  int16_t powerCy = dockY + dockH / 2;
  drawDockIconPower(powerCx, powerCy);
}

// ─── Professional 20x20 Color Glyph Renderer (Vibrant & Crisp) ────────
static void drawDockBitmap20(TFT_eSprite &spr, int16_t cx, int16_t cy,
                             const uint16_t *bmp) {
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

void Display::drawDockIconPower(int16_t cx, int16_t cy) {
  // 1. Sleek crimson accent pill background (anti-aliased SDF corners)
  _spr.fillSmoothRoundRect(cx - 15, cy - 14, 30, 28, 9, RGB565(140, 28, 42));
  _spr.fillSmoothRoundRect(cx - 14, cy - 13, 28, 26, 8, RGB565(36, 10, 16));

  // 2. Neon Crimson Power Icon (⏻)
  _spr.fillSmoothCircle(cx, cy + 1, 7, RGB565(255, 65, 80));
  _spr.fillSmoothCircle(cx, cy + 1, 5, RGB565(36, 10, 16));

  // Clear top opening of ring
  _spr.fillRect(cx - 3, cy - 7, 7, 5, RGB565(36, 10, 16));

  // Crisp vertical tick through center with glowing white core
  _spr.drawFastVLine(cx - 1, cy - 6, 7, RGB565(255, 120, 130));
  _spr.drawFastVLine(cx, cy - 7, 8, TFT_WHITE);
  _spr.drawFastVLine(cx + 1, cy - 6, 7, RGB565(255, 120, 130));
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 1 — LUNAR MONTH CALENDAR (Interactive Month Navigation)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage1_Calendar() {
  if (isMinimalUiMode()) {
    renderPage1_CalendarZen();
    return;
  }
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
  _spr.fillSmoothRoundRect(x3, y3, w3, calH, calR, RGB565(28, 40, 58));
  _spr.fillSmoothRoundRect(x3 + 1, y3 + 1, w3 - 2, calH - 2, calR - 1,
                           RGB565(8, 12, 20));

  // Tờ thứ 2 (Ở giữa): Nhô ra 2px bên trái, 3px ở đáy, mép phải đồng phẳng
  int16_t x2 = calX - 2, y2 = calY + 3, w2 = calRight - x2;
  _spr.fillSmoothRoundRect(x2, y2, w2, calH, calR, RGB565(36, 52, 76));
  _spr.fillSmoothRoundRect(x2 + 1, y2 + 1, w2 - 2, calH - 2, calR - 1,
                           RGB565(13, 17, 28));

  // Tờ thứ 1 (Trang trên cùng): Obsidian Paper sâu thẳm
  _spr.fillSmoothRoundRect(calX, calY, calW, calH, calR, RGB565(48, 68, 102));
  _spr.fillSmoothRoundRect(calX + 1, calY + 1, calW - 2, calH - 2, calR - 1,
                           RGB565(17, 22, 35));

  // Gáy sổ lịch bọc da Đỏ Đô (nằm gọn bên trong viền 1px, kéo dài xuống Y=41)
  _spr.fillSmoothRoundRect(calX + 1, calY + 1, calW - 2, 33, calR - 1,
                           RGB565(120, 22, 36));
  _spr.fillRect(
      calX + 1, calY + 18, calW - 2, 16,
      RGB565(120, 22, 36)); // Phẳng mép dưới tại Y=41 (chia đôi gáy lò xo)

  // 6 Khoen lò xo kim loại (Spiral Rings) — Có khoảng thở trên/dưới rõ ràng
  for (int k = 0; k < 6; k++) {
    int16_t kx = 64 + k * 38;
    // Lỗ đục trên giấy
    _spr.fillSmoothCircle(kx + 2, 42, 3, RGB565(8, 10, 16));
    // Khoen kim loại bạc (H=10px, từ Y=37 đến Y=46)
    _spr.fillSmoothRoundRect(kx, 37, 4, 10, 2, RGB565(180, 195, 215));
    // Ánh sáng specular highlight trắng
    _spr.drawFastVLine(kx + 1, 38, 8, C_WHITE);
  }

  // ── 3. NÚT ĐIỀU HƯỚNG & TIÊU ĐỀ TRÊN GÁY LỊCH (Y=10..34) ──────────
  // Nút [ < ] Prev Month: Tâm (30, 23), Mũi tên tam giác có trọng tâm chính xác
  // (30.0, 23.0)
  _spr.fillSmoothCircle(30, 23, 11, RGB565(170, 45, 65));
  _spr.fillSmoothCircle(30, 23, 10, RGB565(80, 15, 26));
  _spr.fillTriangle(26, 23, 32, 18, 32, 28, RGB565(255, 225, 235));

  // Nút [ > ] Next Month: Tâm (290, 23), Mũi tên tam giác có trọng tâm chính
  // xác (290.0, 23.0)
  _spr.fillSmoothCircle(290, 23, 11, RGB565(170, 45, 65));
  _spr.fillSmoothCircle(290, 23, 10, RGB565(80, 15, 26));
  _spr.fillTriangle(294, 23, 288, 18, 288, 28, RGB565(255, 225, 235));

  // Tiêu đề Tháng • Năm (Be Vietnam Pro Regular nét đều thanh thoát)
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(RGB565(255, 245, 220));
  char titleBuf[40];
  snprintf(
      titleBuf, sizeof(titleBuf),
      (getLanguage() == LANG_VI ? "THÁNG %02d  •  %d" : "MONTH %02d  •  %d"),
      _calMonth, _calYear);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, titleBuf, 160, 17);

  // Dòng phụ: Thông tin Âm lịch chi tiết hôm nay (cách đỉnh gáy lò xo 4px)
  bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);
  char subBuf[80];
  if (isCurrentMonth) {
    LunarDate todayLunar =
        LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
    snprintf(subBuf, sizeof(subBuf),
             (getLanguage() == LANG_VI ? "Hôm nay: %02d/%02d ÂL  -  Năm %s"
                                       : "Today: %02d/%02d Lunar  -  Year %s"),
             todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
    _spr.setTextColor(RGB565(245, 215, 150)); // Gold dịu
  } else {
    LunarDate midLunar =
        LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
    snprintf(subBuf, sizeof(subBuf),
             (getLanguage() == LANG_VI ? "Năm %s  •  [Chạm về hôm nay]"
                                       : "Year %s  •  [Tap for today]"),
             midLunar.yearCanChi);
    _spr.setTextColor(RGB565(255, 200, 100));
  }
  _spr.drawString(subBuf, 160, 29, 1);

  // ── 4. THANH THỨ (T2 to CN, cách đáy gáy lò xo 4px) ───────────────
  const char *daysOfWeekVI[] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
  const char *daysOfWeekEN[] = {"MON", "TUE", "WED", "THU",
                                "FRI", "SAT", "SUN"};
  const char *const *daysOfWeek =
      (getLanguage() == LANG_VI) ? daysOfWeekVI : daysOfWeekEN;
  for (int col = 0; col < 7; col++) {
    int16_t hx = 34 + col * 42;
    if (col == 6)
      _spr.setTextColor(RGB565(255, 110, 110)); // CN (Sunday) Đỏ San Hô
    else if (col == 5)
      _spr.setTextColor(RGB565(130, 215, 255)); // T7 Cyan nhạt
    else
      _spr.setTextColor(RGB565(140, 170, 205)); // Ngày trong tuần màu Slate
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, daysOfWeek[col], hx, 58);
  }
  // Đường chỉ mỏng tinh tế ngăn cách thanh thứ
  _spr.drawFastHLine(24, 69, 272, RGB565(32, 46, 68));

  // ── 5. CALENDAR MATRIX CALCULATION ────────────────────────────────
  int daysInMonth = 31;
  if (_calMonth == 4 || _calMonth == 6 || _calMonth == 9 || _calMonth == 11) {
    daysInMonth = 30;
  } else if (_calMonth == 2) {
    bool isLeapYear =
        (_calYear % 4 == 0 && _calYear % 100 != 0) || (_calYear % 400 == 0);
    daysInMonth = isLeapYear ? 29 : 28;
  }

  static const int tMap[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int yCalc = _calYear;
  if (_calMonth < 3)
    yCalc -= 1;
  int wday1 = (yCalc + yCalc / 4 - yCalc / 100 + yCalc / 400 +
               tMap[_calMonth - 1] + 1) %
              7;
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
    if (ld.day == 1)
      snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
    else
      snprintf(lStr, sizeof(lStr), "%d", ld.day);

    // Đo đạc kích thước chính xác theo pixel của từng con số (Be Vietnam Pro
    // nét mảnh thanh thoát!)
    char sBuf[8];
    snprintf(sBuf, sizeof(sBuf), "%d", d);
    int16_t sW = _spr.measureSmoothFace(&ZEN_FONT_LIGHT, sBuf);
    int16_t lW = _spr.textWidth(lStr, 1);
    int16_t gap = 2;
    int16_t totalW = sW + gap + lW;

    // Căn giữa tuyệt đối cụm (Dương lịch + Âm lịch) quanh trục tâm cx
    int16_t startX = cx - totalW / 2;
    int16_t solarX = startX;
    int16_t solarY = cy - 9;
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
      _spr.fillSmoothRoundRect(boxX, boxY, boxW, boxH, cornerR, theme.green,
                               RGB565(17, 22, 35));

      // Số Dương Lịch (Be Vietnam Pro Regular nét đều)
      _spr.setTextDatum(TL_DATUM);
      _spr.setTextColor(RGB565(12, 24, 18));
      _spr.drawSmoothFace(&ZEN_FONT_LIGHT, sBuf, solarX, solarY);

      // Số Âm Lịch (Font 1, TL_DATUM — góc dưới phải)
      _spr.setTextColor(RGB565(18, 48, 28));
      _spr.drawString(lStr, lunarX, lunarY, 1);
    } else {
      // Ngày thường
      _spr.setTextDatum(TL_DATUM);
      if (col == 6)
        _spr.setTextColor(RGB565(255, 120, 120)); // CN Đỏ San Hô
      else if (col == 5)
        _spr.setTextColor(RGB565(140, 220, 255)); // T7 Cyan nhạt
      else
        _spr.setTextColor(C_WHITE); // Ngày trong tuần trắng sáng
      _spr.drawSmoothFace(&ZEN_FONT_LIGHT, sBuf, solarX, solarY);

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

void Display::drawFinanceBadge(int16_t x, int16_t y, int16_t w, int16_t h,
                               const char *text, bool isPositive) {
  if (h % 2 != 0)
    h++; // Đảm bảo chiều cao luôn chẵn để bán kính r = h/2 tạo thành hình oval
         // tròn hoàn hảo
  int16_t textW = _spr.textWidth(text, 1);
  int16_t badgeW =
      textW + 8; // Tự động co giãn theo độ dài text chống tràn tuyệt đối
  if (badgeW < w)
    badgeW = w;
  int16_t r = h / 2;
  uint16_t bgTint = isPositive ? RGB565(15, 45, 25) : RGB565(50, 15, 20);
  uint16_t fgCol = isPositive ? theme.green : RGB565(255, 110, 110);
  _spr.fillSmoothRoundRect(x, y, badgeW, h, r, fgCol, theme.bg);
  _spr.fillSmoothRoundRect(x + 1, y + 1, badgeW - 2, h - 2, r - 1, bgTint,
                           fgCol);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(fgCol);
  _spr.drawString(text, x + badgeW / 2, y + h / 2, 1);
  _spr.setTextDatum(
      TL_DATUM); // Bảo toàn Datum mặc định cho các hàm vẽ tiếp theo
}

// ── Biểu đồ cột thanh mảnh (Micro Bar Chart) biểu thị biến động tỷ giá các
// ngày trong tuần ──
void Display::drawFinanceMicroBars(int16_t x, int16_t y, int16_t w, int16_t h,
                                   const float *deltas, uint8_t count) {
  if (!deltas || count == 0)
    return;
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
      if (bh < 3)
        bh = 3;
      if (bh > h / 2 - 1)
        bh = h / 2 - 1;
      _spr.fillRoundRect(bx, baseY - bh, barW, bh + 1, 1, C_GREEN);
    } else {
      int16_t bh = (int16_t)(-d * 40.0f);
      if (bh < 3)
        bh = 3;
      if (bh > h / 2 - 1)
        bh = h / 2 - 1;
      _spr.fillRoundRect(bx, baseY, barW, bh + 1, 1, RGB565(255, 95, 95));
    }
  }
}

// ── Biểu đồ Sparkline Vector cao cấp (Anti-Aliased Wedge Line + Area Gradient
// Fill) ──
void Display::drawFinanceSparkline(int16_t x, int16_t y, int16_t w, int16_t h,
                                   const float *pts, uint8_t count,
                                   uint16_t color, bool areaFill) {
  if (!pts || count < 2)
    return;

  float minVal = pts[0];
  float maxVal = pts[0];
  for (uint8_t i = 1; i < count; i++) {
    if (pts[i] < minVal)
      minVal = pts[i];
    if (pts[i] > maxVal)
      maxVal = pts[i];
  }
  float range = maxVal - minVal;
  if (range < 0.0001f)
    range = 1.0f;

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

  // Đổ bóng vùng mờ (Area Gradient Fill) phía dưới đường biểu đồ theo phong
  // cách Apple Stocks
  if (areaFill) {
    int16_t botY = y + h - 3;
    for (uint8_t i = 0; i < n - 1; i++) {
      int16_t x0 = px[i], x1 = px[i + 1];
      int16_t y0 = py[i], y1 = py[i + 1];
      int16_t dx = x1 - x0;
      if (dx <= 0)
        continue;
      for (int16_t cx = x0; cx <= x1; cx++) {
        if (cx == x0 && i > 0)
          continue; // Tránh tô đúp cột giao nhau
        int16_t cyTop = y0 + (int16_t)((int32_t)(y1 - y0) * (cx - x0) / dx);
        int16_t span = botY - cyTop;
        if (span <= 1)
          continue;
        for (int16_t cy = cyTop + 1; cy < botY; cy++) {
          uint8_t alpha = (uint8_t)((int32_t)(botY - cy) * 72 / h);
          if (alpha > 4) {
            uint16_t bg = _spr.readPixel(cx, cy);
            _spr.drawPixel(cx, cy, _spr.alphaBlend(alpha, color, bg));
          }
        }
      }
    }
  }

  // Vẽ đường biểu đồ vector khử răng cưa mượt mà (drawWedgeLine)
  for (uint8_t i = 0; i < n - 1; i++) {
    _spr.drawWedgeLine(px[i], py[i], px[i + 1], py[i + 1], 1.15f, 1.15f, color);
    // Chấm tròn nhỏ tại các mốc dữ liệu
    _spr.fillSmoothCircle(px[i], py[i], 1, blendColor(color, C_WHITE, 180));
  }

  // Chốt điểm dữ liệu cuối cùng: chấm tròn phát sáng tinh tế
  int16_t lastX = px[n - 1];
  int16_t lastY = py[n - 1];
  _spr.fillSmoothCircle(lastX, lastY, 2, color);
  _spr.drawPixel(lastX, lastY, C_WHITE);
}

void Display::renderPage2_Finance() {
  if (isMinimalUiMode()) {
    renderPage2_FinanceZen();
    return;
  }
  drawBackground();
  const FinanceData &fd = finance.getData();

  // ═══════════════════════════════════════════════════════════════════
  //  CỘT TRÁI: VÀNG SJC 9999 & THẾ GIỚI (X = 8, W = 148, Y = 8, H = 224)
  // ═══════════════════════════════════════════════════════════════════
  int16_t leftX = 8;
  int16_t cardW = 148;
  int16_t cardH = 224;
  drawGlassCard(leftX, 8, cardW, cardH,
                10); // Viền kính đồng nhất, không lệch màu đáy

  // Header Vàng
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.yellow);
  _spr.drawString((getLanguage() == LANG_VI ? "VÀNG SJC" : "SJC GOLD"),
                  leftX + 10, 14, 2);

  char sjcBadge[16];
  bool sjcPos = (fd.sjcChangePercent >= 0);
  snprintf(sjcBadge, sizeof(sjcBadge), "%+.1f%%", fd.sjcChangePercent);
  toVNFloat(sjcBadge);
  drawFinanceBadge(leftX + cardW - 46, 14, 38, 14, sjcBadge, sjcPos);

  // Mua & Bán SJC — Bố cục 2 hàng đối xứng, chống tràn tuyệt đối cho số 5 chữ
  // số (140,3 tr)
  char buyStr[16], sellStr[16];
  snprintf(buyStr, sizeof(buyStr), "%.1f", fd.sjcBuy);
  toVNFloat(buyStr);
  snprintf(sellStr, sizeof(sellStr), "%.1f", fd.sjcSell);
  toVNFloat(sellStr);

  // Hàng 1: MUA
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString((getLanguage() == LANG_VI ? "MUA" : "BUY"), leftX + 10, 37,
                  1);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(buyStr, leftX + 42, 33, 4);
  _spr.setTextColor(theme.textVDim);
  _spr.drawString((getLanguage() == LANG_VI ? "tr" : "M"),
                  leftX + 42 + _spr.textWidth(buyStr, 4) + 2, 43, 1);

  // Hàng 2: BÁN
  _spr.setTextColor(theme.textDim);
  _spr.drawString((getLanguage() == LANG_VI ? "BÁN" : "SELL"), leftX + 10, 58,
                  1);
  _spr.setTextColor(theme.yellow);
  _spr.drawString(sellStr, leftX + 42, 54, 4);
  _spr.setTextColor(theme.textVDim);
  _spr.drawString((getLanguage() == LANG_VI ? "tr" : "M"),
                  leftX + 42 + _spr.textWidth(sellStr, 4) + 2, 64, 1);

  // Đường phân cách mờ 1 (đồng bộ thẳng hàng với cột tỉ giá tại Y = 84)
  _spr.drawFastHLine(leftX + 8, 84, cardW - 16,
                     blendColor(theme.cardBorder, theme.bg, 110));

  // Giá Vàng Thế Giới (Kitco)
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString(
      (getLanguage() == LANG_VI ? "THẾ GIỚI (KITCO)" : "WORLD (KITCO)"),
      leftX + 10, 90, 1);

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
  _spr.drawFastHLine(leftX + 8, 140, cardW - 16,
                     blendColor(theme.cardBorder, theme.bg, 110));

  // Biểu đồ 7 ngày Vàng SJC (Vector sắc nét + Area Gradient)
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.yellow);
  _spr.drawString(
      (getLanguage() == LANG_VI ? "XU HƯỚNG 7 NGÀY" : "7-DAY TREND"),
      leftX + 10, 144, 1);

  drawFinanceSparkline(leftX + 10, 156, cardW - 20, 36, fd.sjc7d, 7, C_GOLD,
                       true);

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
    _spr.drawString(getMiniDayNameI18n(w), dx, 196, 1);
  }

  // Call-to-action chân thẻ
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(blendColor(theme.textDim, C_WHITE, 120));
  _spr.drawString(
      (getLanguage() == LANG_VI ? "CHẠM XEM CHI TIẾT >" : "TAP FOR DETAILS >"),
      leftX + cardW / 2, 215, 1);

  // ═══════════════════════════════════════════════════════════════════
  //  CỘT PHẢI: TỶ GIÁ NGOẠI TỆ & VĨ MÔ (X = 164, W = 148, Y = 8, H = 224)
  // ═══════════════════════════════════════════════════════════════════
  int16_t rightX = 164;
  drawGlassCard(rightX, 8, cardW, cardH,
                10); // Viền kính đồng nhất, không lệch màu đáy

  // Header Tỷ giá
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString((getLanguage() == LANG_VI ? "TỶ GIÁ (VND)" : "RATES (VND)"),
                  rightX + 10, 14, 2);
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
  FinanceService::formatVND((long)roundf(fd.cur1Rate), cur1RateStr,
                            sizeof(cur1RateStr));
  _spr.setTextDatum(TL_DATUM); // Đảm bảo căn trái tuyệt đối
  _spr.setTextColor(C_WHITE);
  _spr.drawString(cur1RateStr, rightX + 10, 55, 4);

  // Biểu đồ cột thanh mảnh 5 ngày (+/-)
  drawFinanceMicroBars(rightX + 96, 55, 40, 22, fd.cur1Bars, 5);

  // Đường phân cách mờ 1
  _spr.drawFastHLine(rightX + 8, 84, cardW - 16,
                     blendColor(theme.cardBorder, theme.bg, 110));

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
  FinanceService::formatVND((long)roundf(fd.cur2Rate), cur2RateStr,
                            sizeof(cur2RateStr));
  _spr.setTextDatum(TL_DATUM); // Đảm bảo căn trái tuyệt đối
  _spr.setTextColor(C_WHITE);
  _spr.drawString(cur2RateStr, rightX + 10, 111, 4);

  // Biểu đồ cột thanh mảnh 5 ngày (+/-)
  drawFinanceMicroBars(rightX + 96, 111, 40, 22, fd.cur2Bars, 5);

  // Đường phân cách mờ 2
  _spr.drawFastHLine(rightX + 8, 140, cardW - 16,
                     blendColor(theme.cardBorder, theme.bg, 110));

  // Mục 3: Macro / BTC
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString("BTC / USDT", rightX + 10, 147, 1);

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
  _spr.drawString(
      (getLanguage() == LANG_VI ? "CHẠM XEM CHI TIẾT >" : "TAP FOR DETAILS >"),
      rightX + cardW / 2, 215, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 2 (MINIMAL ZEN) — Signature Apple Stocks Minimalist Finance
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage2_FinanceZen() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(112, 118, 128);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenLine = RGB565(36, 40, 46);
  const uint16_t zenUp = RGB565(52, 211, 153);
  const uint16_t zenDown = RGB565(248, 113, 113);

  _spr.fillSprite(zenBg);
  const FinanceData &fd = finance.getData();
  bool isVI = (getLanguage() == LANG_VI);

  // Subtle vertical hairline divider
  _spr.drawFastVLine(160, 16, 192, zenLine);

  // ── LEFT COLUMN: SJC GOLD + KITCO + 7D SPARKLINE (X = 16..148) ────
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenGold);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, isVI ? "VÀNG SJC" : "SJC GOLD", 16, 14);

  char sjcPct[16];
  snprintf(sjcPct, sizeof(sjcPct), "%+.1f%%", fd.sjcChangePercent);
  toVNFloat(sjcPct);
  _spr.setTextDatum(TR_DATUM);
  _spr.setTextColor(fd.sjcChangePercent >= 0 ? zenUp : zenDown);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, sjcPct, 148, 14);

  // Hero Sell price (silky-smooth Light 32px)
  char sellStr[16], buyStr[24];
  snprintf(sellStr, sizeof(sellStr), "%.1f", fd.sjcSell);
  toVNFloat(sellStr);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_NUM, sellStr, 16, 40);
  int16_t sellW = _spr.measureSmoothFace(&ZEN_FONT_NUM, sellStr);
  _spr.setTextColor(zenMuted);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, isVI ? "tr" : "M", 16 + sellW + 4, 58);

  // Subtle Buy price below
  char buyVal[16];
  snprintf(buyVal, sizeof(buyVal), "%.1f", fd.sjcBuy);
  toVNFloat(buyVal);
  snprintf(buyStr, sizeof(buyStr), "%s %s %s", isVI ? "MUA" : "BUY", buyVal,
           isVI ? "tr" : "M");
  _spr.setTextColor(zenMuted);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, buyStr, 16, 88);

  _spr.drawFastHLine(16, 112, 132, zenLine);

  // World Gold (Kitco)
  char kNum[16], kitcoStr[28];
  FinanceService::formatVND((long)roundf(fd.worldGoldUsd), kNum, sizeof(kNum));
  snprintf(kitcoStr, sizeof(kitcoStr), "KITCO  $%s", kNum);
  _spr.setTextColor(zenMuted);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, kitcoStr, 16, 120);

  // Clean 7-day Gold Sparkline (Y = 148..194)
  drawFinanceSparkline(16, 148, 132, 46, fd.sjc7d, 7, zenGold, false);

  // ── RIGHT COLUMN: FX RATES & BTC (X = 174..306) ───────────────────
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenAccent);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, fd.cur1Code, 174, 14);

  char c1Pct[16];
  snprintf(c1Pct, sizeof(c1Pct), "%+.2f%%", fd.cur1ChangePercent);
  toVNFloat(c1Pct);
  _spr.setTextDatum(TR_DATUM);
  _spr.setTextColor(fd.cur1ChangePercent >= 0 ? zenUp : zenDown);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, c1Pct, 306, 14);

  char c1Rate[16];
  FinanceService::formatVND((long)roundf(fd.cur1Rate), c1Rate, sizeof(c1Rate));
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_NUM, c1Rate, 174, 34);

  _spr.drawFastHLine(174, 78, 132, zenLine);

  // Currency 2 (EUR / custom)
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenAccent);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, fd.cur2Code, 174, 84);

  char c2Pct[16];
  snprintf(c2Pct, sizeof(c2Pct), "%+.2f%%", fd.cur2ChangePercent);
  toVNFloat(c2Pct);
  _spr.setTextDatum(TR_DATUM);
  _spr.setTextColor(fd.cur2ChangePercent >= 0 ? zenUp : zenDown);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, c2Pct, 306, 84);

  char c2Rate[16];
  FinanceService::formatVND((long)roundf(fd.cur2Rate), c2Rate, sizeof(c2Rate));
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_NUM, c2Rate, 174, 104);

  _spr.drawFastHLine(174, 148, 132, zenLine);

  // BTC / USDT
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenMuted);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, "BTC", 174, 154);

  char btcPct[16];
  snprintf(btcPct, sizeof(btcPct), "%+.1f%%", fd.btcChangePercent);
  toVNFloat(btcPct);
  _spr.setTextDatum(TR_DATUM);
  _spr.setTextColor(fd.btcChangePercent >= 0 ? zenUp : zenDown);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, btcPct, 306, 154);

  char btcRateStr[16];
  if (fd.btcRate >= 1000.0f) {
    char btcNum[16];
    FinanceService::formatVND((long)roundf(fd.btcRate), btcNum, sizeof(btcNum));
    snprintf(btcRateStr, sizeof(btcRateStr), "$%s", btcNum);
  } else {
    snprintf(btcRateStr, sizeof(btcRateStr), "$%.0f", fd.btcRate);
  }
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_NUM, btcRateStr, 174, 172);

  // Subtle footer hint (hidden when dock opens)
  if (!_dockVisible) {
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(zenDim);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT,
                        isVI ? "Chạm trái: Vàng • Chạm phải: Tỷ giá"
                             : "Tap Left: Gold • Tap Right: FX",
                        160, 218);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP TỶ GIÁ NGOẠI TỆ — FULL-SCREEN DUAL-TAB (USD / EUR)
// ═══════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════
//  POPUP TÀI CHÍNH (MINIMAL ZEN 16-BIT): CHI TIẾT TỶ GIÁ / BTC & VÀNG SJC
// ═══════════════════════════════════════════════════════════════════════
void Display::renderFinanceCurrencyPopupZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(104, 110, 122);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenUp = RGB565(52, 211, 153);
  const uint16_t zenDown = RGB565(248, 113, 113);
  const uint16_t zenLine = RGB565(34, 38, 46);

  const FinanceData &fd = finance.getData();
  bool isVI = (getLanguage() == LANG_VI);
  uint8_t tab = (_currencyTab <= 2) ? _currencyTab : 0;

  const float *pts7d = fd.cur17d;
  float chgPct = fd.cur1ChangePercent;
  if (tab == 1) {
    pts7d = fd.cur27d;
    chgPct = fd.cur2ChangePercent;
  } else if (tab == 2) {
    pts7d = fd.btc7d;
    chgPct = fd.btcChangePercent;
  }
  bool isPos = (chgPct >= 0.0f);
  uint16_t chartCol = (tab == 2) ? zenGold : (isPos ? zenUp : zenDown);

  const int16_t chartX = 16, chartY = 126, chartW = 288, chartH = 68;

  // Pass 1: Background, close pill, hairlines & 7-day sparkline via Sprite
  _spr.fillSprite(zenBg);
  _spr.fillSmoothCircle(296, 20, 10, RGB565(28, 32, 38), zenBg);
  _spr.drawWideLine(292, 16, 300, 24, 1.5f, zenMuted, RGB565(28, 32, 38));
  _spr.drawWideLine(300, 16, 292, 24, 1.5f, zenMuted, RGB565(28, 32, 38));

  _spr.drawFastHLine(16, 40, 288, zenLine);
  _spr.drawFastHLine(16, 118, 288, zenLine);
  drawFinanceSparkline(chartX, chartY, chartW, chartH, pts7d, 7, chartCol,
                       false);
  _spr.pushSprite(0, 0);

  // Pass 2: True 16-bit Zen typography directly on _tft
  const char *tabLabels[3] = {fd.cur1Code, fd.cur2Code, "BTC"};
  const int16_t tabCx[3] = {54, 136, 218};
  const uint16_t tabCols[3] = {zenAccent, zenAccent, zenGold};

  for (int i = 0; i < 3; i++) {
    bool act = (tab == i);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, tabLabels[i], tabCx[i],
                               20, MC_DATUM, act ? zenText : zenMuted, zenBg,
                               tabCx[i] - 38, 9, 76, 22);
    if (act)
      _tft.fillRoundRect(tabCx[i] - 20, 33, 40, 2, 1, tabCols[i]);
  }

  // Big primary price + unit + 24h change
  char rateStr[20], chgStr[16];
  if (tab < 2) {
    float rVal = (tab == 0) ? fd.cur1Rate : fd.cur2Rate;
    FinanceService::formatVND((long)roundf(rVal), rateStr, sizeof(rateStr));
  } else {
    char btcNum[16];
    FinanceService::formatVND((long)roundf(fd.btcRate), btcNum, sizeof(btcNum));
    snprintf(rateStr, sizeof(rateStr), "$%s", btcNum);
  }
  snprintf(chgStr, sizeof(chgStr), "%+.2f%%", chgPct);
  toVNFloat(chgStr);

  int16_t rateW = _spr.measureSmoothFace(&ZEN_FONT_NUM, rateStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, rateStr, 16, 60, ML_DATUM,
                             zenText, zenBg, 14, 42, rateW + 6, 36);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, (tab == 2) ? "USDT" : "VND",
                             16 + rateW + 6, 65, ML_DATUM, tabCols[tab], zenBg,
                             16 + rateW + 5, 55, 48, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, chgStr, 304, 60, MR_DATUM,
                             isPos ? zenUp : zenDown, zenBg, 224, 50, 82, 20);

  // Sub-metrics: 7-day low/high (and VND equivalent for BTC)
  if (tab < 2) {
    char minStr[16], maxStr[16], dayStr[32], dinhStr[32];
    float minVal = (tab == 0) ? fd.cur1Min7d : fd.cur2Min7d;
    float maxVal = (tab == 0) ? fd.cur1Max7d : fd.cur2Max7d;
    FinanceService::formatVND((long)roundf(minVal), minStr, sizeof(minStr));
    FinanceService::formatVND((long)roundf(maxVal), maxStr, sizeof(maxStr));
    snprintf(dayStr, sizeof(dayStr), isVI ? "Đáy 7d: %s" : "Low 7d: %s",
             minStr);
    snprintf(dinhStr, sizeof(dinhStr), isVI ? "Đỉnh 7d: %s" : "High 7d: %s",
             maxStr);

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dayStr, 16, 105, ML_DATUM,
                               zenMuted, zenBg, 14, 95, 144, 19);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dinhStr, 304, 105,
                               MR_DATUM, isPos ? zenUp : zenAccent, zenBg, 160,
                               95, 146, 19);
  } else {
    float usdRef = (fd.usdVndRate > 10000.0f) ? fd.usdVndRate : 25897.0f;
    float btcTy = (fd.btcRate * usdRef) / 1000000000.0f;
    char eqStr[32], minB[16], maxB[16], dayStr[32], dinhStr[32];
    snprintf(eqStr, sizeof(eqStr),
             isVI ? "~%.2f tỷ VNĐ" : "~%.2fB VND", btcTy);
    toVNFloat(eqStr);
    FinanceService::formatVND((long)roundf(fd.btcMin7d), minB, sizeof(minB));
    FinanceService::formatVND((long)roundf(fd.btcMax7d), maxB, sizeof(maxB));
    snprintf(dayStr, sizeof(dayStr), isVI ? "Đáy 7d: $%s" : "Low 7d: $%s",
             minB);
    snprintf(dinhStr, sizeof(dinhStr), isVI ? "Đỉnh 7d: $%s" : "High 7d: $%s",
             maxB);

    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, eqStr, 16, 85, ML_DATUM,
                               zenAccent, zenBg, 14, 76, 200, 18);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dayStr, 16, 105, ML_DATUM,
                               zenMuted, zenBg, 14, 95, 144, 19);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dinhStr, 304, 105,
                               MR_DATUM, zenGold, zenBg, 160, 95, 146, 19);
  }

  // 7-day weekday labels along chart bottom
  time_t rawPopTime = time(NULL);
  struct tm tmPop;
  localtime_r(&rawPopTime, &tmPop);
  int curWdayPop = tmPop.tm_wday;
  for (int i = 0; i < 7; i++) {
    int16_t dx = chartX + (int16_t)((i * (chartW - 1)) / 6);
    if (dx < 22) dx = 22;
    if (dx > 298) dx = 298;
    int w = (curWdayPop - (6 - i) + 14) % 7;
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, getShortDayNameI18n(w),
                               dx, 205, MC_DATUM,
                               (i == 6) ? tabCols[tab] : zenMuted, zenBg,
                               dx - 16, 195, 32, 20);
  }

  // Footer source label
  const char *srcTxt =
      (tab < 2) ? (isVI ? "Nguồn: Vietcombank & ExchangeRate-API"
                        : "Source: Vietcombank & ExchangeRate-API")
                : (isVI ? "Nguồn: Binance Spot • Biểu đồ 7 ngày"
                        : "Source: Binance Spot • 7-Day Chart");
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, srcTxt, 160, 225, MC_DATUM,
                             zenDim, zenBg, 16, 215, 288, 20);
}

void Display::renderFinanceGoldPopupZen16() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(104, 110, 122);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenUp = RGB565(52, 211, 153);
  const uint16_t zenDown = RGB565(248, 113, 113);
  const uint16_t zenLine = RGB565(34, 38, 46);

  const FinanceData &fd = finance.getData();
  bool isVI = (getLanguage() == LANG_VI);
  bool sjcPos = (fd.sjcChangePercent >= 0.0f);

  const int16_t chartX = 16, chartY = 132, chartW = 288, chartH = 62;

  // Pass 1: Background, close pill, hairlines & 7-day gold sparkline
  _spr.fillSprite(zenBg);
  _spr.fillSmoothCircle(296, 20, 10, RGB565(28, 32, 38), zenBg);
  _spr.drawWideLine(292, 16, 300, 24, 1.5f, zenMuted, RGB565(28, 32, 38));
  _spr.drawWideLine(300, 16, 292, 24, 1.5f, zenMuted, RGB565(28, 32, 38));

  _spr.drawFastHLine(16, 40, 288, zenLine);
  _spr.drawFastHLine(16, 124, 288, zenLine);
  drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.sjc7d, 7, zenGold,
                       false);
  _spr.pushSprite(0, 0);

  // Pass 2: True 16-bit Zen typography directly on _tft
  VnSprite::drawSmoothFace16(
      _tft, &ZEN_FONT_LIGHT,
      isVI ? "VÀNG SJC 9999 & THẾ GIỚI" : "SJC GOLD 9999 & WORLD", 16, 20,
      ML_DATUM, zenGold, zenBg, 14, 9, 254, 22);

  // Row 1 Left: SJC Sell large price + unit
  char sellStr[16];
  snprintf(sellStr, sizeof(sellStr), "%.1f", fd.sjcSell);
  toVNFloat(sellStr);
  int16_t sellW = _spr.measureSmoothFace(&ZEN_FONT_NUM, sellStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_NUM, sellStr, 16, 58, ML_DATUM,
                             zenText, zenBg, 14, 42, sellW + 4, 34);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, isVI ? "tr" : "M",
                             16 + sellW + 5, 63, ML_DATUM, zenMuted, zenBg,
                             16 + sellW + 4, 53, 28, 20);

  // Row 1 Right: SJC Buy & Daily change
  char buyStr[24], chgStr[32];
  snprintf(buyStr, sizeof(buyStr), isVI ? "MUA %.1f tr" : "BUY %.1f M",
           fd.sjcBuy);
  toVNFloat(buyStr);
  snprintf(chgStr, sizeof(chgStr), "%+.1f%% (%s)", fd.sjcChangePercent,
           fd.sjcChangeStr);
  toVNFloat(chgStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, buyStr, 304, 50, MR_DATUM,
                             zenMuted, zenBg, 168, 41, 138, 18);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, chgStr, 304, 69, MR_DATUM,
                             sjcPos ? zenUp : zenDown, zenBg, 168, 60, 138, 18);

  // Row 2: Kitco Spot Gold + Domestic spread
  char kNum[16], kitcoStr[36], diffStr[32];
  long kw = (long)fd.worldGoldUsd;
  int kdec = (int)(roundf((fd.worldGoldUsd - kw) * 10.0f)) % 10;
  FinanceService::formatVND(kw, kNum, sizeof(kNum));
  snprintf(kitcoStr, sizeof(kitcoStr), "KITCO $%s,%d/oz", kNum, kdec);
  snprintf(diffStr, sizeof(diffStr),
           isVI ? "Chênh ~%.1f tr" : "Spread ~%.1f M", fd.domesticDiffTr);
  toVNFloat(diffStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, kitcoStr, 16, 89, ML_DATUM,
                             zenAccent, zenBg, 14, 79, 170, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, diffStr, 304, 89, MR_DATUM,
                             zenMuted, zenBg, 186, 79, 120, 20);

  // Row 3: 7-day Low • High
  char dayGStr[32], dinhGStr[32];
  snprintf(dayGStr, sizeof(dayGStr), isVI ? "Đáy 7d: %.1f tr" : "Low 7d: %.1f M",
           fd.sjcMin7d);
  toVNFloat(dayGStr);
  snprintf(dinhGStr, sizeof(dinhGStr),
           isVI ? "Đỉnh 7d: %.1f tr" : "High 7d: %.1f M", fd.sjcMax7d);
  toVNFloat(dinhGStr);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dayGStr, 16, 110, ML_DATUM,
                             zenMuted, zenBg, 14, 100, 144, 20);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, dinhGStr, 304, 110,
                             MR_DATUM, zenGold, zenBg, 160, 100, 146, 20);

  // 7-day weekday labels
  time_t rawGoldTime = time(NULL);
  struct tm tmGold;
  localtime_r(&rawGoldTime, &tmGold);
  int curWdayGold = tmGold.tm_wday;
  for (int i = 0; i < 7; i++) {
    int16_t dx = chartX + (int16_t)((i * (chartW - 1)) / 6);
    if (dx < 22) dx = 22;
    if (dx > 298) dx = 298;
    int w = (curWdayGold - (6 - i) + 14) % 7;
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, getShortDayNameI18n(w),
                               dx, 205, MC_DATUM,
                               (i == 6) ? zenGold : zenMuted, zenBg, dx - 16,
                               195, 32, 20);
  }

  // Footer source label
  VnSprite::drawSmoothFace16(
      _tft, &ZEN_FONT_LIGHT,
      isVI ? "Nguồn: SJC & Kitco • Triệu VNĐ/Lượng"
           : "Source: SJC & Kitco • Million VND/Tael",
      160, 225, MC_DATUM, zenDim, zenBg, 16, 215, 288, 20);
}

void Display::renderFinanceCurrencyPopup() {
  const FinanceData &fd = finance.getData();

  // Nền tối sâu chuyên nghiệp tràn viền Full Screen (320x240) — Đơn sắc sang
  // trọng, không dải gradient
  _spr.fillRect(0, 0, 320, 240, RGB565(8, 12, 20));

  // ── 2 TAB CHUYỂN NGOẠI TỆ & NÚT [X] ĐÓNG POPUP ──
  char tab1Label[16], tab2Label[16];
  snprintf(tab1Label, sizeof(tab1Label), "%s / VND", fd.cur1Code);
  snprintf(tab2Label, sizeof(tab2Label), "%s / VND", fd.cur2Code);

  // Tab 1 (X: 14..128, Y: 10..36)
  if (_currencyTab == 0) {
    _spr.fillSmoothRoundRect(14, 10, 116, 26, 13, theme.accent,
                             RGB565(8, 12, 20));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(10, 20, 30));
    _spr.drawString(tab1Label, 14 + 58, 10 + 13, 2);
  } else {
    _spr.fillSmoothRoundRect(14, 10, 116, 26, 13, RGB565(42, 60, 85),
                             RGB565(8, 12, 20));
    _spr.fillSmoothRoundRect(15, 11, 114, 24, 12, RGB565(18, 26, 38),
                             RGB565(42, 60, 85));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(tab1Label, 14 + 58, 10 + 13, 2);
  }

  // Tab 2 (X: 136..250, Y: 10..36)
  if (_currencyTab == 1) {
    _spr.fillSmoothRoundRect(136, 10, 116, 26, 13, theme.accent,
                             RGB565(8, 12, 20));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(10, 20, 30));
    _spr.drawString(tab2Label, 136 + 58, 10 + 13, 2);
  } else {
    _spr.fillSmoothRoundRect(136, 10, 116, 26, 13, RGB565(42, 60, 85),
                             RGB565(8, 12, 20));
    _spr.fillSmoothRoundRect(137, 11, 114, 24, 12, RGB565(18, 26, 38),
                             RGB565(42, 60, 85));
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(tab2Label, 136 + 58, 10 + 13, 2);
  }

  // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
  drawPopupCloseButton();

  // Đường kẻ phân cách sau hàng Tab (Y = 44) — Chuẩn POPUP_DIVIDER_COLOR thống
  // nhất
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
    FinanceService::formatVND((long)roundf(fd.cur1Rate), cur1RateStr,
                              sizeof(cur1RateStr));
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
    char range7d[80];
    snprintf(range7d, sizeof(range7d),
             (getLanguage() == LANG_VI ? "Biên độ 7d:   Cao: %s   •   Thấp: %s"
                                       : "7d Range:   High: %s   •   Low: %s"),
             max1, min1);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(170, 200, 235));
    _spr.drawString(range7d, 16, 78, 1);

    // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
    _spr.fillSmoothRoundRect(midX - 125, 96, 250, 20, 10, RGB565(36, 52, 75),
                             RGB565(8, 12, 20));
    _spr.fillSmoothRoundRect(midX - 124, 97, 248, 18, 9, RGB565(16, 24, 36),
                             RGB565(36, 52, 75));

    char dayStr[40], dinhStr[40];
    snprintf(dayStr, sizeof(dayStr),
             (getLanguage() == LANG_VI ? "Đáy: %s" : "Low: %s"), min1);
    snprintf(dinhStr, sizeof(dinhStr),
             (getLanguage() == LANG_VI ? "Đỉnh: %s" : "High: %s"), max1);

    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(160, 205, 255));
    _spr.drawString(dayStr, midX - 10, 106, 1);

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(90, 115, 150));
    _spr.drawString("•", midX, 106, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_GREEN);
    _spr.drawString(dinhStr, midX + 10, 106, 1);

    // Biểu đồ vector 7 ngày khổ rộng 292px + Area Gradient
    drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.cur17d, 7, C_GREEN,
                         true);
  } else {
    // Tab Ngoại tệ 2
    char cur2RateStr[16];
    FinanceService::formatVND((long)roundf(fd.cur2Rate), cur2RateStr,
                              sizeof(cur2RateStr));
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
    char range7d[80];
    snprintf(range7d, sizeof(range7d),
             (getLanguage() == LANG_VI ? "Biên độ 7d:   Cao: %s   •   Thấp: %s"
                                       : "7d Range:   High: %s   •   Low: %s"),
             max2, min2);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(170, 200, 235));
    _spr.drawString(range7d, 16, 78, 1);

    // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
    _spr.fillSmoothRoundRect(midX - 125, 96, 250, 20, 10, RGB565(36, 52, 75),
                             RGB565(8, 12, 20));
    _spr.fillSmoothRoundRect(midX - 124, 97, 248, 18, 9, RGB565(16, 24, 36),
                             RGB565(36, 52, 75));

    char dayStr[40], dinhStr[40];
    snprintf(dayStr, sizeof(dayStr),
             (getLanguage() == LANG_VI ? "Đáy: %s" : "Low: %s"), min2);
    snprintf(dinhStr, sizeof(dinhStr),
             (getLanguage() == LANG_VI ? "Đỉnh: %s" : "High: %s"), max2);

    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(160, 205, 255));
    _spr.drawString(dayStr, midX - 10, 106, 1);

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(90, 115, 150));
    _spr.drawString("•", midX, 106, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(255, 120, 120));
    _spr.drawString(dinhStr, midX + 10, 106, 1);

    // Biểu đồ vector 7 ngày khổ rộng 292px + Area Gradient
    drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.cur27d, 7,
                         RGB565(255, 95, 95), true);
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
  _spr.drawString((getLanguage() == LANG_VI
                       ? "Nguồn: Vietcombank & ExchangeRate-API"
                       : "Source: Vietcombank & ExchangeRate-API"),
                  160, 224, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP VÀNG SJC 9999 & THẾ GIỚI — FULL-SCREEN POPUP (320x240)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderFinanceGoldPopup() {
  const FinanceData &fd = finance.getData();

  // Nền tối sâu sang trọng tràn viền Full Screen (320x240) — Đơn sắc sang
  // trọng, không dải gradient
  _spr.fillRect(0, 0, 320, 240, RGB565(12, 11, 16));

  // Tiêu đề Popup
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(C_GOLD);
  _spr.drawString((getLanguage() == LANG_VI ? "VÀNG SJC 9999 & THẾ GIỚI"
                                            : "SJC GOLD 9999 & WORLD"),
                  16, 23, 2);

  // Nút [X] Đóng Popup — Chuẩn phong cách Windows 11 nền xanh bo tròn góc
  drawPopupCloseButton();

  // Vạch kẻ header phân cách (Y = 42) — Chuẩn POPUP_DIVIDER_COLOR thống nhất
  _spr.drawFastHLine(10, 42, 300, POPUP_DIVIDER_COLOR);

  // Bảng giá SJC & Thế Giới (Bố cục thông thoáng, không đè chữ)
  char sjcBuyStr[36], sjcSellStr[36];
  snprintf(sjcBuyStr, sizeof(sjcBuyStr),
           (getLanguage() == LANG_VI ? "SJC MUA: %.1f tr" : "SJC BUY: %.1f M"),
           fd.sjcBuy);
  toVNFloat(sjcBuyStr);
  snprintf(sjcSellStr, sizeof(sjcSellStr),
           (getLanguage() == LANG_VI ? "BÁN: %.1f tr" : "SELL: %.1f M"),
           fd.sjcSell);
  toVNFloat(sjcSellStr);

  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(sjcBuyStr, 16, 48, 2);
  _spr.setTextColor(C_GOLD);
  _spr.drawString(sjcSellStr, 150, 48, 2);

  bool sjcPos = (fd.sjcChangePercent >= 0);
  drawFinanceBadge(252, 47, 52, 18, fd.sjcChangeStr, sjcPos);

  char kitcoStr[64];
  char kNum[16];
  long kw = (long)fd.worldGoldUsd;
  int kdec = (int)(roundf((fd.worldGoldUsd - kw) * 10.0f)) % 10;
  FinanceService::formatVND(kw, kNum, sizeof(kNum));
  snprintf(kitcoStr, sizeof(kitcoStr),
           (getLanguage() == LANG_VI ? "THẾ GIỚI (KITCO):  $%s,%d / oz"
                                     : "WORLD (KITCO):  $%s,%d / oz"),
           kNum, kdec);
  _spr.setTextDatum(TL_DATUM);
  _spr.setTextColor(RGB565(170, 215, 255));
  _spr.drawString(kitcoStr, 16, 68, 2);

  char diffStr[64];
  snprintf(diffStr, sizeof(diffStr),
           (getLanguage() == LANG_VI ? "Chênh lệch trong nước: ~%.1f tr/lượng"
                                     : "Domestic spread: ~%.1f M/tael"),
           fd.domesticDiffTr);
  toVNFloat(diffStr);
  _spr.setTextColor(RGB565(200, 210, 225));
  _spr.drawString(diffStr, 16, 88, 1);

  // CỤM THẺ THỐNG KÊ ĐÁY & ĐỈNH 7 NGÀY (Rộng 250px — Chống tràn chữ 100%)
  int16_t midX = 160;
  _spr.fillSmoothRoundRect(midX - 125, 104, 250, 20, 10, RGB565(70, 58, 35),
                           RGB565(12, 11, 16));
  _spr.fillSmoothRoundRect(midX - 124, 105, 248, 18, 9, RGB565(24, 22, 18),
                           RGB565(70, 58, 35));

  char dayGStr[40], dinhGStr[40];
  snprintf(dayGStr, sizeof(dayGStr),
           (getLanguage() == LANG_VI ? "Đáy: %.1f tr" : "Low: %.1f M"),
           fd.sjcMin7d);
  toVNFloat(dayGStr);
  snprintf(dinhGStr, sizeof(dinhGStr),
           (getLanguage() == LANG_VI ? "Đỉnh: %.1f tr" : "High: %.1f M"),
           fd.sjcMax7d);
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

  // Biểu đồ vàng 7 ngày khổ rộng 292px + Area Gradient
  int16_t chartX = 14;
  int16_t chartY = 130;
  int16_t chartW = 292;
  int16_t chartH = 64;

  drawFinanceSparkline(chartX, chartY, chartW, chartH, fd.sjc7d, 7, C_GOLD,
                       true);

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
  _spr.drawString((getLanguage() == LANG_VI
                       ? "Nguồn: SJC & Kitco  •  Đơn vị: Triệu VNĐ/Lượng"
                       : "Source: SJC & Kitco  •  Unit: Million VND/Tael"),
                  160, 224, 1);
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
  uint16_t bodyColor = RGB565(255, 30, 0); // Đỏ cờ / Đỏ cà chua chín rực rỡ
  _spr.fillSmoothCircle(cx - 12, cy + 1, 26, bodyColor, theme.bg);
  _spr.fillSmoothCircle(cx + 12, cy + 1, 26, bodyColor, theme.bg);
  _spr.fillSmoothCircle(cx, cy + 5, 25, bodyColor, theme.bg);
  _spr.fillRect(cx - 12, cy - 18, 24, 40, bodyColor);

  // 3. Vệt sáng phản chiếu (Specular Highlight) màu cam đỏ ấm
  _spr.fillSmoothCircle(cx - 15, cy - 8, 5, RGB565(255, 120, 80), bodyColor);
  _spr.fillSmoothCircle(cx - 10, cy - 13, 3, RGB565(255, 160, 120), bodyColor);

  // 4. Cuống và đài lá xanh biếc trên đầu (Green Leaves & Stem)
  uint16_t stemColor = RGB565(20, 110, 40); // Cuống xanh đậm
  uint16_t leafColor = RGB565(35, 190, 60); // Lá xanh tươi
  _spr.fillRoundRect(cx - 3, cy - 29, 6, 11, 2, stemColor);
  // 3 cánh lá sao xòe ra
  _spr.fillTriangle(cx - 1, cy - 18, cx - 18, cy - 25, cx - 4, cy - 18,
                    leafColor);
  _spr.fillTriangle(cx + 1, cy - 18, cx + 18, cy - 25, cx + 4, cy - 18,
                    leafColor);
  _spr.fillTriangle(cx - 8, cy - 18, cx + 8, cy - 18, cx, cy - 10,
                    RGB565(50, 215, 85));

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
    _spr.drawPixel(cx, cy + 13, RGB565(55, 15, 15));
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
  if (isMinimalUiMode()) {
    renderPage3_PomodoroZen();
    return;
  }
  drawBackground();

  // ── 1. THANH CHỌN CHẾ ĐỘ DẠNG VIÊN THUỐC HỢP NHẤT (Top Segmented Capsule) ──
  // Y = 8..28 (H=20px), Capsule nền bo tròn đồng tâm X = 45..275 (W=230px) theo
  // Theme
  int16_t capX = 45, capY = 8, capW = 230, capH = 20, capR = 10;
  _spr.fillSmoothRoundRect(capX, capY, capW, capH, capR, theme.cardBorder,
                           theme.bg);
  _spr.fillSmoothRoundRect(capX + 1, capY + 1, capW - 2, capH - 2, capR - 1,
                           theme.card, theme.cardBorder);

  const char *tabNamesVI[] = {"TẬP TRUNG", "NGHỈ 5P", "NGHỈ 15P"};
  const char *tabNamesEN[] = {"FOCUS", "BREAK 5M", "BREAK 15M"};
  const char *const *tabNames =
      (getLanguage() == LANG_VI) ? tabNamesVI : tabNamesEN;
  int16_t segX[] = {47, 123, 199};
  int16_t segW[] = {75, 75, 74};

  uint16_t modeColors[] = {
      theme.accent,   // Focus: Rực rỡ theo màu nhấn chủ đạo của Theme
                      // (Cyan/Amber/Pink/Emerald...)
      theme.green,    // Short Break: Xanh lá dịu mắt thư giãn
      theme.accentSec // Long Break: Màu phụ phối của Theme
  };
  uint16_t activeColor = modeColors[_pomoMode];

  for (int i = 0; i < 3; i++) {
    bool isActive = (_pomoMode == i);
    if (isActive) {
      _spr.fillSmoothRoundRect(segX[i], capY + 1, segW[i], capH - 2, 8,
                               activeColor, theme.card);
      _spr.setTextDatum(MC_DATUM);
      _spr.setTextColor(C_WHITE);
      _spr.drawString(tabNames[i], segX[i] + segW[i] / 2, capY + capH / 2, 1);
    } else {
      _spr.setTextDatum(MC_DATUM);
      _spr.setTextColor(theme.textDim);
      _spr.drawString(tabNames[i], segX[i] + segW[i] / 2, capY + capH / 2, 1);
    }
  }

  // ── 2. NỬA TRÁI: VÒNG TRÒN TIẾN ĐỘ LỚN & CHÚ CÀ CHUA (Hero Left Dial) ──────
  // Tâm (cx=88, cy=126), Bán kính ngoài R=68, R trong=61 (X: 20..156,
  // Y: 58..194)
  int16_t cx = 88, ringCy = 126;
  int16_t outerR = 68, innerR = 61;

  // Vòng ray nền hòa sắc cùng màu Track của Theme
  _spr.drawArc(cx, ringCy, outerR, innerR, 0, 360, theme.track, theme.bg, true);

  // Vẽ Desk Pet Mascot (Cà chua / Mèo / Shiba / Capybara) ở trung tâm vòng tròn
  deskPet.render(_spr, cx, ringCy + 1, _pomoState, _pomoMode, _pomoRemainingSec,
                 theme.bg);

  // Tính % tiến độ
  float progress = 0.0f;
  if (_pomoTotalSec > 0) {
    progress =
        (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec;
  }
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;

  uint32_t sweepDeg = (uint32_t)(progress * 360.0f);
  if (sweepDeg >= 360) {
    _spr.drawArc(cx, ringCy, outerR, innerR, 0, 360, activeColor, theme.bg,
                 true);
  } else if (sweepDeg > 0) {
    uint32_t startAng = 180; // 12 o'clock in TFT_eSPI
    uint32_t endAng = (180 + sweepDeg) % 360;
    _spr.drawArc(cx, ringCy, outerR, innerR, startAng, endAng, activeColor,
                 theme.bg, true);
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
  const char *statusDesc =
      (getLanguage() == LANG_VI) ? "ĐANG TẬP TRUNG" : "FOCUSING...";
  if (_pomoState == POMO_STOPPED) {
    statusDesc = (_pomoMode == POMO_FOCUS)
                     ? ((getLanguage() == LANG_VI) ? "SẴN SÀNG TẬP TRUNG"
                                                   : "READY TO FOCUS")
                     : ((getLanguage() == LANG_VI) ? "SẴN SÀNG NGHỈ"
                                                   : "READY FOR BREAK");
  } else if (_pomoState == POMO_PAUSED) {
    statusDesc = (getLanguage() == LANG_VI) ? "ĐANG TẠM DỪNG" : "PAUSED";
  } else {
    if (_pomoMode == POMO_SHORT_BREAK)
      statusDesc =
          (getLanguage() == LANG_VI) ? "NGHỈ NGẮN 5 PHÚT" : "SHORT BREAK 5M";
    else if (_pomoMode == POMO_LONG_BREAK)
      statusDesc =
          (getLanguage() == LANG_VI) ? "NGHỈ DÀI 15 PHÚT" : "LONG BREAK 15M";
  }
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(activeColor);
  _spr.drawString(statusDesc, cx2, 48, 2);

  // Đồng hồ số đếm ngược Font 6 cực to, sắc nét, đồng bộ Theme Text (Y=104)
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _pomoRemainingSec / 60,
           _pomoRemainingSec % 60);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString(timeBuf, cx2, 104, 6);

  // ── 4. CỤM NÚT ĐIỆU KHIỂN CÔNG THÁI HỌC (Y = 152..194, H = 42px — Rộng rãi,
  // kết thúc tại Y=194) ──
  int16_t btnY = 152, btnH = 42;

  if (_pomoState == POMO_STOPPED) {
    // Nút Start lớn bo tròn viên thuốc (Radius 21) theo Theme Accent
    int16_t bX = 162, bW = 148;
    _spr.fillSmoothRoundRect(bX, btnY, bW, btnH, 21, activeColor, theme.bg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(L(TXT_POMO_START), bX + bW / 2, btnY + btnH / 2, 2);
  } else if (_pomoState == POMO_RUNNING) {
    // Nút Trái: TẠM DỪNG / PAUSE
    int16_t b1X = 158, b1W = 74;
    _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, RGB565(245, 180, 45),
                             theme.bg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(20, 20, 25));
    _spr.drawString(L(TXT_POMO_PAUSE), b1X + b1W / 2, btnY + btnH / 2, 2);

    // Nút Phải: ĐẶT LẠI / RESET (Bo tròn đồng tâm mượt mà)
    int16_t b2X = 236, b2W = 74;
    _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, theme.cardBorder,
                             theme.bg);
    _spr.fillSmoothRoundRect(b2X + 1, btnY + 1, b2W - 2, btnH - 2, 15,
                             theme.card, theme.cardBorder);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_POMO_RESET), b2X + b2W / 2, btnY + btnH / 2, 2);
  } else if (_pomoState == POMO_PAUSED) {
    // Nút Trái: TIẾP TỤC / RESUME
    int16_t b1X = 158, b1W = 74;
    _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, theme.green, theme.bg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(15, 25, 20));
    _spr.drawString((getLanguage() == LANG_VI) ? "TIẾP TỤC" : "RESUME",
                    b1X + b1W / 2, btnY + btnH / 2, 2);

    // Nút Phải: ĐẶT LẠI / RESET (Bo tròn đồng tâm mượt mà)
    int16_t b2X = 236, b2W = 74;
    _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, theme.cardBorder,
                             theme.bg);
    _spr.fillSmoothRoundRect(b2X + 1, btnY + 1, b2W - 2, btnH - 2, 15,
                             theme.card, theme.cardBorder);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(L(TXT_POMO_RESET), b2X + b2W / 2, btnY + btnH / 2, 2);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 3 (MINIMAL ZEN) — Signature Apple StandBy Minimalist Focus Timer
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage3_PomodoroZen() {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(132, 138, 148);
  const uint16_t zenTrack = RGB565(36, 40, 46);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenGreen = RGB565(52, 211, 153);

  _spr.fillSprite(zenBg);
  bool isVI = (getLanguage() == LANG_VI);

  uint16_t modeColors[] = {zenAccent, zenGreen, zenGold};
  uint16_t activeColor = modeColors[_pomoMode];

  // ── 1. MINIMAL TOP TEXT TABS (Y = 18, crisp 2px active underline) ─
  const char *tabVI[] = {"TẬP TRUNG", "NGHỈ 5P", "NGHỈ 15P"};
  const char *tabEN[] = {"FOCUS", "SHORT 5M", "LONG 15M"};
  const char *const *tabs = isVI ? tabVI : tabEN;
  int16_t tabCx[] = {78, 160, 242};

  _spr.setTextDatum(MC_DATUM);
  for (int i = 0; i < 3; i++) {
    bool act = (_pomoMode == i);
    _spr.setTextColor(act ? zenText : zenMuted);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, tabs[i], tabCx[i], 18);
    if (act) {
      _spr.fillSmoothRoundRect(tabCx[i] - 20, 30, 40, 2, 1, activeColor, zenBg);
    }
  }

  // ── 2. SUBTLE STATUS LINE (Y = 46) ────────────────────────────────
  const char *statusTxt = isVI ? "SẴN SÀNG" : "READY";
  if (_pomoState == POMO_RUNNING) {
    statusTxt = (_pomoMode == POMO_FOCUS)
                    ? (isVI ? "ĐANG TẬP TRUNG" : "FOCUSING")
                    : (isVI ? "ĐANG NGHỈ NGƠI" : "ON BREAK");
  } else if (_pomoState == POMO_PAUSED) {
    statusTxt = isVI ? "TẠM DỪNG" : "PAUSED";
  }
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(activeColor);
  _spr.drawSmoothFace(&ZEN_FONT_LIGHT, statusTxt, 160, 46);

  // ── 3. GIANT SMOOTH LIGHT COUNTDOWN (74px Segoe UI Light, Y = 96) ─
  char mmStr[4], ssStr[4];
  snprintf(mmStr, sizeof(mmStr), "%02d", _pomoRemainingSec / 60);
  snprintf(ssStr, sizeof(ssStr), "%02d", _pomoRemainingSec % 60);
  int16_t mmW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, mmStr);
  int16_t colW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, ":");
  int16_t ssW = _spr.measureSmoothFace(&ZEN_FONT_CLOCK, ssStr);
  int16_t tx0 = 160 - (mmW + colW + ssW) / 2;

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_CLOCK, mmStr, tx0, 96);
  _spr.setTextColor(activeColor);
  _spr.drawSmoothFace(&ZEN_FONT_CLOCK, ":", tx0 + mmW, 92);
  _spr.setTextColor(zenText);
  _spr.drawSmoothFace(&ZEN_FONT_CLOCK, ssStr, tx0 + mmW + colW, 96);

  // ── 4. ULTRA-THIN HORIZONTAL PROGRESS BAR (Y = 148, H = 3px) ──────
  const int16_t barX = 36, barY = 148, barW = 248, barH = 3;
  _spr.fillSmoothRoundRect(barX, barY, barW, barH, 1, zenTrack, zenBg);

  float progress = 0.0f;
  if (_pomoTotalSec > 0) {
    progress =
        (float)(_pomoTotalSec - _pomoRemainingSec) / (float)_pomoTotalSec;
  }
  if (progress < 0.0f)
    progress = 0.0f;
  if (progress > 1.0f)
    progress = 1.0f;
  int16_t fillW = (int16_t)(barW * progress);
  if (fillW < 6 && progress > 0.0f)
    fillW = 6;
  if (fillW > 0) {
    _spr.fillSmoothRoundRect(barX, barY, fillW, barH, 1, activeColor, zenTrack);
  }

  // ── 5. MINIMALIST BOTTOM CONTROLS (Y = 166..202, H = 36px) ────────
  const int16_t btnY = 166, btnH = 36;
  if (_pomoState == POMO_STOPPED) {
    int16_t bX = 96, bW = 128;
    _spr.fillSmoothRoundRect(bX, btnY, bW, btnH, 18, activeColor, zenBg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(12, 14, 18));
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, L(TXT_POMO_START), 160,
                        btnY + btnH / 2);
  } else {
    int16_t b1X = 56, b1W = 98;
    uint16_t c1 = (_pomoState == POMO_RUNNING) ? zenGold : zenGreen;
    _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 18, c1, zenBg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(12, 14, 18));
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT,
                        _pomoState == POMO_RUNNING
                            ? L(TXT_POMO_PAUSE)
                            : (isVI ? "TIẾP TỤC" : "RESUME"),
                        b1X + b1W / 2, btnY + btnH / 2);

    int16_t b2X = 166, b2W = 98;
    uint16_t outlineCol = RGB565(90, 96, 108);
    _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 18, outlineCol, zenBg);
    _spr.fillSmoothRoundRect(b2X + 1, btnY + 1, b2W - 2, btnH - 2, 17, zenBg,
                             outlineCol);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(zenText);
    _spr.drawSmoothFace(&ZEN_FONT_LIGHT, L(TXT_POMO_RESET), b2X + b2W / 2,
                        btnY + btnH / 2);
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 4 — DESK MOOD / VIBE SIGNBOARD
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage4_Mood() {
  deskMood.renderPage(_spr, (uint8_t)getLanguage(), theme.bg);
}

void Display::renderPage4_MoodZen16() {
  deskMood.renderPageZen16(_tft, _spr, &ZEN_FONT_LIGHT, (uint8_t)getLanguage());
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 5 — SETTINGS (Pure Seamless Android Material 3 List — ZERO CARDS)
// ═══════════════════════════════════════════════════════════════════════
//  PAGE 5 — SETTINGS (Ergonomic Bento Control Center — PA1)
//  • Top Card: Wide Brightness Slider + [AUTO] Pill (Y: 8..50, H=42px)
//  • Middle 2x2 Bento Grid (Each Tile: 146x52px — Huge touch target!):
//      [Tile 1: Language]        [Tile 2: Theme Style]
//      [Tile 3: Touch Sound]     [Tile 4: Boot / Alarm Sound + TEST]
//  • Bottom Bar: System IP Status + [WIFI] Setup Button (Y: 168..198, H=30px)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage5_Settings() {
  // Nền trơn tĩnh (Solid Velvet) chuẩn màu Theme để chữ và thẻ nổi bật sắc nét
  _spr.fillSprite(theme.bg);

  bool isVI = (getLanguage() == LANG_VI);
  uint16_t cardBg = blendColor(theme.card, theme.bg, 110);
  uint16_t borderCol = blendColor(theme.cardBorder, theme.bg, 120);

  // ═══════════════════════════════════════════════════════════════════
  //  1. THẺ TRÊN CÙNG: ĐỘ SÁNG MÀN HÌNH [ - ] [ ☀️ 85% ] [ + ] [AUTO]
  //     (X: 10..310, Y: 6..52, H=46px — Vừa bấm nút [-] [+], vừa chạm/kéo
  //     thanh)
  // ═══════════════════════════════════════════════════════════════════
  int16_t brY = 6, brH = 46;
  _spr.fillSmoothRoundRect(10, brY, 300, brH, 10, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(11, brY + 1, 298, brH - 2, 9, cardBg, borderCol);

  int16_t brCy = brY + brH / 2; // Y = 29
  bool isAuto = hardware.isAutoBrightness();
  uint16_t barCol = isAuto ? theme.green : theme.accent;
  int curBri = hardware.getBacklight();

  // Nút [ - ] lớn bên trái (X = 16..54, W = 38, H = 32, bo tròn mượt Smooth
  // Pill 2px concentric)
  uint16_t btnBorder = RGB565(65, 85, 118);
  uint16_t btnFill = RGB565(20, 28, 42);
  _spr.fillSmoothRoundRect(16, brCy - 16, 38, 32, 10, btnBorder, cardBg);
  _spr.fillSmoothRoundRect(18, brCy - 14, 34, 28, 8, btnFill, btnBorder);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString("-", 35, brCy, 4);

  // Thanh độ sáng dạng viên thuốc dày (X = 60..196, W = 136, H = 30)
  int16_t slX = 60, slW = 136, slH = 30;
  int16_t slY = brCy - slH / 2;
  uint16_t slBorder = isAuto ? theme.green : RGB565(60, 80, 112);
  uint16_t slTrack = RGB565(15, 22, 34);
  _spr.fillSmoothRoundRect(slX, slY, slW, slH, 10, slBorder, cardBg);
  _spr.fillSmoothRoundRect(slX + 2, slY + 2, slW - 4, slH - 4, 8, slTrack,
                           slBorder);

  int fillW = map(curBri, 0, 100, 0, slW - 4);
  if (fillW > 0) {
    if (fillW < 16)
      fillW = 16;
    uint16_t fillBg = blendColor(barCol, slTrack, 165);
    _spr.fillSmoothRoundRect(slX + 2, slY + 2, fillW, slH - 4, 8, fillBg,
                             slTrack);
  }

  // Icon Mặt trời nhỏ + Số % độ sáng nằm gọn bên trong lòng thanh trượt
  _spr.fillSmoothCircle(slX + 16, brCy, 5, RGB565(250, 185, 30));
  _spr.fillSmoothCircle(slX + 16, brCy, 2, C_BLACK, RGB565(250, 185, 30));

  char bStr[12];
  snprintf(bStr, sizeof(bStr), "%d%%", curBri);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString(bStr, slX + slW / 2 + 6, brCy, 2);

  // Nút [ + ] lớn bên phải thanh trượt (X = 202..240, W = 38, H = 32)
  _spr.fillSmoothRoundRect(202, brCy - 16, 38, 32, 10, btnBorder, cardBg);
  _spr.fillSmoothRoundRect(204, brCy - 14, 34, 28, 8, btnFill, btnBorder);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString("+", 221, brCy, 4);

  // Nút [AUTO] to rõ ngoài cùng bên phải (X = 246..304, W = 58, H = 32)
  uint16_t autoBorder = isAuto ? theme.green : btnBorder;
  uint16_t autoFill = isAuto ? RGB565(15, 65, 42) : btnFill;
  _spr.fillSmoothRoundRect(246, brCy - 16, 58, 32, 10, autoBorder, cardBg);
  _spr.fillSmoothRoundRect(248, brCy - 14, 54, 28, 8, autoFill, autoBorder);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(isAuto ? theme.green : theme.textDim);
  _spr.drawString(L(TXT_AUTO), 275, brCy, 2);

  // ═══════════════════════════════════════════════════════════════════
  //  2. LƯỚI BENTO 2×2: 4 Ô ĐIỀU KHIỂN LỚN (Mỗi ô: W = 146px, H = 64px)
  //     • Hàng 1: Y = 59..123 (H = 64px)
  //     • Hàng 2: Y = 130..194 (H = 64px)
  //     • Cột Trái: X = 10..156 | Cột Phải: X = 164..310
  // ═══════════════════════════════════════════════════════════════════
  int16_t tW = 146, tH = 64;
  int16_t leftX = 10, rightX = 164;
  int16_t row1Y = 59, row2Y = 130;

  // ── Ô 1 (Trái - Trên): NGÔN NGỮ / LANGUAGE ─────────────────────────
  _spr.fillSmoothRoundRect(leftX, row1Y, tW, tH, 10, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(leftX + 1, row1Y + 1, tW - 2, tH - 2, 9, cardBg,
                           borderCol);

  _spr.fillSmoothCircle(leftX + 18, row1Y + 19, 8, RGB565(30, 180, 90), cardBg);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString("L", leftX + 18, row1Y + 19, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString(isVI ? "NGÔN NGỮ" : "LANGUAGE", leftX + 32, row1Y + 19, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString(getLanguageName(), leftX + 12, row1Y + 45, 2);

  _spr.setTextDatum(MR_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString(">", leftX + tW - 10, row1Y + 45, 2);

  // ── Ô 2 (Phải - Trên): GIAO DIỆN / THEME + NÚT [ZEN]/[STD] ─────────
  _spr.fillSmoothRoundRect(rightX, row1Y, tW, tH, 10, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(rightX + 1, row1Y + 1, tW - 2, tH - 2, 9, cardBg,
                           borderCol);

  _spr.fillSmoothCircle(rightX + 18, row1Y + 19, 8, RGB565(130, 65, 210),
                        cardBg);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString("T", rightX + 18, row1Y + 19, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString(isVI ? "GIAO DIỆN" : "THEME", rightX + 30, row1Y + 19, 1);

  // Nút chuyển nhanh [ZEN] <-> [STD] ở góc trên-phải của ô Theme
  bool isZen = isMinimalUiMode();
  uint16_t zenPillBorder = isZen ? RGB565(229, 184, 105) : RGB565(80, 95, 118);
  uint16_t zenPillFill = isZen ? RGB565(46, 36, 18) : RGB565(20, 26, 38);
  _spr.fillSmoothRoundRect(rightX + 94, row1Y + 9, 44, 22, 11, zenPillBorder,
                           cardBg);
  _spr.fillSmoothRoundRect(rightX + 96, row1Y + 11, 40, 18, 9, zenPillFill,
                           zenPillBorder);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(isZen ? RGB565(245, 205, 125) : theme.textDim);
  _spr.drawString(isZen ? "ZEN" : "STD", rightX + 116, row1Y + 20, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString(getThemeName(), rightX + 12, row1Y + 45, 2);

  _spr.setTextDatum(MR_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString(">", rightX + tW - 10, row1Y + 45, 2);

  // ── Ô 3 (Trái - Dưới): ÂM CHẠM CẢM ỨNG / TOUCH SOUND ───────────────
  _spr.fillSmoothRoundRect(leftX, row2Y, tW, tH, 10, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(leftX + 1, row2Y + 1, tW - 2, tH - 2, 9, cardBg,
                           borderCol);

  _spr.fillSmoothCircle(leftX + 18, row2Y + 19, 8, RGB565(0, 170, 220), cardBg);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(C_WHITE);
  _spr.drawString("S", leftX + 18, row2Y + 19, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textDim);
  _spr.drawString(isVI ? "ÂM CHẠM" : "TOUCH SOUND", leftX + 32, row2Y + 19, 1);

  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString(hardware.getTouchSoundName(), leftX + 12, row2Y + 45, 2);

  _spr.setTextDatum(MR_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString(">", leftX + tW - 10, row2Y + 45, 2);

  // ── Ô 4 (Phải - Dưới): ÂM KHỞI ĐỘNG ↔ CHUÔNG BÁO (Cách 1: Nút lật chế độ bên
  // phải) ──
  _spr.fillSmoothRoundRect(rightX, row2Y, tW, tH, 10, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(rightX + 1, row2Y + 1, tW - 2, tH - 2, 9, cardBg,
                           borderCol);

  if (_settingsSoundIsStartup) {
    _spr.fillSmoothCircle(rightX + 18, row2Y + 19, 8, RGB565(0, 200, 240),
                          cardBg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_BLACK);
    _spr.drawString("K", rightX + 18, row2Y + 19, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(isVI ? "KHỞI ĐỘNG" : "BOOT SND", rightX + 30, row2Y + 19,
                    1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(hardware.getStartupSoundName(), rightX + 12, row2Y + 45, 2);

    // Nút chuyển sang Báo thức ở cột phải (Smooth Pill đồng tâm R=13, inset 2px
    // R=11)
    uint16_t swBorder = RGB565(240, 105, 50);
    uint16_t swFill = RGB565(48, 24, 18);
    _spr.fillSmoothRoundRect(rightX + 86, row2Y + 16, 52, 26, 13, swBorder,
                             cardBg);
    _spr.fillSmoothRoundRect(rightX + 88, row2Y + 18, 48, 22, 11, swFill,
                             swBorder);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(255, 165, 110));
    _spr.drawString(isVI ? "CHUÔNG" : "ALARM", rightX + 112, row2Y + 29, 1);
  } else {
    _spr.fillSmoothCircle(rightX + 18, row2Y + 19, 8, RGB565(240, 95, 45),
                          cardBg);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString("A", rightX + 18, row2Y + 19, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString(isVI ? "BÁO THỨC" : "ALARM", rightX + 30, row2Y + 19, 1);

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(theme.textMain);
    _spr.drawString(hardware.getAlarmMelodyName(), rightX + 12, row2Y + 45, 2);

    // Nút chuyển về Khởi động ở cột phải (Smooth Pill đồng tâm R=13, inset 2px
    // R=11)
    uint16_t swBorder = RGB565(0, 200, 240);
    uint16_t swFill = RGB565(14, 38, 52);
    _spr.fillSmoothRoundRect(rightX + 86, row2Y + 16, 52, 26, 13, swBorder,
                             cardBg);
    _spr.fillSmoothRoundRect(rightX + 88, row2Y + 18, 48, 22, 11, swFill,
                             swBorder);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(RGB565(110, 230, 255));
    _spr.drawString(isVI ? "K.ĐỘNG" : "BOOT", rightX + 112, row2Y + 29, 1);
  }

  // ═══════════════════════════════════════════════════════════════════
  //  3. THANH ĐÁY: TRẠNG THÁI MẠNG IP & NÚT [ĐỔI WIFI] (X: 10..310, Y:
  //  201..231, H=30px)
  //     Phủ kín cân đối toàn màn hình (không còn khoảng đen trống thừa phía
  //     dưới!)
  // ═══════════════════════════════════════════════════════════════════
  int16_t botY = 201, botH = 30;
  _spr.fillSmoothRoundRect(10, botY, 300, botH, 9, borderCol, theme.bg);
  _spr.fillSmoothRoundRect(11, botY + 1, 298, botH - 2, 8, cardBg, borderCol);

  int16_t botCy = botY + botH / 2; // Y = 216

  // Chấm tròn trạng thái mạng (Xanh lá khi Online, Đỏ khi Offline)
  bool wifiOk = WiFi.isConnected();
  _spr.fillSmoothCircle(24, botCy, 4, wifiOk ? theme.green : theme.red, cardBg);

  String ipStr = wifiOk ? ("IP: " + WiFi.localIP().toString())
                        : (isVI ? "WiFi: Chưa kết nối" : "WiFi: Offline");
  _spr.setTextDatum(ML_DATUM);
  _spr.setTextColor(theme.textMain);
  _spr.drawString(ipStr.c_str(), 36, botCy, 2);

  // Nút [ĐỔI WIFI] / [WIFI QR] bo tròn viên thuốc khử răng cưa đồng tâm (Smooth
  // Pill R=11, inset 2px R=9)
  _spr.fillSmoothRoundRect(224, botCy - 11, 80, 22, 11, theme.accent, cardBg);
  _spr.fillSmoothRoundRect(226, botCy - 9, 76, 18, 9, RGB565(20, 38, 66),
                           theme.accent);
  _spr.setTextDatum(MC_DATUM);
  _spr.setTextColor(theme.accent);
  _spr.drawString(isVI ? "ĐỔI WIFI" : "WIFI QR", 264, botCy, 1);
}

void Display::renderSettingsBrightnessBarZen16(bool forceFull) {
  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenDim = RGB565(96, 102, 114);
  const uint16_t zenBorder = RGB565(42, 46, 56);
  const uint16_t zenBtnBorder = RGB565(68, 74, 86);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenGreen = RGB565(52, 211, 153);

  bool isAuto = hardware.isAutoBrightness();
  int curBri = hardware.getBacklight();

  if (forceFull) {
    VnSprite::drawOutlineRoundRect16(_tft, 10, 6, 300, 46, 12, zenBg, zenBorder,
                                     zenBg);

    // [-] button (16..54, 13..45)
    VnSprite::drawOutlineRoundRect16(_tft, 16, 13, 38, 32, 10, zenBg,
                                     zenBtnBorder, zenBg);
    _tft.fillRect(30, 28, 10, 2, zenText);

    // [+] button (202..240, 13..45)
    VnSprite::drawOutlineRoundRect16(_tft, 202, 13, 38, 32, 10, zenBg,
                                     zenBtnBorder, zenBg);
    _tft.fillRect(216, 28, 10, 2, zenText);
    _tft.fillRect(220, 24, 2, 10, zenText);

    // Tiny gold sun icon beside brightness %
    _tft.fillCircle(110, 20, 3, zenGold);
    _tft.drawPixel(110, 15, zenGold);
    _tft.drawPixel(110, 25, zenGold);
    _tft.drawPixel(105, 20, zenGold);
    _tft.drawPixel(115, 20, zenGold);

    // [AUTO] pill (246..304, 13..45)
    _tft.fillRect(248, 15, 54, 28, zenBg);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, L(TXT_AUTO), 275, 29,
                               MC_DATUM, isAuto ? zenGreen : zenDim, zenBg, 255,
                               19, 40, 20);
    VnSprite::drawOutlineRing16(_tft, 246, 13, 58, 32, 10, zenBg,
                                isAuto ? zenGreen : zenBtnBorder, zenBg);
  }

  // Percent label (16-bit atomic overwrite — zero flicker)
  char bStr[12];
  snprintf(bStr, sizeof(bStr), "%d%%", curBri);
  VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, bStr, 122, 20, ML_DATUM,
                             zenText, zenBg, 120, 11, 52, 18);

  // Slim Apple slider strip pushed atomically via _spr (y=31..45)
  _spr.fillRect(60, 31, 136, 14, zenBg);
  _spr.fillRoundRect(64, 35, 128, 5, 2, RGB565(36, 40, 48));
  int16_t fillW = map(constrain(curBri, 0, 100), 0, 100, 4, 128);
  uint16_t barCol = isAuto ? zenGreen : zenAccent;
  _spr.fillRoundRect(64, 35, fillW, 5, 2, barCol);
  int16_t knobX = constrain(64 + fillW - 2, 68, 188);
  _spr.fillSmoothCircle(knobX, 37, 4, zenText, zenBg);
  _spr.pushSprite(60, 31, 60, 31, 136, 14);
}

void Display::renderPage5_SettingsZen16(bool forceFull) {
  int curBri = hardware.getBacklight();
  bool isAuto = hardware.isAutoBrightness();
  bool wifiOk = WiFi.isConnected();
  bool autoChanged = (isAuto != _zen16LastAutoBri);
  bool briChanged = (curBri != _zen16LastBri) || autoChanged;

  if (forceFull) {
    _zen16SetDirty = 0x3F;
  } else if (wifiOk != _zen16LastWifiOk) {
    _zen16SetDirty |= 0x20;
  }

  if (!forceFull && _zen16SetDirty == 0) {
    if (briChanged) {
      _zen16LastBri = curBri;
      _zen16LastAutoBri = isAuto;
      renderSettingsBrightnessBarZen16(autoChanged);
    }
    return;
  }

  uint8_t dirty = _zen16SetDirty;
  _zen16SetDirty = 0;
  _zen16LastBri = curBri;
  _zen16LastAutoBri = isAuto;
  _zen16LastWifiOk = wifiOk;

  const uint16_t zenBg = C_BLACK;
  const uint16_t zenText = C_WHITE;
  const uint16_t zenMuted = RGB565(170, 178, 190);
  const uint16_t zenDim = RGB565(96, 102, 114);
  const uint16_t zenBorder = RGB565(42, 46, 56);
  const uint16_t zenAccent = RGB565(100, 210, 255);
  const uint16_t zenGold = RGB565(235, 190, 95);
  const uint16_t zenGreen = RGB565(52, 211, 153);
  const uint16_t zenCoral = RGB565(255, 95, 87);
  const uint16_t zenPurple = RGB565(175, 130, 255);

  bool isVI = (getLanguage() == LANG_VI);
  char fb[48];

  // Only a full page entry clears the screen; tile taps repaint ONLY the
  // touched tile (no whole-screen blink).
  if (forceFull)
    _tft.fillScreen(zenBg);

  if (dirty & 0x01)
    renderSettingsBrightnessBarZen16(true);
  else if (briChanged)
    renderSettingsBrightnessBarZen16(autoChanged);

  const int16_t tW = 146, tH = 64;
  const int16_t leftX = 10, rightX = 164;
  const int16_t row1Y = 59, row2Y = 130;

  // Tile 1: Language (10..156, 59..123)
  if (dirty & 0x02) {
    VnSprite::drawOutlineRoundRect16(_tft, leftX, row1Y, tW, tH, 12, zenBg,
                                     zenBorder, zenBg);
    _tft.fillCircle(leftX + 14, row1Y + 18, 3, zenGreen);
    VnSprite::fitText(&ZEN_FONT_LIGHT, isVI ? "NGÔN NGỮ" : "LANGUAGE", 108, fb,
                      sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, leftX + 24,
                               row1Y + 18, ML_DATUM, zenDim, zenBg, leftX + 22,
                               row1Y + 8, 114, 20);
    VnSprite::fitText(&ZEN_FONT_LIGHT, getLanguageName(), 100, fb, sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, leftX + 12,
                               row1Y + 45, ML_DATUM, zenText, zenBg, leftX + 10,
                               row1Y + 35, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ">", leftX + tW - 12,
                               row1Y + 45, MR_DATUM, zenDim, zenBg,
                               leftX + tW - 24, row1Y + 35, 16, 20);
  }

  // Tile 2: Theme + [ZEN] Toggle (164..310, 59..123)
  if (dirty & 0x04) {
    VnSprite::drawOutlineRoundRect16(_tft, rightX, row1Y, tW, tH, 12, zenBg,
                                     zenBorder, zenBg);
    _tft.fillCircle(rightX + 14, row1Y + 18, 3, zenPurple);
    VnSprite::fitText(&ZEN_FONT_LIGHT, isVI ? "GIAO DIỆN" : "THEME", 64, fb,
                      sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, rightX + 24,
                               row1Y + 18, ML_DATUM, zenDim, zenBg, rightX + 22,
                               row1Y + 8, 68, 20);

    // Label first, 1px ring AFTER (ring never gets clipped by a text box)
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, "ZEN", rightX + 116,
                               row1Y + 19, MC_DATUM, zenGold, zenBg,
                               rightX + 104, row1Y + 10, 24, 18);
    VnSprite::drawOutlineRing16(_tft, rightX + 94, row1Y + 8, 44, 22, 11, zenBg,
                                zenGold, zenBg);

    VnSprite::fitText(&ZEN_FONT_LIGHT, getThemeName(), 108, fb, sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, rightX + 12,
                               row1Y + 45, ML_DATUM, zenAccent, zenBg,
                               rightX + 10, row1Y + 35, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ">", rightX + tW - 12,
                               row1Y + 45, MR_DATUM, zenDim, zenBg,
                               rightX + tW - 24, row1Y + 35, 16, 20);
  }

  // Tile 3: Touch Sound (10..156, 130..194)
  if (dirty & 0x08) {
    VnSprite::drawOutlineRoundRect16(_tft, leftX, row2Y, tW, tH, 12, zenBg,
                                     zenBorder, zenBg);
    _tft.fillCircle(leftX + 14, row2Y + 18, 3, zenAccent);
    VnSprite::fitText(&ZEN_FONT_LIGHT, isVI ? "ÂM CHẠM" : "TOUCH", 108, fb,
                      sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, leftX + 24,
                               row2Y + 18, ML_DATUM, zenDim, zenBg, leftX + 22,
                               row2Y + 8, 114, 20);
    VnSprite::fitText(&ZEN_FONT_LIGHT, hardware.getTouchSoundName(), 100, fb,
                      sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, leftX + 12,
                               row2Y + 45, ML_DATUM, zenText, zenBg, leftX + 10,
                               row2Y + 35, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ">", leftX + tW - 12,
                               row2Y + 45, MR_DATUM, zenDim, zenBg,
                               leftX + tW - 24, row2Y + 35, 16, 20);
  }

  // Tile 4: Boot / Alarm Sound (164..310, 130..194; mode pill x>=248)
  if (dirty & 0x10) {
    VnSprite::drawOutlineRoundRect16(_tft, rightX, row2Y, tW, tH, 12, zenBg,
                                     zenBorder, zenBg);
    const char *lbl, *pillTxt, *val;
    uint16_t dotCol, pillCol;
    if (_settingsSoundIsStartup) {
      lbl = isVI ? "Khởi động" : "Boot";
      pillTxt = isVI ? "Chuông" : "Alarm";
      val = hardware.getStartupSoundName();
      dotCol = zenAccent;
      pillCol = zenCoral;
    } else {
      lbl = isVI ? "Báo thức" : "Alarm";
      pillTxt = isVI ? "K.động" : "Boot";
      val = hardware.getAlarmMelodyName();
      dotCol = zenCoral;
      pillCol = zenAccent;
    }
    _tft.fillCircle(rightX + 10, row2Y + 19, 3, dotCol);
    VnSprite::fitText(&ZEN_FONT_LIGHT, lbl, 62, fb, sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, rightX + 16,
                               row2Y + 19, ML_DATUM, zenDim, zenBg, rightX + 15,
                               row2Y + 9, 66, 20);

    VnSprite::fitText(&ZEN_FONT_LIGHT, pillTxt, 40, fb, sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, 275, row2Y + 19,
                               MC_DATUM, pillCol, zenBg, 257, row2Y + 10, 36,
                               18);
    VnSprite::drawOutlineRing16(_tft, 248, row2Y + 8, 54, 22, 11, zenBg,
                                pillCol, zenBg);

    VnSprite::fitText(&ZEN_FONT_LIGHT, val, 100, fb, sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, rightX + 12,
                               row2Y + 45, ML_DATUM, zenText, zenBg,
                               rightX + 10, row2Y + 35, 114, 20);
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ">", rightX + tW - 12,
                               row2Y + 45, MR_DATUM, zenDim, zenBg,
                               rightX + tW - 24, row2Y + 35, 16, 20);
  }

  // Bottom System & WiFi Bar (10, 201, 300, 30)
  if (dirty & 0x20) {
    VnSprite::drawOutlineRoundRect16(_tft, 10, 201, 300, 30, 10, zenBg,
                                     zenBorder, zenBg);
    _tft.fillCircle(24, 216, 3, wifiOk ? zenGreen : zenCoral);

    String ipStr = wifiOk ? ("IP: " + WiFi.localIP().toString())
                          : (isVI ? "WiFi: Chưa kết nối" : "WiFi: Offline");
    char ipFit[48];
    VnSprite::fitText(&ZEN_FONT_LIGHT, ipStr.c_str(), 176, ipFit,
                      sizeof(ipFit));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, ipFit, 35, 216,
                               ML_DATUM, zenMuted, zenBg, 34, 206, 184, 20);

    VnSprite::fitText(&ZEN_FONT_LIGHT, isVI ? "Đổi WiFi" : "WiFi QR", 62, fb,
                      sizeof(fb));
    VnSprite::drawSmoothFace16(_tft, &ZEN_FONT_LIGHT, fb, 264, 216, MC_DATUM,
                               zenAccent, zenBg, 234, 207, 60, 18);
    VnSprite::drawOutlineRing16(_tft, 224, 205, 80, 22, 11, zenBg, zenAccent,
                                zenBg);
  }

  if (!_dockVisible) {
    _tft.fillRoundRect(134, 236, 52, 3, 1, zenDim);
  }
}

// ─── Touch Handler for Page 5 Settings (Bento Grid 2x2 + Wide Slider) ──
bool Display::handleTouchSettings(int16_t x, int16_t y) {
  // ── 1. Thẻ Độ sáng trên cùng (Y: 0..55 — H=55px bao phủ cả sát mép trên) ──
  if (y <= 55) {
    if (x >= 244) {
      // Nút [AUTO] (X: 244..319): Bật / tắt cảm biến quang LDR tự động
      hardware.setAutoBrightness(!hardware.isAutoBrightness());
      hardware.playTouchSound();
      _lastRender = 0;
      return true;
    } else if (x <= 56) {
      // Nút [ - ] bên trái (X: 0..56): Giảm độ sáng từng nấc 10% (Tối thiểu 5%)
      hardware.setAutoBrightness(false);
      int curB = hardware.getBacklight();
      int newB = ((curB - 1) / 10) * 10;
      if (newB < 5)
        newB = 5;
      hardware.setBacklight(newB);
      hardware.playTouchSound();
      _lastRender = 0;
      return true;
    } else if (x >= 200 && x < 244) {
      // Nút [ + ] bên phải thanh (X: 200..243): Tăng độ sáng từng nấc 10% (Tối
      // đa 100%)
      hardware.setAutoBrightness(false);
      int curB = hardware.getBacklight();
      int newB = ((curB / 10) + 1) * 10;
      if (newB > 100)
        newB = 100;
      hardware.setBacklight(newB);
      hardware.playTouchSound();
      _lastRender = 0;
      return true;
    } else {
      // Thanh độ sáng ở giữa (X: 57..199): Chạm trực tiếp hoặc kéo trượt mượt
      // mà 5%..100%
      bool wasAuto = hardware.isAutoBrightness();
      hardware.setAutoBrightness(false);
      int oldB = hardware.getBacklight();
      int newB = map(constrain((int)x, 62, 194), 62, 194, 5, 100);
      newB = ((newB + 2) / 5) * 5; // Làm tròn mỗi bậc 5%
      newB = constrain(newB, 5, 100);
      if (newB != oldB || wasAuto) {
        hardware.setBacklight(newB);
        // Chỉ phát âm chạm nhẹ khi nhảy qua từng mốc 10% để không làm khựng
        // thao tác kéo trượt
        if (wasAuto || (newB / 10) != (oldB / 10)) {
          hardware.playTouchSound();
        }
        _lastRender = 0;
      }
      return true;
    }
  }

  // ── 2. Hàng 1 của Bento Grid (Y: 56..126 — Cao 70px) ──────────────────
  if (y >= 56 && y <= 126) {
    if (x < 160) {
      // Ô 1 (Trái - Trên): Đổi Ngôn ngữ (Tiếng Việt <-> English)
      toggleLanguage();
      hardware.playTouchSound();
      _zen16SetDirty = 0x3F;
      _lastRender = 0;
      return true;
    } else {
      // Ô 2 (Phải - Trên): Góc trên-phải [ZEN]/[STD] (x >= 252, y <= 98) -> Đổi
      // phong cách Minimal Zen <-> Classic Còn lại -> Đổi Theme màu kế tiếp
      if (x >= 252 && y <= 98) {
        toggleMinimalUiMode();
        _zen16ScreenValid = false;
      } else {
        nextTheme();
        _zen16SetDirty |= 0x04;
      }
      hardware.playTouchSound();
      _lastRender = 0;
      return true;
    }
  }

  // ── 3. Hàng 2 của Bento Grid (Y: 127..200 — Bao phủ dư thêm 6px dưới đáy 2
  // card để bấm dòng chữ dưới không lệch) ──
  if (y >= 127 && y <= 200) {
    if (x < 160) {
      // Ô 3 (Trái - Dưới): Đổi Âm chạm cảm ứng (nextTouchSound đã tự phát thử
      // âm mới)
      hardware.nextTouchSound();
      _zen16SetDirty |= 0x08;
      _lastRender = 0;
      return true;
    } else {
      // Ô 4 (Phải - Dưới): Âm Khởi động ↔ Chuông Báo thức (Cách 1)
      // • Cột phải (x >= 246, suốt chiều cao ô): Nút [CHUONG]/[ALARM] <->
      // [K.DONG]/[BOOT] để lật chế độ • Cột trái (x < 246, suốt chiều cao ô):
      // Đổi giai điệu kế tiếp và tự phát thử ngay!
      if (x >= 246) {
        _settingsSoundIsStartup = !_settingsSoundIsStartup;
        hardware.playTouchSound();
        _zen16SetDirty |= 0x10;
        _lastRender = 0;
        return true;
      } else {
        if (_settingsSoundIsStartup) {
          hardware.nextStartupSound();
          if (hardware.getStartupSound() != STARTUP_SND_MUTE) {
            hardware.playStartupSound();
          } else {
            hardware.playTouchSound();
          }
        } else {
          hardware.nextAlarmMelody();
          hardware.playAlarmMelody();
        }
        _zen16SetDirty |= 0x10;
        _lastRender = 0;
        return true;
      }
    }
  }

  // ── 4. Thanh Đáy Hệ thống & WiFi (Y: 201..239) ────────────────────────
  // • Góc phải (x >= 210): Nút [DOI WIFI] / [WIFI QR] (Vẽ ở X: 224..304) -> Mở
  // trang mã QR cài đặt WiFi • Chính giữa đáy (x: 115..205, y >= 212): Vùng
  // viên thuốc chỉ báo Dock (X: 134..186, Y: 236) -> Trả về false để đánh thức
  // Dock! • Góc trái dưới (x < 115): Chặn cảm ứng tại chỗ để bấm lệch dưới card
  // trái không bao giờ làm bật Dock
  if (y >= 201) {
    if (x >= 210) {
      hardware.playTouchSound();
      _zen16ScreenValid = false;
      webDashboard.requestWiFiPortal();
      _lastRender = 0;
      return true;
    }
    if (x >= 115 && x <= 205 && y >= 212) {
      return false; // Cho phép đi xuống bước đánh thức thanh Dock
    }
    return true; // Chặn mọi điểm chạm lệch bên dưới card trái hoặc vùng đệm
  }

  return false;
}
