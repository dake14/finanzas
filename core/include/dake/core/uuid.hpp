#pragma once
//
// dake/core/uuid.hpp
//
// CONTRATO de la unidad `core/src/uuid.cpp`.
//
// Identificadores UUIDv7 (RFC 9562): 48 bits de timestamp Unix en milisegundos
// al frente, luego version/variante y 74 bits aleatorios. Se eligio v7 y no v4
// porque queda ordenado por tiempo, lo que da localidad de indice en SQLite y
// permite paginar por id en la sincronizacion.
//
// NUNCA usar enteros autoincrementales como id: dos dispositivos generarian el
// mismo id para registros distintos y la sincronizacion los fusionaria.
//
// La generacion se separa en dos capas: `makeUuidV7` es pura (recibe tiempo y
// aleatoriedad) para que los tests sean deterministas, y `SystemUuidGenerator`
// es la que toca el mundo real.
//
// Dependencias permitidas: <array> <cstdint> <memory> <random> <string>
//                          <string_view>
//
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <string_view>

namespace dake::core {

/// Funcion PURA. `unixMillis` negativo lanza std::invalid_argument.
/// Devuelve la forma canonica en minusculas con guiones:
/// "018f1a2b-3c4d-7e5f-8a9b-0c1d2e3f4a5b" (36 caracteres).
[[nodiscard]] std::string makeUuidV7(std::int64_t unixMillis,
                                     const std::array<std::uint8_t, 10>& randomBytes);

/// Valida forma canonica de 36 caracteres, minusculas o mayusculas, guiones en
/// las posiciones 8/13/18/23. No verifica la version.
[[nodiscard]] bool isValidUuid(std::string_view text);

/// Extrae el timestamp embebido. Lanza std::invalid_argument si `text` no es
/// un UUID valido o si su version no es 7.
[[nodiscard]] std::int64_t uuidV7Timestamp(std::string_view text);

class UuidGenerator {
public:
    virtual ~UuidGenerator() = default;
    [[nodiscard]] virtual std::string next() = 0;
};

/// Usa el reloj del sistema y std::random_device sembrando un mt19937_64.
class SystemUuidGenerator final : public UuidGenerator {
public:
    SystemUuidGenerator();
    [[nodiscard]] std::string next() override;

private:
    std::mt19937_64 engine_;
};

/// Generador determinista para tests: parte de `startMillis` y avanza 1 ms por
/// llamada; los bytes aleatorios salen de un mt19937_64 con semilla fija.
class DeterministicUuidGenerator final : public UuidGenerator {
public:
    DeterministicUuidGenerator(std::int64_t startMillis, std::uint64_t seed);
    [[nodiscard]] std::string next() override;

private:
    std::int64_t millis_;
    std::mt19937_64 engine_;
};

} // namespace dake::core
