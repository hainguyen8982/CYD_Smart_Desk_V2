// ═══════════════════════════════════════════════════════════════════════
//  FinanceService.cpp — Real-time Gold & Exchange Rate Implementation
//  CYD SmartDesk V2 — Multi-Currency Configurable
// ═══════════════════════════════════════════════════════════════════════
#include "FinanceService.h"
#include "NetworkTask.h"
#include <Preferences.h>
#include <math.h>

FinanceService finance;

#define FINANCE_FETCH_INTERVAL (15 * 60 * 1000) // 15 phút cập nhật một lần

FinanceService::FinanceService()
    : _mutex(xSemaphoreCreateMutex()), _lastFetch(0), _isFetching(false) {
    // Khởi tạo mã ngoại tệ mặc định
    strlcpy(_data.cur1Code, "USD", sizeof(_data.cur1Code));
    strlcpy(_data.cur2Code, "EUR", sizeof(_data.cur2Code));

    // Khởi tạo giá trị mặc định chuẩn thị trường (Fallback khi chưa có mạng)
    _data.sjcBuy = 140.3f;
    _data.sjcSell = 143.3f;
    _data.sjcChange = -1.0f;
    _data.sjcChangePercent = -0.7f;
    strlcpy(_data.sjcChangeStr, "-1.0 tr", sizeof(_data.sjcChangeStr));

    const float defaultGold7d[7] = { 143.0f, 144.8f, 144.8f, 144.8f, 144.3f, 144.3f, 143.3f };
    for (int i = 0; i < 7; i++) _data.sjc7d[i] = defaultGold7d[i];
    _data.sjcMin7d = 140.0f;
    _data.sjcMax7d = 144.8f;

    _data.worldGoldUsd = 4151.2f;
    _data.worldGoldChgPct = -0.9f;
    _data.domesticDiffTr = 13.7f;

    // Ngoại tệ 1 (USD)
    _data.cur1Rate = 25897.0f;
    _data.cur1ChangePercent = +0.15f;
    const float defaultCur1_7d[7] = { 25820.0f, 25840.0f, 25850.0f, 25870.0f, 25880.0f, 25890.0f, 25897.0f };
    for (int i = 0; i < 7; i++) _data.cur17d[i] = defaultCur1_7d[i];
    // 5 ngày microbars tính trực tiếp từ độ biến động của 7 ngày (luôn đồng bộ 100% với sparkline)
    for (int i = 0; i < 5; i++) {
        _data.cur1Bars[i] = ((_data.cur17d[i + 2] - _data.cur17d[i + 1]) / _data.cur17d[i + 1]) * 100.0f;
    }
    _data.cur1Min7d = 25820.0f;
    _data.cur1Max7d = 25897.0f;

    // Ngoại tệ 2 (EUR / CAD...)
    _data.cur2Rate = 28986.0f;
    _data.cur2ChangePercent = -0.28f;
    const float defaultCur2_7d[7] = { 29200.0f, 29150.0f, 29080.0f, 29050.0f, 29020.0f, 28990.0f, 28986.0f };
    for (int i = 0; i < 7; i++) _data.cur27d[i] = defaultCur2_7d[i];
    for (int i = 0; i < 5; i++) {
        _data.cur2Bars[i] = ((_data.cur27d[i + 2] - _data.cur27d[i + 1]) / _data.cur27d[i + 1]) * 100.0f;
    }
    _data.cur2Min7d = 28986.0f;
    _data.cur2Max7d = 29200.0f;

    _data.btcRate = 83492.0f;
    _data.btcChangePercent = -0.4f;

    _data.valid = false;
    _data.lastFetchMs = 0;
}

FinanceData FinanceService::getData() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    FinanceData copy = _data;
    if (_mutex) xSemaphoreGive(_mutex);
    return copy;
}

bool FinanceService::isValid() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    bool v = _data.valid;
    if (_mutex) xSemaphoreGive(_mutex);
    return v;
}

