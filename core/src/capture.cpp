#include "dake/core/capture.hpp"

#include <algorithm>
#include <cctype>
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

// ------------------------------------------------------------- Bolsillo

[[nodiscard]] const Pocket* findPocket(const std::vector<Pocket>& pockets, const Id& id) {
    const auto it = std::find_if(pockets.begin(), pockets.end(),
                                 [&id](const Pocket& p) { return p.id == id; });
    return it == pockets.end() ? nullptr : &*it;
}

/// El ultimo bolsillo usado, de una cuenta o de cualquiera. Ultimo por fecha y,
/// dentro del mismo dia, por id: los ids nacen ordenados por tiempo.
[[nodiscard]] Id lastPocket(const std::vector<Pocket>& pockets,
                           const std::vector<Movement>& history,
                           std::optional<Account> account) {
    const Movement* latest = nullptr;
    for (const Movement& m : history) {
        if (m.deleted || m.kind == MovementKind::Traspaso) {
            continue;
        }
        const Pocket* pocket = findPocket(pockets, m.pocketId);
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
    for (const Pocket& pocket : pockets) {
        if (!pocket.archived && (!account || accountOf(pocket) == *account)) {
            return pocket.id;
        }
    }
    return pockets.empty() ? Id() : pockets.front().id;
}

} // namespace

std::optional<std::int64_t> parseAmount(std::string_view text, Currency currency) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    // El signo lo pone el tipo que se elige en el formulario, no el texto.
    if (text.empty() || text.front() == '+') {
        return std::nullopt;
    }
    const auto token = parseAmountToken(text, currency);
    if (!token) {
        return std::nullopt;
    }
    return token->minor;
}

Id suggestedPocket(const std::vector<Pocket>& pockets, const std::vector<Movement>& history,
                   std::optional<Account> account) {
    return lastPocket(pockets, history, account);
}

std::string foldText(std::string_view text) {
    return fold(text);
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
