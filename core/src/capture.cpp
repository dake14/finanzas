#include "dake/core/capture.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>

namespace dake::core {
namespace {

// ------------------------------------------------------------------ Texto

/// Minusculas y sin tildes, sobre UTF-8. Solo lo que aparece al escribir en
/// castellano: el resto de los caracteres de mas de un byte se deja igual.
[[nodiscard]] std::string fold(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte == 0xC3 && i + 1 < text.size()) {
            const auto next = static_cast<unsigned char>(text[i + 1]);
            char plain = 0;
            switch (next) {
                case 0xA1: case 0x81: plain = 'a'; break;
                case 0xA9: case 0x89: plain = 'e'; break;
                case 0xAD: case 0x8D: plain = 'i'; break;
                case 0xB3: case 0x93: plain = 'o'; break;
                case 0xBA: case 0x9A: case 0xBC: case 0x9C: plain = 'u'; break;
                case 0xB1: case 0x91: plain = 'n'; break;
                default: break;
            }
            if (plain != 0) {
                out.push_back(plain);
                ++i;
                continue;
            }
        }
        out.push_back(static_cast<char>(std::tolower(byte)));
    }
    return out;
}

[[nodiscard]] std::vector<std::string> splitSpaces(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

/// Palabras de letras y numeros, ya plegadas. "Josue Rodríguez" -> {"josue",
/// "rodriguez"}; "RTX 3080" -> {"rtx", "3080"}.
[[nodiscard]] std::vector<std::string> words(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    for (const char c : fold(text)) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current.push_back(c);
        } else if (!current.empty()) {
            out.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

[[nodiscard]] bool allDigits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

/// Los digitos del final: "R-0043" -> 43, "INF-2026-004" -> 4. -1 si no hay.
[[nodiscard]] long long trailingNumber(std::string_view text) {
    std::size_t end = text.size();
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(text[begin - 1]))) {
        --begin;
    }
    if (begin == end || end - begin > 12) {
        return -1;
    }
    return std::stoll(std::string(text.substr(begin, end - begin)));
}

// ------------------------------------------------------------------ Monto

struct AmountToken {
    std::int64_t minor = 0;
    bool plus = false;
};

[[nodiscard]] std::optional<AmountToken> parseAmountToken(std::string_view token,
                                                          Currency currency) {
    AmountToken out;
    if (!token.empty() && token.front() == '+') {
        out.plus = true;
        token.remove_prefix(1);
    }
    if (!token.empty() && token.front() == '$') {
        token.remove_prefix(1);
    }
    if (token.empty() || !std::isdigit(static_cast<unsigned char>(token.front()))) {
        return std::nullopt;
    }
    const bool shape = std::all_of(token.begin(), token.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ',';
    });
    if (!shape) {
        return std::nullopt;
    }

    // "1.200" y "12,500" son miles, no decimales: con dos decimales en la
    // moneda, tres cifras despues del separador solo pueden ser miles.
    bool thousands = true;
    {
        std::size_t groupStart = 0;
        int groups = 0;
        for (std::size_t i = 0; i <= token.size(); ++i) {
            if (i == token.size() || token[i] == '.' || token[i] == ',') {
                const std::size_t length = i - groupStart;
                if ((groups == 0 && (length < 1 || length > 3)) || (groups > 0 && length != 3)) {
                    thousands = false;
                }
                ++groups;
                groupStart = i + 1;
            }
        }
        if (groups < 2) {
            thousands = false;
        }
    }
    try {
        if (thousands) {
            std::string digits;
            for (const char c : token) {
                if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);
            }
            out.minor = Money::parse(digits, currency).minor();
        } else {
            out.minor = Money::parse(token, currency).minor();
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (out.minor <= 0) {
        return std::nullopt;
    }
    return out;
}

// ------------------------------------------------------------------ Fecha

