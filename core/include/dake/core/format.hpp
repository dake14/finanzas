#pragma once
//
// dake/core/format.hpp — como se escribe una cifra para que la lea una persona.
//
// Vive en `core` y no en la interfaz porque ahora hay DOS interfaces —la de
// escritorio y la del telefono— y una cifra que se escribe distinto en cada una
// hace dudar de si son el mismo dato. El formato es parte del dominio.
//
// Sigue sin haber punto flotante en ningun lado: se separa la parte entera de
// la fraccion con division entera sobre el int64.
//
// Dependencias permitidas: <cstdint> <string> "money.hpp"
//
#include <cstdint>
#include <string>

#include "dake/core/money.hpp"

namespace dake::core {

/// Punto para los miles y coma para los decimales: 1234567 -> "12.345,67".
/// El signo menos va adelante. No incluye el codigo de moneda.
[[nodiscard]] std::string formatAmount(const Money& amount);

/// Version corta para espacios apretados: "12,3 k", "1,2 M". Por debajo de mil
/// devuelve el numero entero de unidades mayores.
[[nodiscard]] std::string formatCompact(const Money& amount);

/// Puntos basicos como porcentaje legible: 5848 -> "58,5%".
[[nodiscard]] std::string formatBps(int basisPoints);

} // namespace dake::core
