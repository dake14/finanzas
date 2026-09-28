#include "dake/core/repairs.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace dake::core {
namespace {

/// a / b redondeado a la unidad, la mitad lejos del cero. b > 0.
[[nodiscard]] std::int64_t divRound(std::int64_t a, std::int64_t b) {
    const std::int64_t half = b / 2;
    return a >= 0 ? (a + half) / b : -((-a + half) / b);
}

[[nodiscard]] Money perMinutes(std::int64_t ratePerHourMinor, int minutes, Currency currency) {
    return Money::fromMinor(divRound(ratePerHourMinor * minutes, 60), currency);
}

[[nodiscard]] std::optional<Money> suggested(const Money& cost, int targetBps) {
    if (targetBps >= 10000) {
        return std::nullopt;
    }
    return Money::fromMinor(divRound(cost.minor() * 10000, 10000 - targetBps), cost.currency());
}

[[nodiscard]] Date repairDate(const Repair& repair) {
    if (repair.delivered) return *repair.delivered;
    if (repair.received) return *repair.received;
    return Date{};
}

[[nodiscard]] std::string incomeName(const Repair& repair) {
    std::string name = "Reparacion";
    if (!repair.orderNo.empty()) name += " " + repair.orderNo;
    if (!repair.device.empty()) name += " · " + repair.device;
    return name;
}

} // namespace

// ----------------------------------------------------------------- Textos

std::string_view toString(RepairType value) noexcept {
    switch (value) {
        case RepairType::GPU: return "GPU";
        case RepairType::Laptop: return "Laptop";
        case RepairType::PlacaMadre: return "PlacaMadre";
        case RepairType::Otro: return "Otro";
    }
    return "Otro";
}

RepairType repairTypeFromString(std::string_view text) {
    for (const RepairType type : allRepairTypes()) {
        if (toString(type) == text) return type;
    }
    throw std::invalid_argument("RepairType desconocido: '" + std::string(text) + "'");
}

std::string_view toString(RepairStatus value) noexcept {
    switch (value) {
        case RepairStatus::EnProceso: return "EnProceso";
        case RepairStatus::Entregada: return "Entregada";
        case RepairStatus::Cobrada: return "Cobrada";
    }
    return "EnProceso";
}

RepairStatus repairStatusFromString(std::string_view text) {
    for (const RepairStatus status :
         {RepairStatus::EnProceso, RepairStatus::Entregada, RepairStatus::Cobrada}) {
        if (toString(status) == text) return status;
    }
    throw std::invalid_argument("RepairStatus desconocido: '" + std::string(text) + "'");
}

std::vector<RepairType> allRepairTypes() {
    return {RepairType::GPU, RepairType::Laptop, RepairType::PlacaMadre, RepairType::Otro};
}

// --------------------------------------------------------------- Por reparacion

Money repairPrice(const Repair& repair, const std::vector<Movement>& movements,
                  Currency currency) {
    Money income = Money::zero(currency);
    for (const Movement& m : movements) {
        if (!m.deleted && m.kind == MovementKind::Ingreso && m.jobId == repair.jobId) {
            income += Money::fromMinor(m.amountMinor, currency);
        }
    }
    return income.minor() > 0 ? income : Money::fromMinor(repair.priceMinor, currency);
}

Money looseExpenses(const Repair& repair, const std::vector<RepairPart>& parts,
                    const std::vector<Movement>& movements, Currency currency) {
    std::set<Id> claimed;
    for (const RepairPart& part : parts) {
        if (part.jobId == repair.jobId && !part.movementId.empty()) {
            claimed.insert(part.movementId);
        }
    }
    Money total = Money::zero(currency);
    for (const Movement& m : movements) {
        if (!m.deleted && m.kind == MovementKind::Gasto && m.jobId == repair.jobId &&
            !claimed.contains(m.id)) {
            total += Money::fromMinor(m.amountMinor, currency);
        }
    }
    return total;
}

