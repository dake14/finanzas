// UNIDAD: core/src/currency.cpp
//
// Implementa el contrato declarado en dake/core/currency.hpp.
//
// DEPENDENCIAS PERMITIDAS (lista cerrada): "dake/core/currency.hpp",
// <algorithm>, <array>, <cctype>, <stdexcept>, <string>, <string_view>.
// Cualquier otro include es un incumplimiento del contrato.
//
// TABLA DE EXPONENTES (cerrada, no ampliar sin decision de arquitectura):
//   0 decimales: JPY KRW CLP VND ISK PYG XOF XAF KMF RWF UGX VUV GNF DJF
//   3 decimales: BHD IQD JOD KWD LYD OMR TND
//   cualquier otra moneda valida: 2 decimales
//
// CRITERIO DE ACEPTACION:
//   - Currency() == Currency("COP") y su exponent() es 2.
//   - Currency("usd").code() == "USD" (normaliza a mayuscula).
//   - Currency("JPY").exponent() == 0 y scale() == 1.
//   - Currency("KWD").scale() == 1000.
//   - Currency("US"), Currency("US1"), Currency("") y Currency("USDD")
//     lanzan std::invalid_argument.
//   - code() devuelve una vista de 3 caracteres, no 4 (sin el NUL).

#include "dake/core/currency.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dake::core {

CurrencyMismatch::CurrencyMismatch(std::string_view lhs, std::string_view rhs)
    : std::runtime_error("moneda incompatible: " + std::string(lhs) + " vs " + std::string(rhs)) {}

Currency::Currency() noexcept
    : code_{'C', 'O', 'P', '\0'}, exponent_{2} {}

Currency::Currency(std::string_view code) {
    if (code.size() != 3) {
        throw std::invalid_argument("codigo de moneda invalido");
    }

    for (std::size_t i = 0; i < 3; ++i) {
        unsigned char uc = static_cast<unsigned char>(code[i]);
        if (std::isalpha(uc) == 0) {
            throw std::invalid_argument("codigo de moneda invalido");
        }
        code_[i] = static_cast<char>(std::toupper(uc));
    }
    code_[3] = '\0';

    std::string_view norm_code(code_.data(), 3);

    static constexpr std::array<std::string_view, 14> zero_decimals = {
        "JPY", "KRW", "CLP", "VND", "ISK", "PYG", "XOF", "XAF", "KMF", "RWF", "UGX", "VUV", "GNF", "DJF"
    };

    static constexpr std::array<std::string_view, 7> three_decimals = {
        "BHD", "IQD", "JOD", "KWD", "LYD", "OMR", "TND"
    };

    if (std::find(zero_decimals.begin(), zero_decimals.end(), norm_code) != zero_decimals.end()) {
        exponent_ = 0;
    } else if (std::find(three_decimals.begin(), three_decimals.end(), norm_code) != three_decimals.end()) {
        exponent_ = 3;
    } else {
        exponent_ = 2;
    }
}

Currency Currency::cop() noexcept { return Currency{}; }

Currency Currency::usd() noexcept {
    return Currency("USD");
}

Currency Currency::eur() noexcept {
    return Currency("EUR");
}

std::string_view Currency::code() const noexcept {
    return std::string_view(code_.data(), 3);
}

int Currency::exponent() const noexcept { return exponent_; }

std::int64_t Currency::scale() const noexcept {
    std::int64_t result = 1;
    for (int i = 0; i < exponent_; ++i) {
        result *= 10;
    }
    return result;
}

bool Currency::operator==(const Currency& other) const noexcept {
    return code_ == other.code_ && exponent_ == other.exponent_;
}

} // namespace dake::core
