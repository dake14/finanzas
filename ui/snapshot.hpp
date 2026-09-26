#pragma once
//
// ui/snapshot.hpp — todo lo que las pantallas necesitan saber, junto.
//
// Las paginas no leen la base ni guardan nada: reciben esta foto y emiten
// señales. Quien escribe es la ventana principal, por un solo camino. Con eso,
// una pantalla se puede armar en una prueba sin abrir un archivo.
//
#include <QString>
#include <QStringList>
#include <algorithm>
#include <string>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/capture.hpp"
#include "dake/core/fixed.hpp"
#include "dake/core/currency.hpp"
#include "dake/core/model.hpp"
#include "dake/core/quotes.hpp"
#include "dake/core/repairs.hpp"
#include "dake/core/salary.hpp"

namespace dake::ui {

struct Snapshot {
    std::vector<core::Pocket> pockets;
    std::vector<core::Job> jobs;
    std::vector<core::Movement> movements;
    std::vector<core::Category> categories;
    std::vector<core::Repair> repairs;
    std::vector<core::RepairPart> parts;
    std::vector<core::RepairTemplate> templates;
    core::CostSettings costs;

    // --- Fijos y bandeja --------------------------------------------------
    std::vector<core::Recurring> recurring;
    std::vector<core::Tool> tools;
    std::vector<core::MovementMeta> metas;
    core::FixedRate fixedRate;
    core::ProfitSplit split;
    core::SalaryAdvice salary;
    int fallbackMinutesPerMonth = 4800;
    std::vector<core::InboxItem> inbox;
    int reminderWeekday = 6;  ///< 0 = lunes; 6 = domingo
    int reminderHour = 18;
    /// Mediana de lo que tarda una revision completa, en ms. -1 sin datos.
    qint64 reviewMedianMs = -1;
    qint64 repairMedianMs = -1;

    [[nodiscard]] const core::Movement* movement(const core::Id& id) const {
        const auto it = std::find_if(movements.begin(), movements.end(),
                                     [&id](const core::Movement& m) { return m.id == id; });
        return it == movements.end() ? nullptr : &*it;
    }

    [[nodiscard]] const core::MovementMeta* meta(const core::Id& movementId) const {
        const auto it = std::find_if(metas.begin(), metas.end(), [&movementId](const core::MovementMeta& m) {
            return m.movementId == movementId;
        });
        return it == metas.end() ? nullptr : &*it;
    }

    [[nodiscard]] const core::RepairPart* part(const core::Id& id) const {
        const auto it = std::find_if(parts.begin(), parts.end(),
                                     [&id](const core::RepairPart& p) { return p.id == id; });
        return it == parts.end() ? nullptr : &*it;
    }

    // --- DakeLabs Cotizaciones ------------------------------------------
    QString quoteFolder;
    bool quoteFolderFound = false;
    QStringList quoteErrors;
    std::vector<core::QuoteDoc> quoteDocs;
    std::vector<core::QuotePlan> quotePlans;

    /// Los documentos que esperan una decision.
    [[nodiscard]] int quoteHolds() const {
        int n = 0;
        for (const core::QuotePlan& plan : quotePlans) {
            if (plan.decision == core::QuoteDecision::Esperar) ++n;
        }
        return n;
    }

    [[nodiscard]] const core::QuoteDoc* quoteDoc(const std::string& id) const {
        for (const core::QuoteDoc& doc : quoteDocs) {
            if (doc.id == id) return &doc;
        }
        return nullptr;
    }
    core::Currency currency = core::Currency::usd();

    // --- Preferencias de esta computadora -------------------------------
    QString hotkey = QStringLiteral("Ctrl+Alt+Space");
    bool hotkeyRegistered = false;
    bool autostart = true;
    /// Mediana de lo que tardan las capturas, en ms. -1 si no hay datos.
    qint64 captureMedianMs = -1;
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

    /// La cuenta de una categoria. Si no existe todavia, la del bolsillo del
    /// que sale: una categoria nueva escrita pagando desde la caja del taller
    /// es del negocio hasta que alguien diga lo contrario.
    [[nodiscard]] core::Account categoryAccount(const std::string& name,
                                                const core::Id& pocketId) const {
        if (const core::Category* category = core::findCategory(categories, name)) {
            return category->account;
        }
        const core::Pocket* p = pocket(pocketId);
        return p == nullptr ? core::Account::Negocio : core::accountOf(*p);
    }

    /// Adonde va un gasto personal pagado con plata del negocio: el primer
    /// bolsillo personal no archivado. Vacio si no hay ninguno.
    [[nodiscard]] core::Id personalPocket() const {
        for (const core::Pocket& p : pockets) {
            if (!p.archived && core::accountOf(p) == core::Account::Personal) {
                return p.id;
            }
        }
        return {};
    }

    /// Las reparaciones abiertas, como las necesita el interprete de la
    /// captura para reconocerlas dentro de una frase.
    [[nodiscard]] std::vector<core::RepairRef> openRepairRefs() const {
        std::vector<core::RepairRef> out;
        for (const core::Repair& repair : repairs) {
            if (repair.status == core::RepairStatus::Cobrada) {
                continue;
            }
            const core::Job* j = job(repair.jobId);
            if (j == nullptr || j->deleted) {
                continue;
            }
            out.push_back({repair.jobId, repair.orderNo, repair.device, j->client});
        }
        return out;
    }

    [[nodiscard]] const core::Job* job(const core::Id& id) const {
        const auto it = std::find_if(jobs.begin(), jobs.end(),
                                     [&id](const core::Job& j) { return j.id == id; });
        return it == jobs.end() ? nullptr : &*it;
    }

    [[nodiscard]] const core::Repair* repair(const core::Id& jobId) const {
        const auto it = std::find_if(repairs.begin(), repairs.end(),
                                     [&jobId](const core::Repair& r) { return r.jobId == jobId; });
        return it == repairs.end() ? nullptr : &*it;
    }

    [[nodiscard]] std::vector<core::RepairPart> partsOf(const core::Id& jobId) const {
        std::vector<core::RepairPart> out;
        for (const core::RepairPart& part : parts) {
            if (part.jobId == jobId) out.push_back(part);
        }
        return out;
    }

    /// Los clientes ya anotados, sin repetir: para completar al escribir.
    [[nodiscard]] QStringList clients() const {
        QStringList out;
        for (const core::Job& j : jobs) {
            const QString client = QString::fromStdString(j.client).trimmed();
            if (!j.deleted && !client.isEmpty() && !out.contains(client, Qt::CaseInsensitive)) {
                out << client;
            }
        }
        return out;
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
