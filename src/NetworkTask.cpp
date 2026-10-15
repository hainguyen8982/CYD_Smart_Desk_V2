// ═══════════════════════════════════════════════════════════════════════
//  NetworkTask.cpp — Non-blocking FreeRTOS Background Network Worker
//  CYD SmartDesk V2 — Dedicated worker pinned to ESP32 Core 0
// ═══════════════════════════════════════════════════════════════════════
#include "NetworkTask.h"
#include "WeatherService.h"
#include "FinanceService.h"
#include "Display.h"
#include "Config.h"

NetworkTask networkTask;

NetworkTask::NetworkTask()
    : _taskHandle(nullptr),
      _triggerWeather(false),
      _triggerFinance(false),
      _isBusy(false),
      _lastWeatherFetch(0),
      _lastFinanceFetch(0) {}

void NetworkTask::begin() {
    if (_taskHandle != nullptr) return;

    _lastWeatherFetch = millis();
    _lastFinanceFetch = millis();

    BaseType_t res = xTaskCreatePinnedToCore(
        _taskWorker,
        "net_fetch_task",
        8192,
        this,
        1,              // Low priority (background worker)
        &_taskHandle,
        0               // Pinned to Core 0 (Main UI runs on Core 1)
    );

    if (res == pdPASS) {
        Serial.println("[NetworkTask] Worker task successfully pinned to Core 0!");
    } else {
        Serial.println("[NetworkTask] ERROR: Failed to create FreeRTOS task on Core 0!");
    }
}

void NetworkTask::triggerWeather() {
    _triggerWeather = true;
}

void NetworkTask::triggerFinance() {
    _triggerFinance = true;
}

void NetworkTask::_taskWorker(void* arg) {
    NetworkTask* self = (NetworkTask*)arg;

    // Small delay to let network settle after boot
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            unsigned long now = millis();
            bool doWeather = self->_triggerWeather || (now - self->_lastWeatherFetch >= WEATHER_FETCH_INTERVAL);
            bool doFinance = self->_triggerFinance || (now - self->_lastFinanceFetch >= FINANCE_FETCH_INTERVAL);

            if (doWeather || doFinance) {
                self->_isBusy = true;

                if (doWeather) {
                    self->_triggerWeather = false;
                    Serial.println("[NetworkTask] Core 0: Updating Weather in background...");
                    weather.fetchBackground();
                    self->_lastWeatherFetch = millis();
                    display.triggerRender();
                }

                // Breathing gap between requests to prevent WiFi radio congestion
                vTaskDelay(pdMS_TO_TICKS(150));

                if (doFinance) {
                    self->_triggerFinance = false;
                    Serial.println("[NetworkTask] Core 0: Updating Market Rates in background...");
                    finance.fetchBackground();
                    self->_lastFinanceFetch = millis();
                    display.triggerRender();
                }

                self->_isBusy = false;
            }
        }

        // Sleep 500ms on Core 0 before checking next cycle
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
