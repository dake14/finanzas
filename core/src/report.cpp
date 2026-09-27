#include "dake/core/report.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace dake::core {
namespace {

/// Indice de mes absoluto desde el año 0. Restar dos de estos da la distancia
/// en meses sin tener que pensar en el cambio de año.
[[nodiscard]] std::int64_t monthIndex(int year, unsigned month) noexcept {
    return static_cast<std::int64_t>(year) * 12 + static_cast<std::int64_t>(month) - 1;
}

[[nodiscard]] std::int64_t monthIndex(Date date) noexcept {
    return monthIndex(date.year, date.month);
}

[[nodiscard]] std::unordered_map<Id, PocketKind> kindsById(const std::vector<Pocket>& pockets) {
    std::unordered_map<Id, PocketKind> out;
    out.reserve(pockets.size());
    for (const Pocket& pocket : pockets) {
        out.emplace(pocket.id, pocket.kind);
    }
    return out;
}

/// true si el bolsillo existe y es reserva. Un id que no corresponde a ningun
/// bolsillo NO se trata como reserva: inventar la clase de un bolsillo que no
/// esta seria peor que no contar el movimiento.
[[nodiscard]] bool reserveSide(const std::unordered_map<Id, PocketKind>& kinds, const Id& id) {
    const auto it = kinds.find(id);
    return it != kinds.end() && isReserve(it->second);
}

/// Formatea el margen en puntos basicos sin pasar por punto flotante.
[[nodiscard]] int marginBasisPoints(const Money& margin, const Money& income) {
    if (income.minor() == 0) {
        return 0;
    }
    return static_cast<int>((margin.minor() * 10000) / income.minor());
}

} // namespace

// ------------------------------------------------------ 1. Saldo por bolsillo

std::vector<PocketBalance> pocketBalances(const std::vector<Pocket>& pockets,
                                          const std::vector<Movement>& movements,
                                          Currency currency,
                                          Date asOf) {
    std::vector<PocketBalance> out;
    out.reserve(pockets.size());

    std::unordered_map<Id, std::size_t> index;
    for (const Pocket& pocket : pockets) {
        if (pocket.deleted) {
            continue;
        }
        index.emplace(pocket.id, out.size());
        out.push_back(PocketBalance{pocket.id, pocket.name, pocket.kind,
                                    Money::fromMinor(pocket.openingMinor, currency),
                                    Money::zero(currency)});
    }

    for (const Movement& movement : movements) {
        if (movement.deleted || asOf < movement.date || !movement.isWellFormed()) {
            continue;
        }
        const Money amount = Money::fromMinor(movement.amountMinor, currency);

        const auto source = index.find(movement.pocketId);
        if (source == index.end()) {
            continue;
        }
        PocketBalance& from = out[source->second];

        switch (movement.kind) {
            case MovementKind::Ingreso:
                // Un ingreso sin cobrar NO esta en el bolsillo. Sumarlo es la
                // forma mas rapida de creerse rico un martes y descubrir el
                // viernes que no habia con que pagar el material.
                if (movement.settled) {
                    from.balance += amount;
                } else {
                    from.pendingIn += amount;
                }
                break;

            case MovementKind::Gasto:
                if (movement.settled) {
                    from.balance -= amount;
                }
                break;

            case MovementKind::Traspaso: {
                const auto target = index.find(movement.targetPocketId);
                if (target == index.end()) {
                    // Sin destino valido el traspaso quedaria sacando plata de
                    // un lado y no poniendola en ninguno: eso no es un
                    // traspaso, es una fuga inventada por la app.
                    break;
                }
                from.balance -= amount;
                out[target->second].balance += amount;
                break;
            }
        }
    }

    return out;
}

Money reserveTotal(const std::vector<PocketBalance>& balances, Currency currency) {
    Money total = Money::zero(currency);
    for (const PocketBalance& balance : balances) {
        if (isReserve(balance.kind)) {
            total += balance.balance;
        }
    }
    return total;
}

Money totalFor(const std::vector<PocketBalance>& balances, PocketKind kind, Currency currency) {
    Money total = Money::zero(currency);
    for (const PocketBalance& balance : balances) {
        if (balance.kind == kind) {
            total += balance.balance;
        }
    }
    return total;
}

