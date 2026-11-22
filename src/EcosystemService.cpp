#include "EcosystemService.h"
#include "Display.h"
#include "DeskMood.h"
#include "HardwareManager.h"
#include "WeatherService.h"
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>

EcosystemService ecosystem;

void EcosystemService::getDeviceId(char* out, size_t len) {
    uint64_t mac = ESP.getEfuseMac();
    // 3 byte cuối của eFuse MAC -> 6 ký tự HEX viết hoa duy nhất cho từng máy (vd: A3F91C)
    uint8_t b3 = (uint8_t)((mac >> 24) & 0xFF);
    uint8_t b4 = (uint8_t)((mac >> 32) & 0xFF);
    uint8_t b5 = (uint8_t)((mac >> 40) & 0xFF);
    snprintf(out, len, "%02X%02X%02X", b3, b4, b5);
}

void EcosystemService::getMdnsHost(char* out, size_t len) {
    char id[12];
    getDeviceId(id, sizeof(id));
    // Dùng 4 ký tự cuối dạng thường: smartdesk-f91c (tránh trùng mDNS nhiều máy trong văn phòng)
    char tail[5];
    for (int i = 0; i < 4; i++) {
        tail[i] = (char)tolower((unsigned char)id[i + 2]);
    }
    tail[4] = '\0';
    snprintf(out, len, "smartdesk-%s", tail);
}

void EcosystemService::getPairedMacStr(char* out, size_t len) const {
    if (!_isPairedLocked) {
        strlcpy(out, "", len);
        return;
    }
    snprintf(out, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             _pairedMac[0], _pairedMac[1], _pairedMac[2],
             _pairedMac[3], _pairedMac[4], _pairedMac[5]);
}

void EcosystemService::_loadPairedMac() {
    Preferences prefs;
    if (prefs.begin("cyd_eco", true)) {
        if (prefs.getBytesLength("mac") == 6) {
            prefs.getBytes("mac", _pairedMac, 6);
            _isPairedLocked = true;
            Serial.printf("[Ecosystem] Loaded locked EMO MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          _pairedMac[0], _pairedMac[1], _pairedMac[2],
                          _pairedMac[3], _pairedMac[4], _pairedMac[5]);
        }
        prefs.end();
    }
}

void EcosystemService::_savePairedMac() {
    Preferences prefs;
    if (prefs.begin("cyd_eco", false)) {
        if (_isPairedLocked) {
            prefs.putBytes("mac", _pairedMac, 6);
        } else {
            prefs.remove("mac");
        }
        prefs.end();
    }
}

void EcosystemService::startPairingMode(uint32_t durationMs) {
    _pairingMode = true;
    _pairingEndMs = millis() + durationMs;
    _lastPairReqMs = 0;
    Serial.printf("[Ecosystem] Pairing Mode ACTIVE for %lus — waiting for EMO Companion...\n",
                  (unsigned long)(durationMs / 1000));
}

