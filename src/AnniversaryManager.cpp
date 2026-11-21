// ═══════════════════════════════════════════════════════════════════════
//  AnniversaryManager.cpp — Smart Desk Anniversary & Perpetual Calendar Engine
// ═══════════════════════════════════════════════════════════════════════
#include "AnniversaryManager.h"
#include "vn_lunar.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

AnniversaryManager anniversaryMgr;

// ── Built-in Solar Holidays ──────────────────────────────────────────
struct FixedHoliday {
    uint8_t     day;
    uint8_t     month;
    const char* nameVi;
    const char* nameEn;
};

static const FixedHoliday SOLAR_HOLIDAYS[] = {
    {  1,  1, "Tết Dương Lịch", "New Year's Day" },
    { 14,  2, "Lễ Tình Nhân (Valentine)", "Valentine's Day" },
    { 27,  2, "Ngày Thầy Thuốc Việt Nam", "Vietnamese Doctor's Day" },
    {  8,  3, "Quốc Tế Phụ Nữ", "International Women's Day" },
    { 20,  3, "Ngày Quốc Tế Hạnh Phúc", "International Day of Happiness" },
    { 26,  3, "Ngày Thành Lập Đoàn TNCS Hồ Chí Minh", "Youth Union Day" },
    { 30,  4, "Giải Phóng Miền Nam - Thống Nhất Đất Nước", "Reunification Day" },
    {  1,  5, "Quốc Tế Lao Động", "International Workers' Day" },
    {  1,  6, "Quốc Tế Thiếu Nhi", "International Children's Day" },
    { 28,  6, "Ngày Gia Đình Việt Nam", "Vietnamese Family Day" },
    { 27,  7, "Ngày Thương Binh Liệt Sĩ", "War Invalids & Martyrs Day" },
    { 19,  8, "Ngày Truyền Thống Công An Nhân Dân", "People's Public Security Day" },
    {  2,  9, "Quốc Khánh Nước CHXHCN Việt Nam", "Vietnam National Day" },
    { 10, 10, "Ngày Giải Phóng Thủ Đô Hà Nội", "Capital Liberation Day" },
    { 13, 10, "Ngày Doanh Nhân Việt Nam", "Vietnamese Entrepreneurs' Day" },
    { 20, 10, "Ngày Phụ Nữ Việt Nam", "Vietnamese Women's Day" },
    { 20, 11, "Ngày Nhà Giáo Việt Nam", "Vietnamese Teacher's Day" },
    { 22, 12, "Ngày Thành Lập Quân Đội Nhân Dân VN", "Vietnam People's Army Day" },
    { 24, 12, "Đêm Giáng Sinh (Christmas Eve)", "Christmas Eve" },
    { 25, 12, "Lễ Giáng Sinh (Christmas Day)", "Christmas Day" },
    { 31, 12, "Đêm Giao Thừa Dương Lịch", "New Year's Eve" }
};

// ── Built-in Lunar Holidays ──────────────────────────────────────────
static const FixedHoliday LUNAR_HOLIDAYS[] = {
    {  1,  1, "Mùng 1 Tết Nguyên Đán", "1st Day of Lunar New Year" },
    {  2,  1, "Mùng 2 Tết Nguyên Đán", "2nd Day of Lunar New Year" },
    {  3,  1, "Mùng 3 Tết Nguyên Đán", "3rd Day of Lunar New Year" },
    {  4,  1, "Mùng 4 Tết - Khai Xuân Đại Cát", "4th Day of Lunar New Year" },
    { 15,  1, "Tết Nguyên Tiêu (Rằm Tháng Giêng)", "First Full Moon Festival" },
    {  3,  3, "Tết Hàn Thực (Bánh Trôi Bánh Chay)", "Cold Food Festival" },
    { 10,  3, "Giỗ Tổ Hùng Vương (Mùng 10 Tháng 3)", "Hung Kings Commemoration Day" },
    { 15,  4, "Đại Lễ Phật Đản (Vesak)", "Buddha's Birthday (Vesak)" },
    {  5,  5, "Tết Đoan Ngọ (Giết Sâu Bọ)", "Dragon Boat Festival" },
    { 15,  7, "Đại Lễ Vu Lan Báo Hiếu (Rằm Tháng 7)", "Vu Lan Parents' Day" },
    { 15,  8, "Tết Trung Thu (Rằm Tháng Tám)", "Mid-Autumn Festival" },
    {  9,  9, "Tết Trùng Cửu (Lễ Trùng Cửu)", "Double Ninth Festival" },
    { 15, 10, "Tết Hạ Nguyên (Lễ Cơm Mới)", "Ha Nguyen Festival" },
    { 23, 12, "Tết Ông Công Ông Táo Chầu Trời", "Kitchen Gods' Day" },
    { 30, 12, "Đêm Giao Thừa Tết Nguyên Đán", "Lunar New Year's Eve" }
};

