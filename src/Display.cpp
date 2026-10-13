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
#include <time.h>

Display display;

// ─── Constructor ─────────────────────────────────────────────────────
Display::Display()
    : _tft(), _spr(&_tft), _sprReady(false), _lastRender(0),
      _currentPage(PAGE_CLOCK), _dockVisible(true), _lastDockActivity(0),
      _calYear(0), _calMonth(0) {}

// ─── Begin ───────────────────────────────────────────────────────────
void Display::begin() {
    initTheme();                   // Load saved theme from NVS
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

// ─── Update (called from loop) ───────────────────────────────────────
void Display::update() {
    if (!_sprReady) return;

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

    renderDock();

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
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage0_WeatherClock() {
    // ────────────────────────────────────────────────────────────────
    // 1. BACKGROUND — deep navy + pastel blobs + sparkles
    // ────────────────────────────────────────────────────────────────
    drawBackground();

    // ────────────────────────────────────────────────────────────────
    // 2. TIME — Bold rounded digits with cyan outer glow & sparkles
    // ────────────────────────────────────────────────────────────────
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.tm_hour, t.tm_min);

    // Multi-pass neon cyan glow around the numbers (Font 6)
    _spr.setTextDatum(TL_DATUM);

    // Outer halo layer (deep sky cyan)
    _spr.setTextColor(RGB565(0, 100, 180));
    _spr.drawString(timeBuf, 16, 22, 6);
    _spr.drawString(timeBuf, 20, 22, 6);
    _spr.drawString(timeBuf, 18, 20, 6);
    _spr.drawString(timeBuf, 18, 24, 6);

    // Inner bright neon glow layer
    _spr.setTextColor(RGB565(0, 220, 255));
    _spr.drawString(timeBuf, 17, 21, 6);
    _spr.drawString(timeBuf, 19, 21, 6);
    _spr.drawString(timeBuf, 17, 23, 6);
    _spr.drawString(timeBuf, 19, 23, 6);

    // Crisp pure white core
    _spr.setTextColor(C_WHITE);
    _spr.drawString(timeBuf, 18, 22, 6);

    // Cute pastel star sparkles around the clock digits (matching mockup)
    drawSparkle(13,  16, 3, C_SPARKLE_CYAN);
    drawSparkle(78,  14, 2, C_BLOB_PINK);
    drawSparkle(154, 18, 3, C_BLOB_LILAC);
    drawSparkle(164, 56, 2, C_SPARKLE_CYAN);
    drawSparkle(13,  64, 2, C_SPARKLE_W);
    drawStarDot(46,  14, C_WHITE);
    drawStarDot(116, 14, C_CYAN_GLOW);
    drawStarDot(168, 36, C_BLOB_PINK);

    // ────────────────────────────────────────────────────────────────
    // 3. DATE — Clean Vietnamese date line
    // ────────────────────────────────────────────────────────────────
    const char* vnDayNames[] = {
        "CHU NHAT", "THU HAI", "THU BA", "THU TU",
        "THU NAM", "THU SAU", "THU BAY"
    };

    char dateBuf[40];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d/%02d/%d",
             vnDayNames[t.tm_wday],
             t.tm_mday, t.tm_mon + 1, 1900 + t.tm_year);

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(dateBuf, 18, 80, 2);

    // ────────────────────────────────────────────────────────────────
    // 4. MOON PHASE & LUNAR CALENDAR (Astronomical Vietnamese Lunar Calendar)
    // ────────────────────────────────────────────────────────────────
    LunarDate lDate = LunarCalendar::convertSolar2Lunar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    uint8_t lunarDay = lDate.day;

    float moonPct = 0;
    const char* phaseName = "Trang Non";

    if (lunarDay <= 1)       { phaseName = "Trang Non";         moonPct = 0; }
    else if (lunarDay <= 6)  { phaseName = "Trang Luoi Liem";   moonPct = lunarDay * 6.7f; }
    else if (lunarDay <= 8)  { phaseName = "Ban Nguyet Dau";   moonPct = 50; }
    else if (lunarDay <= 13) { phaseName = "Trang Truong Dan";  moonPct = 50 + (lunarDay - 8) * 6.0f; }
    else if (lunarDay <= 16) { phaseName = "Trang Tron (Ram)";  moonPct = 100; }
    else if (lunarDay <= 21) { phaseName = "Trang Khuyet Dan";  moonPct = 100 - (lunarDay - 16) * 6.0f; }
    else if (lunarDay <= 23) { phaseName = "Ban Nguyet Cuoi";  moonPct = 50; }
    else                     { phaseName = "Trang Tan Cuoi";   moonPct = 50 - (lunarDay - 23) * 7.0f; }

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(phaseName, 18, 114, 2);

    // 3D textured moon orb next to phase name
    int16_t moonOrbX = 18 + _spr.textWidth(phaseName, 2) + 10;
    drawMoonIcon(moonOrbX, 122, 9, lunarDay);

    // Vietnamese Lunar Date (Am Lich VN chinh xac theo Ho Ngoc Duc)
    char lunarBuf[32];
    if (lDate.isLeap) {
        snprintf(lunarBuf, sizeof(lunarBuf), "AL: %02d/%02d+ - %s", lDate.day, lDate.month, lDate.yearCanChi);
    } else {
        snprintf(lunarBuf, sizeof(lunarBuf), "AL: %02d/%02d - %s", lDate.day, lDate.month, lDate.yearCanChi);
    }
    _spr.setTextColor(RGB565(140, 205, 255)); // Soft sky cyan
    _spr.drawString(lunarBuf, 18, 140, 2);

    // Can Chi of the Day & illumination
    char dayInfoBuf[32];
    snprintf(dayInfoBuf, sizeof(dayInfoBuf), "Ngay %s (%d%%)", lDate.dayCanChi, (int)moonPct);
    _spr.setTextColor(C_TEXT_DIM);
    _spr.drawString(dayInfoBuf, 18, 164, 1);

    // ────────────────────────────────────────────────────────────────
    // 5. WEATHER CARD — Full-height with City Location Header
    // ────────────────────────────────────────────────────────────────
    const WeatherData& w = weather.getData();
    WeatherType wType = weather.getType();
    bool night = weather.isNight();

    // Use full height (down to Y=232)
    int16_t cardX = 176, cardY = 8;
    int16_t cardW = 136, cardH = 224;

    drawGlassCard(cardX, cardY, cardW, cardH, 14);

    // ── Location Header: GPS Pin + City Name ─────────────────────
    // Pin icon: red dot with downward pointer
    _spr.fillCircle(cardX + 16, cardY + 16, 4, C_RED);
    _spr.fillTriangle(cardX + 13, cardY + 16, cardX + 19, cardY + 16, cardX + 16, cardY + 22, C_RED);
    _spr.drawPixel(cardX + 16, cardY + 15, C_WHITE); // pin center

    // City Name in uppercase Font 2
    const char* cityStr = (strlen(w.city) > 0) ? w.city : "TP. HCM";
    char cityUpper[24];
    size_t cj = 0;
    for (; cityStr[cj] && cj < sizeof(cityUpper) - 1; cj++) {
        cityUpper[cj] = (char)toupper((unsigned char)cityStr[cj]);
    }
    cityUpper[cj] = '\0';

    _spr.setTextDatum(ML_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(cityUpper, cardX + 25, cardY + 16, 2);

    // Frosted separator line
    _spr.drawFastHLine(cardX + 12, cardY + 28, cardW - 24, theme.cardBorder);

    // 3D Weather illustration — shifted down to cardY + 58 to give 10px clear margin under separator line
    drawWeatherIcon(cardX + cardW / 2, cardY + 58, wType, night);

    // Temperature — large bold numbers (Font 6) shifted to tempY = 96
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%.0f", w.valid ? w.temp : 24.0f);

    int16_t numW = _spr.textWidth(tempStr, 6);
    int16_t degSymW = 6;
    int16_t unitW = _spr.textWidth("C", 4);
    int16_t totalW = numW + degSymW + unitW;
    int16_t blockStartX = cardX + (cardW - totalW) / 2;
    int16_t tempY = 96;

    _spr.setTextDatum(TL_DATUM);
    _spr.setTextColor(C_WHITE);
    _spr.drawString(tempStr, blockStartX, tempY, 6);

    // Gold Degree symbol (°)
    int16_t degX = blockStartX + numW + 3;
    _spr.drawCircle(degX + 2, tempY + 6, 3, C_GOLD);
    _spr.drawCircle(degX + 2, tempY + 6, 2, C_GOLD);

    // Gold "C" unit
    _spr.setTextColor(C_GOLD);
    _spr.drawString("C", degX + 8, tempY + 3, 4);

    // Condition name: 100% Vietnamese uppercase title
    const char* condVn = "QUANG MAY";
    switch (wType) {
        case WEATHER_CLEAR:  condVn = night ? "DEM QUANG" : "TROI NANG"; break;
        case WEATHER_CLOUDY: condVn = "CO MAY"; break;
        case WEATHER_RAIN:   condVn = "CO MUA"; break;
        case WEATHER_STORM:  condVn = "MUA DONG"; break;
        case WEATHER_SNOW:   condVn = "CO TUYET"; break;
        case WEATHER_MIST:   condVn = "SUONG MU"; break;
        default:             condVn = night ? "DEM DEP" : "TROI DEP"; break;
    }

    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(C_GOLD);
    _spr.drawString(condVn, cardX + cardW / 2, tempY + 44, 2);

    // Localized condition description in Vietnamese
    _spr.setTextColor(RGB565(255, 215, 0));
    _spr.drawString(w.valid ? w.description : (night ? "Troi quang, gio nhe" : "Nang dep, it may"),
                    cardX + cardW / 2, tempY + 58, 1);

    // ── Weather metrics: Balanced vertical layout filling card down to bottom ──
    int16_t detailY = tempY + 72;
    _spr.setTextDatum(TL_DATUM);

    // Row 1: Humidity
    _spr.fillCircle(cardX + 16, detailY + 4, 3, C_CYAN);
    _spr.drawPixel(cardX + 16, detailY + 1, C_CYAN);
    char humBuf[20];
    snprintf(humBuf, sizeof(humBuf), "Do am: %d%%", w.valid ? w.humidity : 65);
    _spr.setTextColor(C_TEXT_DIM);
    _spr.drawString(humBuf, cardX + 26, detailY, 1);

    // Row 2: Wind
    detailY += 18;
    _spr.drawFastHLine(cardX + 13, detailY + 3, 6, C_CYAN);
    _spr.drawFastHLine(cardX + 15, detailY + 6, 4, C_CYAN);
    char windBuf[20];
    snprintf(windBuf, sizeof(windBuf), "Gio: %.0f km/h", w.valid ? w.windSpeed : 12.0f);
    _spr.drawString(windBuf, cardX + 26, detailY, 1);

    // Row 3: UV Index
    detailY += 18;
    _spr.fillCircle(cardX + 16, detailY + 4, 3, C_SUN_YELLOW);
    _spr.drawCircle(cardX + 16, detailY + 4, 4, C_SUN_ORANGE);
    char uvBuf[24];
    float uvVal = w.valid ? w.uvIndex : 4.0f;
    const char* uvTag = (uvVal >= 8) ? "Rat cao" : (uvVal >= 6) ? "Cao" : (uvVal >= 3) ? "Vua" : "Thap";
    snprintf(uvBuf, sizeof(uvBuf), "UV: %.0f (%s)", uvVal, uvTag);
    _spr.drawString(uvBuf, cardX + 26, detailY, 1);

    // Bottom dock is drawn as a floating overlay by Display::update()
}

// ═══════════════════════════════════════════════════════════════════════
//  BACKGROUND — Organic blobs + sparkle stars using current Theme
// ═══════════════════════════════════════════════════════════════════════
void Display::drawBackground() {
    _spr.fillSprite(theme.bg);

    // Animated time offset for subtle blob movement
    uint32_t tick = millis() / 80;

    // ── Large pastel blobs (organic shapes via overlapping circles) ──

    // Top-right cluster: blue + cyan blobs
    int16_t bx1 = 280 + (int16_t)(sin(tick * 0.013) * 5);
    int16_t by1 = 30  + (int16_t)(cos(tick * 0.017) * 3);
    drawBlob(bx1, by1, 50, C_BLOB_BLUE);
    drawBlob(bx1 + 15, by1 - 20, 30, RGB565(60, 160, 240));

    // Top-left cluster: pink + coral
    int16_t bx2 = 45  + (int16_t)(sin(tick * 0.011) * 4);
    int16_t by2 = 15  + (int16_t)(cos(tick * 0.019) * 3);
    drawBlob(bx2, by2, 28, C_BLOB_PINK);
    drawBlob(bx2 + 30, by2 + 5, 18, C_BLOB_CORAL);

    // Mid-right: mint/teal blob
    int16_t bx3 = 295 + (int16_t)(sin(tick * 0.015) * 3);
    int16_t by3 = 130 + (int16_t)(cos(tick * 0.012) * 4);
    drawBlob(bx3, by3, 35, C_BLOB_MINT);

    // Bottom-left: lilac/purple blob
    int16_t bx4 = 30  + (int16_t)(sin(tick * 0.009) * 5);
    int16_t by4 = 200 + (int16_t)(cos(tick * 0.014) * 3);
    drawBlob(bx4, by4, 30, C_BLOB_LILAC);

    // Mid-left small accent blob
    drawBlob(10, 90, 20, RGB565(200, 140, 255));

    // ── Sparkles (tiny 4-point stars) ────────────────────────────────
    // Deterministic positions with subtle flicker via tick
    uint8_t flickerPhase = (tick / 3) & 0x07;  // 8 phases

    drawSparkle(85,  40,  3, (flickerPhase == 0) ? C_SPARKLE_CYAN : C_SPARKLE_W);
    drawSparkle(130, 25,  2, (flickerPhase == 1) ? C_SPARKLE_CYAN : C_SPARKLE_W);
    drawSparkle(180, 15,  2, (flickerPhase == 2) ? C_SPARKLE_W : C_CYAN_DIM);
    drawSparkle(250, 60,  3, (flickerPhase == 3) ? C_SPARKLE_CYAN : C_SPARKLE_W);
    drawSparkle(50,  65,  2, (flickerPhase == 4) ? C_SPARKLE_W : C_CYAN_DIM);
    drawSparkle(155, 50,  2, (flickerPhase == 5) ? C_SPARKLE_CYAN : C_SPARKLE_W);
    drawSparkle(300, 90,  2, (flickerPhase == 6) ? C_SPARKLE_W : C_CYAN_DIM);
    drawSparkle(200, 100, 2, (flickerPhase == 7) ? C_SPARKLE_CYAN : C_SPARKLE_W);

    // Extra tiny star dots
    drawStarDot(70,  18, C_WHITE);
    drawStarDot(115, 45, C_CYAN_GLOW);
    drawStarDot(190, 35, C_WHITE);
    drawStarDot(240, 20, C_BLOB_PINK);
    drawStarDot(170, 65, C_WHITE);
    drawStarDot(280, 75, C_CYAN_GLOW);
    drawStarDot(40, 140, C_WHITE);
    drawStarDot(310, 45, C_BLOB_MINT);
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
                // Night sky: artistic golden crescent moon + sparkling stars
                _spr.fillCircle(cx - 3, cy - 2, 13, C_GOLD);
                _spr.fillCircle(cx + 4, cy - 6, 13, theme.card); // clean crescent
                drawCloudIcon(cx + 8, cy + 8, 26, 12);
                drawSparkle(cx - 10, cy + 8, 2, C_WHITE);
                drawStarDot(cx + 12, cy - 8, C_CYAN_GLOW);
            } else {
                drawSunIcon(cx, cy, 14);
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
            drawCloudIcon(cx, cy - 5, 30, 18);
            // snowflakes
            for (int i = 0; i < 5; i++) {
                int16_t sx = cx - 10 + i * 5;
                int16_t sy = cy + 12 + (i & 1) * 4;
                drawSparkle(sx, sy, 2, C_WHITE);
            }
            break;
        case WEATHER_MIST:
            for (int i = 0; i < 4; i++) {
                int16_t lw = 24 - i * 2;
                _spr.drawFastHLine(cx - lw / 2, cy - 6 + i * 5, lw, C_TEXT_DIM);
            }
            break;
        default:
            drawPartlyCloudyIcon(cx, cy);
            break;
    }
}

