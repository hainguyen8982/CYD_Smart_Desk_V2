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
#include "WeatherService.h"
#include "TouchManager.h"
#include "vn_lunar.h"
#include "HardwareManager.h"
#include "DockIconsColor20.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <time.h>

Display display;

static TFT_eSprite* s_sprInstance = nullptr;

bool Display::tjpgRenderCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!s_sprInstance) return false;
    // Dim pixels by ~38% (62% luminance) for optimal contrast with neon and white text
    for (int i = 0; i < w * h; i++) {
        uint16_t p = bitmap[i];
        uint16_t r = (((p >> 11) & 0x1F) * 62) / 100;
        uint16_t g = (((p >> 5)  & 0x3F) * 62) / 100;
        uint16_t b = (( p        & 0x1F) * 62) / 100;
        bitmap[i] = (r << 11) | (g << 5) | b;
    }
    s_sprInstance->pushImage(x, y, w, h, bitmap);
    return true;
}

void Display::notifyWallpaperUpdated() {
    _hasWallpaper = LittleFS.exists("/wallpaper.jpg");
    _wpCheckNeeded = true;
}

// ─── Constructor ─────────────────────────────────────────────────────
Display::Display()
    : _hasWallpaper(false), _wpCheckNeeded(true),
      _tft(), _spr(&_tft), _sprReady(false), _lastRender(0),
      _currentPage(PAGE_CLOCK), _dockVisible(true), _lastDockActivity(0),
      _activePopup(POPUP_NONE),
      _calYear(0), _calMonth(0),
      _pomoMode(POMO_FOCUS), _pomoState(POMO_STOPPED),
      _pomoTotalSec(25 * 60), _pomoRemainingSec(25 * 60),
      _pomoSession(1), _pomoLastSecTick(0) {}

// ─── Begin ───────────────────────────────────────────────────────────
void Display::begin() {
    initTheme();                   // Load saved theme from NVS

    // Mount LittleFS for custom wallpaper storage
    if (LittleFS.begin(true)) {
        Serial.println("[Display] LittleFS mounted successfully");
        _hasWallpaper = LittleFS.exists("/wallpaper.jpg");
        if (_hasWallpaper) {
            Serial.println("[Display] Custom wallpaper detected (/wallpaper.jpg)");
        }
    } else {
        Serial.println("[Display] LittleFS mount failed");
    }

    _tft.init();
    delay(100);
    _tft.setRotation(1);           // Landscape 320×240
    _tft.invertDisplay(true);      // CYD panel requires color inversion
    _tft.fillScreen(theme.bg);

    // Set backlight on
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, 200);

    // Show boot splash immediately so screen is NEVER blank while WiFi connects
    _tft.setTextColor(C_WHITE, C_BLACK);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("CYD SmartDesk V2", SCREEN_W / 2, SCREEN_H / 2 - 25, 4);
    _tft.setTextColor(C_CYAN, C_BLACK);
    _tft.drawString("Initializing...", SCREEN_W / 2, SCREEN_H / 2 + 15, 2);

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
        Serial.printf("[Display] Sprite created OK! Depth: %d-bit (%d bytes)\n",
                      _spr.getColorDepth(),
                      SCREEN_W * SCREEN_H * (_spr.getColorDepth() / 8));
    } else {
        Serial.println("[Display] ERROR: Both 16-bit and 8-bit Sprite alloc failed!");
        _sprReady = false;
        _tft.setTextColor(TFT_RED, C_BLACK);
        _tft.drawString("RAM Error: Sprite failed!", SCREEN_W / 2, SCREEN_H / 2 + 40, 2);
    }
}

void Display::showBootStatus(const char* status) {
    if (!status) return;
    _tft.setTextDatum(MC_DATUM);
    _tft.fillRect(0, SCREEN_H / 2, SCREEN_W, 60, C_BLACK);
    _tft.setTextColor(C_CYAN, C_BLACK);
    _tft.drawString(status, SCREEN_W / 2, SCREEN_H / 2 + 15, 2);
}