// ── 31 Daily Inspirational Positive Quotes ────────────────────────────
static const char* const QUOTES_VI[31] = {
    "Khởi đầu tháng mới tràn đầy năng lượng và may mắn!",
    "Mỗi ngày mới là một cơ hội để bạn tỏa sáng.",
    "Kiên trì hôm nay, thành công rực rỡ ngày mai.",
    "Tập trung vào mục tiêu và không ngừng tiến bước.",
    "Hãy mỉm cười, mọi điều tốt đẹp đang chờ đón bạn.",
    "Biến áp lực thành động lực để bứt phá giới hạn.",
    "Học hỏi mỗi ngày để hoàn thiện bản thân hơn.",
    "Sáng tạo không giới hạn, tự tin vào chính mình.",
    "Làm việc bằng cả đam mê sẽ tạo nên điều kỳ diệu.",
    "Thành công bắt đầu từ những thói quen nhỏ nhất.",
    "Giữ tinh thần tích cực trước mọi thử thách.",
    "Hành trình vạn dặm khởi đầu từ một bước chân.",
    "Sự tập trung là chìa khóa mở cánh cửa năng suất.",
    "Dành trọn tâm huyết cho những gì bạn làm hôm nay.",
    "Nửa chặng đường rồi, tiếp tục duy trì phong độ nhé!",
    "Mỗi khó khăn là một bài học vô cùng quý giá.",
    "Sống hết mình cho hiện tại, hướng tới tương lai.",
    "Sự kiên nhẫn tạo nên thành quả vững bền theo thời gian.",
    "Năng lượng tích cực sẽ lan tỏa những điều tốt lành.",
    "Hãy tự hào về những nỗ lực thầm lặng của chính bạn.",
    "Thói quen tốt kiến tạo một cuộc đời ý nghĩa.",
    "Đừng ngại đổi mới để tạo ra sự khác biệt vượt trội.",
    "Bình tĩnh, tự tin và giải quyết chu đáo từng việc một.",
    "Sự tử tế là món quà đẹp nhất bạn có thể trao đi.",
    "Làm việc thông minh, nghỉ ngơi hợp lý để tái tạo năng lượng.",
    "Đích đến gần hơn bạn nghĩ, tiếp tục cố gắng nhé!",
    "Hãy luôn giữ ngọn lửa nhiệt huyết trong công việc.",
    "Trân trọng từng khoảnh khắc ý nghĩa bên người thân yêu.",
    "Nhìn lại hành trình để vững tin bước tiếp chặng đường mới.",
    "Cảm ơn vì đã luôn nỗ lực hết mình trong tháng này!",
    "Hoàn thành xuất sắc mục tiêu, sẵn sàng đón nhận điều mới mẻ!"
};

static const char* const QUOTES_EN[31] = {
    "A brand new month full of energy and fresh opportunities!",
    "Every new day brings a chance to shine and grow.",
    "Consistency today builds undeniable success tomorrow.",
    "Stay focused on your goals and take confident steps forward.",
    "Smile, wonderful things are unfolding for you today.",
    "Turn pressure into power and break through your limits.",
    "Learn something new every day to elevate yourself.",
    "Unleash your creativity and believe in your potential.",
    "Passionate work always creates extraordinary results.",
    "Great achievements begin with the smallest daily habits.",
    "Keep a positive mindset in front of every challenge.",
    "A journey of a thousand miles begins with a single step.",
    "Deep focus is the master key to extraordinary productivity.",
    "Put your heart into everything you do today.",
    "Halfway through the month, keep your momentum strong!",
    "Every obstacle is a stepping stone to wisdom.",
    "Live fully in the present and build an inspiring future.",
    "Patience and persistence create long-lasting triumph.",
    "Your positive energy inspires everyone around you.",
    "Be proud of every effort you have quietly put in.",
    "Good daily habits create a truly meaningful life.",
    "Embrace innovation and make your unique difference.",
    "Stay calm, stay focused, and conquer tasks one by one.",
    "Kindness is the greatest gift you can share today.",
    "Work smart, take mindful breaks, and recharge your energy.",
    "You are closer to your goal than you think. Keep going!",
    "Keep the fire of passion burning brightly in your craft.",
    "Cherish every precious moment with those who matter most.",
    "Reflect on how far you've come and march forward proudly.",
    "Thank you for giving your absolute best this month!",
    "Mission accomplished! Ready to welcome exciting new triumphs!"
};

