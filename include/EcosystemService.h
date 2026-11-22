#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "EcosystemProtocol.h"

class EcosystemService {
public:
    EcosystemService();
    void begin();
    void update();
    bool sendEvent(uint8_t eventType, uint16_t payload = 0);
    void broadcastWeatherSync();

    bool isCompanionPaired() const;
    bool isPairedLocked() const { return _isPairedLocked; }
    bool isPairingMode() const { return _pairingMode; }
    void startPairingMode(uint32_t durationMs = 30000);
    void unpairCompanion();
    void getPairedMacStr(char* out, size_t len) const;
    static void getDeviceId(char* out, size_t len);
    static void getMdnsHost(char* out, size_t len);
    const char* getStatusText(bool isEnglish = false) const;

    // Callback xử lý gói tin nhận từ tầng Wi-Fi (Core 0)
    void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);

private:
    bool _initialized = false;
    volatile uint32_t _lastCompanionHeartbeat = 0;
    uint32_t _lastPingSent = 0;
    static constexpr uint32_t COMPANION_TIMEOUT_MS = 45000; // 45s không phản hồi -> Độc lập (Standalone)
    static constexpr uint32_t PING_INTERVAL_MS = 15000;     // Phát nhịp tim mỗi 15s

    uint8_t _broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t _peerMac[6] = {0, 0, 0, 0, 0, 0};
    bool _hasPeerMac = false;
    uint8_t _pairedMac[6] = {0, 0, 0, 0, 0, 0};
    volatile bool _isPairedLocked = false;
    volatile bool _pairingMode = false;
    uint32_t _pairingEndMs = 0;
    uint32_t _lastPairReqMs = 0;
    uint8_t _pendingPeerMac[6] = {0, 0, 0, 0, 0, 0};
    volatile bool _hasPendingPeerMac = false;
    volatile bool _pendingPairLock = false;

    // Hàng đợi vòng nhẹ (Lock-free Ring Buffer) để đẩy sự kiện từ Callback Core 0 sang Core 1
    static constexpr uint8_t RX_QUEUE_SIZE = 8;
    EcosystemPacket _rxQueue[RX_QUEUE_SIZE];
    volatile uint8_t _rxHead = 0;
    volatile uint8_t _rxTail = 0;

    void _loadPairedMac();
    void _savePairedMac();
    void _ensureBroadcastPeer();
    void _registerPeerMac(const uint8_t *mac);
    void _processPacket(const EcosystemPacket &pkt);
};

extern EcosystemService ecosystem;
