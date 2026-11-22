// ═══════════════════════════════════════════════════════════════════════
//  Config.h — Hardware pinout & system constants for ESP32-2432S028 CYD
//  CYD SmartDesk V2 — Commercial-grade 16-bit color dashboard
// ═══════════════════════════════════════════════════════════════════════
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── Hardware Pinout ──────────────────────────────────────────────────
#ifdef BOARD_NICEMCU_28IPS
// NiceMCU-32S-DEV 2.8" IPS
#define PIN_TFT_BL       25      // Backlight PWM on GPIO 25
#define PIN_LDR          34      // Light-dependent resistor (analog)
#define PIN_SPEAKER      26      // Onboard speaker / audio amp (DAC2)
#define PIN_RGB_RED       4      // RGB LED red
#define PIN_RGB_GREEN    16      // RGB LED green
#define PIN_RGB_BLUE     17      // RGB LED blue
#define PIN_TOUCH_SDA    33      // CST816D I2C SDA
#define PIN_TOUCH_SCL    32      // CST816D I2C SCL
#define PIN_TOUCH_INT    36      // CST816D Interrupt
#define PIN_TOUCH_RST     2      // CST816D Reset (shared with LCD RST)
#define PIN_BOOT_BTN      0      // BOOT button
#else
// ESP32-2432S028 CYD board
#define PIN_TFT_BL       21      // Backlight PWM
#define PIN_LDR          34      // Light-dependent resistor (analog)
#define PIN_SPEAKER      26      // Onboard speaker / buzzer
#define PIN_RGB_RED       4      // RGB LED red
#define PIN_RGB_GREEN    16      // RGB LED green
#define PIN_RGB_BLUE     17      // RGB LED blue
#define PIN_TOUCH_CS     33      // Touch chip select (SPI)
#define PIN_TOUCH_IRQ    36      // Touch interrupt
#define PIN_BOOT_BTN      0      // BOOT button (active LOW)
#endif

// ─── Display ─────────────────────────────────────────────────────────
#define SCREEN_W        320
#define SCREEN_H        240

// ─── Touch type flag ────────────────────────────────────────
// Automatically enabled for NiceMCU 2.8" IPS (CST816D capacitive).
#ifdef BOARD_NICEMCU_28IPS
#define USE_CAPACITIVE_TOUCH
#endif


// ─── Defaults ────────────────────────────────────────────────────────
#define DEFAULT_CITY            "Hanoi"
#define NTP_SERVER              "pool.ntp.org"
#define TIMEZONE_OFFSET_SEC     25200   // UTC+7 Vietnam

// ─── Timing ──────────────────────────────────────────────────────────
#define WEATHER_FETCH_INTERVAL  600000  // 10 min
#define FINANCE_FETCH_INTERVAL  900000  // 15 min
#define LDR_CHECK_INTERVAL      500     // 500 ms
#define DISPLAY_FPS_INTERVAL    33      // ~30 FPS target

#endif // CONFIG_H
