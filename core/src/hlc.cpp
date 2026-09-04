// UNIDAD: core/src/hlc.cpp
//
// Implementa el contrato declarado en dake/core/hlc.hpp.
//
// DEPENDENCIAS PERMITIDAS (lista cerrada): "dake/core/hlc.hpp", <algorithm>,
// <charconv>, <compare>, <cstdint>, <stdexcept>, <string>, <string_view>.
// PROHIBIDO: <chrono>. El tiempo fisico SIEMPRE entra por parametro.
//
// ALGORITMO now(pt):
//   nuevoWall = max(wall_, pt)
//   si nuevoWall == wall_  -> counter_ += 1
//   si no                  -> wall_ = nuevoWall; counter_ = 0
//   devolver {wall_, counter_, deviceId_}
//
// ALGORITMO update(remote, pt):
//   nuevoWall = max(wall_, remote.wallMillis, pt)
//   si nuevoWall == wall_ y nuevoWall == remote.wallMillis
//        -> counter_ = max(counter_, remote.counter) + 1
//   si no si nuevoWall == wall_      -> counter_ += 1
//   si no si nuevoWall == remote.wallMillis -> counter_ = remote.counter + 1
//   si no                            -> counter_ = 0
//   wall_ = nuevoWall
//   devolver {wall_, counter_, deviceId_}
//
// Un desbordamiento de counter_ (llega a UINT32_MAX) lanza
// std::overflow_error en lugar de dar la vuelta: perder la monotonia rompe la
// causalidad de la sincronizacion en silencio, y eso es peor que fallar.
//
// CRITERIO DE ACEPTACION:
//   - HlcClock("") lanza std::invalid_argument.
//   - now(1000) dos veces seguidas da counter 0 y luego 1, mismo wall.
//   - now(1000) y despues now(500) (reloj que retrocede) mantiene wall 1000 y
//     sube el counter: nunca retrocede.
//   - now(1000) < now(1000) siempre (estrictamente creciente).
//   - update({2000, 5, "otro"}, 1000) devuelve wall 2000, counter 6.
//   - El resultado de update es SIEMPRE estrictamente mayor que `remote`.
//   - Hlc::decode(h.encode()) == h para wall 0, 1 y 1754611200000.
//   - encode() de wall 1754611200000, counter 42, device "abc" es exactamente
//     "1754611200000-00042-abc".
//   - Dos Hlc con igual wall y counter se ordenan por deviceId.
//   - decode("basura") y decode("123-456") lanzan std::invalid_argument.
//   - Orden lexicografico de encode() coincide con operator<=>, para wall de
//     hasta 13 digitos.

#include "dake/core/hlc.hpp"

#include <algorithm>
#include <charconv>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dake::core {

std::string Hlc::encode() const {
    if (wallMillis < 0 || wallMillis > 9999999999999LL) {
        throw std::out_of_range("wallMillis out of bounds");
    }
    std::string wStr = std::to_string(wallMillis);
    std::string res = std::string(wStr.size() < 13U ? 13U - wStr.size() : 0U, '0') + wStr + "-";
    
    std::string cStr = std::to_string(counter);
    res += std::string(cStr.size() < 5U ? 5U - cStr.size() : 0U, '0') + cStr + "-";
    res += deviceId;
    return res;
}

Hlc Hlc::decode(std::string_view text) {
    auto dash1 = text.find('-');
    auto dash2 = text.find('-', dash1 == std::string_view::npos ? 0 : dash1 + 1);
    if (dash1 == std::string_view::npos || dash2 == std::string_view::npos) {
        throw std::invalid_argument("formato invalido");
    }
    
    auto wStr = text.substr(0, dash1);
    auto cStr = text.substr(dash1 + 1, dash2 - dash1 - 1);
    
    if (wStr.empty() || cStr.empty()) {
        throw std::invalid_argument("formato invalido");
    }
    
    Hlc h;
    h.deviceId = std::string(text.substr(dash2 + 1));
    
    auto wRes = std::from_chars(wStr.data(), wStr.data() + wStr.size(), h.wallMillis);
    if (wRes.ec != std::errc{} || wRes.ptr != wStr.data() + wStr.size()) {
        throw std::invalid_argument("formato invalido");
    }
    
    auto cRes = std::from_chars(cStr.data(), cStr.data() + cStr.size(), h.counter);
    if (cRes.ec != std::errc{} || cRes.ptr != cStr.data() + cStr.size()) {
        throw std::invalid_argument("formato invalido");
    }
    
    return h;
}

std::strong_ordering Hlc::operator<=>(const Hlc& other) const noexcept {
    if (auto cmp = wallMillis <=> other.wallMillis; cmp != 0) return cmp;
    if (auto cmp = counter <=> other.counter; cmp != 0) return cmp;
    return deviceId <=> other.deviceId;
}

bool Hlc::operator==(const Hlc& other) const noexcept {
    return wallMillis == other.wallMillis && counter == other.counter && deviceId == other.deviceId;
}

HlcClock::HlcClock(std::string deviceId) : deviceId_(std::move(deviceId)) {
    if (deviceId_.empty()) {
        throw std::invalid_argument("deviceId vacio");
    }
}

Hlc HlcClock::now(std::int64_t physicalMillis) {
    std::int64_t nuevoWall = std::max(wallMillis_, physicalMillis);
    if (nuevoWall == wallMillis_) {
        if (counter_ == UINT32_MAX) throw std::overflow_error("counter overflow");
        counter_ += 1U;
    } else {
        wallMillis_ = nuevoWall;
        counter_ = 0;
    }
    return Hlc{wallMillis_, counter_, deviceId_};
}

Hlc HlcClock::update(const Hlc& remote, std::int64_t physicalMillis) {
    std::int64_t nuevoWall = std::max({wallMillis_, remote.wallMillis, physicalMillis});
    if (nuevoWall == wallMillis_ && nuevoWall == remote.wallMillis) {
        std::uint32_t max_cnt = std::max(counter_, remote.counter);
        if (max_cnt == UINT32_MAX) throw std::overflow_error("counter overflow");
        counter_ = max_cnt + 1U;
    } else if (nuevoWall == wallMillis_) {
        if (counter_ == UINT32_MAX) throw std::overflow_error("counter overflow");
        counter_ += 1U;
    } else if (nuevoWall == remote.wallMillis) {
        if (remote.counter == UINT32_MAX) throw std::overflow_error("counter overflow");
        counter_ = remote.counter + 1U;
    } else {
        counter_ = 0;
    }
    wallMillis_ = nuevoWall;
    return Hlc{wallMillis_, counter_, deviceId_};
}

const std::string& HlcClock::deviceId() const noexcept { return deviceId_; }

Hlc HlcClock::last() const noexcept { return Hlc{wallMillis_, counter_, deviceId_}; }

} // namespace dake::core
