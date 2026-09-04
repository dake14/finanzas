#pragma once
//
// dake/core/currency.hpp
//
// CONTRATO de la unidad `core/src/currency.cpp`.
//
// Una moneda ISO-4217 con su numero de decimales. Es un value type barato de
// copiar. El exponente determina cuantas unidades minimas caben en una unidad
// mayor (COP/USD -> 2 decimales; JPY -> 0; KWD -> 3).
//
// Dependencias permitidas: <array> <cstdint> <stdexcept> <string> <string_view>
//
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dake::core {

/// Lanzada al operar aritmeticamente con dos monedas distintas.
class CurrencyMismatch : public std::runtime_error {
public:
    CurrencyMismatch(std::string_view lhs, std::string_view rhs);
};

class Currency {
public:
    /// Construye COP. Es la moneda base de DakeLabs.
    Currency() noexcept;

    /// `code` debe ser exactamente 3 letras A-Z (se acepta minuscula y se
    /// normaliza a mayuscula). En cualquier otro caso lanza
    /// std::invalid_argument.
    explicit Currency(std::string_view code);

    static Currency cop() noexcept;
    static Currency usd() noexcept;
    static Currency eur() noexcept;

    /// Vista al codigo normalizado ("COP"). Valida mientras viva el objeto.
    [[nodiscard]] std::string_view code() const noexcept;

    /// Numero de decimales de la moneda.
    [[nodiscard]] int exponent() const noexcept;

    /// 10^exponent. Unidades minimas por unidad mayor (100 para COP y USD).
    [[nodiscard]] std::int64_t scale() const noexcept;

    [[nodiscard]] bool operator==(const Currency& other) const noexcept;

private:
    std::array<char, 4> code_{}; // terminado en NUL
    int exponent_ = 2;
};

} // namespace dake::core
