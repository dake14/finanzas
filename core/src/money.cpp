// UNIDAD: core/src/money.cpp
//
// Implementa el contrato declarado en dake/core/money.hpp.
//
// DEPENDENCIAS PERMITIDAS (lista cerrada): "dake/core/money.hpp",
// "dake/core/currency.hpp", <algorithm>, <cctype>, <charconv>, <cstdint>,
// <cstdlib>, <limits>, <stdexcept>, <string>, <string_view>, <vector>.
//
// PROHIBIDO EN ESTE ARCHIVO: <cmath>, float, double, long double. Ninguna
// operacion puede pasar por punto flotante, ni siquiera de forma intermedia.
//
// REDONDEO: half-even (bancario) en percent() y convertTo(). Implementarlo
// sobre enteros: calcular cociente y resto, y si el doble del resto en valor
// absoluto es mayor que el divisor redondear hacia afuera; si es exactamente
// igual, redondear al cociente PAR. Cuidado con el signo: -2.5 -> -2.
//
// DESBORDAMIENTO: usar __int128 o detectar antes de operar. Toda suma, resta o
// multiplicacion que no quepa en int64 lanza std::overflow_error.
//
// CRITERIO DE ACEPTACION:
//   - Money::parse("1.234,56", cop).minor() == 123456
//   - Money::parse("1234.56", cop).minor() == 123456
//   - Money::parse("-1 234,56", cop).minor() == -123456
//   - Money::parse("1234", cop).minor() == 123400
//   - Money::parse("1.2345", cop) lanza std::invalid_argument (sobran decimales)
//   - Money::parse("abc", cop) lanza std::invalid_argument
//   - fromMinor(123456, cop).toString() == "1234.56 COP"
//   - fromMinor(-5, cop).toString() == "-0.05 COP"
//   - fromMinor(100, cop) + fromMinor(100, usd) lanza CurrencyMismatch
//   - fromMinor(100, cop) == fromMinor(100, usd) es false y NO lanza
//   - fromMinor(100000, cop).percent(1900).minor() == 19000  (IVA 19%)
//   - fromMinor(125, cop).percent(5000).minor() == 62   (62.5 -> par -> 62)
//   - fromMinor(375, cop).percent(5000).minor() == 188  (187.5 -> par -> 188)
//   - fromMinor(-125, cop).percent(5000).minor() == -62
//   - fromMinor(10000, usd).convertTo(cop, 4'000'000'000).minor() == 40'000'000
//     (100.00 USD a 4000 COP/USD = 400000.00 COP). El producto intermedio
//     10000 * 4e9 = 4e13 excede lo comodo: calcular en __int128 (MSVC no lo
//     tiene, usar _mul128 o dividir antes de multiplicar) y lanzar
//     std::overflow_error si el resultado final no cabe en int64.
//   - fromMinor(100, cop).allocate(3) da {34, 33, 33} y suma exactamente 100
//   - fromMinor(-100, cop).allocate(3) da {-34, -33, -33}
//   - allocate(0) lanza std::invalid_argument
//   - convertTo(misma moneda, 500000) lanza std::invalid_argument

#include "dake/core/money.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/currency.hpp"

namespace dake::core {

namespace {

std::int64_t safe_add(std::int64_t a, std::int64_t b) {
    if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) throw std::overflow_error("overflow");
    if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) throw std::overflow_error("overflow");
    return a + b;
}

std::int64_t safe_sub(std::int64_t a, std::int64_t b) {
    if (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) throw std::overflow_error("overflow");
    if (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) throw std::overflow_error("overflow");
    return a - b;
}

std::int64_t safe_mul(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) return 0;
    if (a == std::numeric_limits<std::int64_t>::min() && b == -1) throw std::overflow_error("overflow");
    if (b == std::numeric_limits<std::int64_t>::min() && a == -1) throw std::overflow_error("overflow");
    if (a > 0 && b > 0 && a > std::numeric_limits<std::int64_t>::max() / b) throw std::overflow_error("overflow");
    if (a > 0 && b < 0 && b < std::numeric_limits<std::int64_t>::min() / a) throw std::overflow_error("overflow");
    if (a < 0 && b > 0 && a < std::numeric_limits<std::int64_t>::min() / b) throw std::overflow_error("overflow");
    if (a < 0 && b < 0 && a < std::numeric_limits<std::int64_t>::max() / b) throw std::overflow_error("overflow");
    return a * b;
}

