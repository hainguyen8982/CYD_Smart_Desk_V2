#pragma once
#include <Arduino.h>

struct LunarDate {
    uint8_t day;          // 1..30
    uint8_t month;        // 1..12
    uint16_t year;        // e.g. 2026
    bool isLeap;          // true if leap month
    char yearCanChi[16];  // e.g. "Binh Ngo"
    char dayCanChi[16];   // e.g. "Canh Than"
    char monthCanChi[16]; // e.g. "Mau Tuat"
};

class LunarCalendar {
public:
    static LunarDate convertSolar2Lunar(uint8_t dd, uint8_t mm, uint16_t yy);
    static long long getJulius(uint32_t dd, uint32_t mm, uint32_t yy);
    static const char* getCanName(uint8_t index);
    static const char* getChiName(uint8_t index);
    static const char* getSolarTermName(uint8_t dd, uint8_t mm, uint16_t yy);

private:
    static uint32_t getNewMoonDay(uint32_t k);
    static uint32_t getSunLongitude(uint32_t jdn);
    static uint32_t getLunarMonth11(uint32_t yy);
    static uint32_t getLeapMonthOffset(double a11);
};