void EcosystemService::unpairCompanion() {
    _pairingMode = false;
    _isPairedLocked = false;
    _hasPeerMac = false;
    _lastCompanionHeartbeat = 0;
    memset(_pairedMac, 0, 6);
    memset(_peerMac, 0, 6);
    _savePairedMac();
    Serial.println("[Ecosystem] Unpaired EMO Companion (MAC lock cleared)");
}

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
        if (!esp_now_is_peer_exist(_broadcastMac)) {
            // WiFiManager hoặc quá trình đổi Wi-Fi có thể đã tắt tầng ESP-NOW -> khởi tạo lại
            if (esp_now_init() == ESP_OK) {
                esp_now_register_recv_cb(espNowRxCallback);
            }
            _ensureBroadcastPeer();
            _hasPeerMac = false;
        }
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
    _loadPairedMac();
    if (_isPairedLocked) {
        _registerPeerMac(_pairedMac);
    }

    _initialized = true;
    _lastPingSent = millis();
    char devId[12];
    getDeviceId(devId, sizeof(devId));
    Serial.printf("[Ecosystem] ESP-NOW Master Station initialized | DeviceID=%s | Locked=%d\n",
                  devId, _isPairedLocked ? 1 : 0);

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

    // Nếu đã khóa cặp với 1 EMO cụ thể -> CHỈ gửi Unicast tới đúng MAC đó (không bao giờ Broadcast sang bàn khác!)
    if (_isPairedLocked) {
        if (!_hasPeerMac || !esp_now_is_peer_exist(_pairedMac)) {
            _registerPeerMac(_pairedMac);
        }
        esp_err_t res = esp_now_send(_pairedMac, (const uint8_t *)&pkt, sizeof(pkt));
        return (res == ESP_OK);
    }

    // Chưa khóa cặp: ưu tiên Unicast nếu đã biết MAC tạm, fallback Broadcast
    if (_hasPeerMac && esp_now_is_peer_exist(_peerMac)) {
        if (esp_now_send(_peerMac, (const uint8_t *)&pkt, sizeof(pkt)) == ESP_OK) {
            return true;
        }
    }

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

    // Lọc chống nhiễu văn phòng: nếu đã khóa cặp MAC và không ở Pairing Mode -> bỏ qua 100% MAC lạ!
    if (_isPairedLocked && !_pairingMode) {
        if (!mac || memcmp(mac, _pairedMac, 6) != 0) {
            return;
        }
    }

    // Nếu đang ở Pairing Mode -> nhận EMO đầu tiên phản hồi (PAIR_ACK hoặc HEARTBEAT) để khóa cứng MAC trên Core 1
    if (_pairingMode && mac && (pkt.eventType == EVT_PAIR_ACK || pkt.eventType == EVT_HEARTBEAT || pkt.eventType == EVT_HEARTBEAT_ACK)) {
        memcpy(_pendingPeerMac, mac, 6);
        _pendingPairLock = true;
        _hasPendingPeerMac = true;
    } else if (mac && (!_hasPeerMac || memcmp(_peerMac, mac, 6) != 0)) {
        memcpy(_pendingPeerMac, mac, 6);
        _hasPendingPeerMac = true;
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

    // 0. Đăng ký Unicast Peer an toàn trên Core 1 nếu vừa nhận gói tin từ EMO Companion
    if (_hasPendingPeerMac) {
        uint8_t macCopy[6];
        memcpy(macCopy, _pendingPeerMac, 6);
        bool shouldLock = _pendingPairLock;
        _hasPendingPeerMac = false;
        _pendingPairLock = false;
        _registerPeerMac(macCopy);
        if (shouldLock) {
            memcpy(_pairedMac, macCopy, 6);
            _isPairedLocked = true;
            _pairingMode = false;
            _savePairedMac();
            Serial.printf("[Ecosystem] LOCKED pair with EMO MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          macCopy[0], macCopy[1], macCopy[2], macCopy[3], macCopy[4], macCopy[5]);
            sendEvent(EVT_PAIR_ACK, 0);
            broadcastWeatherSync();
        }
    }

    // Xử lý Pairing Mode (phát Broadcast EVT_PAIR_REQ mỗi 1.5s trong 30 giây)
    uint32_t now = millis();
    if (_pairingMode) {
        if ((int32_t)(now - _pairingEndMs) >= 0) {
            _pairingMode = false;
            Serial.println("[Ecosystem] Pairing Mode timed out (30s)");
        } else if (now - _lastPairReqMs >= 1500) {
            _lastPairReqMs = now;
            EcosystemPacket pkt = { ECO_MAGIC, ECO_SENDER_CYD, EVT_PAIR_REQ, 0, 0 };
            pkt.checksum = ecoCalculateChecksum(pkt);
            _ensureBroadcastPeer();
            esp_now_send(_broadcastMac, (const uint8_t *)&pkt, sizeof(pkt));
        }
    }

    // Nếu chưa khóa cặp và EMO mất tín hiệu > 45s -> xóa cache Unicast tạm để tự Broadcast dò lại
    if (!_isPairedLocked && _hasPeerMac && !isCompanionPaired()) {
        _hasPeerMac = false;
    }

    // 1. Xử lý các gói tin nhận được từ EMO Companion trên luồng chính Core 1
    while (_rxTail != _rxHead) {
        EcosystemPacket pkt = _rxQueue[_rxTail];
        _rxTail = (_rxTail + 1) % RX_QUEUE_SIZE;
        _processPacket(pkt);
    }

    // 2. Phát nhịp tim định kỳ mỗi 15 giây để tự động nhận diện và giữ kết nối
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
