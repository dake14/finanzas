#include "dake/core/bankcsv.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>

#include "dake/core/capture.hpp"

namespace dake::core {
namespace {

[[nodiscard]] std::string trim(std::string_view text) {
    std::size_t a = 0;
    std::size_t b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    return std::string(text.substr(a, b - a));
}

[[nodiscard]] std::optional<Date> parseDate(const std::string& raw, DateFormat format) {
    std::vector<int> parts;
    std::string current;
    for (const char c : raw) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            current.push_back(c);
        } else if (!current.empty()) {
            parts.push_back(std::stoi(current));
            current.clear();
        }
        if (c == ' ' && parts.size() == 3) break;  // "01/09/2026 00:00"
    }
    if (!current.empty() && parts.size() < 3) parts.push_back(std::stoi(current));
    if (parts.size() < 3) return std::nullopt;
    int day = 0;
    int month = 0;
    int year = 0;
    switch (format) {
        case DateFormat::DiaMesAnio: day = parts[0]; month = parts[1]; year = parts[2]; break;
        case DateFormat::AnioMesDia: year = parts[0]; month = parts[1]; day = parts[2]; break;
        case DateFormat::MesDiaAnio: month = parts[0]; day = parts[1]; year = parts[2]; break;
    }
    if (year < 100) year += 2000;
    try {
        return Date::fromYmd(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

/// Un monto como lo escriben los bancos: "-2,50", "1.250,00", "1,250.00",
/// "(25.00)", "$ 25", "25-". Con signo, en centavos. Vacio si no se entiende.
[[nodiscard]] std::optional<std::int64_t> parseAmount(const std::string& raw, Currency currency) {
    std::string text;
    bool negative = false;
    for (const char c : raw) {
        if (c == '-' || c == '(') negative = true;
        else if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ',') text.push_back(c);
    }
    if (text.empty()) return std::nullopt;
    const auto lastDot = text.rfind('.');
    const auto lastComma = text.rfind(',');
    std::string normalized;
    if (lastDot != std::string::npos && lastComma != std::string::npos) {
        // Los dos: el ultimo es el decimal.
        const char decimal = lastDot > lastComma ? '.' : ',';
        for (const char c : text) {
            if (c == decimal) normalized.push_back('.');
            else if (std::isdigit(static_cast<unsigned char>(c))) normalized.push_back(c);
        }
    } else if (lastDot != std::string::npos || lastComma != std::string::npos) {
        // Uno solo: con dos cifras despues es decimal; con tres, miles.
        const char sep = lastDot != std::string::npos ? '.' : ',';
        const std::size_t pos = text.rfind(sep);
        const std::size_t after = text.size() - pos - 1;
        const bool thousands = after == 3;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == sep) {
                if (!thousands && i == pos) normalized.push_back('.');
            } else {
                normalized.push_back(text[i]);
            }
        }
    } else {
        normalized = text;
    }
    try {
        const std::int64_t minor = Money::parse(normalized, currency).minor();
        return negative ? -minor : minor;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

[[nodiscard]] const std::string& cell(const std::vector<std::string>& row, int index) {
    static const std::string empty;
    return index >= 0 && index < static_cast<int>(row.size()) ? row[static_cast<std::size_t>(index)] : empty;
}

} // namespace

char detectSeparator(std::string_view text) {
    int semicolons = 0;
    int commas = 0;
    int tabs = 0;
    bool quoted = false;
    for (const char c : text) {
        if (c == '\n') break;
        if (c == '"') quoted = !quoted;
        if (quoted) continue;
        if (c == ';') ++semicolons;
        if (c == ',') ++commas;
        if (c == '\t') ++tabs;
    }
    if (tabs > semicolons && tabs > commas) return '\t';
    if (semicolons >= commas && semicolons > 0) return ';';
    return ',';
}

std::vector<std::vector<std::string>> parseCsv(std::string_view text, char separator) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false;
    bool any = false;  // la fila tiene algo, aunque sea un campo vacio con separador
    auto endRow = [&] {
        if (any || !field.empty()) {
            row.push_back(field);
            rows.push_back(row);
        }
        row.clear();
        field.clear();
        any = false;
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            quoted = true;
            any = true;
        } else if (c == separator) {
            row.push_back(field);
            field.clear();
            any = true;
        } else if (c == '\n') {
            endRow();
        } else if (c != '\r') {
            field.push_back(c);
        }
    }
    endRow();
    return rows;
}