AnniversaryManager::AnniversaryManager() : _count(0), _loaded(false) {
    memset(_items, 0, sizeof(_items));
}

void AnniversaryManager::begin() {
    if (_loaded) return;
    _count = 0;

    Preferences prefs;
    if (prefs.begin("cyd_anniv", false)) {
        String json = prefs.getString("data", "");
        prefs.end();

        if (json.length() > 5) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, json);
            if (!err && doc.is<JsonArray>()) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    if (_count >= MAX_ANNIVERSARIES) break;
                    _items[_count].day = obj["d"] | 1;
                    _items[_count].month = obj["m"] | 1;
                    _items[_count].isLunar = obj["l"] | false;
                    const char* t = obj["t"] | "";
                    strncpy(_items[_count].title, t, sizeof(_items[_count].title) - 1);
                    _items[_count].title[sizeof(_items[_count].title) - 1] = '\0';
                    _count++;
                }
            }
        }
    }

    _loaded = true;
    Serial.printf("[AnniversaryManager] Loaded %d personal anniversaries.\n", _count);
}

void AnniversaryManager::save() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < _count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["d"] = _items[i].day;
        obj["m"] = _items[i].month;
        obj["l"] = _items[i].isLunar;
        obj["t"] = _items[i].title;
    }

    String json;
    serializeJson(doc, json);

    Preferences prefs;
    if (prefs.begin("cyd_anniv", false)) {
        size_t written = prefs.putString("data", json);
        prefs.end();
        Serial.printf("[AnniversaryManager] Saved %d items to NVS (%d bytes, written %d)\n", _count, json.length(), written);
    } else {
        Serial.println("[AnniversaryManager] ERROR: prefs.begin('cyd_anniv', false) failed!");
    }
}

bool AnniversaryManager::addItem(uint8_t day, uint8_t month, bool isLunar, const char* title) {
    if (!_loaded) begin();
    if (_count >= MAX_ANNIVERSARIES || !title || strlen(title) == 0) return false;
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;

    _items[_count].day = day;
    _items[_count].month = month;
    _items[_count].isLunar = isLunar;
    strncpy(_items[_count].title, title, sizeof(_items[_count].title) - 1);
    _items[_count].title[sizeof(_items[_count].title) - 1] = '\0';
    _count++;

    save();
    return true;
}

bool AnniversaryManager::deleteItem(uint8_t index) {
    if (index >= _count) return false;

    for (uint8_t i = index; i < _count - 1; i++) {
        _items[i] = _items[i + 1];
    }
    _count--;
    memset(&_items[_count], 0, sizeof(AnniversaryItem));

    save();
    return true;
}

void AnniversaryManager::clear() {
    _count = 0;
    memset(_items, 0, sizeof(_items));
    save();
}

const AnniversaryItem* AnniversaryManager::getItem(uint8_t index) const {
    if (index < _count) return &_items[index];
    return nullptr;
}

uint8_t AnniversaryManager::getCount() {
    if (!_loaded) begin();
    return _count;
}

String AnniversaryManager::checkPersonalAnniversary(uint8_t sDay, uint8_t sMonth, uint8_t lDay, uint8_t lMonth, bool isEnglish) {
    for (uint8_t i = 0; i < _count; i++) {
        if (!_items[i].isLunar) {
            // Dương lịch
            if (_items[i].day == sDay && _items[i].month == sMonth) {
                if (isEnglish) {
                    return String("[★ ANNIVERSARY] ") + _items[i].title + " • Happy special day!";
                } else {
                    return String("[★ KỶ NIỆM] ") + _items[i].title + " • Chúc mừng ngày đặc biệt của bạn!";
                }
            }
        } else {
            // Âm lịch
            if (_items[i].day == lDay && _items[i].month == lMonth) {
                if (isEnglish) {
                    return String("[★ LUNAR REMINDER] ") + _items[i].title + " • Wishing you joy & blessings!";
                } else {
                    return String("[★ KỶ NIỆM ÂL] ") + _items[i].title + " • Chúc mừng ngày kỷ niệm âm lịch của bạn!";
                }
            }
        }
    }
    return "";
}