void Display::drawSunIcon(int16_t cx, int16_t cy, int16_t r) {
    // 8 rounded pill / circular rays around the sun
    for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int16_t rx = cx + (int16_t)(cos(rad) * (r + 7));
        int16_t ry = cy + (int16_t)(sin(rad) * (r + 7));
        _spr.fillCircle(rx, ry, 2, C_SUN_YELLOW);
        _spr.drawPixel(rx, ry, RGB565(255, 245, 180));
    }

    // 3D Sun body with warm radial depth
    _spr.fillCircle(cx, cy, r + 2, C_SUN_ORANGE);
    _spr.fillCircle(cx, cy, r, C_SUN_YELLOW);
    _spr.fillCircle(cx - 2, cy - 2, r - 3, RGB565(255, 245, 160));
    _spr.drawPixel(cx - 3, cy - 3, C_WHITE);
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
    _spr.fillCircle(cx + 1, cy - baseR - 6, baseR - 1, RGB565(255, 255, 255));
    _spr.fillCircle(cx - w / 4, cy - baseR - 4, baseR - 2, RGB565(255, 255, 255));
}

void Display::drawPartlyCloudyIcon(int16_t cx, int16_t cy) {
    // 1. 3D Sun behind top-left
    int16_t sx = cx - 10;
    int16_t sy = cy - 8;
    int16_t sr = 12;

    // Sun rays (visible around upper / left arc)
    for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int16_t rx = sx + (int16_t)(cos(rad) * (sr + 5));
        int16_t ry = sy + (int16_t)(sin(rad) * (sr + 5));
        _spr.fillCircle(rx, ry, 2, C_SUN_YELLOW);
    }
    // Sun body with warm radial depth
    _spr.fillCircle(sx, sy, sr + 2, C_SUN_ORANGE);
    _spr.fillCircle(sx, sy, sr, C_SUN_YELLOW);
    _spr.fillCircle(sx - 2, sy - 2, sr - 3, RGB565(255, 245, 170));

    // 2. 3D Fluffy Cloud in foreground
    int16_t clx = cx + 8;
    int16_t cly = cy + 6;
    drawCloudIcon(clx, cly, 34, 16);
}