/// 0 = lunes. El 1970-01-01 fue jueves.
[[nodiscard]] int weekday(Date date) {
    const std::int64_t days = date.toEpochDays();
    return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

[[nodiscard]] std::optional<Date> parseDateToken(const std::string& folded, Date today) {
    if (folded == "hoy") return today;
    if (folded == "ayer") return today.addDays(-1);
    if (folded == "anteayer") return today.addDays(-2);

    static const std::array<std::pair<const char*, int>, 14> kDays{{
        {"lun", 0}, {"lunes", 0}, {"mar", 1}, {"martes", 1}, {"mie", 2}, {"miercoles", 2},
        {"jue", 3}, {"jueves", 3}, {"vie", 4}, {"viernes", 4}, {"sab", 5}, {"sabado", 5},
        {"dom", 6}, {"domingo", 6},
    }};
    for (const auto& [name, index] : kDays) {
        if (folded == name) {
            const int back = (weekday(today) - index + 7) % 7;
            return today.addDays(-back);
        }
    }

    const auto slash = folded.find('/');
    if (slash != std::string::npos) {
        const std::string day = folded.substr(0, slash);
        const std::string month = folded.substr(slash + 1);
        if (allDigits(day) && allDigits(month) && day.size() <= 2 && month.size() <= 2) {
            try {
                Date date = Date::fromYmd(today.year, static_cast<unsigned>(std::stoi(month)),
                                          static_cast<unsigned>(std::stoi(day)));
                // Nadie anota lo que todavia no paso: una fecha futura es del
                // año anterior.
                if (today < date) {
                    date = Date::fromYmd(today.year - 1, date.month, date.day);
                }
                return date;
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

// ------------------------------------------------------------------ Tipo

[[nodiscard]] bool isIncomeWord(const std::string& folded) {
    static const std::set<std::string> kWords{"cobro", "cobre",  "cobramos", "abono",
                                              "venta", "vendi",  "ingreso",  "cobrado"};
    return kWords.contains(folded);
}

[[nodiscard]] bool isSalaryWord(const std::string& folded) {
    static const std::set<std::string> kWords{"sueldo", "salario", "retiro"};
    return kWords.contains(folded);
}

[[nodiscard]] bool isStopWord(const std::string& folded) {
    static const std::set<std::string> kWords{"de", "del", "la", "el", "los", "las", "con",
                                              "para", "por", "en", "un", "una", "y", "a"};
    return kWords.contains(folded);
}

// ------------------------------------------------------------ Reparacion

[[nodiscard]] Id matchRepair(const std::vector<std::string>& tokens,
                             const std::vector<RepairRef>& repairs) {
    // Primero el numero de orden: es exacto. "#43", "R-43", "INF-2026-004".
    for (const std::string& token : tokens) {
        const std::string folded = fold(token);
        for (const RepairRef& repair : repairs) {
            if (!repair.orderNo.empty() && folded == fold(repair.orderNo)) {
                return repair.jobId;
            }
        }
        const long long number = trailingNumber(folded);
        if (number < 0) {
            continue;
        }
        const bool hash = folded.front() == '#';
        const auto dash = folded.find('-');
        const std::string prefix = dash == std::string::npos ? std::string() : folded.substr(0, dash);
        if (!hash && (prefix.empty() || allDigits(prefix))) {
            continue;
        }
        for (const RepairRef& repair : repairs) {
            const std::string order = fold(repair.orderNo);
            if (trailingNumber(order) != number) {
                continue;
            }
            if (hash || order.rfind(prefix + "-", 0) == 0) {
                return repair.jobId;
            }
        }
    }

    // Despues, palabras del equipo o del cliente. Gana la que mas coincide, y
    // solo si gana sola: con un empate, adivinar es peor que no enlazar.
    std::vector<std::string> useful;
    for (const std::string& token : tokens) {
        for (const std::string& word : words(token)) {
            if (word.size() >= 3 && !isStopWord(word) && !isIncomeWord(word) &&
                !isSalaryWord(word)) {
                useful.push_back(word);
            }
        }
    }
    Id best;
    int bestScore = 0;
    bool tie = false;
    for (const RepairRef& repair : repairs) {
        std::set<std::string> known;
        for (const std::string& word : words(repair.device)) known.insert(word);
        for (const std::string& word : words(repair.client)) known.insert(word);
        std::set<std::string> hits;
        for (const std::string& word : useful) {
            if (known.contains(word)) hits.insert(word);
        }
        const int score = static_cast<int>(hits.size());
        if (score > bestScore) {
            best = repair.jobId;
            bestScore = score;
            tie = false;
        } else if (score == bestScore && score > 0) {
            tie = true;
        }
    }
    return tie ? Id() : best;
}

// ------------------------------------------------------------- Bolsillo

[[nodiscard]] const Pocket* findPocket(const std::vector<Pocket>& pockets, const Id& id) {
    const auto it = std::find_if(pockets.begin(), pockets.end(),
                                 [&id](const Pocket& p) { return p.id == id; });
    return it == pockets.end() ? nullptr : &*it;
}

/// El ultimo bolsillo usado, de una cuenta o de cualquiera. Ultimo por fecha y,
/// dentro del mismo dia, por id: los ids nacen ordenados por tiempo.
[[nodiscard]] Id lastPocket(const CaptureContext& context, std::optional<Account> account) {
    const Movement* latest = nullptr;
    for (const Movement& m : context.history) {
        if (m.deleted || m.kind == MovementKind::Traspaso) {
            continue;
        }
        const Pocket* pocket = findPocket(context.pockets, m.pocketId);
        if (pocket == nullptr || pocket->archived) {
            continue;
        }
        if (account && accountOf(*pocket) != *account) {
            continue;
        }
        if (latest == nullptr || latest->date < m.date ||
            (latest->date == m.date && latest->id < m.id)) {
            latest = &m;
        }
    }
    if (latest != nullptr) {
        return latest->pocketId;
    }
    for (const Pocket& pocket : context.pockets) {
        if (!pocket.archived && (!account || accountOf(pocket) == *account)) {
            return pocket.id;
        }
    }
    return context.pockets.empty() ? Id() : context.pockets.front().id;
}

/// La categoria mas usada entre los movimientos de reparaciones de un tipo, o
/// `fallback` si nunca se anoto ninguna.
[[nodiscard]] std::string repairCategory(const CaptureContext& context, MovementKind kind,
                                         std::string_view fallback) {
    std::map<std::string, int> counts;
    for (const Movement& m : context.history) {
        if (!m.deleted && !m.jobId.empty() && m.kind == kind && !m.category.empty()) {
            ++counts[m.category];
        }
    }
    std::string best;
    int bestCount = 0;
    for (const auto& [name, count] : counts) {
        if (count > bestCount) {
            best = name;
            bestCount = count;
        }
    }
    if (!best.empty()) {
        return best;
    }
    if (const Category* existing = findCategory(context.categories, fallback)) {
        return existing->name;
    }
    return std::string(fallback);
}

} // namespace

// ----------------------------------------------------------------- Borrador

bool CaptureDraft::canSave() const noexcept {
    return amountMinor && *amountMinor > 0 && !pocketId.empty() &&
           (!salary || (!targetPocketId.empty() && targetPocketId != pocketId));
}

bool CaptureDraft::incomplete() const noexcept {
    return !salary && category.empty();
}

Movement CaptureDraft::toMovement() const {
    Movement m;
    m.date = date;
    m.amountMinor = amountMinor.value_or(0);
    m.pocketId = pocketId;
    m.jobId = jobId;
    if (salary) {
        m.kind = MovementKind::Traspaso;
        m.targetPocketId = targetPocketId;
        m.name = description.empty() ? "Sueldo" : description;
        m.jobId.clear();
        return m;
    }
    m.kind = kind;
    m.category = category;
    if (!description.empty()) {
        m.name = description;
    } else if (!category.empty()) {
        m.name = category;
    } else {
        m.name = "Sin descripcion";
    }
    return m;
}

// ---------------------------------------------------------------- Interprete

CaptureDraft parseCapture(std::string_view text, const CaptureContext& context) {
    CaptureDraft draft;
    draft.date = context.today;

    const std::vector<std::string> tokens = splitSpaces(text);
    if (tokens.empty()) {
        return draft;
    }

    // --- Monto: el primero, si no el ultimo, si no cualquiera -------------
    std::optional<std::size_t> amountIndex;
    std::optional<AmountToken> amount;
    auto tryAt = [&](std::size_t index) {
        if (amount) return;
        if (auto parsed = parseAmountToken(tokens[index], context.currency)) {
            amount = parsed;
            amountIndex = index;
        }
    };
    tryAt(0);
    tryAt(tokens.size() - 1);
    for (std::size_t i = 1; i + 1 < tokens.size() && !amount; ++i) {
        tryAt(i);
    }
    if (amount) {
        draft.amountMinor = amount->minor;
    }

    // --- Fecha, tipo y lo que queda como descripcion ----------------------
    bool incomeWord = amount && amount->plus;
    std::vector<std::string> rest;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (amountIndex && i == *amountIndex) {
            continue;
        }
        const std::string folded = fold(tokens[i]);
        if (const auto date = parseDateToken(folded, context.today)) {
            draft.date = *date;
            continue;
        }
        if (isIncomeWord(folded)) incomeWord = true;
        if (isSalaryWord(folded)) draft.salary = true;
        rest.push_back(tokens[i]);
    }
    for (std::size_t i = 0; i < rest.size(); ++i) {
        draft.description += (i == 0 ? "" : " ") + rest[i];
    }

    if (draft.salary) {
        draft.kind = MovementKind::Traspaso;
        draft.pocketId = lastPocket(context, Account::Negocio);
        for (const Pocket& pocket : context.pockets) {
            if (!pocket.archived && accountOf(pocket) == Account::Personal) {
                draft.targetPocketId = pocket.id;
                break;
            }
        }
        return draft;
    }

    draft.jobId = matchRepair(rest, context.openRepairs);

    // --- Categoria --------------------------------------------------------
    draft.category = learnedCategory(draft.description, context.history);
    if (!draft.category.empty()) {
        draft.categorySource = CategorySource::Historial;
    } else {
        for (const std::string& token : rest) {
            if (const Category* named = findCategory(context.categories, token)) {
                draft.category = named->name;
                draft.categorySource = CategorySource::Nombre;
                break;
            }
        }
    }

    // --- Tipo -------------------------------------------------------------
    const Category* known = findCategory(context.categories, draft.category);
    if (incomeWord || (known != nullptr && known->kind == MovementKind::Ingreso)) {
        draft.kind = MovementKind::Ingreso;
    }

    if (draft.category.empty() && !draft.jobId.empty()) {
        draft.category = draft.kind == MovementKind::Ingreso
                             ? repairCategory(context, MovementKind::Ingreso, "Reparaciones")
                             : repairCategory(context, MovementKind::Gasto, "Repuestos");
        draft.categorySource = CategorySource::Reparacion;
        known = findCategory(context.categories, draft.category);
    }

    // --- Bolsillo: la cuenta la da la categoria ---------------------------
    std::optional<Account> account;
    if (known != nullptr) {
        account = known->account;
    } else if (!draft.jobId.empty()) {
        account = Account::Negocio;
    }
    draft.pocketId = lastPocket(context, account);
    return draft;
}

Id suggestedPocket(const CaptureContext& context, std::optional<Account> account) {
    return lastPocket(context, account);
}

// ----------------------------------------------------------------- Aprender

std::string normalizeDescription(std::string_view text) {
    std::string out;
    bool space = false;
    for (const char c : fold(text)) {
        if (c >= 'a' && c <= 'z') {
            if (space && !out.empty()) out.push_back(' ');
            out.push_back(c);
            space = false;
        } else if (!std::isdigit(static_cast<unsigned char>(c))) {
            space = true;
        }
    }
    return out;
}

std::string learnedCategory(std::string_view description, const std::vector<Movement>& history) {
    const std::string key = normalizeDescription(description);
    if (key.empty()) {
        return {};
    }
    // Se cuenta por la forma plegada de la categoria para que "comida" y
    // "Comida" sumen juntas, pero se devuelve la ultima forma escrita.
    std::map<std::string, std::pair<int, std::string>> counts;
    for (const Movement& m : history) {
        if (m.deleted || m.category.empty() || normalizeDescription(m.name) != key) {
            continue;
        }
        auto& entry = counts[fold(m.category)];
        ++entry.first;
        entry.second = m.category;
    }
    std::string best;
    int bestCount = 0;
    for (const auto& [folded, entry] : counts) {
        if (entry.first > bestCount) {
            best = entry.second;
            bestCount = entry.first;
        }
    }
    return best;
}

std::vector<std::string> categoriesByUse(const std::vector<Category>& categories,
                                         const std::vector<Movement>& history,
                                         MovementKind kind, Date today) {
    const Date since = today.addDays(-90);
    std::vector<std::pair<std::string, int>> ranked;
    for (const Category& category : categories) {
        if (category.kind != kind) {
            continue;
        }
        const std::string folded = fold(category.name);
        int uses = 0;
        for (const Movement& m : history) {
            if (!m.deleted && m.date >= since && m.date <= today && fold(m.category) == folded) {
                ++uses;
            }
        }
        ranked.emplace_back(category.name, uses);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return fold(a.first) < fold(b.first);
    });
    std::vector<std::string> out;
    for (const auto& [name, uses] : ranked) {
        out.push_back(name);
    }
    return out;
}

} // namespace dake::core
