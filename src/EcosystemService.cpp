#include "EcosystemService.h"
#include "Display.h"
#include "DeskMood.h"
#include "HardwareManager.h"
#include "WeatherService.h"
#include <WiFi.h>
#include <math.h>

EcosystemService ecosystem;

static void espNowRxCallback(const uint8_t *mac, const uint8_t *data, int len) {
    ecosystem.onDataReceived(mac, data, len);
}

EcosystemService::EcosystemService() {}

void EcosystemService::_ensureBroadcastPeer() {
    if (esp_now_is_peer_exist(_broadcastMac)) return;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, _broadcastMac, 6);
    peerInfo.channel = 0; // Dùng kênh Wi-Fi hiện hành
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("[Ecosystem] Registered Broadcast Peer (FF:FF:FF:FF:FF:FF)");
    }
}

void EcosystemService::_registerPeerMac(const uint8_t *mac) {
    if (!mac) return;
    if (_hasPeerMac && memcmp(_peerMac, mac, 6) == 0 && esp_now_is_peer_exist(_peerMac)) {
        return;
    }
    memcpy(_peerMac, mac, 6);
    _hasPeerMac = true;

    if (!esp_now_is_peer_exist(_peerMac)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, _peerMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) == ESP_OK) {
            Serial.printf("[Ecosystem] Paired Unicast EMO MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
}

void EcosystemService::begin() {
    if (_initialized) {
        WiFi.setSleep(false);
        _ensureBroadcastPeer();
        return;
    }

    // ESP-NOW yêu cầu Wi-Fi ở chế độ STA hoặc AP_STA
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }
    // Tắt Wi-Fi Modem Sleep để nhận gói tin ESP-NOW tức thì (< 2ms, 0% rớt gói)
    WiFi.setSleep(false);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[Ecosystem] Failed to initialize ESP-NOW!");
        return;
    }

    esp_now_register_recv_cb(espNowRxCallback);
    _ensureBroadcastPeer();

    _initialized = true;
    _lastPingSent = millis();
    Serial.println("[Ecosystem] ESP-NOW Master Station initialized (CYD SmartDesk V2)");

    // Gửi gói tin Heartbeat đầu tiên để dò tìm EMO Companion trên bàn
    sendEvent(EVT_HEARTBEAT, 0);
}

bool EcosystemService::sendEvent(uint8_t eventType, uint16_t payload) {
    if (!_initialized) return false;

    _ensureBroadcastPeer();

    EcosystemPacket pkt;
    pkt.magic     = ECO_MAGIC;
    pkt.senderId  = ECO_SENDER_CYD;
    pkt.eventType = eventType;
    pkt.payload   = payload;
    pkt.checksum  = ecoCalculateChecksum(pkt);

    // Nếu đã biết địa chỉ MAC của EMO Companion -> Gửi Unicast (có 802.11 ACK & tự động phát lại phần cứng)
    if (_hasPeerMac && esp_now_is_peer_exist(_peerMac)) {
        esp_now_send(_peerMac, (const uint8_t *)&pkt, sizeof(pkt));
    }

    // Đồng thời phát Broadcast để đảm bảo mọi thiết bị trong hệ sinh thái đều nhận được
    esp_err_t res = esp_now_send(_broadcastMac, (const uint8_t *)&pkt, sizeof(pkt));
    return (res == ESP_OK);
}

void EcosystemService::broadcastWeatherSync() {
    if (!_initialized) return;
    WeatherData wd = weather.getData();
    if (!wd.valid) return;

    int16_t tempTenths = (int16_t)roundf(wd.temp * 10.0f);
    uint16_t humWmo = ((uint16_t)(wd.humidity & 0xFF) << 8) | (uint16_t)(wd.wmoCode & 0xFF);

    sendEvent(EVT_WEATHER_SYNC, (uint16_t)tempTenths);
    sendEvent(EVT_HUMIDITY_SYNC, humWmo);
}