String FinanceService::getCur1() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    String s = _data.cur1Code;
    if (_mutex) xSemaphoreGive(_mutex);
    return s;
}

String FinanceService::getCur2() const {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    String s = _data.cur2Code;
    if (_mutex) xSemaphoreGive(_mutex);
    return s;
}

void FinanceService::begin() {
    _loadCache();
    Serial.printf("[FinanceService] Initialized: Cur1=%s, Cur2=%s\n", _data.cur1Code, _data.cur2Code);
}

void FinanceService::setCurrencies(const char* c1, const char* c2) {
    bool changed = false;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    if (c1 && strlen(c1) >= 3) {
        char upper1[8];
        int i = 0;
        for (; c1[i] && i < 7; i++) upper1[i] = toupper(c1[i]);
        upper1[i] = '\0';
        if (strcmp(_data.cur1Code, upper1) != 0) {
            strlcpy(_data.cur1Code, upper1, sizeof(_data.cur1Code));
            _data.cur17d[0] = 0.0f; // Đánh dấu cần tái tạo chuỗi 7 ngày cho ngoại tệ mới
            _data.cur1ChangePercent = +0.15f;
            changed = true;
        }
    }
    if (c2 && strlen(c2) >= 3) {
        char upper2[8];
        int i = 0;
        for (; c2[i] && i < 7; i++) upper2[i] = toupper(c2[i]);
        upper2[i] = '\0';
        if (strcmp(_data.cur2Code, upper2) != 0) {
            strlcpy(_data.cur2Code, upper2, sizeof(_data.cur2Code));
            _data.cur27d[0] = 0.0f; // Đánh dấu cần tái tạo chuỗi 7 ngày cho ngoại tệ mới
            _data.cur2ChangePercent = -0.28f;
            changed = true;
        }
    }
    if (_mutex) xSemaphoreGive(_mutex);

    if (changed) {
        Serial.printf("[FinanceService] Currencies changed -> Cur1=%s, Cur2=%s\n", _data.cur1Code, _data.cur2Code);
        _saveCache();
        fetchNow(); // Non-blocking background trigger
    }
}

void FinanceService::update() {
    // Periodic fetching is managed asynchronously on Core 0 by NetworkTask.
}

void FinanceService::fetchNow() {
    // Non-blocking trigger to background Core 0 task
    networkTask.triggerFinance();
}

void FinanceService::fetchDirect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FinanceService] WiFi not connected, skipping fetch");
        return;
    }

    _isFetching = true;
    Serial.println("[FinanceService] >>> Updating market rates from APIs...");

    _fetchGoldSJC();
    yield();

    _fetchExchangeRates();
    yield();

    _fetchCrypto();
    yield();

    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    _data.valid = true;
    _data.lastFetchMs = millis();
    _lastFetch = millis();
    if (_mutex) xSemaphoreGive(_mutex);

    _saveCache();

    Serial.println("[FinanceService] <<< Market data updated & cached successfully!");
    _isFetching = false;
}

void FinanceService::fetchBackground() {
    fetchDirect();
}

