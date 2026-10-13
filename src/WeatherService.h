// ═══════════════════════════════════════════════════════════════════════
//  WeatherService.h — WiFi connection + OpenWeatherMap API client
//  CYD SmartDesk V2
// ═══════════════════════════════════════════════════════════════════════
#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Config.h"

// ─── Classify weather into simple type for icon rendering ────────────
enum WeatherType : uint8_t {
    WEATHER_CLEAR   = 0,
    WEATHER_CLOUDY  = 1,
    WEATHER_RAIN    = 2,
    WEATHER_STORM   = 3,
    WEATHER_SNOW    = 4,
    WEATHER_MIST    = 5,
};

// ─── Daily Forecast structure (3-day forecast) ───────────────────────
struct DailyForecast {
    float tempMax;          // Max temperature (°C)
    float tempMin;          // Min temperature (°C)
    WeatherType type;       // WEATHER_CLEAR, etc.
    char dayName[12];       // "H.NAY", "T.MAI", "THU 4", etc.
    char condText[16];      // "Nang dep", "Co mua", etc.
};

// ─── Weather data structure ──────────────────────────────────────────
struct WeatherData {
    float temp;             // Current temperature (°C)
    uint8_t humidity;       // Relative humidity (%)
    float windSpeed;        // Wind speed (km/h)
    uint16_t weatherId;     // OWM condition code (200–804)
    char main[24];          // "Clear", "Clouds", "Rain", etc.
    char description[48];   // "few clouds", "light rain", etc.
    char icon[8];           // OWM icon code e.g. "01d", "10n"
    char city[32];          // City name
    int32_t sunrise;        // Unix timestamp
    int32_t sunset;         // Unix timestamp
    uint8_t sunriseHour;    // 0..23
    uint8_t sunriseMin;     // 0..59
    uint8_t sunsetHour;     // 0..23
    uint8_t sunsetMin;      // 0..59
    float uvIndex;          // UV index
    bool valid;             // Data fetched successfully?
    DailyForecast forecast[3]; // 3-day forecast: [0] Today, [1] Tomorrow, [2] Day after
};

class WeatherService {
public:
    WeatherService();
    void begin();                       // Connect WiFi via WiFiManager
    void update();                      // Periodic fetch
    void fetchNow();                    // Force immediate fetch

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool isTimeSynced() const { return _timeSynced; }
    const WeatherData& getData() const { return _data; }
    WeatherType getType() const;
    bool isNight() const;

    // Moon phase calculation (0–29, 0=new moon, 15=full)
    uint8_t getLunarDay() const;

private:
    void _connectWiFi();
    void _syncTime();
    void _detectCityFromIP();
    void _fetchWeather();

    WeatherData _data;
    unsigned long _lastFetch;
    unsigned long _lastNtpAttempt;
    bool _timeSynced;
    bool _cityDetected;
    float _lat;
    float _lon;
};

extern WeatherService weather;

#endif // WEATHER_SERVICE_H