std::int64_t mul_div_halfeven(std::int64_t a, std::int64_t b, std::int64_t c) {
    bool neg = (a < 0) ^ (b < 0);
    std::uint64_t ua = a == std::numeric_limits<std::int64_t>::min() ? 
                       (static_cast<std::uint64_t>(1) << 63) : static_cast<std::uint64_t>(std::abs(a));
    std::uint64_t ub = b == std::numeric_limits<std::int64_t>::min() ? 
                       (static_cast<std::uint64_t>(1) << 63) : static_cast<std::uint64_t>(std::abs(b));
    std::uint64_t uc = c;

    std::uint64_t a_lo = ua & 0xFFFFFFFF;
    std::uint64_t a_hi = ua >> 32;
    std::uint64_t b_lo = ub & 0xFFFFFFFF;
    std::uint64_t b_hi = ub >> 32;

    std::uint64_t p0 = a_lo * b_lo;
    std::uint64_t p1 = a_lo * b_hi;
    std::uint64_t p2 = a_hi * b_lo;
    std::uint64_t p3 = a_hi * b_hi;

    std::uint64_t mid = p1 + (p0 >> 32);
    std::uint64_t carry_mid = mid < p1 ? 1 : 0;
    mid += p2;
    carry_mid += mid < p2 ? 1 : 0;

    std::uint64_t p_lo = (p0 & 0xFFFFFFFF) | (mid << 32);
    std::uint64_t p_hi = p3 + (carry_mid << 32) + (mid >> 32);

    std::uint64_t rem = p_hi;
    std::uint64_t quo = 0;
    
    if (rem >= uc) {
        throw std::overflow_error("overflow");
    }
    
    for (int i = 0; i < 64; ++i) {
        std::uint64_t next_bit = (p_lo >> 63);
        rem = (rem << 1) | next_bit;
        p_lo <<= 1;
        
        quo <<= 1;
        if (rem >= uc) {
            rem -= uc;
            quo |= 1;
        }
    }
    
    bool round_up = false;
    if (rem > uc - rem) {
        round_up = true;
    } else if (rem == uc - rem) {
        if (quo % 2 != 0) {
            round_up = true;
        }
    }
    
    if (round_up) {
        quo++;
    }
    
    if (quo > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + (neg ? 1 : 0)) {
        throw std::overflow_error("overflow");
    }
    
    std::int64_t result;
    if (neg) {
        if (quo == 9223372036854775808ULL) {
            result = std::numeric_limits<std::int64_t>::min();
        } else {
            result = -static_cast<std::int64_t>(quo);
        }
    } else {
        result = static_cast<std::int64_t>(quo);
    }
    return result;
}

} // namespace

Money::Money() noexcept = default;

Money Money::fromMinor(std::int64_t minor, Currency currency) noexcept {
    Money m;
    m.minor_ = minor;
    m.currency_ = currency;
    return m;
}

Money Money::fromMajor(std::int64_t major, std::int64_t minorPart, Currency currency) {
    if ((major > 0 && minorPart < 0) || (major < 0 && minorPart > 0)) {
        throw std::invalid_argument("major y minorPart deben tener el mismo signo o minorPart ser 0");
    }
    std::int64_t scale = currency.scale();
    if (minorPart <= -scale || minorPart >= scale) {
        throw std::invalid_argument("minorPart fuera de limites");
    }
    std::int64_t final_minor = safe_add(safe_mul(major, scale), minorPart);
    return fromMinor(final_minor, currency);
}

Money Money::zero(Currency currency) noexcept { 
    return fromMinor(0, currency); 
}

Money Money::parse(std::string_view text, Currency currency) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    if (text.empty()) throw std::invalid_argument("empty input");

    bool negative = false;
    if (text.front() == '-') {
        negative = true;
        text.remove_prefix(1);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    }
    if (text.empty()) throw std::invalid_argument("only sign");

    bool has_dec_sep = false;
    size_t dec_sep_idx = std::string_view::npos;
    
    size_t last_sep_idx = text.find_last_not_of("0123456789");
    if (last_sep_idx != std::string_view::npos) {
        char last_sep = text[last_sep_idx];
        if (last_sep == '.' || last_sep == ',') {
            size_t digits_after = text.size() - 1 - last_sep_idx;
            if (digits_after != 3) {
                has_dec_sep = true;
                dec_sep_idx = last_sep_idx;
            } else {
                size_t prev_sep_idx = text.find_last_not_of("0123456789", last_sep_idx - 1);
                if (prev_sep_idx != std::string_view::npos) {
                    char prev_sep = text[prev_sep_idx];
                    if (prev_sep == last_sep) {
                        has_dec_sep = false;
                    } else {
                        has_dec_sep = true;
                        dec_sep_idx = last_sep_idx;
                    }
                } else {
                    has_dec_sep = false;
                }
            }
        } else if (last_sep == ' ') {
            has_dec_sep = false;
        } else {
            throw std::invalid_argument("invalid separator character");
        }
    }

    std::string_view int_part_text = has_dec_sep ? text.substr(0, dec_sep_idx) : text;
    std::string_view dec_part_text = has_dec_sep ? text.substr(dec_sep_idx + 1) : "";

    std::string clean_int;
    char thou_sep = '\0';
    for (char c : int_part_text) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            clean_int += c;
        } else if (c == ' ' || c == '.' || c == ',') {
            if (thou_sep == '\0') thou_sep = c;
            else if (thou_sep != c) throw std::invalid_argument("inconsistent thousands separator");
        } else {
            throw std::invalid_argument("invalid char in integer part");
        }
    }
    
    std::int64_t major = 0;
    if (!clean_int.empty()) {
        auto [ptr, ec] = std::from_chars(clean_int.data(), clean_int.data() + clean_int.size(), major);
        if (ec != std::errc() || ptr != clean_int.data() + clean_int.size()) throw std::out_of_range("major overflow");
    }

    std::int64_t minor_part = 0;
    if (has_dec_sep) {
        for (char c : dec_part_text) {
            if (!std::isdigit(static_cast<unsigned char>(c))) throw std::invalid_argument("invalid decimal part");
        }
        if (static_cast<int>(dec_part_text.size()) > currency.exponent()) {
            throw std::invalid_argument("too many decimals");
        }
        
        if (!dec_part_text.empty()) {
            std::string padded_dec = std::string(dec_part_text);
            while (static_cast<int>(padded_dec.size()) < currency.exponent()) {
                padded_dec += '0';
            }
            auto [ptr, ec] = std::from_chars(padded_dec.data(), padded_dec.data() + padded_dec.size(), minor_part);
            if (ec != std::errc()) throw std::out_of_range("minor_part overflow");
        }
    }

    if (negative) {
        major = -major;
        minor_part = -minor_part;
    }
    
    std::int64_t final_minor = safe_add(safe_mul(major, currency.scale()), minor_part);
    return Money::fromMinor(final_minor, currency);
}