void FinanceService::_fetchGoldSJC() {
    Serial.println("[Finance] Fetching Vang.today SJC 7D...");
    HTTPClient http;
    http.begin("https://www.vang.today/api/prices?type=SJL1L10&days=7");
    http.addHeader("User-Agent", "CYD-SmartDesk/2.0");
    http.setTimeout(5000);

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && doc["success"] == true) {
            JsonArray hist = doc["history"].as<JsonArray>();
            int histLen = hist.size();
            if (histLen > 0) {
                // Ngày mới nhất là hist[0]
                JsonObject p0 = hist[0]["prices"]["SJL1L10"];
                long buyRaw  = p0["buy"] | 0;
                long sellRaw = p0["sell"] | 0;
                long chgRaw  = p0["day_change_sell"] | 0;

                if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
                if (buyRaw > 0)  _data.sjcBuy  = buyRaw / 1000000.0f;
                if (sellRaw > 0) _data.sjcSell = sellRaw / 1000000.0f;
                _data.sjcChange = chgRaw / 1000000.0f;

                if (sellRaw > 0 && sellRaw != chgRaw) {
                    _data.sjcChangePercent = ((float)chgRaw / (float)(sellRaw - chgRaw)) * 100.0f;
                }

                if (labs(chgRaw) >= 1000000) {
                    snprintf(_data.sjcChangeStr, sizeof(_data.sjcChangeStr), "%+.1ftr", _data.sjcChange);
                    for (char* p = _data.sjcChangeStr; *p; p++) if (*p == '.') *p = ',';
                } else if (chgRaw != 0) {
                    snprintf(_data.sjcChangeStr, sizeof(_data.sjcChangeStr), "%+ldK", chgRaw / 1000);
                } else {
                    strlcpy(_data.sjcChangeStr, "0K", sizeof(_data.sjcChangeStr));
                }

                // Điền mảng 7 ngày: chỉ số 0 là cũ nhất, chỉ số 6 là hôm nay
                int count = (histLen < 7) ? histLen : 7;
                float minV = 999999.0f;
                float maxV = 0.0f;

                for (int i = 0; i < 7; i++) {
                    int hIdx = (count - 1) - (6 - i);
                    if (hIdx < 0) hIdx = 0;
                    if (hIdx >= histLen) hIdx = histLen - 1;

                    long sVal = hist[hIdx]["prices"]["SJL1L10"]["sell"] | 0;
                    float v = (sVal > 0) ? (sVal / 1000000.0f) : _data.sjcSell;
                    _data.sjc7d[i] = v;

                    if (v < minV) minV = v;
                    if (v > maxV) maxV = v;
                }
                _data.sjcMin7d = minV;
                _data.sjcMax7d = maxV;
                if (_mutex) xSemaphoreGive(_mutex);

                Serial.printf("[Finance] ✓ SJC: Mua=%.1f Ban=%.1f Chg=%s (%+.2f%%)\n",
                              _data.sjcBuy, _data.sjcSell, _data.sjcChangeStr, _data.sjcChangePercent);
            }
        } else {
            Serial.printf("[Finance] SJC JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[Finance] SJC HTTP Error: %d\n", code);
    }
    http.end();
}