BankRead readBankRows(const std::vector<std::vector<std::string>>& csv, const BankProfile& profile,
                      Currency currency) {
    BankRead read;
    std::map<std::string, int> seen;
    for (std::size_t i = static_cast<std::size_t>(std::max(0, profile.headerRows)); i < csv.size(); ++i) {
        const auto& row = csv[i];
        const std::string line = "fila " + std::to_string(i + 1) + ": ";
        const auto date = parseDate(trim(cell(row, profile.dateColumn)), profile.dateFormat);
        if (!date) {
            read.errors.push_back(line + "la fecha '" + trim(cell(row, profile.dateColumn)) + "' no se entiende");
            continue;
        }
        BankRow out;
        out.date = *date;
        out.description = trim(cell(row, profile.descriptionColumn));

        std::optional<std::int64_t> amount;
        if (profile.amountColumn >= 0) {
            amount = parseAmount(cell(row, profile.amountColumn), currency);
            if (amount) {
                const bool expense = (*amount < 0) == profile.negativeIsExpense;
                out.kind = expense ? MovementKind::Gasto : MovementKind::Ingreso;
                out.amountMinor = *amount < 0 ? -*amount : *amount;
            }
        } else {
            const auto debit = parseAmount(cell(row, profile.debitColumn), currency);
            const auto credit = parseAmount(cell(row, profile.creditColumn), currency);
            if (debit && *debit != 0) {
                amount = debit;
                out.kind = MovementKind::Gasto;
                out.amountMinor = *debit < 0 ? -*debit : *debit;
            } else if (credit && *credit != 0) {
                amount = credit;
                out.kind = MovementKind::Ingreso;
                out.amountMinor = *credit < 0 ? -*credit : *credit;
            }
        }
        if (!amount || out.amountMinor == 0) {
            read.errors.push_back(line + "sin monto");
            continue;
        }

        // La huella: lo que identifica el renglon, y cuantos iguales van
        // antes en el mismo archivo.
        const std::string key = "bank:" + out.date.toIso() + "|" + std::string(toString(out.kind)) + "|" +
                                std::to_string(out.amountMinor) + "|" + foldText(out.description);
        out.fingerprint = key + "#" + std::to_string(seen[key]++);
        read.rows.push_back(out);
    }
    return read;
}

std::vector<BankMatch> matchBankRows(const std::vector<BankRow>& rows, const std::vector<Movement>& movements,
                                     const std::vector<MovementMeta>& metas) {
    std::set<std::string> imported;
    std::set<Id> linked;
    for (const MovementMeta& m : metas) {
        if (!m.externalRef.empty()) {
            imported.insert(m.externalRef);
            linked.insert(m.movementId);
        }
    }

    std::vector<BankMatch> out;
    for (const BankRow& row : rows) {
        BankMatch match;
        match.row = row;
        if (imported.contains(row.fingerprint)) {
            match.status = BankStatus::YaImportado;
            out.push_back(match);
            continue;
        }
        for (const Movement& m : movements) {
            if (m.deleted || m.kind != row.kind || m.amountMinor != row.amountMinor || linked.contains(m.id)) continue;
            const std::int64_t days = m.date.toEpochDays() - row.date.toEpochDays();
            if (days < -2 || days > 2) continue;
            match.status = BankStatus::YaAnotado;
            match.matchedMovementId = m.id;
            linked.insert(m.id);
            break;
        }
        if (match.status == BankStatus::Nuevo) {
            match.suggestedCategory = learnedCategory(row.description, movements);
        }
        out.push_back(match);
    }
    return out;
}

} // namespace dake::core
