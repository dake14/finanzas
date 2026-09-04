#pragma once
//
// dake/core/money.hpp
//
// CONTRATO de la unidad `core/src/money.cpp`.
//
// Cantidad monetaria exacta. El monto se guarda SIEMPRE como entero de 64 bits
// en unidades minimas (centavos). En este archivo, y en todo el proyecto, esta
// terminantemente prohibido usar float o double para representar dinero: un
// solo redondeo binario mal hecho descuadra los reportes de forma permanente.
//
// Toda operacion entre dos Money de distinta moneda lanza CurrencyMismatch.
// Todo redondeo es half-even (bancario): 0.5 se va al par mas cercano. Esto
// evita el sesgo acumulado al alza del redondeo half-up sobre miles de filas.
//
// Dependencias permitidas: <compare> <cstdint> <string> <string_view> <vector>
//                          "dake/core/currency.hpp"
//
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/currency.hpp"

namespace dake::core {

class Money {
public:
    /// Cero en COP.
    Money() noexcept;

    /// Constructor primario: `minor` ya viene en unidades minimas.
    static Money fromMinor(std::int64_t minor, Currency currency) noexcept;

    /// Construye desde unidades mayores mas fraccion. `major` y `minorPart`
    /// deben tener el mismo signo o `minorPart` ser 0; si no, lanza
    /// std::invalid_argument. `minorPart` fuera de [-(scale-1), scale-1]
    /// tambien lanza std::invalid_argument.
    static Money fromMajor(std::int64_t major, std::int64_t minorPart, Currency currency);

    static Money zero(Currency currency) noexcept;

    /// Parsea texto humano. Acepta separador decimal '.' o ',' (uno solo),
    /// separadores de miles ' ', '.' o ',' consistentes, signo '-' inicial y
    /// espacios al borde. Ejemplos validos con COP: "1.234,56", "1234.56",
    /// "-1 234,56", "1234". Si sobran decimales respecto al exponente de la
    /// moneda, lanza std::invalid_argument (nunca trunca en silencio).
    /// Cualquier entrada no parseable lanza std::invalid_argument.
    /// Un desbordamiento de int64 lanza std::out_of_range.
    static Money parse(std::string_view text, Currency currency);

    [[nodiscard]] std::int64_t minor() const noexcept;
    [[nodiscard]] Currency currency() const noexcept;
    [[nodiscard]] bool isZero() const noexcept;
    [[nodiscard]] bool isNegative() const noexcept;

    /// Formato canonico sin separador de miles y con punto decimal, seguido
    /// del codigo: "-1234.56 COP". Es el formato de serializacion, no el de
    /// presentacion al usuario (eso vive en la capa de UI).
    [[nodiscard]] std::string toString() const;

    /// Aritmetica. Lanza CurrencyMismatch si las monedas difieren y
    /// std::overflow_error si el resultado no cabe en int64.
    Money operator+(const Money& other) const;
    Money operator-(const Money& other) const;
    Money& operator+=(const Money& other);
    Money& operator-=(const Money& other);
    Money operator-() const;

    /// Multiplicacion por un entero puro (p. ej. cantidad de horas).
    Money operator*(std::int64_t factor) const;

    /// Comparacion. `operator==` devuelve false si las monedas difieren (no
    /// lanza). `operator<=>` SI lanza CurrencyMismatch si difieren, porque un
    /// orden entre monedas distintas no tiene significado.
    [[nodiscard]] bool operator==(const Money& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(const Money& other) const;

    /// Porcentaje expresado en puntos basicos (1 bps = 0.01%). El 19% de IVA
    /// es 1900 bps. Redondeo half-even.
    [[nodiscard]] Money percent(std::int64_t basisPoints) const;

    /// Convierte a otra moneda. `rateMicros` es la tasa multiplicada por
    /// 1'000'000 (nunca un double). Convertir a la misma moneda con una tasa
    /// distinta de 1'000'000 lanza std::invalid_argument. Redondeo half-even
    /// contra el exponente de la moneda destino.
    [[nodiscard]] Money convertTo(Currency target, std::int64_t rateMicros) const;

    /// Reparte el monto en `parts` porciones cuya suma es EXACTAMENTE igual al
    /// original: las unidades minimas sobrantes se distribuyen de a una desde
    /// la primera porcion. Dividir 100 en 3 da {34, 33, 33}, nunca {33,33,33}.
    /// `parts` <= 0 lanza std::invalid_argument.
    [[nodiscard]] std::vector<Money> allocate(int parts) const;

private:
    std::int64_t minor_ = 0;
    Currency currency_{};
};

} // namespace dake::core
