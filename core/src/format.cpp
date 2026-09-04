#include "dake/core/format.hpp"

#include <cstdlib>
#include <string>

namespace dake::core {
namespace {

/// Agrupa de a tres con punto, sobre el entero. Formatear dinero pasando por
/// double seria el mismo error que guardarlo en double.
[[nodiscard]] std::string groupThousands(std::int64_t value) {
    std::string digits = std::to_string(value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), ".");
    }
    return digits;
}

/// Valor absoluto sin desbordar en el minimo de int64: negar INT64_MIN es
/// comportamiento indefinido, y un saldo no tiene por que ser la excepcion que
/// tire la aplicacion abajo.
[[nodiscard]] std::uint64_t magnitude(std::int64_t value) {
    return value < 0 ? (~static_cast<std::uint64_t>(value) + 1u)
                     : static_cast<std::uint64_t>(value);
}

} // namespace

std::string formatAmount(const Money& amount) {
    const std::int64_t scale = amount.currency().scale();
    const std::uint64_t absolute = magnitude(amount.minor());

    const auto major = static_cast<std::int64_t>(absolute / static_cast<std::uint64_t>(scale));
    const auto fraction = static_cast<std::int64_t>(absolute % static_cast<std::uint64_t>(scale));

    std::string out;
    if (amount.minor() < 0) {
        out += "-";
    }
    out += groupThousands(major);

    if (amount.currency().exponent() > 0) {
        std::string decimals = std::to_string(fraction);
        while (decimals.size() < static_cast<std::size_t>(amount.currency().exponent())) {
            decimals.insert(decimals.begin(), '0');
        }
        out += "," + decimals;
    }
    return out;
}

std::string formatCompact(const Money& amount) {
    const std::int64_t scale = amount.currency().scale();
    const std::uint64_t absolute = magnitude(amount.minor());
    const auto major = static_cast<std::int64_t>(absolute / static_cast<std::uint64_t>(scale));

    const std::string sign = amount.minor() < 0 ? "-" : "";

    // Una cifra decimal, calculada con enteros: 1234 -> "1,2 k". Pasar por
    // double para dividir por mil seria abrir la puerta que todo el resto del
    // sistema mantiene cerrada.
    auto abbreviate = [&sign](std::int64_t value, std::int64_t unit, const char* suffix) {
        const std::int64_t whole = value / unit;
        const std::int64_t tenth = (value % unit) * 10 / unit;
        std::string out = sign + std::to_string(whole);
        if (whole < 10) {
            out += "," + std::to_string(tenth);
        }
        return out + " " + suffix;
    };

    if (major >= 1'000'000) {
        return abbreviate(major, 1'000'000, "M");
    }
    if (major >= 1'000) {
        return abbreviate(major, 1'000, "k");
    }
    return sign + std::to_string(major);
}

std::string formatBps(int basisPoints) {
    const bool negative = basisPoints < 0;
    const int absolute = negative ? -basisPoints : basisPoints;

    const int whole = absolute / 100;
    const int tenth = (absolute % 100) / 10;

    return (negative ? "-" : "") + std::to_string(whole) + "," + std::to_string(tenth) + "%";
}

} // namespace dake::core
