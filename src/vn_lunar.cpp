#include "vn_lunar.h"
#include <math.h>

static const char* const CAN_NAMES[10] = {
    "Giap", "At", "Binh", "Dinh", "Mau", "Ky", "Canh", "Tan", "Nham", "Quy"
};

static const char* const CHI_NAMES[12] = {
    "Ty", "Suu", "Dan", "Mao", "Thin", "Ty", "Ngo", "Mui", "Than", "Dau", "Tuat", "Hoi"
};

const char* LunarCalendar::getCanName(uint8_t index) {
    return CAN_NAMES[index % 10];
}

const char* LunarCalendar::getChiName(uint8_t index) {
    return CHI_NAMES[index % 12];
}

long long LunarCalendar::getJulius(uint32_t dd, uint32_t mm, uint32_t yy) {
    uint32_t a = (14 - mm) / 12;
    uint32_t y = yy + 4800 - a;
    uint32_t m = mm + 12 * a - 3;
    long long jd = dd + (uint32_t)((153 * m + 2) / 5) + 365 * y + (uint32_t)(y / 4) - (uint32_t)(y / 100) + (uint32_t)(y / 400) - 32045;
    if (jd < 2299161) {
        jd = dd + (uint32_t)((153 * m + 2) / 5) + 365 * y + (uint32_t)(y / 4) - 32083;
    }
    return jd;
}

uint32_t LunarCalendar::getNewMoonDay(uint32_t k) {
    double T = k / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double dr = PI / 180.0;
    double timeZone = 7.0;

    double Jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
    Jd1 += 0.00033 * sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);

    double M = 359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
    double Mpr = 306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
    double F = 21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;

    double C1 = (0.1734 - 0.000393 * T) * sin(M * dr) + 0.0021 * sin(2 * dr * M);
    C1 -= 0.4068 * sin(Mpr * dr) + 0.0161 * sin(dr * 2 * Mpr);
    C1 -= 0.0004 * sin(dr * 3 * Mpr);
    C1 += 0.0104 * sin(dr * 2 * F) - 0.0051 * sin(dr * (M + Mpr));
    C1 -= 0.0074 * sin(dr * (M - Mpr)) + 0.0004 * sin(dr * (2 * F + M));
    C1 -= 0.0004 * sin(dr * (2 * F - M)) - 0.0006 * sin(dr * (2 * F + Mpr));
    C1 += 0.0010 * sin(dr * (2 * F - Mpr)) + 0.0005 * sin(dr * (2 * Mpr + M));

    double deltat;
    if (T < -11) {
        deltat = 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3;
    } else {
        deltat = -0.000278 + 0.000265 * T + 0.000262 * T2;
    }

    double JdNew = Jd1 + C1 - deltat;
    return (uint32_t)(JdNew + 0.5 + timeZone / 24.0);
}

uint32_t LunarCalendar::getSunLongitude(uint32_t jdn) {
    double timeZone = 7.0;
    double T = (jdn - 2451545.5 - timeZone / 24.0) / 36525.0;
    double T2 = T * T;
    double dr = PI / 180.0;

    double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
    double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
    double DL = (1.914600 - 0.004817 * T - 0.000014 * T2) * sin(dr * M);
    DL += (0.019993 - 0.000101 * T) * sin(dr * 2 * M) + 0.000290 * sin(dr * 3 * M);

    double L = (L0 + DL) * dr;
    L = L - PI * 2 * (uint32_t)(L / (PI * 2));
    return (uint32_t)(L / PI * 6.0);
}

uint32_t LunarCalendar::getLunarMonth11(uint32_t yy) {
    double off = getJulius(31, 12, yy) - 2415021;
    uint32_t k = (uint32_t)(off / 29.530588853);
    uint32_t nm = getNewMoonDay(k);
    uint32_t sunLong = getSunLongitude(nm);
    if (sunLong >= 9) {
        nm = getNewMoonDay(k - 1);
    }
    return nm;
}

uint32_t LunarCalendar::getLeapMonthOffset(double a11) {
    uint32_t k = (uint32_t)((a11 - 2415021.076998695) / 29.530588853 + 0.5);
    uint32_t last = 0;
    uint32_t i = 1;
    uint32_t arc = getSunLongitude(getNewMoonDay(k + i));
    do {
        last = arc;
        i++;
        arc = getSunLongitude(getNewMoonDay(k + i));
    } while (arc != last && i < 14);
    return i - 1;
}

LunarDate LunarCalendar::convertSolar2Lunar(uint8_t dd, uint8_t mm, uint16_t yy) {
    LunarDate result;
    long long dayNumber = getJulius(dd, mm, yy);
    uint32_t k = (uint32_t)((dayNumber - 2415021.076998695) / 29.530588853);
    uint32_t monthStart = getNewMoonDay(k + 1);
    if (monthStart > (uint32_t)dayNumber) {
        monthStart = getNewMoonDay(k);
    }

    uint32_t a11 = getLunarMonth11(yy);
    uint32_t b11 = a11;
    uint32_t lunarYear = yy;

    if (a11 >= monthStart) {
        lunarYear = yy;
        a11 = getLunarMonth11(yy - 1);
    } else {
        lunarYear = yy + 1;
        b11 = getLunarMonth11(yy + 1);
    }

    uint32_t lunarDay = dayNumber - monthStart + 1;
    uint32_t diff = (monthStart - a11) / 29;
    uint32_t lunarMonth = diff + 11;
    bool isLeap = false;

    if (b11 - a11 > 365) {
        uint32_t leapMonthDiff = getLeapMonthOffset(a11);
        if (diff >= leapMonthDiff) {
            lunarMonth = diff + 10;
            if (diff == leapMonthDiff) {
                isLeap = true;
            }
        }
    }

    if (lunarMonth > 12) {
        lunarMonth -= 12;
    }
    if (lunarMonth >= 11 && diff < 4) {
        lunarYear -= 1;
    }

    result.day = (uint8_t)lunarDay;
    result.month = (uint8_t)lunarMonth;
    result.year = (uint16_t)lunarYear;
    result.isLeap = isLeap;

    // Can Chi for Year
    uint8_t canY = (lunarYear + 6) % 10;
    uint8_t chiY = (lunarYear + 8) % 12;
    snprintf(result.yearCanChi, sizeof(result.yearCanChi), "%s %s", CAN_NAMES[canY], CHI_NAMES[chiY]);

    // Can Chi for Day
    uint8_t canD = (dayNumber + 9) % 10;
    uint8_t chiD = (dayNumber + 1) % 12;
    snprintf(result.dayCanChi, sizeof(result.dayCanChi), "%s %s", CAN_NAMES[canD], CHI_NAMES[chiD]);

    // Can Chi for Month
    uint8_t canM = (canY * 2 + lunarMonth + 1) % 10;
    uint8_t chiM = (lunarMonth + 1) % 12;
    snprintf(result.monthCanChi, sizeof(result.monthCanChi), "%s %s", CAN_NAMES[canM], CHI_NAMES[chiM]);

    return result;
}