Money totalAll(const std::vector<PocketBalance>& balances, Currency currency) {
    Money total = Money::zero(currency);
    for (const PocketBalance& balance : balances) {
        total += balance.balance;
    }
    return total;
}

// -------------------------------------------------- 2. De donde salio la plata

bool Funding::eatingReserves() const noexcept {
    return net.minor() > 0;
}

int Funding::monthsOfRunway(int periodMonths) const {
    if (!eatingReserves() || periodMonths < 1) {
        return -1;
    }
    const std::int64_t perMonth = net.minor() / periodMonths;
    if (perMonth <= 0) {
        return -1;
    }
    if (reserveBalance.minor() <= 0) {
        return 0;
    }
    return static_cast<int>(reserveBalance.minor() / perMonth);
}

Funding funding(const std::vector<Pocket>& pockets,
                const std::vector<Movement>& movements,
                Currency currency,
                Date from,
                Date to) {
    const auto kinds = kindsById(pockets);

    Funding result{Money::zero(currency), Money::zero(currency), Money::zero(currency),
                   Money::zero(currency)};

    for (const Movement& movement : inRange(movements, from, to)) {
        if (movement.kind != MovementKind::Traspaso || !movement.isWellFormed()) {
            continue;
        }
        const bool sourceIsReserve = reserveSide(kinds, movement.pocketId);
        const bool targetIsReserve = reserveSide(kinds, movement.targetPocketId);

        // Mover de ahorro a inversion no descapitaliza nada: la plata sigue
        // siendo reserva. Solo cuenta lo que cruza la frontera.
        if (sourceIsReserve == targetIsReserve) {
            continue;
        }
        const Money amount = Money::fromMinor(movement.amountMinor, currency);
        if (sourceIsReserve) {
            result.fromReserves += amount;
        } else {
            result.toReserves += amount;
        }
    }

    result.net = result.fromReserves - result.toReserves;

    const auto balances = pocketBalances(pockets, movements, currency, to);
    result.reserveBalance = reserveTotal(balances, currency);

    return result;
}

// ------------------------------------------------------------- 3. Caja y costo

Money costInMonth(const Movement& movement, Currency currency, int year, unsigned month) {
    if (movement.deleted || movement.kind != MovementKind::Gasto || !movement.isWellFormed()) {
        return Money::zero(currency);
    }

    const std::int64_t offset = monthIndex(year, month) - monthIndex(movement.date);
    if (offset < 0 || offset >= movement.spreadMonths) {
        return Money::zero(currency);
    }

    const Money total = Money::fromMinor(movement.amountMinor, currency);
    if (movement.spreadMonths == 1) {
        return total;
    }
    // allocate reparte sin perder unidades minimas: la suma de los meses es
    // exactamente el importe pagado, nunca un centavo mas ni menos.
    return total.allocate(movement.spreadMonths)[static_cast<std::size_t>(offset)];
}

namespace {

/// Costo imputado a todos los meses que toca el rango. El costo es de grano
/// mensual a proposito: un reparto por dias fingiria una precision que la
/// compra de un rollo de filamento no tiene.
[[nodiscard]] Money costOverRange(const std::vector<Movement>& movements,
                                  Currency currency,
                                  Date from,
                                  Date to,
                                  bool onlyWithoutJob) {
    Money total = Money::zero(currency);
    for (std::int64_t index = monthIndex(from); index <= monthIndex(to); ++index) {
        const int year = static_cast<int>(index / 12);
        const unsigned month = static_cast<unsigned>(index % 12) + 1;
        for (const Movement& movement : movements) {
            if (onlyWithoutJob && !movement.jobId.empty()) {
                continue;
            }
            total += costInMonth(movement, currency, year, month);
        }
    }
    return total;
}

} // namespace

