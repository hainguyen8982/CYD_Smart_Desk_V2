// ═══════════════════════════════════════════════════════════════════════
//  FinanceService.h — Real-time Gold & Exchange Rate Service
//  CYD SmartDesk V2 — Multi-Currency Configurable
// ═══════════════════════════════════════════════════════════════════════
#ifndef FINANCE_SERVICE_H
#define FINANCE_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct FinanceData {
    // ── Vàng SJC 9999 ──
    float sjcBuy;               // Triệu VND / lượng (vd: 140.3f)
    float sjcSell;              // Triệu VND / lượng (vd: 143.3f)
    float sjcChange;            // Triệu VND / lượng (vd: -1.0f)
    float sjcChangePercent;     // Phần trăm biến động (vd: -0.7f)
    char  sjcChangeStr[12];     // Chuỗi hiển thị: "+400K" hoặc "-1.0tr"
    float sjc7d[7];             // Lịch sử 7 ngày bán ra (0: cũ nhất, 6: hôm nay)
    float sjcMin7d;             // Giá thấp nhất 7 ngày
    float sjcMax7d;             // Giá cao nhất 7 ngày

    // ── Vàng Thế Giới (Kitco / PAX Gold) ──
    float worldGoldUsd;         // USD / troy oz (vd: 4151.2f)
    float worldGoldChgPct;      // % biến động 24h
    float domesticDiffTr;       // Chênh lệch trong nước vs TG (triệu / lượng)

    // ── Ngoại tệ Vị trí 1 (Mặc định: USD) ──
    char  cur1Code[8];          // Vd: "USD", "EUR", "CAD", "JPY"...
    float cur1Rate;             // Tỉ giá VND (vd: 25897.0f)
    float cur1ChangePercent;    // % biến động (vd: +0.15f)
    float cur1Bars[5];          // Delta 5 ngày cho biểu đồ microbars
    float cur17d[7];            // Lịch sử 7 ngày cho popup
    float cur1Min7d;
    float cur1Max7d;

    // ── Ngoại tệ Vị trí 2 (Mặc định: EUR, có thể đổi sang CAD, JPY, GBP...) ──
    char  cur2Code[8];          // Vd: "EUR", "CAD", "JPY", "GBP"...
    float cur2Rate;             // Tỉ giá VND (vd: 18630.0f hoặc 28986.0f)
    float cur2ChangePercent;    // % biến động (vd: -0.28f)
    float cur2Bars[5];          // Delta 5 ngày cho biểu đồ microbars
    float cur27d[7];            // Lịch sử 7 ngày cho popup
    float cur2Min7d;
    float cur2Max7d;

    // ── Macro / Crypto: BTC / USDT ──
    float btcRate;              // USD (vd: 83492.0f)
    float btcChangePercent;     // % biến động 24h (vd: -0.4f)

    bool          valid;
    unsigned long lastFetchMs;
};

class FinanceService {
public:
    FinanceService();

    void begin();               // Khởi tạo, tải bộ nhớ đệm NVS
    void update();              // Kiểm tra định kỳ (mỗi 15 phút)
    void fetchNow();            // Buộc cập nhật ngay lập tức

    // Tùy chỉnh cặp ngoại tệ (lưu vào NVS và tự làm mới)
    void setCurrencies(const char* c1, const char* c2);

    const FinanceData& getData() const { return _data; }
    bool isValid() const { return _data.valid; }
    const char* getCur1() const { return _data.cur1Code; }
    const char* getCur2() const { return _data.cur2Code; }

    static void formatVND(long value, char* out, size_t outSize);

private:
    void _loadCache();
    void _saveCache();
    void _fetchGoldSJC();
    void _fetchExchangeRates();
    void _fetchCrypto();

    FinanceData   _data;
    unsigned long _lastFetch;
    bool          _isFetching;
};

extern FinanceService finance;

#endif // FINANCE_SERVICE_H
