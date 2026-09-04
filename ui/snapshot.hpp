#pragma once
//
// ui/snapshot.hpp — todo lo que las pantallas necesitan saber, junto.
//
// Las paginas no leen la base ni guardan nada: reciben esta foto y emiten
// señales. Quien escribe es la ventana principal, por un solo camino. Con eso,
// una pantalla se puede armar en una prueba sin abrir un archivo.
//
#include <QString>
#include <algorithm>
#include <string>
#include <vector>

#include "dake/core/currency.hpp"
#include "dake/core/model.hpp"

namespace dake::ui {

struct Snapshot {
    std::vector<core::Pocket> pockets;
    std::vector<core::Job> jobs;
    std::vector<core::Movement> movements;
    core::Currency currency = core::Currency::usd();
    core::Date today{2026, 9, 2};

    [[nodiscard]] QString pocketName(const core::Id& id) const {
        const auto it = std::find_if(pockets.begin(), pockets.end(),
                                     [&id](const core::Pocket& p) { return p.id == id; });
        return it == pockets.end() ? QString() : QString::fromStdString(it->name);
    }

    [[nodiscard]] const core::Pocket* pocket(const core::Id& id) const {
        const auto it = std::find_if(pockets.begin(), pockets.end(),
                                     [&id](const core::Pocket& p) { return p.id == id; });
        return it == pockets.end() ? nullptr : &*it;
    }

    [[nodiscard]] QString jobName(const core::Id& id) const {
        const auto it = std::find_if(jobs.begin(), jobs.end(),
                                     [&id](const core::Job& j) { return j.id == id; });
        return it == jobs.end() ? QString() : QString::fromStdString(it->name);
    }

    /// Primer y ultimo dia del mes de `today`. Todo el tablero mira ese rango.
    [[nodiscard]] core::Date monthStart() const { return today.firstDayOfMonth(); }
    [[nodiscard]] core::Date monthEnd() const { return today.lastDayOfMonth(); }

    /// Las categorias ya usadas para un tipo de movimiento, de la mas usada a
    /// la menos. Se aprenden de los datos: una lista fija se queda vieja el
    /// primer dia y obliga a escribir a mano lo que uno ya escribio diez veces.
    [[nodiscard]] QStringList categoriesFor(core::MovementKind kind) const {
        std::vector<std::pair<std::string, int>> counts;
        for (const core::Movement& movement : movements) {
            if (movement.deleted || movement.kind != kind || movement.category.empty()) {
                continue;
            }
            const auto it = std::find_if(
                counts.begin(), counts.end(),
                [&movement](const auto& entry) { return entry.first == movement.category; });
            if (it == counts.end()) {
                counts.emplace_back(movement.category, 1);
            } else {
                ++it->second;
            }
        }
        std::stable_sort(counts.begin(), counts.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });

        QStringList out;
        for (const auto& [category, uses] : counts) {
            out << QString::fromStdString(category);
        }
        return out;
    }
};

} // namespace dake::ui