std::int64_t Money::minor() const noexcept { return minor_; }

Currency Money::currency() const noexcept { return currency_; }

bool Money::isZero() const noexcept { return minor_ == 0; }

bool Money::isNegative() const noexcept { return minor_ < 0; }

std::string Money::toString() const {
    bool neg = minor_ < 0;
    std::uint64_t u_minor = neg ? (0ULL - static_cast<std::uint64_t>(minor_)) : static_cast<std::uint64_t>(minor_);
    
    std::uint64_t scale = currency_.scale();
    std::uint64_t major = u_minor / scale;
    std::uint64_t frac = u_minor % scale;
    
    std::string s;
    if (neg && u_minor != 0) s += '-';
    s += std::to_string(major);
    if (currency_.exponent() > 0) {
        s += '.';
        std::string f = std::to_string(frac);
        while (static_cast<int>(f.size()) < currency_.exponent()) {
            f.insert(f.begin(), '0');
        }
        s += f;
    }
    s += ' ';
    s += currency_.code();
    return s;
}

Money Money::operator+(const Money& other) const {
    if (currency_ != other.currency_) throw CurrencyMismatch(currency_.code(), other.currency_.code());
    return fromMinor(safe_add(minor_, other.minor_), currency_);
}

Money Money::operator-(const Money& other) const {
    if (currency_ != other.currency_) throw CurrencyMismatch(currency_.code(), other.currency_.code());
    return fromMinor(safe_sub(minor_, other.minor_), currency_);
}

Money& Money::operator+=(const Money& other) {
    *this = *this + other;
    return *this;
}

Money& Money::operator-=(const Money& other) {
    *this = *this - other;
    return *this;
}

Money Money::operator-() const {
    if (minor_ == std::numeric_limits<std::int64_t>::min()) throw std::overflow_error("overflow");
    return fromMinor(-minor_, currency_);
}

Money Money::operator*(std::int64_t factor) const {
    return fromMinor(safe_mul(minor_, factor), currency_);
}

bool Money::operator==(const Money& other) const noexcept {
    return minor_ == other.minor_ && currency_ == other.currency_;
}

std::strong_ordering Money::operator<=>(const Money& other) const {
    if (currency_ != other.currency_) throw CurrencyMismatch(currency_.code(), other.currency_.code());
    return minor_ <=> other.minor_;
}

Money Money::percent(std::int64_t basisPoints) const {
    return fromMinor(mul_div_halfeven(minor_, basisPoints, 10000), currency_);
}

Money Money::convertTo(Currency target, std::int64_t rateMicros) const {
    if (target == currency_) {
        if (rateMicros != 1000000) throw std::invalid_argument("rate must be 1000000 for same currency");
        return *this;
    }
    
    int exp_diff = target.exponent() - currency_.exponent() - 6;
    std::int64_t p10 = 1;
    for (int i = 0; i < std::abs(exp_diff); ++i) p10 *= 10;
    
    if (exp_diff == 0) {
        return fromMinor(mul_div_halfeven(minor_, rateMicros, 1), target);
    } else if (exp_diff < 0) {
        return fromMinor(mul_div_halfeven(minor_, rateMicros, p10), target);
    } else {
        return fromMinor(mul_div_halfeven(minor_, safe_mul(rateMicros, p10), 1), target);
    }
}

std::vector<Money> Money::allocate(int parts) const {
    if (parts <= 0) throw std::invalid_argument("parts must be > 0");
    std::vector<Money> result;
    result.reserve(parts);
    
    std::int64_t base = minor_ / parts;
    std::int64_t rem = minor_ % parts;
    
    std::int64_t abs_rem = std::abs(rem);
    std::int64_t sign = minor_ < 0 ? -1 : 1;
    
    for (int i = 0; i < parts; ++i) {
        std::int64_t portion = base;
        if (i < abs_rem) {
            portion += sign;
        }
        result.push_back(fromMinor(portion, currency_));
    }
    
    return result;
}

} // namespace dake::core