void EcosystemService::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(EcosystemPacket)) return;

    EcosystemPacket pkt;
    memcpy(&pkt, data, sizeof(EcosystemPacket));

    if (pkt.magic != ECO_MAGIC) return;
    if (pkt.checksum != ecoCalculateChecksum(pkt)) return;

    // Chỉ nhận gói tin từ EMO Companion (0x02)
    if (pkt.senderId != ECO_SENDER_EMO) return;

    // Ghi nhớ địa chỉ MAC của EMO Companion để truyền Unicast tin cậy 100%
    if (mac) {
        _registerPeerMac(mac);
    }

    // Cập nhật mốc thời gian sống ngay lập tức
    _lastCompanionHeartbeat = millis();

    // Đẩy vào hàng đợi vòng để xử lý an toàn trên Core 1 (update())
    uint8_t nextHead = (_rxHead + 1) % RX_QUEUE_SIZE;
    if (nextHead != _rxTail) {
        _rxQueue[_rxHead] = pkt;
        _rxHead = nextHead;
    }
}

void EcosystemService::update() {
    if (!_initialized) return;

    // 1. Xử lý các gói tin nhận được từ EMO Companion trên luồng chính Core 1
    while (_rxTail != _rxHead) {
        EcosystemPacket pkt = _rxQueue[_rxTail];
        _rxTail = (_rxTail + 1) % RX_QUEUE_SIZE;
        _processPacket(pkt);
    }

    // 2. Phát nhịp tim định kỳ mỗi 15 giây để tự động nhận diện và giữ kết nối
    uint32_t now = millis();
    if (now - _lastPingSent >= PING_INTERVAL_MS) {
        _lastPingSent = now;
        sendEvent(EVT_HEARTBEAT, 0);
        broadcastWeatherSync();
    }
}

void EcosystemService::_processPacket(const EcosystemPacket &pkt) {
    switch (pkt.eventType) {
        case EVT_HEARTBEAT:
            // EMO Companion vừa ping -> Phản hồi ACK và đồng bộ ngay Thời tiết hiện tại
            sendEvent(EVT_HEARTBEAT_ACK, 0);
            broadcastWeatherSync();
            break;

        case EVT_HEARTBEAT_ACK:
            // Đã xác nhận kết nối 2 chiều với EMO Companion -> Đồng bộ Thời tiết ngay
            broadcastWeatherSync();
            break;

        case EVT_ALARM_DISMISS:
            // Người dùng chạm/bấm trên EMO Companion để tắt chuông báo thức CYD
            if (display.isAlarmRinging()) {
                Serial.println("[Ecosystem] Remote Alarm Dismiss received from EMO Companion!");
                display.dismissAlarm();
            }
            break;

        case EVT_POMODORO_START: {
            uint16_t mins = (pkt.payload > 0) ? pkt.payload : 25;
            Serial.printf("[Ecosystem] Remote Pomodoro Start (%u min) from EMO Companion\n", mins);
            display.syncRemotePomodoroStart(mins);
            break;
        }

        case EVT_POMODORO_STOP:
            Serial.println("[Ecosystem] Remote Pomodoro Stop from EMO Companion");
            display.syncRemotePomodoroStop();
            break;

        case EVT_MOOD_SYNC:
            if (pkt.payload < MOOD_COUNT) {
                Serial.printf("[Ecosystem] Remote Mood Sync (%u) from EMO Companion\n", pkt.payload);
                deskMood.setMood((MoodType)pkt.payload, false);
                display.triggerRender();
            }
            break;

        default:
            break;
    }
}

bool EcosystemService::isCompanionPaired() const {
    if (!_initialized || _lastCompanionHeartbeat == 0) return false;
    return (millis() - _lastCompanionHeartbeat < COMPANION_TIMEOUT_MS);
}

const char* EcosystemService::getStatusText(bool isEnglish) const {
    if (isCompanionPaired()) {
        return isEnglish ? "Paired with EMO Companion" : "Đã kết nối EMO Companion";
    }
    return isEnglish ? "Standalone Mode" : "Chế độ Độc lập";
}