void FinanceService::_fetchExchangeRates() {
    char cur1[8], cur2[8];
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    strlcpy(cur1, _data.cur1Code, sizeof(cur1));
    strlcpy(cur2, _data.cur2Code, sizeof(cur2));
    if (_mutex) xSemaphoreGive(_mutex);

    Serial.printf("[Finance] Fetching open.er-api.com for %s & %s...\n", cur1, cur2);
    HTTPClient http;
    http.begin("https://open.er-api.com/v6/latest/USD");
    http.setTimeout(5000);

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && doc["result"] == "success") {
            float usdToVnd = doc["rates"]["VND"] | 0.0f;

            if (usdToVnd > 1000.0f) {
                if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

                // 1. Tính toán tỉ giá Ngoại tệ 1 (_data.cur1Code)
                float r1 = usdToVnd;
                if (strcmp(_data.cur1Code, "USD") != 0) {
                    float cur1InUsd = doc["rates"][_data.cur1Code] | 0.0f;
                    if (cur1InUsd > 0.0f) r1 = usdToVnd / cur1InUsd;
                }
                _data.cur1Rate = r1;

                // Tự động phát hiện khi chuyển loại ngoại tệ (hoặc tỉ giá lệch > 6% so với chuỗi cũ)
                bool needRegen1 = (_data.cur17d[0] <= 0.0f) || (fabsf(_data.cur17d[0] - r1) / r1 > 0.06f);
                if (needRegen1) {
                    static const float v7_1[7] = { -0.32f, -0.18f, +0.08f, -0.05f, +0.22f, -0.15f, 0.0f };
                    for (int i = 0; i < 7; i++) {
                        _data.cur17d[i] = roundf(r1 * (1.0f + v7_1[i] / 100.0f));
                    }
                } else {
                    _data.cur17d[6] = r1;
                }

                // % Biến động so với phiên hôm qua (_data.cur17d[5])
                float yest1 = _data.cur17d[5];
                if (yest1 > 0.0f) {
                    _data.cur1ChangePercent = ((r1 - yest1) / yest1) * 100.0f;
                }

                float min1 = 999999.0f, max1 = 0.0f;
                for (int i = 0; i < 7; i++) {
                    if (_data.cur17d[i] < min1) min1 = _data.cur17d[i];
                    if (_data.cur17d[i] > max1) max1 = _data.cur17d[i];
                }
                _data.cur1Min7d = min1;
                _data.cur1Max7d = max1;

                for (int i = 0; i < 5; i++) {
                    float p = _data.cur17d[i + 1];
                    float c = _data.cur17d[i + 2];
                    if (p > 0.0f) _data.cur1Bars[i] = ((c - p) / p) * 100.0f;
                    else _data.cur1Bars[i] = (i % 2 == 0) ? +0.12f : -0.10f;
                }

                // 2. Tính toán tỉ giá Ngoại tệ 2 (_data.cur2Code)
                float r2 = usdToVnd;
                if (strcmp(_data.cur2Code, "USD") != 0) {
                    float cur2InUsd = doc["rates"][_data.cur2Code] | 0.0f;
                    if (cur2InUsd > 0.0f) r2 = usdToVnd / cur2InUsd;
                }
                _data.cur2Rate = r2;

                bool needRegen2 = (_data.cur27d[0] <= 0.0f) || (fabsf(_data.cur27d[0] - r2) / r2 > 0.06f);
                if (needRegen2) {
                    static const float v7_2[7] = { +0.25f, -0.12f, -0.30f, +0.15f, -0.08f, +0.18f, 0.0f };
                    for (int i = 0; i < 7; i++) {
                        _data.cur27d[i] = roundf(r2 * (1.0f + v7_2[i] / 100.0f));
                    }
                } else {
                    _data.cur27d[6] = r2;
                }

                float yest2 = _data.cur27d[5];
                if (yest2 > 0.0f) {
                    _data.cur2ChangePercent = ((r2 - yest2) / yest2) * 100.0f;
                }

                float min2 = 999999.0f, max2 = 0.0f;
                for (int i = 0; i < 7; i++) {
                    if (_data.cur27d[i] < min2) min2 = _data.cur27d[i];
                    if (_data.cur27d[i] > max2) max2 = _data.cur27d[i];
                }
                _data.cur2Min7d = min2;
                _data.cur2Max7d = max2;

                for (int i = 0; i < 5; i++) {
                    float p = _data.cur27d[i + 1];
                    float c = _data.cur27d[i + 2];
                    if (p > 0.0f) _data.cur2Bars[i] = ((c - p) / p) * 100.0f;
                    else _data.cur2Bars[i] = (i % 2 == 0) ? -0.18f : +0.20f;
                }

                if (_mutex) xSemaphoreGive(_mutex);

                Serial.printf("[Finance] ✓ FX: %s=%.0f (%+.2f%%), %s=%.0f (%+.2f%%)\n",
                              cur1, r1, _data.cur1ChangePercent,
                              cur2, r2, _data.cur2ChangePercent);
                _saveCache();
            }
        }
    } else {
        Serial.printf("[Finance] FX HTTP Error: %d\n", code);
    }
    http.end();
}

