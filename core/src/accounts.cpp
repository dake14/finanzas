#include "dake/core/accounts.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <stdexcept>

namespace dake::core {
namespace {

[[nodiscard]] const Pocket* findPocket(const std::vector<Pocket>& pockets, const Id& id) {
    const auto it = std::find_if(pockets.begin(), pockets.end(),
                                 [&id](const Pocket& p) { return p.id == id; });
    return it == pockets.end() ? nullptr : &*it;
}

[[nodiscard]] bool sameIgnoringCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

} // namespace

std::string_view toString(Account value) noexcept {
    return value == Account::Personal ? "Personal" : "Negocio";
}

Account accountFromString(std::string_view text) {
    if (text == "Negocio") return Account::Negocio;
    if (text == "Personal") return Account::Personal;
    throw std::invalid_argument("Account desconocida: '" + std::string(text) + "'");
}

Account accountOf(const Pocket& pocket) noexcept {
    if (pocket.accountOverride) {
        return *pocket.accountOverride;
    }
    return pocket.kind == PocketKind::Personal ? Account::Personal : Account::Negocio;
}

Account accountOf(const Movement& movement, const std::vector<Pocket>& pockets) {
    const Pocket* pocket = findPocket(pockets, movement.pocketId);
    return pocket == nullptr ? Account::Negocio : accountOf(*pocket);
}

bool isSalary(const Movement& movement, const std::vector<Pocket>& pockets) {
    if (movement.kind != MovementKind::Traspaso) {
        return false;
    }
    const Pocket* from = findPocket(pockets, movement.pocketId);
    const Pocket* to = findPocket(pockets, movement.targetPocketId);
    return from != nullptr && to != nullptr && accountOf(*from) == Account::Negocio &&
           accountOf(*to) == Account::Personal;
}

// ---------------------------------------------------------------- Categorias

std::string_view toString(CategoryClass value) noexcept {
    switch (value) {
        case CategoryClass::General: return "General";
        case CategoryClass::Fija: return "Fija";
        case CategoryClass::Variable: return "Variable";
        case CategoryClass::Activo: return "Activo";
    }
    return "General";
}

CategoryClass categoryClassFromString(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, CategoryClass>, 4> table{{
        {"General", CategoryClass::General},
        {"Fija", CategoryClass::Fija},
        {"Variable", CategoryClass::Variable},
        {"Activo", CategoryClass::Activo},
    }};
    for (const auto& [name, value] : table) {
        if (name == text) {
            return value;
        }
    }
    throw std::invalid_argument("CategoryClass desconocida: '" + std::string(text) + "'");
}

const Category* findCategory(const std::vector<Category>& categories, std::string_view name) {
    const auto it = std::find_if(categories.begin(), categories.end(), [name](const Category& c) {
        return sameIgnoringCase(c.name, name);
    });
    return it == categories.end() ? nullptr : &*it;
}

std::vector<Category> inferCategories(const std::vector<Movement>& movements,
                                      const std::vector<Pocket>& pockets,
                                      const std::vector<Category>& known) {
    struct Tally {
        Category category;
        int personal = 0;
        int business = 0;
    };
    std::vector<Tally> tallies;
    for (const Movement& m : movements) {
        // "Sin categoria" lo escribia la aplicacion vieja en lugar de nada: no
        // es una categoria, es la falta de una.
        if (m.deleted || m.category.empty() || m.category == kUncategorized ||
            m.kind == MovementKind::Traspaso || findCategory(known, m.category) != nullptr) {
            continue;
        }
        auto it = std::find_if(tallies.begin(), tallies.end(), [&m](const Tally& t) {
            return sameIgnoringCase(t.category.name, m.category);
        });
        if (it == tallies.end()) {
            Category fresh;
            fresh.name = m.category;
            fresh.kind = m.kind;
            tallies.push_back({fresh, 0, 0});
            it = std::prev(tallies.end());
        }
        (accountOf(m, pockets) == Account::Personal ? it->personal : it->business) += 1;
    }

    std::vector<Category> out;
    for (Tally& tally : tallies) {
        tally.category.account =
            tally.personal > tally.business ? Account::Personal : Account::Negocio;
        out.push_back(tally.category);
    }
    return out;
}

// -------------------------------------------------------- Gasto cruzado

std::vector<Movement> splitCrossExpense(const Movement& draft,
                                        const std::vector<Pocket>& pockets,
                                        Account categoryAccount,
                                        const Id& personalPocketId) {
    if (draft.kind != MovementKind::Gasto || categoryAccount != Account::Personal ||
        personalPocketId.empty() || accountOf(draft, pockets) != Account::Negocio) {
        return {draft};
    }

    Movement salary = draft;
    salary.kind = MovementKind::Traspaso;
    salary.targetPocketId = personalPocketId;
    salary.category.clear();
    salary.jobId.clear();
    salary.spreadMonths = 1;
    salary.settled = true;
    salary.name = "Sueldo: " + draft.name;

    Movement expense = draft;
    expense.pocketId = personalPocketId;
    expense.jobId.clear();

    return {salary, expense};
}

// ------------------------------------------------ Ingreso fuera del negocio
//
// CONTRATO: el de accounts.hpp, al pie de la letra. Dependencias: solo lo que
// este archivo ya incluye.

Id personalSavingsPocket(const std::vector<Pocket>& pockets) {
    for (const Pocket& pocket : pockets) {
        if (!pocket.archived && pocket.kind == PocketKind::Ahorro && accountOf(pocket) == Account::Personal) {
            return pocket.id;
        }
    }
    return {};
}

std::vector<Movement> splitPersonalIncome(const Movement& draft,
                                          Account incomeAccount,
                                          const Id& savingsPocketId,
                                          int savingsBps) {
    if (draft.kind != MovementKind::Ingreso ||
        !draft.settled ||
        incomeAccount != Account::Personal ||
        savingsBps < 1 || savingsBps > 10000 ||
        savingsPocketId.empty() ||
        savingsPocketId == draft.pocketId) {
        return {draft};
    }

    // Redondeo al entero mas cercano: (x + 5000) / 10000
    const std::int64_t part = (draft.amountMinor * static_cast<std::int64_t>(savingsBps) + 5000) / 10000;
    if (part <= 0) {
        return {draft};
    }

    Movement saving = draft;
    saving.kind = MovementKind::Traspaso;
    saving.targetPocketId = savingsPocketId;
    saving.amountMinor = part;
    saving.name = "Ahorro: " + draft.name;
    saving.category.clear();
    saving.jobId.clear();
    saving.spreadMonths = 1;
    saving.settled = true;

    return {draft, saving};
}

// ----------------------------------------------------- Gasto por categoria

SpendingReport spendingByCategory(const std::vector<Movement>& movements,
                                  const std::vector<Pocket>& pockets,
                                  Account account,
                                  Date month,
                                  Currency currency) {
    const Date from = month.firstDayOfMonth();
    const Date to = month.lastDayOfMonth();
    const Date prevFrom = from.addMonths(-1);
    const Date prevTo = prevFrom.lastDayOfMonth();

    SpendingReport report;
    report.account = account;
    report.totalCurrent = Money::zero(currency);
    report.totalPrevious = Money::zero(currency);

    // std::map da un orden estable por nombre, que es el desempate cuando dos
    // categorias cambiaron lo mismo.
    std::map<std::string, CategorySpend> byName;
    for (const Movement& m : movements) {
        if (m.deleted || m.kind != MovementKind::Gasto || accountOf(m, pockets) != account) {
            continue;
        }
        const bool inCurrent = m.date >= from && m.date <= to;
        const bool inPrevious = m.date >= prevFrom && m.date <= prevTo;
        if (!inCurrent && !inPrevious) {
            continue;
        }
        const std::string name = m.category.empty() ? std::string(kUncategorized) : m.category;
        auto [it, fresh] = byName.try_emplace(
            name, CategorySpend{name, Money::zero(currency), Money::zero(currency)});
        const Money amount = Money::fromMinor(m.amountMinor, currency);
        if (inCurrent) {
            it->second.current += amount;
            report.totalCurrent += amount;
        } else {
            it->second.previous += amount;
            report.totalPrevious += amount;
        }
    }

    for (auto& [name, row] : byName) {
        report.rows.push_back(row);
    }
    std::stable_sort(report.rows.begin(), report.rows.end(),
                     [](const CategorySpend& a, const CategorySpend& b) {
                         return a.change().minor() > b.change().minor();
                     });
    return report;
}

} // namespace dake::core