CashFlow cashFlow(const std::vector<Movement>& movements,
                  Currency currency,
                  Date from,
                  Date to) {
    CashFlow flow{Money::zero(currency), Money::zero(currency), Money::zero(currency),
                  Money::zero(currency), Money::zero(currency), Money::zero(currency)};

    for (const Movement& movement : inRange(movements, from, to)) {
        if (!movement.isWellFormed()) {
            continue;
        }
        const Money amount = Money::fromMinor(movement.amountMinor, currency);
        switch (movement.kind) {
            case MovementKind::Ingreso:
                flow.incomeAccrued += amount;
                if (movement.settled) {
                    flow.incomeCash += amount;
                }
                break;
            case MovementKind::Gasto:
                if (movement.settled) {
                    flow.outflow += amount;
                }
                break;
            case MovementKind::Traspaso:
                // Un traspaso no es ingreso ni gasto: mover plata del ahorro a
                // la caja no te hizo ganar nada.
                break;
        }
    }

    flow.cost = costOverRange(movements, currency, from, to, false);
    flow.cashDelta = flow.incomeCash - flow.outflow;
    flow.result = flow.incomeAccrued - flow.cost;
    return flow;
}

Money unusedPrepaid(const std::vector<Movement>& movements, Currency currency, Date asOf) {
    Money total = Money::zero(currency);
    const std::int64_t current = monthIndex(asOf);

    for (const Movement& movement : movements) {
        if (movement.deleted || movement.kind != MovementKind::Gasto ||
            movement.spreadMonths <= 1 || !movement.isWellFormed()) {
            continue;
        }
        const std::int64_t start = monthIndex(movement.date);
        const auto parts = Money::fromMinor(movement.amountMinor, currency)
                               .allocate(movement.spreadMonths);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (start + static_cast<std::int64_t>(i) > current) {
                total += parts[i];
            }
        }
    }
    return total;
}

// ---------------------------------------------------------- 4. Por trabajo

std::vector<JobResult> jobResults(const std::vector<Job>& jobs,
                                  const std::vector<Movement>& movements,
                                  Currency currency) {
    std::vector<JobResult> out;
    std::unordered_map<Id, std::size_t> index;

    for (const Job& job : jobs) {
        if (job.deleted) {
            continue;
        }
        index.emplace(job.id, out.size());
        JobResult result;
        result.jobId = job.id;
        result.name = job.name;
        result.client = job.client;
        result.closed = job.closed;
        result.income = Money::zero(currency);
        result.pending = Money::zero(currency);
        result.cost = Money::zero(currency);
        result.margin = Money::zero(currency);
        out.push_back(std::move(result));
    }

    for (const Movement& movement : movements) {
        if (movement.deleted || movement.jobId.empty() || !movement.isWellFormed()) {
            continue;
        }
        const auto it = index.find(movement.jobId);
        if (it == index.end()) {
            continue;
        }
        JobResult& result = out[it->second];
        const Money amount = Money::fromMinor(movement.amountMinor, currency);

        if (movement.kind == MovementKind::Ingreso) {
            // El trabajo se hizo aunque el cliente todavia no pague: el margen
            // del trabajo no cambia por la fecha del pago. Lo que si cambia es
            // la caja, y eso se mira en `pending`.
            result.income += amount;
            if (!movement.settled) {
                result.pending += amount;
            }
        } else if (movement.kind == MovementKind::Gasto) {
            // El costo de un trabajo NO se reparte en meses: el teclado se
            // compro para ese Macbook y ahi se quedo.
            result.cost += amount;
        }
    }

    for (JobResult& result : out) {
        result.margin = result.income - result.cost;
        result.marginBps = marginBasisPoints(result.margin, result.income);
    }

    return out;
}

Money overhead(const std::vector<Movement>& movements,
               Currency currency,
               Date from,
               Date to) {
    return costOverRange(movements, currency, from, to, true);
}

// --------------------------------------------------------------- 5. Alertas