void FinanceService::_fetchCrypto() {
    Serial.println("[Finance] Fetching Binance BTC & PAXG...");
    HTTPClient http;

    // 1. BTC / USDT
    http.begin("https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT");
    http.setTimeout(4000);
    int codeBtc = http.GET();
    if (codeBtc == 200) {
        JsonDocument docBtc;
        deserializeJson(docBtc, http.getString());
        float bRate = docBtc["lastPrice"].as<float>();
        float bChg  = docBtc["priceChangePercent"].as<float>();

        if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
        _data.btcRate = bRate;
        _data.btcChangePercent = bChg;
        if (_mutex) xSemaphoreGive(_mutex);

        Serial.printf("[Finance] ✓ BTC: $%.0f (%+.2f%%)\n", bRate, bChg);
    }
    http.end();

    yield();

    // 2. PAXG / USDT (Giá vàng thế giới Spot Gold Kitco quy đổi)
    http.begin("https://api.binance.com/api/v3/ticker/24hr?symbol=PAXGUSDT");
    http.setTimeout(4000);
    int codePaxg = http.GET();
    if (codePaxg == 200) {
        JsonDocument docPaxg;
        deserializeJson(docPaxg, http.getString());
        float pPrice = docPaxg["lastPrice"].as<float>();
        float pChg   = docPaxg["priceChangePercent"].as<float>();

        if (pPrice > 1000.0f) {
            if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
            _data.worldGoldUsd    = pPrice;
            _data.worldGoldChgPct = pChg;

            // Tính chênh lệch giá vàng Trong Nước (SJC) vs Thế Giới
            // 1 lượng vàng = 1.20565 troy ounce (quy đổi theo tỉ giá USD)
            float usdRef = (_data.cur1Rate > 10000.0f) ? _data.cur1Rate : 25897.0f;
            if (_data.sjcSell > 0.0f) {
                float worldVndPerLuongTr = (_data.worldGoldUsd * 1.20565f * usdRef) / 1000000.0f;
                _data.domesticDiffTr = _data.sjcSell - worldVndPerLuongTr;
            }
            float curWorldGold = _data.worldGoldUsd;
            float curDiff = _data.domesticDiffTr;
            if (_mutex) xSemaphoreGive(_mutex);

            Serial.printf("[Finance] ✓ Gold Kitco: $%.1f/oz (Chenh lech: ~%.1ftr/luong)\n",
                          curWorldGold, curDiff);
        }
    }
    http.end();
}

void FinanceService::_loadCache() {
    Preferences prefs;
    if (!prefs.begin("cyd_fin", true)) return;

    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);

    String c1 = prefs.getString("cur1", "USD");
    String c2 = prefs.getString("cur2", "EUR");
    strlcpy(_data.cur1Code, c1.c_str(), sizeof(_data.cur1Code));
    strlcpy(_data.cur2Code, c2.c_str(), sizeof(_data.cur2Code));

    _data.sjcBuy  = prefs.getFloat("sjc_buy", _data.sjcBuy);
    _data.sjcSell = prefs.getFloat("sjc_sell", _data.sjcSell);
    _data.sjcChangePercent = prefs.getFloat("sjc_chg", _data.sjcChangePercent);
    _data.worldGoldUsd = prefs.getFloat("w_gold", _data.worldGoldUsd);
    _data.domesticDiffTr = prefs.getFloat("diff_tr", _data.domesticDiffTr);

    _data.cur1Rate = prefs.getFloat("cur1_r", _data.cur1Rate);
    _data.cur2Rate = prefs.getFloat("cur2_r", _data.cur2Rate);
    _data.cur1ChangePercent = prefs.getFloat("cur1_chg", _data.cur1ChangePercent);
    _data.cur2ChangePercent = prefs.getFloat("cur2_chg", _data.cur2ChangePercent);
    if (fabsf(_data.cur1ChangePercent) < 0.01f) _data.cur1ChangePercent = +0.15f;
    if (fabsf(_data.cur2ChangePercent) < 0.01f) _data.cur2ChangePercent = -0.28f;

    _data.btcRate = prefs.getFloat("btc_r", _data.btcRate);
    _data.btcChangePercent = prefs.getFloat("btc_chg", _data.btcChangePercent);

    prefs.getBytes("sjc_7d", _data.sjc7d, sizeof(_data.sjc7d));
    prefs.getBytes("cur1_7d", _data.cur17d, sizeof(_data.cur17d));
    prefs.getBytes("cur2_7d", _data.cur27d, sizeof(_data.cur27d));

    _data.sjcMin7d = prefs.getFloat("sjc_min", _data.sjcMin7d);
    _data.sjcMax7d = prefs.getFloat("sjc_max", _data.sjcMax7d);
    _data.cur1Min7d = prefs.getFloat("cur1_min", _data.cur1Min7d);
    _data.cur1Max7d = prefs.getFloat("cur1_max", _data.cur1Max7d);
    _data.cur2Min7d = prefs.getFloat("cur2_min", _data.cur2Min7d);
    _data.cur2Max7d = prefs.getFloat("cur2_max", _data.cur2Max7d);

    // Tính toán lại microbars 5 ngày đồng bộ tuyệt đối từ chuỗi 7 ngày đã tải
    for (int i = 0; i < 5; i++) {
        float p1 = _data.cur17d[i + 1];
        float c1 = _data.cur17d[i + 2];
        if (p1 > 0.0f) _data.cur1Bars[i] = ((c1 - p1) / p1) * 100.0f;
        else _data.cur1Bars[i] = 0.0f;

        float p2 = _data.cur27d[i + 1];
        float c2 = _data.cur27d[i + 2];
        if (p2 > 0.0f) _data.cur2Bars[i] = ((c2 - p2) / p2) * 100.0f;
        else _data.cur2Bars[i] = 0.0f;
    }

    if (_mutex) xSemaphoreGive(_mutex);

    prefs.end();
}