RepairCosting costRepair(const Repair& repair, const std::vector<RepairPart>& parts,
                         const std::vector<Movement>& movements, const CostSettings& settings,
                         Currency currency) {
    RepairCosting c;
    c.jobId = repair.jobId;
    c.price = repairPrice(repair, movements, currency);

    c.parts = looseExpenses(repair, parts, movements, currency);
    for (const RepairPart& part : parts) {
        if (part.jobId != repair.jobId) continue;
        if (part.costKnown) {
            c.parts += Money::fromMinor(part.costMinor, currency);
        } else {
            c.partsIncomplete = true;
        }
    }
    c.direct = c.parts;

    c.hoursEstimated = !repair.realMinutes.has_value();
    c.minutes = repair.realMinutes.value_or(repair.estMinutes);
    c.labor = perMinutes(settings.hourlyRateMinor, c.minutes, currency);
    c.fixedShare = perMinutes(settings.fixedPerHourMinor, c.minutes, currency);
    c.cost = c.direct + c.labor + c.fixedShare;
    c.profit = c.price - c.cost;

    if (c.price.minor() > 0) {
        c.marginBps = static_cast<int>(divRound(c.profit.minor() * 10000, c.price.minor()));
    }
    if (c.minutes > 0) {
        c.profitPerHour =
            Money::fromMinor(divRound((c.price - c.direct).minor() * 60, c.minutes), currency);
    }
    c.suggestedPrice = suggested(c.cost, settings.targetMarginBps);
    return c;
}

// ------------------------------------------------------------------ Por tipo

Money hourlyNeeded(const CostSettings& settings, Currency currency) {
    return Money::fromMinor(settings.hourlyRateMinor + settings.fixedPerHourMinor, currency);
}

std::vector<TypeStats> statsByType(const std::vector<Repair>& repairs,
                                   const std::vector<RepairPart>& parts,
                                   const std::vector<Movement>& movements,
                                   const CostSettings& settings, Currency currency, Date from,
                                   Date to) {
    std::vector<TypeStats> out;
    for (const RepairType type : allRepairTypes()) {
        TypeStats stats;
        stats.type = type;
        stats.revenue = stats.profit = stats.averagePrice = stats.averageCost =
            Money::zero(currency);
        Money cost = Money::zero(currency);
        Money beforeHours = Money::zero(currency);
        int minutes = 0;
        int realMinutes = 0;
        int estMinutes = 0;

        for (const Repair& repair : repairs) {
            if (repair.type != type || repair.status == RepairStatus::EnProceso) continue;
            const Date date = repairDate(repair);
            if (date < from || to < date) continue;

            const RepairCosting c = costRepair(repair, parts, movements, settings, currency);
            ++stats.count;
            stats.revenue += c.price;
            stats.profit += c.profit;
            cost += c.cost;
            beforeHours += c.price - c.direct;
            minutes += c.minutes;
            if (repair.realMinutes && repair.estMinutes > 0) {
                realMinutes += *repair.realMinutes;
                estMinutes += repair.estMinutes;
            }
        }
        if (stats.count == 0) continue;

        stats.averagePrice = Money::fromMinor(divRound(stats.revenue.minor(), stats.count), currency);
        stats.averageCost = Money::fromMinor(divRound(cost.minor(), stats.count), currency);
        if (stats.revenue.minor() > 0) {
            stats.marginBps =
                static_cast<int>(divRound(stats.profit.minor() * 10000, stats.revenue.minor()));
        }
        if (minutes > 0) {
            stats.profitPerHour =
                Money::fromMinor(divRound(beforeHours.minor() * 60, minutes), currency);
        }
        if (estMinutes > 0) {
            stats.hoursRatioPermille = static_cast<int>(divRound(realMinutes * 1000LL, estMinutes));
        }
        stats.suggestedPrice = suggested(stats.averageCost, settings.targetMarginBps);

        if (stats.count < 3 || !stats.marginBps) {
            stats.verdict = Verdict::PocosDatos;
        } else if (*stats.marginBps >= settings.targetMarginBps) {
            stats.verdict = Verdict::Bien;
        } else if (*stats.marginBps >= settings.targetMarginBps - 500) {
            stats.verdict = Verdict::Cerca;
        } else {
            stats.verdict = Verdict::Bajo;
        }
        out.push_back(stats);
    }
    return out;
}

// ---------------------------------------------------------- Plantillas y alta

std::string nextOrderNumber(const std::vector<Repair>& repairs) {
    long long highest = 0;
    for (const Repair& repair : repairs) {
        const std::string& order = repair.orderNo;
        if (order.size() < 3 || order.rfind("R-", 0) != 0) continue;
        const std::string digits = order.substr(2);
        if (digits.empty() || digits.size() > 9 ||
            !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }
        highest = std::max(highest, std::stoll(digits));
    }
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "R-%04lld", highest + 1);
    return buffer.data();
}

Repair repairFromTemplate(const RepairTemplate& tpl, const Id& jobId, const std::string& orderNo,
                          const std::string& device, Date today) {
    Repair repair;
    repair.jobId = jobId;
    repair.orderNo = orderNo;
    repair.device = device;
    repair.type = tpl.type;
    repair.templateId = tpl.id;
    repair.received = today;
    repair.status = RepairStatus::EnProceso;
    repair.priceMinor = tpl.priceMinor;
    repair.estMinutes = tpl.estMinutes;
    repair.consumablesMinor = tpl.consumablesMinor;
    repair.shippingMinor = tpl.shippingMinor;
    return repair;
}