std::vector<Alert> alerts(const std::vector<Pocket>& pockets,
                          const std::vector<Movement>& movements,
                          const std::vector<Job>& jobs,
                          Currency currency,
                          Date asOf,
                          const MoneyFormatter& format) {
    // Formato canonico del nucleo ("46.00 USD") cuando la interfaz no pasa el
    // suyo, para que estas funciones sigan siendo probables sin montar Qt.
    const auto plain = [&format](const Money& amount) {
        return format ? format(amount) : amount.toString();
    };

    std::vector<Alert> out;
    const Date last30 = asOf.addDays(-30);

    // --- 1. Estas financiando el negocio con tus reservas -------------------
    const Funding recent = funding(pockets, movements, currency, last30, asOf);
    if (recent.eatingReserves()) {
        const int runway = recent.monthsOfRunway(1);
        std::string detail = "Eso no es una perdida ni una ganancia: es capital "
                             "tuyo tapando un hueco. Si el mes que viene vuelve a "
                             "pasar, el problema no es el mes: es el precio.";
        if (runway >= 0) {
            detail += " Al ritmo de los ultimos 30 dias, te quedan cerca de " +
                      std::to_string(runway) + " meses de reserva.";
        }
        out.push_back(Alert{AlertLevel::Danger,
                            "En los ultimos 30 dias sacaste " + plain(recent.net) +
                                " de tus ahorros para sostener la operacion.",
                            std::move(detail)});
    }

    // --- 2. Caja de operacion en rojo --------------------------------------
    const auto balances = pocketBalances(pockets, movements, currency, asOf);
    const Money operating = totalFor(balances, PocketKind::Operacion, currency);
    if (operating.isNegative()) {
        out.push_back(Alert{AlertLevel::Danger,
                            "La caja de operacion esta en " + plain(operating) + ".",
                            "O falta cargar un ingreso, o hay que traer plata de "
                            "otro bolsillo y dejarlo anotado como lo que es."});
    }

    // --- 3. Facturado y sin cobrar -----------------------------------------
    Money pending = Money::zero(currency);
    int oldestPendingDays = 0;
    for (const Movement& movement : movements) {
        if (movement.deleted || movement.kind != MovementKind::Ingreso || movement.settled) {
            continue;
        }
        pending += Money::fromMinor(movement.amountMinor, currency);
        const auto age = static_cast<int>(asOf.toEpochDays() - movement.date.toEpochDays());
        oldestPendingDays = std::max(oldestPendingDays, age);
    }
    if (!pending.isZero()) {
        out.push_back(Alert{oldestPendingDays >= 15 ? AlertLevel::Warning : AlertLevel::Info,
                            "Tienes " + plain(pending) + " de trabajo hecho y sin cobrar.",
                            "El mas viejo lleva " + std::to_string(oldestPendingDays) +
                                " dias. Esa plata ya la trabajaste; cobrarla es mas barato "
                                "que sacarla del ahorro."});
    }

    // --- 4. Trabajos abiertos con costo y sin un peso de ingreso -----------
    Money sunk = Money::zero(currency);
    int openJobsAtLoss = 0;
    for (const JobResult& result : jobResults(jobs, movements, currency)) {
        if (result.closed) {
            if (result.margin.isNegative()) {
                out.push_back(Alert{AlertLevel::Warning,
                                    "El trabajo \"" + result.name + "\" cerro con " +
                                        plain(result.margin) + ".",
                                    "Si se repite el mismo tipo de trabajo, el precio no "
                                    "cubre el material."});
            }
            continue;
        }
        if (result.income.isZero() && !result.cost.isZero()) {
            sunk += result.cost;
            ++openJobsAtLoss;
        }
    }
    if (openJobsAtLoss > 0) {
        out.push_back(Alert{AlertLevel::Warning,
                            std::to_string(openJobsAtLoss) +
                                " trabajo(s) abiertos ya te costaron " + plain(sunk) +
                                " y todavia no cobraste nada.",
                            "Es el momento de facturar, no cuando la reserva se acabe."});
    }

    // --- 5. Material comprado por delante ----------------------------------
    const Money prepaid = unusedPrepaid(movements, currency, asOf);
    if (!prepaid.isZero()) {
        out.push_back(Alert{AlertLevel::Info,
                            "Tienes " + plain(prepaid) + " en material ya pagado que le "
                            "toca a meses que todavia no llegaron.",
                            "Ese dinero ya salio de la caja, pero no es costo de este mes. "
                            "Por eso el mes de la compra no tiene por que dar en perdida."});
    }

    // --- 6. Silencio ------------------------------------------------------
    const int silence = daysSinceLastEntry(movements, asOf);
    if (silence >= 5) {
        out.push_back(Alert{AlertLevel::Info,
                            "Llevas " + std::to_string(silence) +
                                " dias sin registrar un movimiento.",
                            "Cuanto mas se deja pasar, menos se acuerda uno; y una app con "
                            "huecos deja de servir para decidir."});
    } else if (silence < 0) {
        out.push_back(Alert{AlertLevel::Info, "Todavia no hay ningun movimiento cargado.",
                            "Empeza por el saldo real de cada bolsillo: es lo unico que "
                            "despues permite cuadrar."});
    }

    // El orden es por gravedad: lo primero de la lista tiene que ser lo que
    // mas cuesta, no lo que se calculo primero.
    std::stable_sort(out.begin(), out.end(), [](const Alert& a, const Alert& b) {
        return static_cast<int>(a.level) > static_cast<int>(b.level);
    });
    return out;
}

