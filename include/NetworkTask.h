// ═══════════════════════════════════════════════════════════════════════
//  NetworkTask.h — Non-blocking FreeRTOS Background Network Worker
//  CYD SmartDesk V2 — Runs on Core 0 to prevent UI freezes
// ═══════════════════════════════════════════════════════════════════════
#ifndef NETWORK_TASK_H
#define NETWORK_TASK_H

#include <Arduino.h>

class NetworkTask {
public:
    NetworkTask();

    // Start FreeRTOS worker pinned to Core 0
    void begin();

    // Trigger immediate background fetch (non-blocking)
    void triggerWeather();
    void triggerFinance();

    bool isBusy() const { return _isBusy; }

private:
    static void _taskWorker(void* arg);

    TaskHandle_t _taskHandle;
    volatile bool _triggerWeather;
    volatile bool _triggerFinance;
    volatile bool _isBusy;
    unsigned long _lastWeatherFetch;
    unsigned long _lastFinanceFetch;
};

extern NetworkTask networkTask;

#endif // NETWORK_TASK_H
