// ═══════════════════════════════════════════════════════════════════════
//  Config.h — Hardware pinout & system constants for ESP32-2432S028 CYD
//  CYD SmartDesk V2 — Commercial-grade 16-bit color dashboard
// ═══════════════════════════════════════════════════════════════════════
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── Hardware Pinout (ESP32-2432S028 CYD board) ──────────────────────
#define PIN_TFT_BL      21      // Backlight PWM
#define PIN_LDR          34      // Light-dependent resistor (analog)
#define PIN_SPEAKER      26      // Onboard speaker / buzzer
#define PIN_RGB_RED       4      // RGB LED red
#define PIN_RGB_GREEN    16      // RGB LED green
#define PIN_RGB_BLUE     17      // RGB LED blue
#define PIN_TOUCH_CS     33      // Touch chip select (SPI)
#define PIN_TOUCH_IRQ    36      // Touch interrupt
#define PIN_BOOT_BTN      0      // BOOT button (active LOW)

// ─── Display ─────────────────────────────────────────────────────────
#define SCREEN_W        320
#define SCREEN_H        240

// ─── Defaults ────────────────────────────────────────────────────────
#define DEFAULT_CITY            "Hanoi"
#define DEFAULT_OWM_KEY         "bd5e378503939ddaee5f12d250261283"
#define NTP_SERVER              "pool.ntp.org"
#define TIMEZONE_OFFSET_SEC     25200   // UTC+7 Vietnam

// ─── Timing ──────────────────────────────────────────────────────────
#define WEATHER_FETCH_INTERVAL  600000  // 10 min
#define LDR_CHECK_INTERVAL      500     // 500 ms
#define DISPLAY_FPS_INTERVAL    33      // ~30 FPS target

#endif // CONFIG_H