// --------------------------------------------------------- Resumen por mes

std::string MonthSummary::label() const {
    static constexpr std::array<const char*, 12> names{"ene", "feb", "mar", "abr",
                                                       "may", "jun", "jul", "ago",
                                                       "sep", "oct", "nov", "dic"};
    const unsigned index = (month >= 1 && month <= 12) ? month - 1 : 0;
    return std::string(names[index]) + " " + std::to_string(year);
}

std::vector<MonthSummary> summarizeByMonth(const std::vector<Pocket>& pockets,
                                           const std::vector<Movement>& movements,
                                           Currency currency) {
    // std::map por indice de mes: sale ordenado cronologicamente sin un sort
    // aparte, y sin que un mes sin actividad aparezca como una fila en cero.
    std::map<std::int64_t, MonthSummary> byMonth;

    for (const Movement& movement : movements) {
        if (movement.deleted || !movement.isWellFormed()) {
            continue;
        }
        const std::int64_t start = monthIndex(movement.date);
        const std::int64_t span =
            movement.kind == MovementKind::Gasto ? movement.spreadMonths : 1;
        for (std::int64_t i = 0; i < span; ++i) {
            const std::int64_t index = start + i;
            auto it = byMonth.find(index);
            if (it == byMonth.end()) {
                MonthSummary summary;
                summary.year = static_cast<int>(index / 12);
                summary.month = static_cast<unsigned>(index % 12) + 1;
                summary.incomeAccrued = Money::zero(currency);
                summary.cost = Money::zero(currency);
                summary.result = Money::zero(currency);
                summary.netFunding = Money::zero(currency);
                it = byMonth.emplace(index, std::move(summary)).first;
            }
        }
    }

    for (auto& [index, summary] : byMonth) {
        const Date first = Date::fromYmd(summary.year, summary.month, 1);
        const Date last = first.lastDayOfMonth();
        const CashFlow flow = cashFlow(movements, currency, first, last);
        summary.incomeAccrued = flow.incomeAccrued;
        summary.cost = flow.cost;
        summary.result = flow.result;
        summary.netFunding = funding(pockets, movements, currency, first, last).net;
    }

    std::vector<MonthSummary> out;
    out.reserve(byMonth.size());
    for (auto& [index, summary] : byMonth) {
        out.push_back(std::move(summary));
    }
    return out;
}

// ------------------------------------------------- 6. Salud del negocio

BreakEven breakEven(const std::vector<Job>& jobs,
                    const std::vector<Movement>& movements,
                    Currency currency,
                    Date from,
                    Date to) {
    BreakEven out;
    out.overheadPerMonth = Money::zero(currency);
    out.marginBps = 0;
    out.revenueNeeded = Money::zero(currency);

    const std::int64_t months = std::max<std::int64_t>(1, monthIndex(to) - monthIndex(from) + 1);
    const Money oh = overhead(movements, currency, from, to);
    out.overheadPerMonth = Money::fromMinor(oh.minor() / months, currency);

    std::unordered_map<Id, bool> activeJobs;
    for (const Movement& movement : inRange(movements, from, to)) {
        if (!movement.jobId.empty() && movement.isWellFormed()) {
            activeJobs[movement.jobId] = true;
        }
    }

    Money totalIncome = Money::zero(currency);
    Money totalMargin = Money::zero(currency);

    for (const JobResult& result : jobResults(jobs, movements, currency)) {
        if (activeJobs.find(result.jobId) != activeJobs.end()) {
            totalIncome += result.income;
            totalMargin += result.margin;
        }
    }

    out.marginBps = marginBasisPoints(totalMargin, totalIncome);
    if (out.marginBps > 0) {
        out.revenueNeeded = Money::fromMinor((out.overheadPerMonth.minor() * 10000) / out.marginBps, currency);
    }

    return out;
}

