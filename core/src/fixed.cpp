#include "dake/core/fixed.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace dake::core {
namespace {

[[nodiscard]] std::int64_t divRound(std::int64_t a, std::int64_t b) {
    const std::int64_t half = b / 2;
    return a >= 0 ? (a + half) / b : -((-a + half) / b);
}

/// Meses enteros de `from` a `to` (0 si son el mismo mes).
[[nodiscard]] int monthsBetween(Date from, Date to) {
    return (to.year - from.year) * 12 + static_cast<int>(to.month) - static_cast<int>(from.month);
}

[[nodiscard]] bool isSnoozed(const InboxInput& input, const Id& id) {
    return std::any_of(input.snoozed.begin(), input.snoozed.end(), [&](const auto& entry) {
        return entry.first == id && input.today < entry.second;
    });
}

[[nodiscard]] const MovementMeta* metaOf(const std::vector<MovementMeta>& metas, const Id& id) {
    const auto it = std::find_if(metas.begin(), metas.end(),
                                 [&id](const MovementMeta& m) { return m.movementId == id; });
    return it == metas.end() ? nullptr : &*it;
}

} // namespace

std::string periodOf(Date date) {
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%04d-%02u", date.year, date.month);
    return buffer.data();
}

// ------------------------------------------------------------ Recurrentes

std::vector<GeneratedRecurring> dueRecurring(const std::vector<Recurring>& recurring,
                                             const std::vector<MovementMeta>& metas, Date today) {
    std::vector<GeneratedRecurring> out;
    for (const Recurring& r : recurring) {
        if (!r.active || r.amountMinor <= 0) continue;
        for (Date month = r.starts.firstDayOfMonth(); month <= today; month = month.addMonths(1)) {
            const unsigned day = std::min(static_cast<unsigned>(std::max(1, r.dayOfMonth)),
                                          daysInMonth(month.year, month.month));
            const Date due = Date::fromYmd(month.year, month.month, day);
            if (today < due || (r.ends && *r.ends < due)) continue;
            const std::string period = periodOf(month);
            const bool done = std::any_of(metas.begin(), metas.end(), [&](const MovementMeta& m) {
                return m.recurringId == r.id && m.period == period;
            });
            if (done) continue;

            GeneratedRecurring g;
            g.movement.id = "rec-" + r.id + "-" + period;
            g.movement.date = due;
            g.movement.name = r.name;
            g.movement.kind = MovementKind::Gasto;
            g.movement.amountMinor = r.amountMinor;
            g.movement.pocketId = r.pocketId;
            g.movement.category = r.category;
            g.meta.movementId = g.movement.id;
            g.meta.origin = "Recurrente";
            g.meta.review = "confirmar";
            g.meta.recurringId = r.id;
            g.meta.period = period;
            out.push_back(g);
        }
    }
    return out;
}

// ------------------------------------------------------------ Herramientas

Money depreciationInMonth(const Tool& tool, Date month, Currency currency) {
    const int index = monthsBetween(tool.bought, month);
    if (index < 0 || index >= tool.lifeMonths || tool.lifeMonths <= 0 || tool.costMinor <= 0) {
        return Money::zero(currency);
    }
    if (tool.retired && tool.retired->firstDayOfMonth() <= month.firstDayOfMonth()) {
        return Money::zero(currency);
    }
    return Money::fromMinor(tool.costMinor, currency).allocate(tool.lifeMonths)[static_cast<std::size_t>(index)];
}

Money depreciationInMonth(const std::vector<Tool>& tools, Date month, Currency currency) {
    Money total = Money::zero(currency);
    for (const Tool& tool : tools) {
        total += depreciationInMonth(tool, month, currency);
    }
    return total;
}

// --------------------------------------------------------- Tasa de fijos

