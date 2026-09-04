// UNIDAD: core/src/date.cpp
//
// Implementa el contrato declarado en dake/core/date.hpp.
//
// DEPENDENCIAS PERMITIDAS (lista cerrada): "dake/core/date.hpp", <array>,
// <charconv>, <cstdint>, <stdexcept>, <string>, <string_view>.
// PROHIBIDO: <chrono>, <ctime>, <iomanip>, <sstream>.
//
// ALGORITMO: days_from_civil / civil_from_days de Howard Hinnant, calendario
// gregoriano proleptico, epoch 1970-01-01 = dia 0. Implementarlo con
// aritmetica entera; no aproximar con 365.25.
//
// CRITERIO DE ACEPTACION:
//   - Date::fromIso("1970-01-01").toEpochDays() == 0
//   - Date::fromIso("2026-08-08").toEpochDays() == 20673
//   - Date::fromEpochDays(0).toIso() == "1970-01-01"
//   - Date::fromEpochDays(-1).toIso() == "1969-12-31"
//   - round-trip: fromEpochDays(n).toEpochDays() == n para n en [-25000, 25000]
//   - Date::fromIso("2026-8-8") lanza std::invalid_argument
//   - Date::fromIso("2026-02-30") lanza std::invalid_argument
//   - Date::fromIso("2026-08-08T00:00:00") lanza std::invalid_argument
//   - isLeapYear(2000) == true, isLeapYear(1900) == false, isLeapYear(2024) == true
//   - daysInMonth(2024, 2) == 29, daysInMonth(2026, 2) == 28
//   - daysInMonth(2026, 13) lanza std::invalid_argument
//   - fromIso("2026-01-31").addMonths(1).toIso() == "2026-02-28"  (satura)
//   - fromIso("2026-01-31").addMonths(-1).toIso() == "2025-12-31"
//   - fromIso("2026-03-15").lastDayOfMonth().toIso() == "2026-03-31"
//   - toIso() rellena con ceros: año 7 -> "0007-01-01"

#include "dake/core/date.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dake::core {

bool isLeapYear(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

unsigned daysInMonth(int year, unsigned month) {
    if (month < 1 || month > 12) {
        throw std::invalid_argument("invalid month");
    }
    if (month == 2) {
        return isLeapYear(year) ? 29 : 28;
    }
    static constexpr std::array<unsigned, 12> days = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    return days[month - 1];
}

Date Date::fromIso(std::string_view text) {
    if (text.size() != 10) {
        throw std::invalid_argument("invalid iso length");
    }
    if (text[4] != '-' || text[7] != '-') {
        throw std::invalid_argument("invalid iso hyphens");
    }
    
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;

    auto parse = [](std::string_view sv, auto& val) {
        auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        return static_cast<int>(ec) == 0 && p == sv.data() + sv.size();
    };

    if (!parse(text.substr(0, 4), year)) throw std::invalid_argument("invalid year");
    if (!parse(text.substr(5, 2), month)) throw std::invalid_argument("invalid month");
    if (!parse(text.substr(8, 2), day)) throw std::invalid_argument("invalid day");

    return fromYmd(year, month, day);
}

Date Date::fromYmd(int year, unsigned month, unsigned day) {
    Date d{year, month, day};
    if (!d.isValid()) {
        throw std::invalid_argument("invalid date");
    }
    return d;
}

Date Date::fromEpochDays(std::int64_t days) {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    return Date::fromYmd(static_cast<int>(y + (m <= 2 ? 1 : 0)), m, d);
}

std::string Date::toIso() const {
    char buf[32];
    auto write_padded = [](char* dest, unsigned val, int width) {
        char temp[32];
        auto [p, ec] = std::to_chars(temp, temp + 32, val);
        int len = static_cast<int>(p - temp);
        int pad = width > len ? width - len : 0;
        for(int i = 0; i < pad; ++i) *dest++ = '0';
        for(int i = 0; i < len; ++i) *dest++ = temp[i];
        return dest;
    };
    
    char* p = buf;
    if (year < 0) {
        *p++ = '-';
        unsigned u_year = static_cast<unsigned>(-(year + 1)) + 1;
        p = write_padded(p, u_year, 4);
    } else {
        p = write_padded(p, static_cast<unsigned>(year), 4);
    }
    *p++ = '-';
    p = write_padded(p, month, 2);
    *p++ = '-';
    p = write_padded(p, day, 2);
    
    return std::string(buf, p - buf);
}

std::int64_t Date::toEpochDays() const {
    std::int64_t y = year;
    y -= month <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned m_adj = month > 2 ? month - 3 : month + 9;
    const unsigned doy = (153 * m_adj + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

Date Date::firstDayOfMonth() const {
    return Date::fromYmd(year, month, 1);
}

Date Date::lastDayOfMonth() const {
    return Date::fromYmd(year, month, daysInMonth(year, month));
}

Date Date::addMonths(int months) const {
    std::int64_t total_months = static_cast<std::int64_t>(year) * 12 + static_cast<std::int64_t>(month - 1) + months;
    std::int64_t new_year_64 = total_months >= 0 ? total_months / 12 : (total_months - 11) / 12;
    int new_year = static_cast<int>(new_year_64);
    unsigned new_month = static_cast<unsigned>(total_months - new_year_64 * 12) + 1;
    
    unsigned new_day = day;
    unsigned max_days = daysInMonth(new_year, new_month);
    if (new_day > max_days) {
        new_day = max_days;
    }
    
    return Date::fromYmd(new_year, new_month, new_day);
}

Date Date::addDays(std::int64_t days) const {
    return Date::fromEpochDays(toEpochDays() + days);
}

bool Date::isValid() const noexcept {
    if (month < 1 || month > 12) return false;
    unsigned max_d = 31;
    if (month == 2) {
        max_d = isLeapYear(year) ? 29 : 28;
    } else {
        static constexpr std::array<unsigned, 12> max_days = {
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
        };
        max_d = max_days[month - 1];
    }
    return day >= 1 && day <= max_d;
}

} // namespace dake::core