TicketStats ticketStats(const std::vector<Job>& jobs,
                        const std::vector<Movement>& movements,
                        Currency currency,
                        Date from,
                        Date to) {
    TicketStats out;
    out.jobCount = 0;
    out.averageIncome = Money::zero(currency);
    out.averageMargin = Money::zero(currency);

    std::unordered_map<Id, bool> activeJobs;
    for (const Movement& movement : inRange(movements, from, to)) {
        if (!movement.jobId.empty() && movement.isWellFormed()) {
            activeJobs[movement.jobId] = true;
        }
    }

    if (activeJobs.empty()) {
        return out;
    }

    out.jobCount = static_cast<int>(activeJobs.size());

    Money totalIncome = Money::zero(currency);
    Money totalMargin = Money::zero(currency);

    for (const JobResult& result : jobResults(jobs, movements, currency)) {
        if (activeJobs.find(result.jobId) != activeJobs.end()) {
            totalIncome += result.income;
            totalMargin += result.margin;
        }
    }

    out.averageIncome = Money::fromMinor(totalIncome.minor() / out.jobCount, currency);
    out.averageMargin = Money::fromMinor(totalMargin.minor() / out.jobCount, currency);

    return out;
}

CollectionStats collectionStats(const std::vector<Movement>& movements,
                                Date from,
                                Date to) {
    CollectionStats out;
    out.sampled = 0;
    out.averageDays = 0;
    out.worstDays = 0;
    out.uncollected = 0;

    std::int64_t totalDays = 0;

    for (const Movement& movement : inRange(movements, from, to)) {
        if (movement.kind != MovementKind::Ingreso || !movement.isWellFormed()) {
            continue;
        }

        if (!movement.settled) {
            out.uncollected++;
        } else if (movement.settledDate.has_value()) {
            // has_value() y no una comparacion contra Date{}: una Date por
            // defecto es 1970-01-01, una fecha real, y compararse contra ella
            // confundiria "no se sabe" con "se cobro ese dia".
            const std::int64_t days =
                movement.settledDate->toEpochDays() - movement.date.toEpochDays();
            const int diff = days < 0 ? 0 : static_cast<int>(days);

            totalDays += diff;
            if (diff > out.worstDays) {
                out.worstDays = diff;
            }
            out.sampled++;
        }
    }

    if (out.sampled > 0) {
        out.averageDays = static_cast<int>((totalDays + (out.sampled / 2)) / out.sampled);
    }

    return out;
}

std::vector<CategoryTotal> costByCategory(const std::vector<Movement>& movements,
                                          Currency currency,
                                          Date from,
                                          Date to) {
    std::map<std::string, Money> totals;

    for (std::int64_t index = monthIndex(from); index <= monthIndex(to); ++index) {
        const int year = static_cast<int>(index / 12);
        const unsigned month = static_cast<unsigned>(index % 12) + 1;
        for (const Movement& movement : movements) {
            const Money share = costInMonth(movement, currency, year, month);
            if (share.isZero()) {
                continue;
            }
            const std::string key =
                movement.category.empty() ? std::string(kUncategorized) : movement.category;
            const auto [it, inserted] = totals.try_emplace(key, Money::zero(currency));
            it->second += share;
        }
    }

    std::vector<CategoryTotal> out;
    out.reserve(totals.size());
    for (auto& [category, total] : totals) {
        out.push_back(CategoryTotal{category, total});
    }
    std::sort(out.begin(), out.end(), [](const CategoryTotal& a, const CategoryTotal& b) {
        if (a.total.minor() != b.total.minor()) {
            return a.total.minor() > b.total.minor();
        }
        return a.category < b.category;
    });
    return out;
}

} // namespace dake::core
