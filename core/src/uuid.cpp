// UNIDAD: core/src/uuid.cpp
//
// Implementa el contrato declarado en dake/core/uuid.hpp.
//
// DEPENDENCIAS PERMITIDAS (lista cerrada): "dake/core/uuid.hpp", <array>,
// <charconv>, <chrono>, <cstdint>, <random>, <stdexcept>, <string>,
// <string_view>.
// <chrono> se permite UNICAMENTE dentro de SystemUuidGenerator.
//
// DISPOSICION DE BITS (RFC 9562, UUIDv7):
//   bits  0..47  : unixMillis big-endian (6 bytes)
//   bits 48..51  : version = 0b0111
//   bits 52..63  : 12 bits aleatorios
//   bits 64..65  : variante = 0b10
//   bits 66..127 : 62 bits aleatorios
// randomBytes[0..1] alimentan los 12 bits (se descartan los 4 altos del byte
// 0), randomBytes[2..9] alimentan los 62 bits restantes (se pisan los 2 bits
// altos con la variante). Determinista: la misma entrada da la misma salida.
//
// CRITERIO DE ACEPTACION:
//   - makeUuidV7(0, ceros) tiene 36 caracteres, guiones en 8/13/18/23,
//     el digito en la posicion 14 es '7' y el de la posicion 19 esta en
//     {'8','9','a','b'}.
//   - Salida siempre en minusculas.
//   - uuidV7Timestamp(makeUuidV7(n, r)) == n para n en {0, 1, 1754611200000}.
//   - makeUuidV7(-1, r) lanza std::invalid_argument.
//   - Orden lexicografico: si a < b en milisegundos, makeUuidV7(a,r) <
//     makeUuidV7(b,r) como string. Esta propiedad es la razon de usar v7.
//   - isValidUuid acepta mayusculas y minusculas; rechaza 35 o 37 caracteres,
//     guiones mal ubicados y caracteres no hexadecimales.
//   - uuidV7Timestamp sobre un UUID version 4 lanza std::invalid_argument.
//   - DeterministicUuidGenerator con la misma semilla produce exactamente la
//     misma secuencia en dos instancias distintas, y cada next() avanza 1 ms.

#include "dake/core/uuid.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dake::core {

std::string makeUuidV7(std::int64_t unixMillis, const std::array<std::uint8_t, 10>& randomBytes) {
    if (unixMillis < 0) {
        throw std::invalid_argument("unixMillis negativo");
    }

    std::array<std::uint8_t, 16> buf;
    buf[0] = static_cast<std::uint8_t>((unixMillis >> 40) & 0xFF);
    buf[1] = static_cast<std::uint8_t>((unixMillis >> 32) & 0xFF);
    buf[2] = static_cast<std::uint8_t>((unixMillis >> 24) & 0xFF);
    buf[3] = static_cast<std::uint8_t>((unixMillis >> 16) & 0xFF);
    buf[4] = static_cast<std::uint8_t>((unixMillis >> 8) & 0xFF);
    buf[5] = static_cast<std::uint8_t>(unixMillis & 0xFF);

    buf[6] = static_cast<std::uint8_t>(0x70 | (randomBytes[0] & 0x0F));
    buf[7] = randomBytes[1];

    buf[8] = static_cast<std::uint8_t>(0x80 | (randomBytes[2] & 0x3F));
    buf[9] = randomBytes[3];

    for (int i = 4; i < 10; ++i) {
        buf[6 + i] = randomBytes[i];
    }

    constexpr char hex[] = "0123456789abcdef";
    std::string out(36, '-');
    int outIdx = 0;
    for (int i = 0; i < 16; ++i) {
        out[outIdx++] = hex[(buf[i] >> 4) & 0x0F];
        out[outIdx++] = hex[buf[i] & 0x0F];
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            outIdx++;
        }
    }
    return out;
}

bool isValidUuid(std::string_view text) {
    if (text.size() != 36) return false;
    for (std::size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') return false;
        } else {
            char c = text[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }
    }
    return true;
}

std::int64_t uuidV7Timestamp(std::string_view text) {
    if (!isValidUuid(text)) {
        throw std::invalid_argument("UUID invalido");
    }
    if (text[14] != '7') {
        throw std::invalid_argument("Version de UUID incorrecta");
    }

    auto hexVal = [](char c) -> std::int64_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    std::int64_t ts = 0;
    for (int i = 0; i < 8; ++i) {
        ts = (ts << 4) | hexVal(text[i]);
    }
    for (int i = 9; i < 13; ++i) {
        ts = (ts << 4) | hexVal(text[i]);
    }
    return ts;
}

SystemUuidGenerator::SystemUuidGenerator() : engine_(std::random_device{}()) {}

std::string SystemUuidGenerator::next() {
    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::array<std::uint8_t, 10> rb;
    std::uint64_t r1 = engine_();
    std::uint64_t r2 = engine_();

    rb[0] = static_cast<std::uint8_t>(r1 & 0xFF);
    rb[1] = static_cast<std::uint8_t>((r1 >> 8) & 0xFF);
    rb[2] = static_cast<std::uint8_t>((r1 >> 16) & 0xFF);
    rb[3] = static_cast<std::uint8_t>((r1 >> 24) & 0xFF);
    rb[4] = static_cast<std::uint8_t>((r1 >> 32) & 0xFF);
    rb[5] = static_cast<std::uint8_t>((r1 >> 40) & 0xFF);
    rb[6] = static_cast<std::uint8_t>((r1 >> 48) & 0xFF);
    rb[7] = static_cast<std::uint8_t>((r1 >> 56) & 0xFF);
    rb[8] = static_cast<std::uint8_t>(r2 & 0xFF);
    rb[9] = static_cast<std::uint8_t>((r2 >> 8) & 0xFF);

    return makeUuidV7(millis, rb);
}

DeterministicUuidGenerator::DeterministicUuidGenerator(std::int64_t startMillis, std::uint64_t seed)
    : millis_(startMillis), engine_(seed) {}

std::string DeterministicUuidGenerator::next() {
    std::int64_t ms = millis_++;
    
    std::array<std::uint8_t, 10> rb;
    std::uint64_t r1 = engine_();
    std::uint64_t r2 = engine_();

    rb[0] = static_cast<std::uint8_t>(r1 & 0xFF);
    rb[1] = static_cast<std::uint8_t>((r1 >> 8) & 0xFF);
    rb[2] = static_cast<std::uint8_t>((r1 >> 16) & 0xFF);
    rb[3] = static_cast<std::uint8_t>((r1 >> 24) & 0xFF);
    rb[4] = static_cast<std::uint8_t>((r1 >> 32) & 0xFF);
    rb[5] = static_cast<std::uint8_t>((r1 >> 40) & 0xFF);
    rb[6] = static_cast<std::uint8_t>((r1 >> 48) & 0xFF);
    rb[7] = static_cast<std::uint8_t>((r1 >> 56) & 0xFF);
    rb[8] = static_cast<std::uint8_t>(r2 & 0xFF);
    rb[9] = static_cast<std::uint8_t>((r2 >> 8) & 0xFF);

    return makeUuidV7(ms, rb);
}

} // namespace dake::core
