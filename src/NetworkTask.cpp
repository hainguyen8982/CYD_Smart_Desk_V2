// ═══════════════════════════════════════════════════════════════════════
//  NetworkTask.cpp — Non-blocking FreeRTOS Background Network Worker
//  CYD SmartDesk V2 — Dedicated worker pinned to ESP32 Core 0
// ═══════════════════════════════════════════════════════════════════════
#include "NetworkTask.h"
#include "WeatherService.h"
#include "FinanceService.h"
#include "EcosystemService.h"
#include "WebDashboard.h"
#include "Display.h"
#include "Config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

static void runCloudOtaDownload(const char* url) {
    Serial.printf("[CloudOTA] Starting HTTPS download: %s (FreeHeap=%u)\n", url, ESP.getFreeHeap());
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(client, url)) {
        webDashboard.finishCloudOta(false, "Cannot connect to Cloud OTA server");
        return;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        char err[64];
        snprintf(err, sizeof(err), "HTTP error %d", code);
        http.end();
        webDashboard.finishCloudOta(false, err);
        return;
    }

    int totalLen = http.getSize();
    if (totalLen <= 10000) {
        http.end();
        webDashboard.finishCloudOta(false, "Invalid firmware size");
        return;
    }

    if (!Update.begin((size_t)totalLen, U_FLASH)) {
        String e = Update.errorString();
        http.end();
        webDashboard.finishCloudOta(false, e.c_str());
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t* buf = (uint8_t*)malloc(1024);
    if (!buf) {
        Update.abort();
        http.end();
        webDashboard.finishCloudOta(false, "Out of memory");
        return;
    }

    size_t written = 0;
    unsigned long lastDataMs = millis();
    uint8_t lastPct = 255;

    while (http.connected() && written < (size_t)totalLen) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t toRead = (avail > 1024) ? 1024 : avail;
            int n = stream->readBytes(buf, toRead);
            if (n > 0) {
                if (Update.write(buf, (size_t)n) != (size_t)n) {
                    String e = Update.errorString();
                    free(buf);
                    Update.abort();
                    http.end();
                    webDashboard.finishCloudOta(false, e.c_str());
                    return;
                }
                written += (size_t)n;
                lastDataMs = millis();
                uint8_t pct = (uint8_t)((written * 100ULL) / (size_t)totalLen);
                if (pct != lastPct) {
                    lastPct = pct;
                    webDashboard.setCloudOtaProgress(pct);
                }
            }
        } else {
            if (millis() - lastDataMs > 15000UL) {
                free(buf);
                Update.abort();
                http.end();
                webDashboard.finishCloudOta(false, "Download timeout");
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    free(buf);
    http.end();

    if (written != (size_t)totalLen) {
        Update.abort();
        webDashboard.finishCloudOta(false, "Incomplete download");
        return;
    }

    if (Update.end(true)) {
        Serial.printf("[CloudOTA] Success! Written %u bytes -> scheduling reboot\n", (unsigned)written);
        webDashboard.finishCloudOta(true, "");
    } else {
        String e = Update.errorString();
        webDashboard.finishCloudOta(false, e.c_str());
    }
}

static void reportNfcLanIpIfNeeded() {
    if (strlen(NFC_REDIRECTOR_URL) == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    static String lastReportedIp = "";
    static unsigned long lastReportMs = 0;
    static unsigned long lastAttemptMs = 0;
    String curIp = WiFi.localIP().toString();
    unsigned long now = millis();

    // Chỉ báo IP khi mới kết nối, khi đổi IP LAN, hoặc định kỳ mỗi 6 giờ
    if (curIp == lastReportedIp && lastReportMs != 0 && (now - lastReportMs < 21600000UL)) {
        return;
    }
    // Backoff 5 phút nếu lần thử trước thất bại (tránh bão retry 500ms khi server sập)
    if (curIp == lastReportedIp || (lastAttemptMs != 0 && (now - lastAttemptMs < 300000UL))) {
        if (lastReportMs == 0 || curIp != lastReportedIp) {
            if (lastAttemptMs != 0 && (now - lastAttemptMs < 300000UL)) return;
        }
    }
    lastAttemptMs = now;

    char devId[12], mdnsHost[24];
    EcosystemService::getDeviceId(devId, sizeof(devId));
    EcosystemService::getMdnsHost(mdnsHost, sizeof(mdnsHost));

    char body[224];
    snprintf(body, sizeof(body),
             "{\"id\":\"%s\",\"ip\":\"%s\",\"mdns\":\"%s.local\",\"fw\":\"%s\",\"hw\":\"%s\",\"rssi\":%d}",
             devId, curIp.c_str(), mdnsHost, FIRMWARE_VERSION_STR, HARDWARE_MODEL_STR, (int)WiFi.RSSI());

    HTTPClient http;
    http.begin(NFC_REDIRECTOR_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", NFC_CHECKIN_SECRET);
    http.setTimeout(4000);
    int code = http.POST((uint8_t*)body, strlen(body));
    if (code == 200) {
        lastReportedIp = curIp;
        lastReportMs = now;
        Serial.printf("[NFC-Cloud] Checkin OK: %s -> %s\n", devId, curIp.c_str());
    } else {
        Serial.printf("[NFC-Cloud] Checkin HTTP %d\n", code);
    }
    http.end();
}

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
        // Xử lý Cloud OTA (tải HTTPS firmware trên Core 0 khi Core 1 đã tạm dừng SPI/Display)
        char cloudOtaUrl[192];
        if (webDashboard.consumeCloudOtaRequest(cloudOtaUrl, sizeof(cloudOtaUrl))) {
            runCloudOtaDownload(cloudOtaUrl);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Khi đang nạp OTA: tạm dừng mọi tác vụ mạng trên Core 0 để nhường RAM & Flash
        if (webDashboard.isOtaInProgress()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (WiFi.status() == WL_CONNECTED) {
            // Báo IP LAN lên NFC Cloud Redirector (chỉ chạy khi NFC_REDIRECTOR_URL được cấu hình)
            reportNfcLanIpIfNeeded();

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
        } else {
            // Mất kết nối WiFi: Tự động thử kết nối lại ngầm trên Core 0 sau mỗi 15 giây
            // Hoàn toàn không làm ảnh hưởng hay gián đoạn vòng lặp đồ họa và đồng hồ trên Core 1
            unsigned long now = millis();
            static unsigned long lastReconnectAttempt = 0;
            if (now - lastReconnectAttempt >= 15000 || lastReconnectAttempt == 0) {
                lastReconnectAttempt = now;
                Serial.println("[NetworkTask] Core 0: WiFi lost! Reconnecting in background...");
                WiFi.reconnect();
            }
        }

        // Sleep 500ms on Core 0 before checking next cycle
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
