#pragma once
//
// dake/core/date.hpp
//
// CONTRATO de la unidad `core/src/date.cpp`.
//
// Fecha civil sin hora ni zona horaria. Las transacciones financieras se
// fechan por dia calendario; meter husos horarios aca provoca que una venta
// del 31 de marzo aparezca en abril segun donde se abra la app.
//
// La conversion a/desde dias epoch usa el algoritmo civil_from_days de Howard
// Hinnant (proleptico gregoriano, epoch 1970-01-01 = dia 0).
//
// Dependencias permitidas: <compare> <cstdint> <string> <string_view>
//
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace dake::core {

struct Date {
    int year = 1970;
    unsigned month = 1; // 1-12
    unsigned day = 1;   // 1-31, validado contra el mes y el año bisiesto

    /// Parsea ISO-8601 estricto "YYYY-MM-DD". Cualquier otra forma (incluido
    /// "2026-8-8" o texto sobrante) lanza std::invalid_argument. Una fecha
    /// sintacticamente correcta pero inexistente ("2026-02-30") tambien lanza
    /// std::invalid_argument.
    static Date fromIso(std::string_view text);

    /// Lanza std::invalid_argument si la combinacion no existe.
    static Date fromYmd(int year, unsigned month, unsigned day);

    static Date fromEpochDays(std::int64_t days);

    /// Siempre "YYYY-MM-DD" con relleno de ceros.
    [[nodiscard]] std::string toIso() const;

    [[nodiscard]] std::int64_t toEpochDays() const;

    /// Primer y ultimo dia del mes de esta fecha. Utiles para los reportes
    /// mensuales de P&L.
    [[nodiscard]] Date firstDayOfMonth() const;
    [[nodiscard]] Date lastDayOfMonth() const;

    /// Suma de meses con saturacion de dia: 2026-01-31 + 1 mes = 2026-02-28.
    [[nodiscard]] Date addMonths(int months) const;
    [[nodiscard]] Date addDays(std::int64_t days) const;

    [[nodiscard]] bool isValid() const noexcept;

    /// Orden lexicografico por (year, month, day), que coincide con el
    /// cronologico.
    [[nodiscard]] auto operator<=>(const Date& other) const noexcept = default;
    [[nodiscard]] bool operator==(const Date& other) const noexcept = default;
};

[[nodiscard]] bool isLeapYear(int year) noexcept;

/// Dias del mes indicado. `month` fuera de 1-12 lanza std::invalid_argument.
[[nodiscard]] unsigned daysInMonth(int year, unsigned month);

} // namespace dake::core
