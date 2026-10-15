// ═══════════════════════════════════════════════════════════════════════
//  main.cpp — CYD SmartDesk V2 entry point
//  ESP32-2432S028 (Cheap Yellow Display) — Commercial-grade desk clock
//
//  V2 Architecture:
//  • Clean modular design, written from scratch
//  • 16-bit color rendering via full-screen sprite (no PSRAM needed)
//  • Procedural vector graphics (no bitmap images)
//  • WiFi + NTP + OpenWeatherMap integration
// ═══════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include "Config.h"
#include "HardwareManager.h"
#include "TouchManager.h"
#include "Display.h"
#include "WeatherService.h"
#include "FinanceService.h"
#include "WebDashboard.h"
#include "NetworkTask.h"

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║   CYD SmartDesk V2 — Commercial Edition   ║");
    Serial.println("║   16-bit UI • Multi-Theme • Touch Dock    ║");
    Serial.println("╚═══════════════════════════════════════════╝");

    // 1. Initialize hardware subsystems (Backlight, LDR, Speaker, RGB)
    hardware.begin();

    // 2. Initialize display & show boot splash with particle convergence
    display.begin();

    // 3. Initialize touch input
    display.showBootStatus("Initializing Touch...", 22);
    touch.begin();

    // 4. BOOT button as input with pullup (for 3s calibration trigger)
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    // 5. Connect WiFi, sync NTP, fetch weather
    weather.begin();

    // 6. Initialize & fetch real-time finance market rates (direct on boot)
    finance.begin();
    if (weather.isConnected()) {
        display.showBootStatus("Fetching Market Rates...", 96);
        finance.fetchDirect();
    }

    // 7. Start Web Dashboard for mobile controls & settings
    if (weather.isConnected()) {
        display.showBootStatus("Starting Dashboard...", 98);
        webDashboard.begin();
    }

    // 8. Start non-blocking FreeRTOS background task pinned to Core 0
    networkTask.begin();

    display.showBootStatus("System Ready!", 100);
    hardware.playStartupJingle();
    delay(200);

    Serial.println("[System] Setup complete!");
    Serial.printf("[System] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[System] Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
}

static unsigned long bootPressStart = 0;
static bool bootWasPressed = false;

void loop() {
    // 1. BOOT Button (GPIO 0) — Hold 3s to enter Touch Calibration Mode
    if (!touch.isCalibrating()) {
        if (digitalRead(PIN_BOOT_BTN) == LOW) {
            if (!bootWasPressed) {
                bootWasPressed = true;
                bootPressStart = millis();
            } else if (millis() - bootPressStart > 3000) {
                Serial.println("[System] BOOT button held 3s -> Starting Touch Calibration!");
                touch.startCalibration();
                bootWasPressed = false;
            }
        } else {
            bootWasPressed = false;
        }
    }

    // 2. Auto-adjust backlight according to ambient room light (LDR)
    hardware.updateAutoBrightness();

    // 3. Check touch tap events (switches pages or wakes dock with audio feedback)
    uint16_t tx, ty;
    if (touch.checkTap(&tx, &ty)) {
        display.handleTouch(tx, ty);
    }

    // 4. Update weather data periodically
    weather.update();

    // 5. Update finance rates periodically (every 15m)
    finance.update();

    // 6. Maintain Web Dashboard server
    if (weather.isConnected() && !webDashboard.isRunning()) {
        webDashboard.begin();
    }
    webDashboard.update();

    // 7. Render display & dock (internally rate-limited to ~30 FPS)
    display.update();
}