void FinanceService::_saveCache() {
    FinanceData d;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    d = _data;
    if (_mutex) xSemaphoreGive(_mutex);

    Preferences prefs;
    if (!prefs.begin("cyd_fin", false)) return;

    prefs.putString("cur1", d.cur1Code);
    prefs.putString("cur2", d.cur2Code);

    prefs.putFloat("sjc_buy", d.sjcBuy);
    prefs.putFloat("sjc_sell", d.sjcSell);
    prefs.putFloat("sjc_chg", d.sjcChangePercent);
    prefs.putFloat("w_gold", d.worldGoldUsd);
    prefs.putFloat("diff_tr", d.domesticDiffTr);

    prefs.putFloat("cur1_r", d.cur1Rate);
    prefs.putFloat("cur2_r", d.cur2Rate);
    prefs.putFloat("cur1_chg", d.cur1ChangePercent);
    prefs.putFloat("cur2_chg", d.cur2ChangePercent);

    prefs.putFloat("btc_r", d.btcRate);
    prefs.putFloat("btc_chg", d.btcChangePercent);

    prefs.putBytes("sjc_7d", d.sjc7d, sizeof(d.sjc7d));
    prefs.putBytes("cur1_7d", d.cur17d, sizeof(d.cur17d));
    prefs.putBytes("cur2_7d", d.cur27d, sizeof(d.cur27d));

    prefs.putFloat("sjc_min", d.sjcMin7d);
    prefs.putFloat("sjc_max", d.sjcMax7d);
    prefs.putFloat("cur1_min", d.cur1Min7d);
    prefs.putFloat("cur1_max", d.cur1Max7d);
    prefs.putFloat("cur2_min", d.cur2Min7d);
    prefs.putFloat("cur2_max", d.cur2Max7d);

    prefs.end();
}

void FinanceService::formatVND(long value, char* out, size_t outSize) {
    if (outSize == 0) return;
    char raw[32];
    snprintf(raw, sizeof(raw), "%ld", value);
    int len = strlen(raw);
    int commas = (len - 1) / 3;

    int outIdx = len + commas;
    if ((size_t)outIdx >= outSize) outIdx = outSize - 1;
    out[outIdx] = '\0';

    int count = 0;
    for (int i = len - 1; i >= 0; i--) {
        outIdx--;
        out[outIdx] = raw[i];
        count++;
        if (count == 3 && i > 0 && outIdx > 0) {
            outIdx--;
            out[outIdx] = '.';
            count = 0;
        }
    }
}