std::vector<RepairTemplate> defaultTemplates() {
    std::vector<RepairTemplate> out;
    auto add = [&out](RepairType type, const char* name, int minutes, std::int64_t consumables) {
        RepairTemplate tpl;
        tpl.type = type;
        tpl.name = name;
        tpl.estMinutes = minutes;
        tpl.consumablesMinor = consumables;
        out.push_back(tpl);
    };
    add(RepairType::GPU, "GPU", 120, 5'00);
    add(RepairType::Laptop, "Laptop", 90, 3'00);
    add(RepairType::PlacaMadre, "Placa madre", 180, 4'00);
    add(RepairType::Otro, "Otro", 60, 0);
    return out;
}

std::vector<RepairPart> partsFromTemplate(const RepairTemplate& tpl, const Id& jobId) {
    std::vector<RepairPart> out;
    for (const TemplatePart& part : tpl.parts) {
        RepairPart p;
        p.jobId = jobId;
        p.name = part.name;
        p.costMinor = part.costMinor;
        p.costKnown = true;
        out.push_back(p);
    }
    return out;
}

// ---------------------------------------------------------- Entregar y cobrar

std::optional<Movement> repairIncome(const Id& jobId, const std::vector<Movement>& movements) {
    for (const Movement& m : movements) {
        if (!m.deleted && m.kind == MovementKind::Ingreso && m.jobId == jobId) {
            return m;
        }
    }
    return std::nullopt;
}

DeliveryResult deliverRepair(const Repair& repair, const std::vector<Movement>& movements,
                             Date date, int realMinutes, std::int64_t priceMinor, bool charged,
                             const Id& pocketId, const std::string& category) {
    DeliveryResult result;
    result.repair = repair;
    result.repair.delivered = date;
    result.repair.realMinutes = realMinutes;
    result.repair.priceMinor = priceMinor;

    Movement income;
    if (auto existing = repairIncome(repair.jobId, movements)) {
        income = *existing;
    } else {
        income.kind = MovementKind::Ingreso;
        income.jobId = repair.jobId;
        income.pocketId = pocketId;
        income.category = category;
        income.name = incomeName(repair);
        income.settled = false;
    }
    income.date = date;
    income.amountMinor = priceMinor;
    if (charged && !income.settled) {
        income.settled = true;
        income.settledDate = date;
    }

    result.repair.status = income.settled ? RepairStatus::Cobrada : RepairStatus::Entregada;
    if (income.amountMinor > 0) {
        result.income = income;
    }
    return result;
}

DeliveryResult chargeRepair(const Repair& repair, const std::vector<Movement>& movements,
                            Date date, const Id& pocketId, const std::string& category) {
    DeliveryResult result;
    result.repair = repair;
    result.repair.status = RepairStatus::Cobrada;
    if (!result.repair.delivered) {
        result.repair.delivered = date;
    }

    Movement income;
    if (auto existing = repairIncome(repair.jobId, movements)) {
        income = *existing;
    } else {
        income.kind = MovementKind::Ingreso;
        income.jobId = repair.jobId;
        income.pocketId = pocketId;
        income.category = category;
        income.name = incomeName(repair);
        income.date = result.repair.delivered.value_or(date);
        income.amountMinor = repair.priceMinor;
    }
    if (!income.settled || !income.settledDate) {
        income.settled = true;
        income.settledDate = date;
    }
    if (income.amountMinor > 0) {
        result.income = income;
    }
    return result;
}

Repair reconcileRepair(const Repair& repair, const std::vector<Movement>& movements) {
    Repair out = repair;
    bool anySettled = false;
    bool anyIncome = false;
    std::optional<Date> earliest;
    for (const Movement& m : movements) {
        if (m.deleted || m.kind != MovementKind::Ingreso || m.jobId != repair.jobId) continue;
        anyIncome = true;
        anySettled = anySettled || m.settled;
        if (!earliest || m.date < *earliest) earliest = m.date;
    }
    if (!anyIncome) {
        return out;
    }
    // El estado solo avanza: los movimientos pueden decir que ya se entrego o
    // se cobro, nunca que se "descobro".
    if (anySettled) {
        out.status = RepairStatus::Cobrada;
    } else if (out.status == RepairStatus::EnProceso) {
        out.status = RepairStatus::Entregada;
    }
    if (!out.delivered) {
        out.delivered = earliest;
    }
    return out;
}

} // namespace dake::core
