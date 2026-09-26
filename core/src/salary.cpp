#include "dake/core/salary.hpp"

#include <algorithm>

#include "dake/core/report.hpp"

namespace dake::core {
namespace {

[[nodiscard]] std::int64_t divRound(std::int64_t a, std::int64_t b) {
    const std::int64_t half = b / 2;
    return a >= 0 ? (a + half) / b : -((-a + half) / b);
}

[[nodiscard]] const Pocket* findPocket(const std::vector<Pocket>& pockets, const Id& id) {
    const auto it = std::find_if(pockets.begin(), pockets.end(), [&id](const Pocket& p) { return p.id == id; });
    return it == pockets.end() ? nullptr : &*it;
}

[[nodiscard]] bool inBusiness(const Movement& m, const std::vector<Pocket>& pockets) {
    const Pocket* p = findPocket(pockets, m.pocketId);
    return p == nullptr || accountOf(*p) == Account::Negocio;
}

[[nodiscard]] bool inMonth(Date date, Date month) {
    return date.year == month.year && date.month == month.month;
}

[[nodiscard]] Money average(const std::vector<Money>& values, std::size_t count, Currency currency) {
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < count; ++i) sum += values[i].minor();
    return Money::fromMinor(divRound(sum, static_cast<std::int64_t>(count)), currency);
}

} // namespace

MonthNet businessNet(const std::vector<Movement>& movements, const std::vector<Pocket>& pockets,
                     const std::vector<Category>& categories, const std::vector<Tool>& tools, Date month,
                     Currency currency) {
    MonthNet out;
    out.month = month.firstDayOfMonth();
    out.income = Money::zero(currency);
    out.cost = depreciationInMonth(tools, out.month, currency);
    for (const Movement& m : movements) {
        if (m.deleted || !inBusiness(m, pockets)) continue;
        if (m.kind == MovementKind::Ingreso && inMonth(m.date, out.month)) {
            out.income += Money::fromMinor(m.amountMinor, currency);
        } else if (m.kind == MovementKind::Gasto) {
            // Las herramientas entran por depreciacion: su compra no es costo.
            const Category* category = findCategory(categories, m.category);
            if (category != nullptr && category->cls == CategoryClass::Activo) continue;
            out.cost += costInMonth(m, currency, out.month.year, out.month.month);
        }
    }
    out.net = out.income - out.cost;
    return out;
}

SalaryAdvice salaryAdvice(const std::vector<Movement>& movements, const std::vector<Pocket>& pockets,
                          const std::vector<Category>& categories, const std::vector<Tool>& tools,
                          const ProfitSplit& split, Date today, Currency currency) {
    SalaryAdvice a;
    a.base = a.shortfall = a.salary = a.taxes = a.reinvest = a.emergency = a.paidAverage =
        Money::zero(currency);

    // Desde el primer mes con actividad del negocio: antes de usar la
    // aplicacion no hay meses en cero, hay meses que no se anotaron.
    std::optional<Date> first;
    for (const Movement& m : movements) {
        if (!m.deleted && inBusiness(m, pockets) && (!first || m.date < *first)) first = m.date;
    }
    const Date thisMonth = today.firstDayOfMonth();
    if (!first) return a;
    for (Date month = thisMonth.addMonths(-1); month >= first->firstDayOfMonth() && a.months.size() < 6;
         month = month.addMonths(-1)) {
        a.months.push_back(businessNet(movements, pockets, categories, tools, month, currency));
    }
    a.closedMonths = static_cast<int>(a.months.size());
    if (a.closedMonths == 0) return a;
    a.provisional = a.closedMonths < 3;

    std::vector<Money> nets;
    for (const MonthNet& m : a.months) nets.push_back(m.net);
    const std::size_t recent = std::min<std::size_t>(3, nets.size());
    a.average3 = average(nets, recent, currency);
    a.average6 = average(nets, nets.size(), currency);
    Money base = std::min(*a.average3, *a.average6);
    if (base.isNegative()) {
        a.shortfall = -base;
        base = Money::zero(currency);
    }
    a.base = base;

    // El reparto suma exactamente la base: el ultimo se lleva lo que quede
    // del redondeo.
    if (split.valid()) {
        a.salary = base.percent(split.salaryBps);
        a.taxes = base.percent(split.taxesBps);
        a.reinvest = base.percent(split.reinvestBps);
        a.emergency = base - a.salary - a.taxes - a.reinvest;
    }

    // Gasto personal y sueldo pagado, en los mismos meses que el promedio de 3.
    std::int64_t personal = 0;
    std::int64_t paid = 0;
    for (std::size_t i = 0; i < recent; ++i) {
        const Date month = a.months[i].month;
        for (const Movement& m : movements) {
            if (m.deleted || !inMonth(m.date, month)) continue;
            if (m.kind == MovementKind::Gasto && !inBusiness(m, pockets)) personal += m.amountMinor;
            if (isSalary(m, pockets)) paid += m.amountMinor;
        }
    }
    a.personalSpend = Money::fromMinor(divRound(personal, static_cast<std::int64_t>(recent)), currency);
    a.paidAverage = Money::fromMinor(divRound(paid, static_cast<std::int64_t>(recent)), currency);
    return a;
}

std::vector<CashMonth> businessCashFlow(const std::vector<Movement>& movements,
                                        const std::vector<Pocket>& pockets, Date from, Date to,
                                        Currency currency) {
    std::vector<CashMonth> out;
    for (Date month = from.firstDayOfMonth(); month <= to; month = month.addMonths(1)) {
        CashMonth c;
        c.month = month;
        c.in = c.outExpenses = c.outSalary = c.balance = Money::zero(currency);
        for (const Movement& m : movements) {
            if (m.deleted || !m.settled || !inMonth(m.date, month)) continue;
            const Money amount = Money::fromMinor(m.amountMinor, currency);
            const bool fromBusiness = inBusiness(m, pockets);
            if (m.kind == MovementKind::Ingreso && fromBusiness) {
                c.in += amount;
            } else if (m.kind == MovementKind::Gasto && fromBusiness) {
                c.outExpenses += amount;
            } else if (m.kind == MovementKind::Traspaso) {
                const Pocket* target = findPocket(pockets, m.targetPocketId);
                const bool toBusiness = target == nullptr || accountOf(*target) == Account::Negocio;
                if (isSalary(m, pockets)) {
                    c.outSalary += amount;
                } else if (!fromBusiness && toBusiness) {
                    c.in += amount;  // plata personal puesta en el negocio
                }
            }
        }
        for (const PocketBalance& balance : pocketBalances(pockets, movements, currency, month.lastDayOfMonth())) {
            const Pocket* p = findPocket(pockets, balance.pocketId);
            if (p != nullptr && accountOf(*p) == Account::Negocio) c.balance += balance.balance;
        }
        out.push_back(c);
    }
    return out;
}

} // namespace dake::core