void Display::drawRainIcon(int16_t cx, int16_t cy) {
    drawCloudIcon(cx, cy - 6, 28, 14);
    // Rain drops
    for (int i = 0; i < 4; i++) {
        int16_t dx = cx - 8 + i * 6;
        int16_t dy = cy + 6 + (i & 1) * 3;
        _spr.drawLine(dx, dy, dx - 2, dy + 5, C_RAIN_BLUE);
    }
}

void Display::drawStormIcon(int16_t cx, int16_t cy) {
    drawCloudIcon(cx, cy - 6, 28, 14);
    // Lightning bolt
    _spr.drawLine(cx,     cy + 4, cx - 3, cy + 10, C_GOLD);
    _spr.drawLine(cx - 3, cy + 10, cx + 1, cy + 10, C_GOLD);
    _spr.drawLine(cx + 1, cy + 10, cx - 2, cy + 18, C_GOLD);
}

void Display::drawMoonIcon(int16_t cx, int16_t cy, int16_t r, uint8_t lunarDay) {
    // Full moon base
    _spr.fillCircle(cx, cy, r, C_MOON_FACE);

    // Craters
    _spr.fillCircle(cx - r / 3, cy - r / 4, r / 5, C_MOON_CRATER);
    _spr.fillCircle(cx + r / 4, cy + r / 3, r / 6, C_MOON_CRATER);
    _spr.fillCircle(cx + r / 5, cy - r / 3, r / 7, C_MOON_CRATER);

    // Shadow for crescent effect
    if (lunarDay < 15) {
        // Waxing: shadow on the left
        int16_t offset = map(lunarDay, 0, 14, r + 2, -r);
        _spr.fillCircle(cx + offset, cy, r + 1, C_BG_DARK);
    } else if (lunarDay > 15) {
        // Waning: shadow on the right
        int16_t offset = map(lunarDay, 16, 29, -r, r + 2);
        _spr.fillCircle(cx + offset, cy, r + 1, C_BG_DARK);
    }
    // lunarDay == 15: full moon, no shadow
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

    // Specular top highlight & frosted glowing glass border
    _spr.drawRoundRect(x, y, w, h, radius, theme.cardBorder);
    _spr.drawFastHLine(x + radius, y, w - radius * 2, RGB565(140, 190, 245));
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

bool Display::handleTouch(uint16_t x, uint16_t y) {
    // If on Calendar page and tapping in the top Title Card (Y <= 50)
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

    // If dock is hidden, tap anywhere in the lower 60px wakes it up
    if (!_dockVisible) {
        if (y >= 180) {
            wakeDock();
            return true;
        }
        return false;
    }

    // Dock is visible -> hit test buttons (5 slots, 42px wide each)
    if (y >= 180) {
        wakeDock(); // Reset 5s auto-hide timer
        if (x < 97)                   { setPage(PAGE_CLOCK);    return true; }
        else if (x >= 97 && x < 139)  { setPage(PAGE_CALENDAR); return true; }
        else if (x >= 139 && x < 181) { setPage(PAGE_FINANCE);  return true; }
        else if (x >= 181 && x < 223) { setPage(PAGE_POMODORO); return true; }
        else if (x >= 223)            { nextTheme(); wakeDock(); return true; }
        return true;
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

    // Centered Floating Glossy Obsidian & Crystal Edge Capsule Dock
    int16_t dockX = 55;
    int16_t dockY = 198;
    int16_t dockW = 210;
    int16_t dockH = 34;
    int16_t dockR = 17;

    // 1. Soft ambient outer shadow (gives floating 3D lift)
    _spr.fillRoundRect(dockX - 1, dockY + 1, dockW + 2, dockH + 2, dockR + 1, RGB565(4, 6, 12));

    // 2. Glossy Obsidian Deep Glass Base
    _spr.fillRoundRect(dockX, dockY, dockW, dockH, dockR, RGB565(10, 14, 22));

    // 3. Curved specular sheen on top half (genuine glossy reflection)
    _spr.fillRoundRect(dockX + 2, dockY + 1, dockW - 4, (dockH / 2) - 1, dockR - 2, RGB565(24, 34, 52));

    // 4. Crystal Bezel Border (Diamond-cut edge)
    _spr.drawRoundRect(dockX, dockY, dockW, dockH, dockR, RGB565(45, 68, 100));

    // 5. Double Crystal Light Reflection on top edge (Diamond highlight)
    _spr.drawFastHLine(dockX + dockR, dockY, dockW - dockR * 2, RGB565(225, 245, 255)); // Pure ice-white crystal line
    _spr.drawFastHLine(dockX + dockR + 2, dockY + 1, dockW - dockR * 2 - 4, RGB565(130, 190, 255)); // Soft refraction line

    // 5 Vector Icon Slots
    for (int i = 0; i < 5; i++) {
        int16_t cx = dockX + 21 + i * 42;
        int16_t cy = dockY + dockH / 2 - 1;
        bool isActive = (i < 4 && _currentPage == i);

        if (isActive) {
            // Active state: Glowing theme pill capsule
            _spr.fillRoundRect(cx - 16, dockY + 3, 32, 28, 14, RGB565(18, 30, 48));
            _spr.drawRoundRect(cx - 16, dockY + 3, 32, 28, 14, theme.accent);

            // Apple macOS style active underglow indicator bar
            _spr.fillRoundRect(cx - 7, dockY + dockH - 4, 14, 2, 1, theme.accent);
            _spr.drawFastHLine(cx - 4, dockY + dockH - 4, 8, C_WHITE); // Bright center sparkle in indicator
        }

        // Crisp platinum silver for inactive, radiant theme.accent for active
        uint16_t iconColor = isActive ? theme.accent : RGB565(200, 218, 238);

        switch (i) {
            case 0: drawDockIconHome(cx, cy, iconColor); break;
            case 1: drawDockIconCalendar(cx, cy, iconColor); break;
            case 2: drawDockIconFinance(cx, cy, iconColor); break;
            case 3: drawDockIconPomodoro(cx, cy, iconColor); break;
            case 4: drawDockIconTheme(cx, cy, iconColor); break;
        }
    }
}

// ─── Dock Vector Icons (Sharp Unified Minimalist Vector Glyphs) ──────
void Display::drawDockIconHome(int16_t cx, int16_t cy, uint16_t color) {
    // Modern geometric roof with clean overhang
    _spr.fillTriangle(cx, cy - 8, cx - 9, cy - 1, cx + 9, cy - 1, color);
    // House body
    _spr.fillRoundRect(cx - 7, cy - 1, 14, 9, 2, color);
    // Clean door cutout (reveals obsidian dock background)
    _spr.fillRoundRect(cx - 2, cy + 1, 5, 7, 1, RGB565(10, 14, 22));
}

void Display::drawDockIconCalendar(int16_t cx, int16_t cy, uint16_t color) {
    // Modern desk calendar body
    _spr.fillRoundRect(cx - 8, cy - 6, 16, 14, 2, color);
    // Header cutout groove
    _spr.drawFastHLine(cx - 6, cy - 2, 12, RGB565(10, 14, 22));
    // Top metallic ring loops
    _spr.fillRoundRect(cx - 5, cy - 9, 2, 4, 1, color);
    _spr.fillRoundRect(cx + 3, cy - 9, 2, 4, 1, color);
    // 4 clean date dots
    _spr.fillRect(cx - 5, cy + 1, 3, 2, RGB565(10, 14, 22));
    _spr.fillRect(cx + 2, cy + 1, 3, 2, RGB565(10, 14, 22));
    _spr.fillRect(cx - 5, cy + 4, 3, 2, RGB565(10, 14, 22));
    _spr.fillRect(cx + 2, cy + 4, 3, 2, RGB565(10, 14, 22));
}

void Display::drawDockIconFinance(int16_t cx, int16_t cy, uint16_t color) {
    // 3 ascending solid financial growth bars
    _spr.fillRoundRect(cx - 7, cy + 2, 3, 6, 1, color);
    _spr.fillRoundRect(cx - 2, cy - 2, 3, 10, 1, color);
    _spr.fillRoundRect(cx + 3, cy - 6, 3, 14, 1, color);
    // Dynamic upward trendline with arrow tip
    _spr.drawLine(cx - 7, cy + 1, cx - 2, cy - 3, color);
    _spr.drawLine(cx - 2, cy - 3, cx + 6, cy - 9, color);
    _spr.fillTriangle(cx + 7, cy - 11, cx + 4, cy - 8, cx + 8, cy - 7, color);
}

void Display::drawDockIconPomodoro(int16_t cx, int16_t cy, uint16_t color) {
    // Minimalist luxury chronograph body
    _spr.drawCircle(cx, cy + 1, 7, color);
    _spr.drawCircle(cx, cy + 1, 6, color);
    // Crown button
    _spr.fillRoundRect(cx - 2, cy - 8, 4, 2, 1, color);
    // Clock hands (pointing to 10:10)
    _spr.drawPixel(cx, cy + 1, color);
    _spr.drawLine(cx, cy + 1, cx - 3, cy - 3, color);
    _spr.drawLine(cx, cy + 1, cx + 3, cy - 1, color);
}

void Display::drawDockIconTheme(int16_t cx, int16_t cy, uint16_t color) {
    // Artist palette body
    _spr.fillCircle(cx, cy, 7, color);
    // Thumb hole cutout
    _spr.fillCircle(cx + 2, cy + 2, 2, RGB565(10, 14, 22));
    // 3 clean pigment dots
    _spr.fillCircle(cx - 3, cy - 2, 1, RGB565(10, 14, 22));
    _spr.fillCircle(cx + 1, cy - 3, 1, RGB565(10, 14, 22));
    _spr.fillCircle(cx - 3, cy + 2, 1, RGB565(10, 14, 22));
}

// ═══════════════════════════════════════════════════════════════════════
//  PAGE 1 — LUNAR MONTH CALENDAR (Interactive Month Navigation)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage1_Calendar() {
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

    // ── 1. TITLE CARD WITH INTERACTIVE NAVIGATION BUTTONS (H=36px) ───
    drawGlassCard(12, 8, 296, 36, 10);

    // [ < ] Prev Month Button
    _spr.fillRoundRect(18, 12, 34, 28, 6, theme.card);
    _spr.drawRoundRect(18, 12, 34, 28, 6, theme.cardBorder);
    _spr.fillTriangle(37, 18, 37, 34, 28, 26, theme.accent); // Left arrow

    // [ > ] Next Month Button
    _spr.fillRoundRect(264, 12, 34, 28, 6, theme.card);
    _spr.drawRoundRect(264, 12, 34, 28, 6, theme.cardBorder);
    _spr.fillTriangle(279, 18, 279, 34, 288, 26, theme.accent); // Right arrow

    // Center Month Title (Font 2) & Status Subtitle (Font 1)
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.accent);
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "THANG %02d / %d", _calMonth, _calYear);
    _spr.drawString(titleBuf, 160, 20, 2);

    bool isCurrentMonth = (_calYear == curYear && _calMonth == curMonth);
    char subBuf[48];
    if (isCurrentMonth) {
        LunarDate todayLunar = LunarCalendar::convertSolar2Lunar(curDay, curMonth, curYear);
        snprintf(subBuf, sizeof(subBuf), "Hom nay: %02d/%02d AL - Nam %s", 
                 todayLunar.day, todayLunar.month, todayLunar.yearCanChi);
        _spr.setTextColor(RGB565(140, 210, 255));
    } else {
        LunarDate midLunar = LunarCalendar::convertSolar2Lunar(15, _calMonth, _calYear);
        snprintf(subBuf, sizeof(subBuf), "Nam %s  •  [Cham ve hom nay]", midLunar.yearCanChi);
        _spr.setTextColor(RGB565(255, 200, 100));
    }
    _spr.drawString(subBuf, 160, 33, 1);

    // ── 2. CALENDAR GRID CARD (Extended to H=178px down to Y=226) ────
    drawGlassCard(12, 48, 296, 178, 10);

    // Day of week headers (T2 to CN)
    const char* daysOfWeek[] = { "T2", "T3", "T4", "T5", "T6", "T7", "CN" };
    for (int col = 0; col < 7; col++) {
        int16_t hx = 46 + col * 38;
        if (col == 6)      _spr.setTextColor(RGB565(255, 120, 120)); // CN (Sunday) in soft red
        else if (col == 5) _spr.setTextColor(RGB565(140, 220, 255)); // T7 (Saturday) in soft cyan
        else               _spr.setTextColor(theme.accentSec);
        _spr.drawString(daysOfWeek[col], hx, 58, 2);
    }
    _spr.drawFastHLine(20, 69, 280, theme.cardBorder);

    // Calculate days in viewed month
    int daysInMonth = 31;
    if (_calMonth == 4 || _calMonth == 6 || _calMonth == 9 || _calMonth == 11) {
        daysInMonth = 30;
    } else if (_calMonth == 2) {
        bool isLeapYear = (_calYear % 4 == 0 && _calYear % 100 != 0) || (_calYear % 400 == 0);
        daysInMonth = isLeapYear ? 29 : 28;
    }

    // Sakamoto's algorithm to get day of week for day 1 (0 = Sunday, 1 = Monday, ...)
    static const int tMap[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int yCalc = _calYear;
    if (_calMonth < 3) yCalc -= 1;
    int wday1 = (yCalc + yCalc / 4 - yCalc / 100 + yCalc / 400 + tMap[_calMonth - 1] + 1) % 7;
    int startCol = (wday1 + 6) % 7; // Convert to Monday = 0, ..., Sunday = 6

    int totalSlots = startCol + daysInMonth;
    int numRows = (totalSlots + 6) / 7; // 4, 5, or 6 rows

    // Dynamic row spacing: 5 rows gets 28px height, 6 rows gets 24px height
    int16_t rowH = (numRows <= 5) ? 28 : 24;
    int16_t gridStartY = (numRows <= 5) ? 75 : 73;

    // Render calendar grid with distinct vertical separation between Solar & Lunar dates
    for (int d = 1; d <= daysInMonth; d++) {
        int slot = startCol + (d - 1);
        int col = slot % 7;
        int row = slot / 7;
        int16_t cx = 46 + col * 38;
        int16_t cy = gridStartY + row * rowH;

        LunarDate ld = LunarCalendar::convertSolar2Lunar(d, _calMonth, _calYear);
        bool isToday = (d == curDay && isCurrentMonth);

        int16_t cellH = rowH - 3;
        int16_t solarY = cy + (rowH == 28 ? 8 : 6);
        int16_t lunarY = cy + (rowH == 28 ? 19 : 15);

        if (isToday) {
            // Highlight today with filled accent pill
            _spr.fillRoundRect(cx - 16, cy, 32, cellH, 6, theme.accent);
            _spr.setTextColor(theme.bg);
            _spr.drawNumber(d, cx, solarY, 2);

            // Lunar date inside today pill
            char lStr[8];
            if (ld.day == 1) snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
            else snprintf(lStr, sizeof(lStr), "%d", ld.day);
            _spr.setTextColor(RGB565(30, 30, 30));
            _spr.drawString(lStr, cx, lunarY, 1);
        } else {
            // Subtle plate background for all days
            _spr.fillRoundRect(cx - 16, cy, 32, cellH, 4, theme.card);

            // Solar day (Font 2)
            if (col == 6)      _spr.setTextColor(RGB565(255, 120, 120)); // Sunday red
            else if (col == 5) _spr.setTextColor(RGB565(140, 220, 255)); // Saturday cyan
            else               _spr.setTextColor(theme.textMain);
            _spr.drawNumber(d, cx, solarY, 2);

            // Lunar day (Font 1)
            char lStr[8];
            if (ld.day == 1) {
                // Highlight Mùng 1 in bright gold
                snprintf(lStr, sizeof(lStr), "1/%d", ld.month);
                _spr.setTextColor(RGB565(255, 200, 0));
            } else if (ld.day == 15) {
                // Highlight Ngày Rằm in bright gold
                snprintf(lStr, sizeof(lStr), "15");
                _spr.setTextColor(RGB565(255, 200, 0));
            } else {
                snprintf(lStr, sizeof(lStr), "%d", ld.day);
                _spr.setTextColor(theme.textDim);
            }
            _spr.drawString(lStr, cx, lunarY, 1);
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
//  PAGE 3 — POMODORO & ALARM (Full-Height Layout)
// ═══════════════════════════════════════════════════════════════════════
void Display::renderPage3_Pomodoro() {
    drawBackground();

    // Pomodoro Timer Card — Extended to full height (H=222px down to Y=230)
    drawGlassCardAccent(12, 8, 190, 222, 10, theme.red);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.red);
    _spr.drawString("POMODORO", 107, 24, 4);

    _spr.setTextColor(theme.textDim);
    _spr.drawString("TAP TRUNG", 107, 46, 2);

    // Large Countdown Digits
    _spr.setTextColor(theme.textMain);
    _spr.drawString("25:00", 107, 100, 7);

    // Start Button — comfortable hit target
    _spr.fillRoundRect(47, 155, 120, 44, 10, theme.red);
    _spr.setTextColor(theme.textMain);
    _spr.drawString("START", 107, 177, 4);

    // Alarm Card on Right Side — Extended to full height (H=222px down to Y=230)
    drawGlassCardAccent(210, 8, 98, 222, 10, theme.yellow);
    _spr.setTextDatum(MC_DATUM);
    _spr.setTextColor(theme.yellow);
    _spr.drawString("BAO THUC", 259, 24, 2);

    _spr.setTextColor(theme.textMain);
    _spr.drawString("07:00", 259, 78, 6);

    _spr.setTextColor(theme.green);
    _spr.drawString("DANG BAT", 259, 118, 2);

    _spr.fillRoundRect(220, 155, 78, 44, 10, theme.card);
    _spr.drawRoundRect(220, 155, 78, 44, 10, theme.cardBorder);
    _spr.setTextColor(theme.textDim);
    _spr.drawString("CAI DAT", 259, 177, 2);
}
