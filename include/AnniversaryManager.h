// ═══════════════════════════════════════════════════════════════════════
//  AnniversaryManager.h — Smart Desk Anniversary & Perpetual Calendar Engine
//  Features: User personal anniversaries (Solar & Lunar), national holidays,
//            24 solar terms, Can Chi, daily inspirational motivational quotes.
// ═══════════════════════════════════════════════════════════════════════
#ifndef ANNIVERSARY_MANAGER_H
#define ANNIVERSARY_MANAGER_H

#include <Arduino.h>

#define MAX_ANNIVERSARIES 32

struct AnniversaryItem {
    uint8_t day;        // 1..31
    uint8_t month;      // 1..12
    bool    isLunar;    // true = Âm lịch, false = Dương lịch
    char    title[48];  // Tên sự kiện (UTF-8)
};

class AnniversaryManager {
public:
    AnniversaryManager();
    void begin();
    void save();

    bool addItem(uint8_t day, uint8_t month, bool isLunar, const char* title);
    bool deleteItem(uint8_t index);
    void clear();

    uint8_t getCount() const { return _count; }
    const AnniversaryItem* getItem(uint8_t index) const;

    // Trả về thông điệp ticker đầy đủ cho ngày hôm nay theo mức độ ưu tiên
    String getTodayTickerMessage(bool isEnglish = false);

    // Các hàm kiểm tra chi tiết
    String checkPersonalAnniversary(uint8_t sDay, uint8_t sMonth, uint8_t lDay, uint8_t lMonth, bool isEnglish);
    String checkHoliday(uint8_t sDay, uint8_t sMonth, uint8_t lDay, uint8_t lMonth, bool isEnglish);
    String getPerpetualCalendarFallback(uint8_t sDay, uint8_t sMonth, uint16_t sYear, uint8_t lDay, uint8_t lMonth, const char* dayCanChi, bool isEnglish);

private:
    AnniversaryItem _items[MAX_ANNIVERSARIES];
    uint8_t         _count;
    bool            _loaded;
};

extern AnniversaryManager anniversaryMgr;

#endif // ANNIVERSARY_MANAGER_H
