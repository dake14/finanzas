#pragma once
//
// dake/core/hlc.hpp
//
// CONTRATO de la unidad `core/src/hlc.cpp`.
//
// Reloj logico hibrido (Hybrid Logical Clock). Es la pieza que hace posible
// sincronizar dos equipos sin un servidor que arbitre el orden.
//
// Un timestamp de pared solo no sirve: si el portatil tiene el reloj 3 minutos
// adelantado, sus ediciones ganarian siempre. El HLC combina tiempo fisico,
// un contador que rompe empates y el id de dispositivo como desempate final,
// dando un orden TOTAL y estable entre dispositivos.
//
// El tiempo fisico se recibe como parametro y no se lee del sistema aca. Sin
// esa inyeccion, los tests de conflictos no serian deterministas.
//
// Dependencias permitidas: <compare> <cstdint> <string> <string_view>
//
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace dake::core {

struct Hlc {
    std::int64_t wallMillis = 0;
    std::uint32_t counter = 0;
    std::string deviceId; // UUID del dispositivo

    /// Codificacion ordenable como texto plano, apta para ORDER BY en SQLite:
    /// "%013lld-%05u-%s" -> "1754611200000-00042-<deviceId>".
    /// wallMillis negativo o mayor a 9999999999999 lanza std::out_of_range.
    [[nodiscard]] std::string encode() const;

    /// Inversa exacta de encode(). Formato invalido lanza
    /// std::invalid_argument.
    static Hlc decode(std::string_view text);

    /// Orden: wallMillis, luego counter, luego deviceId lexicografico.
    [[nodiscard]] std::strong_ordering operator<=>(const Hlc& other) const noexcept;
    [[nodiscard]] bool operator==(const Hlc& other) const noexcept;
};

class HlcClock {
public:
    /// `deviceId` vacio lanza std::invalid_argument.
    explicit HlcClock(std::string deviceId);

    /// Evento local. Si `physicalMillis` es mayor al ultimo wall conocido,
    /// adopta ese valor y resetea el contador a 0; si no, mantiene el wall y
    /// incrementa el contador. Garantiza que dos llamadas seguidas nunca
    /// devuelven el mismo Hlc.
    Hlc now(std::int64_t physicalMillis);

    /// Recepcion de un evento remoto. El nuevo wall es el maximo entre el
    /// local, el remoto y `physicalMillis`; el contador se ajusta segun de
    /// donde vino ese maximo, siempre por encima del contador remoto cuando
    /// los walls empatan. Garantiza causalidad: el Hlc devuelto es
    /// estrictamente mayor que `remote`.
    Hlc update(const Hlc& remote, std::int64_t physicalMillis);

    [[nodiscard]] const std::string& deviceId() const noexcept;
    [[nodiscard]] Hlc last() const noexcept;

private:
    std::string deviceId_;
    std::int64_t wallMillis_ = 0;
    std::uint32_t counter_ = 0;
};

} // namespace dake::core