String AnniversaryManager::checkHoliday(uint8_t sDay, uint8_t sMonth, uint8_t lDay, uint8_t lMonth, bool isEnglish) {
    // 1. Solar Holidays
    size_t numSolar = sizeof(SOLAR_HOLIDAYS) / sizeof(SOLAR_HOLIDAYS[0]);
    for (size_t i = 0; i < numSolar; i++) {
        if (SOLAR_HOLIDAYS[i].day == sDay && SOLAR_HOLIDAYS[i].month == sMonth) {
            if (isEnglish) {
                return String("[✿ HOLIDAY] ") + SOLAR_HOLIDAYS[i].nameEn;
            } else {
                return String("[✿ NGÀY LỄ] ") + SOLAR_HOLIDAYS[i].nameVi;
            }
        }
    }

    // 2. Lunar Holidays
    size_t numLunar = sizeof(LUNAR_HOLIDAYS) / sizeof(LUNAR_HOLIDAYS[0]);
    for (size_t i = 0; i < numLunar; i++) {
        if (LUNAR_HOLIDAYS[i].day == lDay && LUNAR_HOLIDAYS[i].month == lMonth) {
            if (isEnglish) {
                return String("[✿ FESTIVAL] ") + LUNAR_HOLIDAYS[i].nameEn;
            } else {
                return String("[✿ TẾT TRUYỀN THỐNG] ") + LUNAR_HOLIDAYS[i].nameVi;
            }
        }
    }

    // Đặc biệt: Tháng Chạp nếu chỉ có 29 ngày thì ngày 29/12 chính là Đêm Giao Thừa
    if (lMonth == 12 && lDay == 29) {
        LunarDate tomorrow = LunarCalendar::convertSolar2Lunar(sDay + 1, sMonth, 2026);
        if (tomorrow.day == 1 && tomorrow.month == 1) {
            return isEnglish ? "[✿ FESTIVAL] Lunar New Year's Eve (Year-End Gathering)"
                             : "[✿ TẾT TRUYỀN THỐNG] Đêm Giao Thừa Tết Nguyên Đán";
        }
    }

    return "";
}

String AnniversaryManager::getPerpetualCalendarFallback(uint8_t sDay, uint8_t sMonth, uint16_t sYear, uint8_t lDay, uint8_t lMonth, const char* dayCanChi, bool isEnglish) {
    const char* term = LunarCalendar::getSolarTermName(sDay, sMonth, sYear);
    uint8_t qIdx = (sDay >= 1 && sDay <= 31) ? (sDay - 1) : 0;
    const char* quote = isEnglish ? QUOTES_EN[qIdx] : QUOTES_VI[qIdx];

    String result;
    if (isEnglish) {
        result = "[☯ DESK CALENDAR] Day " + String(dayCanChi ? dayCanChi : "");
        if (term && strlen(term) > 0) {
            result += " • " + String(term);
        }
        result += " • " + String(quote);
    } else {
        result = "[☯ LỊCH VẠN NIÊN] Ngày " + String(dayCanChi ? dayCanChi : "");
        if (term && strlen(term) > 0) {
            result += " • Tiết " + String(term);
        }
        result += " • " + String(quote);
    }
    return result;
}

String AnniversaryManager::getTodayTickerMessage(bool isEnglish) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    uint8_t sDay = t.tm_mday;
    uint8_t sMonth = t.tm_mon + 1;
    uint16_t sYear = 1900 + t.tm_year;

    if (sYear < 2024) {
        return isEnglish ? "CYD SmartDesk V2 • Syncing NTP Time..."
                         : "CYD SmartDesk V2 • Đang đồng bộ thời gian NTP...";
    }

    LunarDate lDate = LunarCalendar::convertSolar2Lunar(sDay, sMonth, sYear);

    // 1. Kiểm tra ngày kỷ niệm cá nhân của người dùng (Ưu tiên cao nhất)
    String personal = checkPersonalAnniversary(sDay, sMonth, lDate.day, lDate.month, isEnglish);
    if (personal.length() > 0) {
        return personal;
    }

    // 2. Kiểm tra ngày lễ quốc gia / tết truyền thống (Ưu tiên 2)
    String holiday = checkHoliday(sDay, sMonth, lDate.day, lDate.month, isEnglish);
    if (holiday.length() > 0) {
        return holiday;
    }

    // 3. Fallback Lịch Vạn Niên: Can Chi ngày + Tiết khí + Danh ngôn tích cực mỗi ngày (Ưu tiên 3)
    return getPerpetualCalendarFallback(sDay, sMonth, sYear, lDate.day, lDate.month, lDate.dayCanChi, isEnglish);
}
