#include "dake/core/model.hpp"

#include <algorithm>
#include <stdexcept>

namespace dake::core {
namespace {

/// Los enums se persisten como TEXTO, no como enteros: si mañana se inserta un
/// valor en medio del enum, los enteros ya guardados pasan a significar otra
/// cosa en silencio y no hay forma de notarlo mirando la base.
template <typename Enum, std::size_t N>
[[nodiscard]] Enum fromName(std::string_view text,
                            const std::array<std::pair<std::string_view, Enum>, N>& table,
                            const char* what) {
    for (const auto& [name, value] : table) {
        if (name == text) {
            return value;
        }
    }
    throw std::invalid_argument(std::string(what) + " desconocido: '" + std::string(text) + "'");
}

} // namespace

// ---------------------------------------------------------------- Bolsillos

std::string_view toString(PocketKind value) noexcept {
    switch (value) {
        case PocketKind::Operacion: return "Operacion";
        case PocketKind::Ahorro: return "Ahorro";
        case PocketKind::Inversion: return "Inversion";
        case PocketKind::Personal: return "Personal";
    }
    return "Operacion";
}

PocketKind pocketKindFromString(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, PocketKind>, 4> table{{
        {"Operacion", PocketKind::Operacion},
        {"Ahorro", PocketKind::Ahorro},
        {"Inversion", PocketKind::Inversion},
        {"Personal", PocketKind::Personal},
    }};
    return fromName(text, table, "PocketKind");
}

std::array<PocketKind, 4> allPocketKinds() noexcept {
    return {PocketKind::Operacion, PocketKind::Ahorro, PocketKind::Inversion,
            PocketKind::Personal};
}

bool isReserve(PocketKind value) noexcept {
    return value == PocketKind::Ahorro || value == PocketKind::Inversion;
}

// -------------------------------------------------------------- Movimientos

std::string_view toString(MovementKind value) noexcept {
    switch (value) {
        case MovementKind::Ingreso: return "Ingreso";
        case MovementKind::Gasto: return "Gasto";
        case MovementKind::Traspaso: return "Traspaso";
    }
    return "Gasto";
}

MovementKind movementKindFromString(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, MovementKind>, 3> table{{
        {"Ingreso", MovementKind::Ingreso},
        {"Gasto", MovementKind::Gasto},
        {"Traspaso", MovementKind::Traspaso},
    }};
    return fromName(text, table, "MovementKind");
}

std::string_view toString(Recurrence value) noexcept {
    return value == Recurrence::Mensual ? "Mensual" : "Puntual";
}

Recurrence recurrenceFromString(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, Recurrence>, 2> table{{
        {"Puntual", Recurrence::Puntual},
        {"Mensual", Recurrence::Mensual},
    }};
    return fromName(text, table, "Recurrence");
}

bool Movement::isWellFormed() const noexcept {
    if (amountMinor <= 0 || name.empty() || pocketId.empty() || !date.isValid()) {
        return false;
    }
    if (spreadMonths < 1) {
        return false;
    }
    if (kind == MovementKind::Traspaso) {
        // Un traspaso a si mismo mueve cero plata y ensucia los totales de
        // "salio de ahorro" con una fila que en realidad no movio nada.
        return !targetPocketId.empty() && targetPocketId != pocketId;
    }
    return targetPocketId.empty();
}

// ------------------------------------------------------------------ Helpers

std::vector<Movement> inRange(const std::vector<Movement>& movements, Date from, Date to) {
    std::vector<Movement> out;
    out.reserve(movements.size());
    for (const Movement& movement : movements) {
        if (movement.deleted) {
            continue;
        }
        if (movement.date < from || to < movement.date) {
            continue;
        }
        out.push_back(movement);
    }
    return out;
}

std::vector<Movement> recentMovements(const std::vector<Movement>& movements, int count) {
    if (count <= 0) {
        return {};
    }

    std::vector<Movement> live;
    live.reserve(movements.size());
    for (const Movement& movement : movements) {
        if (!movement.deleted) {
            live.push_back(movement);
        }
    }

    std::sort(live.begin(), live.end(), [](const Movement& a, const Movement& b) {
        if (a.date != b.date) {
            return b.date < a.date;
        }
        return a.id > b.id;
    });

    if (live.size() > static_cast<std::size_t>(count)) {
        live.resize(static_cast<std::size_t>(count));
    }
    return live;
}

int daysSinceLastEntry(const std::vector<Movement>& movements, Date asOf) {
    bool found = false;
    Date last{};
    for (const Movement& movement : movements) {
        if (movement.deleted) {
            continue;
        }
        if (!found || last < movement.date) {
            last = movement.date;
            found = true;
        }
    }
    if (!found) {
        return -1;
    }
    const std::int64_t days = asOf.toEpochDays() - last.toEpochDays();
    // Una fecha futura (un movimiento programado, o el reloj corrido) no es
    // "hace -3 dias": es cero dias sin cargar nada.
    return days <= 0 ? 0 : static_cast<int>(days);
}

} // namespace dake::core