FixedRate fixedRate(const std::vector<Recurring>& recurring, const std::vector<Tool>& tools,
                    const std::vector<Repair>& repairs, Date today, int fallbackMinutesPerMonth,
                    Currency currency) {
    FixedRate rate;
    rate.monthlyFixed = depreciationInMonth(tools, today, currency);
    for (const Recurring& r : recurring) {
        if (r.active && (!r.ends || today <= *r.ends)) {
            rate.monthlyFixed += Money::fromMinor(r.amountMinor, currency);
        }
    }

    // Los ultimos tres meses cerrados; menos si la primera entrega es mas
    // nueva: dividir por tres con un solo mes de historia daria un taller que
    // trabaja un tercio de lo que trabaja.
    const Date thisMonth = today.firstDayOfMonth();
    const Date windowStart = thisMonth.addMonths(-3);
    std::optional<Date> firstDelivery;
    long long minutes = 0;
    for (const Repair& r : repairs) {
        if (!r.delivered || !r.realMinutes) continue;
        const Date month = r.delivered->firstDayOfMonth();
        if (month < windowStart || thisMonth <= month) continue;
        minutes += *r.realMinutes;
        if (!firstDelivery || month < *firstDelivery) firstDelivery = month;
    }
    if (minutes > 0 && firstDelivery) {
        const int months = std::max(1, monthsBetween(*firstDelivery, thisMonth));
        rate.monthlyMinutes = static_cast<int>(minutes / months);
    } else {
        rate.monthlyMinutes = fallbackMinutesPerMonth;
        rate.minutesFromSettings = true;
    }
    rate.perHour = rate.monthlyMinutes > 0
                       ? Money::fromMinor(divRound(rate.monthlyFixed.minor() * 60, rate.monthlyMinutes),
                                          currency)
                       : Money::zero(currency);
    return rate;
}

// ------------------------------------------------------------------ Bandeja

std::vector<InboxItem> inbox(const InboxInput& input) {
    std::vector<InboxItem> out;
    auto add = [&out](InboxKind kind, const Id& id, int days = 0) { out.push_back({kind, id, days}); };

    // Lo que ensucia los numeros.
    for (const Movement& m : input.movements) {
        const bool uncategorized = m.category.empty() || m.category == kUncategorized;
        if (!m.deleted && m.kind != MovementKind::Traspaso && uncategorized && !isSnoozed(input, m.id)) {
            add(InboxKind::SinCategoria, m.id);
        }
    }
    for (const std::pair<const char*, InboxKind> review :
         {std::pair{"confirmar", InboxKind::PorConfirmar}, std::pair{"sugerido", InboxKind::Sugerido},
          std::pair{"vida", InboxKind::VidaUtil}}) {
        for (const Movement& m : input.movements) {
            const MovementMeta* meta = metaOf(input.metas, m.id);
            if (!m.deleted && meta != nullptr && meta->review == review.first &&
                !isSnoozed(input, m.id)) {
                add(review.second, m.id);
            }
        }
    }
    if (input.quoteHolds > 0) {
        add(InboxKind::Cotizaciones, Id(), input.quoteHolds);
    }

    // Lo que cuesta plata.
    const std::int64_t today = input.today.toEpochDays();
    for (const Repair& r : input.repairs) {
        if (r.status == RepairStatus::Entregada && r.delivered && !isSnoozed(input, r.jobId)) {
            const auto days = static_cast<int>(today - r.delivered->toEpochDays());
            if (days > 7) add(InboxKind::PorCobrar, r.jobId, days);
        }
    }
    for (const Repair& r : input.repairs) {
        if (r.status == RepairStatus::EnProceso && r.received && !isSnoozed(input, r.jobId)) {
            const auto days = static_cast<int>(today - r.received->toEpochDays());
            if (days > 14) add(InboxKind::SinEntregar, r.jobId, days);
        }
    }

    // Lo que solo afina.
    for (const Repair& r : input.repairs) {
        if (r.status != RepairStatus::EnProceso && !r.realMinutes &&
            !isSnoozed(input, r.jobId + ":horas")) {
            add(InboxKind::SinHoras, r.jobId);
        }
    }
    for (const RepairPart& p : input.parts) {
        if (!p.costKnown && !isSnoozed(input, p.id)) {
            add(InboxKind::CostoRepuesto, p.id);
        }
    }
    return out;
}

} // namespace dake::core