void Display::update() {
    if (!_sprReady) return;

    // Duy trì bộ đếm Pomodoro chạy ngầm liên tục
    pomoTick();

    unsigned long now = millis();
    if (now - _lastRender < DISPLAY_FPS_INTERVAL) return;
    _lastRender = now;

    // If in touch calibration mode, render crosshair overlay
    if (touch.isCalibrating()) {
        renderCalibrationScreen();
        _spr.pushSprite(0, 0);
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
        default:            renderPage0_WeatherClock(); break;
    }

    // If popup modal is active, render it over page; otherwise render dock
    if (_activePopup == POPUP_WEATHER) {
        renderWeatherPopup();
    } else if (_activePopup == POPUP_LUNAR) {
        renderLunarPopup();
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

    // ────────────────────────────────────────────────────────────────
    // 3. DATE LINE — Centered with ample space (Y = 107, Font 2, Pure White)
    // ────────────────────────────────────────────────────────────────
    const char* vnDayNames[] = {
        "CHU NHAT", "THU HAI", "THU BA", "THU TU",
        "THU NAM", "THU SAU", "THU BAY"
    };
    char dateBuf[40];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
             vnDayNames[t.tm_wday],
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
    const char* condStr = "Quang";
    switch (wType) {
        case WEATHER_CLEAR:  condStr = night ? "Dem quang" : "Troi nang"; break;
        case WEATHER_CLOUDY: condStr = "Co may"; break;
        case WEATHER_RAIN:   condStr = "Co mua"; break;
        case WEATHER_STORM:  condStr = "Mua dong"; break;
        case WEATHER_SNOW:   condStr = "Tuyet"; break;
        case WEATHER_MIST:   condStr = "Suong mu"; break;
        default:             condStr = "Troi dep"; break;
    }
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(condStr, leftX + 50, cardY + 60, 1);

    // ── RIGHT CARD: AM LICH (X = 164..310, H = 82) ──────────────────
    int16_t rightX = 164;
    drawGlassCard(rightX, cardY, cardW, cardH, 12);

    uint8_t lunarDay = lDate.day;
    float moonPct = 0;
    const char* phaseName = "Trang Non";
    if (lunarDay <= 1)       { phaseName = "Trang Non";        moonPct = 0; }
    else if (lunarDay <= 6)  { phaseName = "Trang Luoi Liem";  moonPct = lunarDay * 6.7f; }
    else if (lunarDay <= 8)  { phaseName = "Ban Nguyet Dau";   moonPct = 50; }
    else if (lunarDay <= 13) { phaseName = "Trang Truong Dan"; moonPct = 50 + (lunarDay - 8) * 6.0f; }
    else if (lunarDay <= 16) { phaseName = "Trang Tron (Ram)"; moonPct = 100; }
    else if (lunarDay <= 21) { phaseName = "Trang Khuyet Dan"; moonPct = 100 - (lunarDay - 16) * 6.0f; }
    else if (lunarDay <= 23) { phaseName = "Ban Nguyet Cuoi";  moonPct = 50; }
    else                     { phaseName = "Trang Tan Cuoi";   moonPct = 50 - (lunarDay - 23) * 7.0f; }

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
    _spr.drawString(alDayStr, rightX + 48, cardY + 31, 4);

    _spr.setTextColor(RGB565(140, 205, 255));
    _spr.drawString("AL", rightX + 48 + _spr.textWidth(alDayStr, 4) + 4, cardY + 33, 2);

    // Can Chi day placed 6px cleanly below 04/09 AL — NEVER overlaps!
    char dayCanChiBuf[28];
    snprintf(dayCanChiBuf, sizeof(dayCanChiBuf), "Ngay %s", lDate.dayCanChi);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(dayCanChiBuf, rightX + 50, cardY + 60, 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 1: CHI TIET THOI TIET (Tầng 1: Hiện tại | Tầng 2: Dự báo 3 ngày)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderWeatherPopup() {
    int16_t mx = 20;
    int16_t my = 20;
    int16_t mw = 280;
    int16_t mh = 195;
    int16_t mr = 16;

    // Dark frosted glass backing
    _spr.fillRoundRect(mx, my, mw, mh, mr, RGB565(10, 15, 26));
    _spr.drawRoundRect(mx, my, mw, mh, mr, RGB565(0, 200, 255));
    _spr.drawRoundRect(mx + 1, my + 1, mw - 2, mh - 2, mr - 1, RGB565(0, 80, 160));
    _spr.drawFastHLine(mx + mr, my, mw - mr * 2, C_WHITE);

    const WeatherData& w = weather.getData();
    WeatherType wType = weather.getType();
    bool night = weather.isNight();

    // 1. Header (Y = my + 16): GPS Pin + City Name
    _spr.fillCircle(mx + 16, my + 16, 3, C_RED);
    _spr.fillTriangle(mx + 14, my + 16, mx + 18, my + 16, mx + 16, my + 21, C_RED);
    const char* cityStr = (strlen(w.city) > 0) ? w.city : "TP. HO CHI MINH";
    char cityUpper[28];
    size_t cj = 0;
    for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
        cityUpper[cj] = (char)toupper((unsigned char)cityStr[cj]);
    }
    cityUpper[cj] = '\0';
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cityUpper, mx + 26, my + 16, 2);

    // Close button [ X ]
    _spr.fillRoundRect(mx + mw - 26, my + 8, 18, 16, 4, RGB565(40, 50, 70));
    _spr.setTextColor(C_WHITE);
    _spr.setTextDatum(MC_DATUM);
    _spr.drawString("X", mx + mw - 17, my + 16, 2);

    // Top divider line (Y = my + 30)
    _spr.drawFastHLine(mx + 12, my + 30, mw - 24, RGB565(40, 60, 90));

    // ══════════════════════════════════════════════════════════════════
    // TẦNG 1: Thời tiết hiện tại (Layout chuẩn: Icon to bên trái, 2 dòng thông số bên phải)
    // ══════════════════════════════════════════════════════════════════
    // Cột trái: Icon thời tiết lớn (tâm Y = my + 57, cân đối hoàn hảo giữa 2 vạch kẻ)
    drawWeatherIcon(mx + 25, my + 57, wType, night);

    // Cột phải: Canh trái từ infoX = mx + 50 (ngay sau icon)
    int16_t infoX = mx + 50;

    // Dòng 1 (Y = my + 38): Nhiệt độ to + Trạng thái
    char tempNum[8];
    snprintf(tempNum, sizeof(tempNum), "%.0f", w.valid ? w.temp : 29.0f);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(tempNum, infoX, my + 38, 4);

    int16_t numW = _spr.textWidth(tempNum, 4);
    int16_t degX = infoX + numW + 2;
    _spr.drawCircle(degX + 2, my + 41, 2, C_GOLD);
    _spr.setTextColor(C_GOLD);
    _spr.drawString("C", degX + 7, my + 40, 2);

    // Dấu chấm phân cách •
    _spr.setTextColor(RGB565(90, 130, 180));
    _spr.drawString("•", degX + 23, my + 40, 2);

    const char* condStr = "Quang dang";
    switch (wType) {
        case WEATHER_CLEAR:  condStr = night ? "Dem quang" : "Troi nang dep"; break;
        case WEATHER_CLOUDY: condStr = "Nhieu may"; break;
        case WEATHER_RAIN:   condStr = "Troi mua rao"; break;
        case WEATHER_STORM:  condStr = "Mua dong set"; break;
        case WEATHER_SNOW:   condStr = "Tuyet roi"; break;
        case WEATHER_MIST:   condStr = "Suong mu"; break;
        default:             condStr = "Troi mat me"; break;
    }
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(condStr, degX + 33, my + 40, 2);

    // Dòng 2 (Y = my + 68): 3 thông số Độ ẩm • Gió • UV tinh gọn (bỏ chữ thừa vì đã có icon)
    _spr.setTextDatum(ML_DATUM);

    char humStr[16];
    snprintf(humStr, sizeof(humStr), "%d%%", w.valid ? w.humidity : 78);
    char windStr[20];
    snprintf(windStr, sizeof(windStr), "%.1f km/h", w.valid ? w.windSpeed : 7.2f);
    char uvStr[16];
    float uv = w.valid ? w.uvIndex : 6.0f;
    snprintf(uvStr, sizeof(uvStr), "UV %.0f", uv);

    const int16_t gap = 10; // Khoảng cách đệm đồng đều rộng rãi
    int16_t currX = infoX;

    // 1. Độ ẩm (Giọt nước + 78%, nâng lên 1px cho cân chuẩn trục với số)
    drawMiniDrop(_spr, currX + 2, my + 67, C_CYAN);
    _spr.setTextColor(RGB565(170, 205, 235));
    _spr.drawString(humStr, currX + 11, my + 68, 1);
    currX += 11 + _spr.textWidth(humStr, 1) + gap;

    // Dấu chấm phân cách 1 •
    _spr.setTextColor(RGB565(80, 110, 150));
    _spr.drawString("•", currX, my + 68, 1);
    currX += _spr.textWidth("•", 1) + gap;

    // 2. Tốc độ gió (Cánh gió + 7.2 km/h)
    drawMiniWind(_spr, currX + 2, my + 68, RGB565(120, 210, 255));
    _spr.setTextColor(RGB565(170, 205, 235));
    _spr.drawString(windStr, currX + 12, my + 68, 1);
    currX += 12 + _spr.textWidth(windStr, 1) + gap;

    // Dấu chấm phân cách 2 •
    _spr.setTextColor(RGB565(80, 110, 150));
    _spr.drawString("•", currX, my + 68, 1);
    currX += _spr.textWidth("•", 1) + gap;

    // 3. Chỉ số UV (Mặt trời + UV 6)
    drawMiniSun(_spr, currX + 2, my + 68, C_GOLD);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(uvStr, currX + 12, my + 68, 1);

    // Đường kẻ phân cách giữa Tầng 1 và Tầng 2 (Y = my + 84)
    _spr.drawFastHLine(mx + 12, my + 84, mw - 24, RGB565(40, 60, 90));

    // ══════════════════════════════════════════════════════════════════
    // TẦNG 2: 3 Cột Dự Báo Thời Tiết Liền Mạch (Phân cách bằng 2 vạch dọc thanh mảnh)
    // ══════════════════════════════════════════════════════════════════
    int16_t colW = (mw - 16) / 3; // ~88px mỗi cột
    int16_t startX = mx + 8;

    // Vẽ 2 vạch ngăn cách dọc mảnh, tinh tế giữa 3 cột (từ Y = my + 94 đến my + 184)
    _spr.drawFastVLine(startX + colW, my + 94, 90, RGB565(25, 40, 62));
    _spr.drawFastVLine(startX + colW * 2, my + 94, 90, RGB565(25, 40, 62));

    for (int d = 0; d < 3; d++) {
        int16_t colCenterX = startX + d * colW + colW / 2;

        // 1. Tên Thứ (Font 2, đồng bộ màu trắng sáng cho cả 3 ngày)
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString(w.forecast[d].dayName, colCenterX, my + 104, 2);

        // 2. Weather Icon mini (16px, nằm thoáng ở tâm)
        drawForecastIcon(colCenterX, my + 127, w.forecast[d].type);

        // 3. Trạng thái thời tiết (Font 1, Gold)
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(255, 215, 0)); // Gold
        _spr.drawString(w.forecast[d].condText, colCenterX, my + 149, 1);

        // 4. Nhiệt độ Min - Max (Font 1)
        char tRange[16];
        snprintf(tRange, sizeof(tRange), "%.0f - %.0f C", w.forecast[d].tempMin, w.forecast[d].tempMax);
        _spr.setTextColor(RGB565(200, 225, 245));
        _spr.drawString(tRange, colCenterX, my + 169, 1);
        _spr.drawCircle(colCenterX + _spr.textWidth(tRange, 1) / 2 - 6, my + 166, 1, C_GOLD);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  POPUP MODAL 2: CHI TIET THIEN VAN & AM LICH
// ═══════════════════════════════════════════════════════════════════════
void Display::renderLunarPopup() {
    int16_t mx = 20;
    int16_t my = 20;
    int16_t mw = 280;
    int16_t mh = 195;
    int16_t mr = 16;

    // Dark glass backing with golden/cyan border
    _spr.fillRoundRect(mx, my, mw, mh, mr, RGB565(10, 15, 26));
    _spr.drawRoundRect(mx, my, mw, mh, mr, RGB565(255, 215, 0));
    _spr.drawRoundRect(mx + 1, my + 1, mw - 2, mh - 2, mr - 1, RGB565(180, 140, 20));
    _spr.drawFastHLine(mx + mr, my, mw - mr * 2, C_WHITE);

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    LunarDate ld = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1, 1900 + t.tm_year);

    uint8_t lunarDay = ld.day;
    float moonPct = 0;
    const char* phaseName = "Trang Non";
    if (lunarDay <= 1)       { phaseName = "Trang Non";        moonPct = 0; }
    else if (lunarDay <= 6)  { phaseName = "Trang Luoi Liem";  moonPct = lunarDay * 6.7f; }
    else if (lunarDay <= 8)  { phaseName = "Ban Nguyet Dau";   moonPct = 50; }
    else if (lunarDay <= 13) { phaseName = "Trang Truong Dan"; moonPct = 50 + (lunarDay - 8) * 6.0f; }
    else if (lunarDay <= 16) { phaseName = "Trang Tron (Ram)"; moonPct = 100; }
    else if (lunarDay <= 21) { phaseName = "Trang Khuyet Dan"; moonPct = 100 - (lunarDay - 16) * 6.0f; }
    else if (lunarDay <= 23) { phaseName = "Ban Nguyet Cuoi";  moonPct = 50; }
    else                     { phaseName = "Trang Tan Cuoi";   moonPct = 50 - (lunarDay - 23) * 7.0f; }

    // 1. Header (Y = my + 16)
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0));
    _spr.drawString("THIEN VAN & AM LICH", mx + 16, my + 16, 2);

    // Close button [ X ]
    _spr.fillRoundRect(mx + mw - 26, my + 8, 18, 16, 4, RGB565(40, 50, 70));
    _spr.setTextColor(C_WHITE);
    _spr.setTextDatum(MC_DATUM);
    _spr.drawString("X", mx + mw - 17, my + 16, 2);

    // 2. Large Moon Orb (R = 20, diameter 40px) + Phase Name + Illumination
    // Centered evenly between Y = my + 30 and Y = my + 100
    int16_t midCy = my + 68;
    drawMoonIcon(mx + 42, midCy, 20, lunarDay, RGB565(10, 15, 26));

    // Top divider line (Y = my + 30) — drawn after moon icon to guarantee 100% crisp, continuous line
    _spr.drawFastHLine(mx + 12, my + 30, mw - 24, RGB565(70, 60, 90));

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(phaseName, mx + 80, my + 54, 4);

    char illuBuf[32];
    snprintf(illuBuf, sizeof(illuBuf), "Do sang mat trang: %.0f%%", moonPct);
    _spr.setTextColor(RGB565(140, 205, 255));
    _spr.drawString(illuBuf, mx + 80, my + 78, 2);

    // Bottom divider line (Y = my + 100)
    _spr.drawFastHLine(mx + 12, my + 100, mw - 24, RGB565(70, 60, 90));

    // 3. Lunar Details List (Evenly spaced without bottom footer)
    // Row 1: Ngay am lich
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString("Ngay am lich:", mx + 20, my + 120, 2);
    char lDateBuf[24];
    snprintf(lDateBuf, sizeof(lDateBuf), "%02d thang %02d", ld.day, ld.month);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(lDateBuf, mx + mw - 20, my + 120, 2);

    // Row 2: Ngay can chi
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString("Ngay can chi:", mx + 20, my + 145, 2);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(ld.dayCanChi, mx + mw - 20, my + 145, 2);

    // Row 3: Nam can chi
    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(RGB565(170, 200, 230));
    _spr.drawString("Nam can chi:", mx + 20, my + 170, 2);
    char yrBuf[32];
    snprintf(yrBuf, sizeof(yrBuf), "%s (2026)", ld.yearCanChi);
    _spr.setTextDatum(MR_DATUM);
    _spr.setTextColor(RGB565(255, 215, 0)); // Gold
    _spr.drawString(yrBuf, mx + mw - 20, my + 170, 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  BACKGROUND — Custom Wallpaper OR Organic Blobs + Sparkle Stars
// ═══════════════════════════════════════════════════════════════════════
void Display::drawBackground() {
    // 1. Try decoding custom wallpaper from LittleFS if present
    if (_hasWallpaper) {
        s_sprInstance = &_spr;
        TJpgDec.setJpgScale(1);
        TJpgDec.setCallback(Display::tjpgRenderCallback);
        if (TJpgDec.drawFsJpg(0, 0, "/wallpaper.jpg", LittleFS) == 0) {
            return; // Successfully rendered wallpaper background!
        }
        // If decoding failed (corrupted file), fallback to procedural
        _hasWallpaper = false;
    }

    // 2. Default: Deep Obsidian Cosmic Sky (Pure velvety black with delicate twinkling stars)
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
    if (p <= 3 && _currentPage != p) {
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
    // 1. If a popup modal is currently open, tap anywhere to close it!
    if (_activePopup != POPUP_NONE) {
        _activePopup = POPUP_NONE;
        hardware.playClick();
        _lastRender = 0; // Trigger instant redraw
        return true;
    }

    // 2. ƯU TIÊN TUYỆT ĐỐI CHO THANH DOCK: Khi Dock đang hiển thị, chạm vào vùng Dock (Y >= 185) luôn xử lý cho Dock!
    if (_dockVisible && y >= 185) {
        wakeDock(); // Reset 5s auto-hide timer
        if (x < 94)                   { setPage(PAGE_CLOCK);    return true; }
        else if (x >= 94 && x < 138)  { setPage(PAGE_CALENDAR); return true; }
        else if (x >= 138 && x < 182) { setPage(PAGE_FINANCE);  return true; }
        else if (x >= 182 && x < 226) { setPage(PAGE_POMODORO); return true; }
        else if (x >= 226)            { nextTheme(); wakeDock(); return true; }
        return true;
    }

    // 3. If on Calendar page and tapping in the top Title Card (Y <= 50)
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

    // 4. If on Clock page (PAGE_CLOCK), tap on Cards to open detailed Popups
    // GIỚI HẠN CHẶT CHẼ TRONG THÂN CARD (Y: 126..194)
    // Chạm từ Y >= 195 trở xuống hoàn toàn dành cho việc bật Dock, KHÔNG BAO GIỜ bị bật nhầm popup!
    if (_currentPage == PAGE_CLOCK && y >= 126 && y <= 194) {
        if (x >= 10 && x <= 156) {
            _activePopup = POPUP_WEATHER;
            hardware.playClick();
            _lastRender = 0; // Trigger instant redraw
            return true;
        } else if (x >= 164 && x <= 310) {
            _activePopup = POPUP_LUNAR;
            hardware.playClick();
            _lastRender = 0; // Trigger instant redraw
            return true;
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

        // B. Chạm vào Vòng tròn / Mascot Cà chua bên trái (X: 15..156, Y: 45..194) -> Đổi Chạy / Tạm dừng
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

    // 6. Khi Dock đang ẩn, chạm vào vùng dưới màn hình (Y >= 196) sẽ đánh thức Dock
    if (!_dockVisible) {
        if (y >= 196) {
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
    _spr.drawString("HIEU CHUAN CAM UNG (TOUCH)", 160, 10, 2);

    CalibState state = touch.getCalibState();
    if (state == CALIB_DONE) {
        _spr.setTextColor(TFT_GREEN, TFT_BLACK);
        _spr.setTextDatum(MC_DATUM);
        _spr.drawString("HIEU CHUAN THANH CONG!", 160, 100, 4);
        _spr.setTextColor(TFT_WHITE, TFT_BLACK);
        _spr.drawString("Dang luu vao NVS Flash...", 160, 135, 2);
        return;
    }

    int tx = 160, ty = 120;
    const char* hint = "";
    const char* step = "";

    switch (state) {
        case CALIB_WAIT_TL:
            tx = CALIB_TL_X; ty = CALIB_TL_Y;
            hint = "Chay tam do GOC TREN - TRAI";
            step = "Buoc 1/3";
            break;
        case CALIB_WAIT_TR:
            tx = CALIB_TR_X; ty = CALIB_TR_Y;
            hint = "Chay tam do GOC TREN - PHAI";
            step = "Buoc 2/3";
            break;
        case CALIB_WAIT_BL:
            tx = CALIB_BL_X; ty = CALIB_BL_Y;
            hint = "Chay tam do GOC DUOI - TRAI";
            step = "Buoc 3/3";
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
    int16_t dockX = 50;
    int16_t dockY = 196;
    int16_t dockW = 220;
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

    // 5 Vector Icon Slots (spaced 44px apart)
    for (int i = 0; i < 5; i++) {
        int16_t cx = dockX + 22 + i * 44;
        int16_t cy = dockY + dockH / 2;
        bool isActive = (i < 4 && _currentPage == i);

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
            case 4: drawDockIconTheme(cx, cy, isActive); break;
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
    snprintf(titleBuf, sizeof(titleBuf), "THANG %02d  •  %d", _calMonth, _calYear);
    _spr.drawString(titleBuf, 160, 17, 2);

    // Dòng phụ: Thông tin Âm lịch chi tiết hôm nay (cách đỉnh gáy lò xo 4px)
    bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);
    char subBuf[48];
    if (isCurrentMonth) {
        LunarDate todayLunar = LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
        snprintf(subBuf, sizeof(subBuf), "Hom nay: %02d/%02d AL  -  Nam %s", 
                 todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
        _spr.setTextColor(RGB565(245, 215, 150)); // Gold dịu
    } else {
        LunarDate midLunar = LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
        snprintf(subBuf, sizeof(subBuf), "Nam %s  •  [Cham ve hom nay]", midLunar.yearCanChi);
        _spr.setTextColor(RGB565(255, 200, 100));
    }
    _spr.drawString(subBuf, 160, 29, 1);

    // ── 4. THANH THỨ (T2 to CN, cách đáy gáy lò xo 4px) ───────────────
    const char* daysOfWeek[] = { "T2", "T3", "T4", "T5", "T6", "T7", "CN" };
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
//  PAGE 2 — FINANCE & 7-DAY SPARKLINE (Full-Height Layout)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage2_Finance() {
    drawBackground();

    // SJC Gold Card — Extended height (H=98px)
    drawGlassCardAccent(12, 8, 296, 98, 10, theme.yellow);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.yellow);
    _spr.drawString("VANG SJC 9999", 22, 16, 4);

    _spr.setTextDatum(TR_DATUM);
    _spr.setTextColor(theme.green);
    _spr.drawString("-0.5M [24h]", 296, 18, 2);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("MUA:", 22, 48, 2);
    _spr.drawString("BAN:", 160, 48, 2);

    _spr.setTextColor(theme.textMain);
    _spr.drawString("141.30 tr", 62, 46, 4);
    _spr.drawString("144.30 tr", 200, 46, 4);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("THE GIOI: $4,064 / oz", 22, 78, 2);

    // Foreign Currency Card + 7-Day Sparkline — Extended height (H=118px down to Y=230)
    drawGlassCardAccent(12, 112, 296, 118, 10, theme.accent);
    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(theme.accent);
    _spr.drawString("TI GIA NGOAI TE (VND)", 22, 120, 2);

    _spr.setTextColor(theme.textMain);
    _spr.drawString("USD: 26,180", 22, 145, 4);
    _spr.drawString("EUR: 28,450", 22, 180, 4);

    // 7-Day Sparkline curve placeholder on right side
    _spr.setTextDatum(TR_DATUM);
    _spr.setTextColor(theme.accentSec);
    _spr.drawString("XU HUONG 7D", 296, 120, 2);

    int16_t pts[7] = { 185, 175, 182, 160, 155, 168, 148 };
    for (int i = 0; i < 6; i++) {
        int16_t x1 = 175 + i * 20;
        int16_t y1 = pts[i];
        int16_t x2 = 175 + (i + 1) * 20;
        int16_t y2 = pts[i + 1];
        _spr.drawLine(x1, y1, x2, y2, theme.accent);
        _spr.drawLine(x1, y1 + 1, x2, y2 + 1, theme.accent);
        _spr.fillCircle(x1, y1, 2, theme.accentSec);
    }
    _spr.fillCircle(175 + 6 * 20, pts[6], 3, theme.accent);
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

    const char* tabNames[] = { "TAP TRUNG", "NGHI 5P", "NGHI 15P" };
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

    // Vẽ chú Cà chua Mascot lớn dễ thương ở trung tâm vòng tròn
    bool isFocusMode = (_pomoMode == POMO_FOCUS);
    drawTomatoMascot(cx, ringCy + 1, isFocusMode);

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
    const char* statusDesc = "DANG TAP TRUNG";
    if (_pomoState == POMO_STOPPED) {
        statusDesc = (_pomoMode == POMO_FOCUS) ? "SAN SANG TAP TRUNG" : "SAN SANG NGHI";
    } else if (_pomoState == POMO_PAUSED) {
        statusDesc = "DANG TAM DUNG";
    } else {
        if (_pomoMode == POMO_SHORT_BREAK) statusDesc = "NGHI NGAN 5 PHUT";
        else if (_pomoMode == POMO_LONG_BREAK) statusDesc = "NGHI DAI 15 PHUT";
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
        _spr.drawString("BAT DAU", bX + bW / 2, btnY + btnH / 2, 2);
    } else if (_pomoState == POMO_RUNNING) {
        // Nút Trái: TAM DUNG
        int16_t b1X = 158, b1W = 74;
        _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, RGB565(245, 180, 45), theme.bg);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(20, 20, 25));
        _spr.drawString("TAM DUNG", b1X + b1W / 2, btnY + btnH / 2, 2);

        // Nút Phải: DAT LAI
        int16_t b2X = 236, b2W = 74;
        _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(35, 42, 52), theme.bg);
        _spr.drawRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(65, 78, 95));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(220, 230, 245));
        _spr.drawString("DAT LAI", b2X + b2W / 2, btnY + btnH / 2, 2);
    } else if (_pomoState == POMO_PAUSED) {
        // Nút Trái: TIEP TUC
        int16_t b1X = 158, b1W = 74;
        _spr.fillSmoothRoundRect(b1X, btnY, b1W, btnH, 16, RGB565(35, 215, 140), theme.bg);
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(RGB565(15, 25, 20));
        _spr.drawString("TIEP TUC", b1X + b1W / 2, btnY + btnH / 2, 2);

        // Nút Phải: DAT LAI
        int16_t b2X = 236, b2W = 74;
        _spr.fillSmoothRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(180, 35, 50), theme.bg);
        _spr.drawRoundRect(b2X, btnY, b2W, btnH, 16, RGB565(220, 60, 80));
        _spr.setTextDatum(MC_DATUM);
        _spr.setTextColor(C_WHITE);
        _spr.drawString("DAT LAI", b2X + b2W / 2, btnY + btnH / 2, 2);
    }
}
