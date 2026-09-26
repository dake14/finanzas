#pragma once
//
// dake/core/bankcsv.hpp — importar un extracto del banco en CSV.
//
// Cada banco escribe su CSV distinto: separador, orden de columnas, formato de
// fecha, signo de los gastos. La primera vez se dice que columna es cual y se
// guarda como perfil del banco; despues se importa sin preguntar.
//
// NUNCA DOS VECES. Cada fila lleva una huella (fecha, tipo, monto,
// descripcion y cuantas iguales van antes en el mismo archivo: dos cafes
// iguales el mismo dia son dos cafes). Importar el mismo extracto otra vez no
// agrega nada. Y lo que ya se anoto a mano —mismo tipo, mismo monto, hasta dos
// dias de diferencia— se enlaza en vez de duplicarse.
//
// La categoria se sugiere con lo aprendido: lo importado entra con el nombre
// que le puso el banco, asi que la segunda vez que aparece "UBER *TRIP" ya se
// sabe que es Transporte.
//
// Dependencias permitidas: <optional> <string> <string_view> <vector>
//                          "model.hpp" "money.hpp" "fixed.hpp"
//
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/fixed.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

/// ';', ',' o tabulador: el que mas aparece en la primera linea, fuera de
/// comillas.
[[nodiscard]] char detectSeparator(std::string_view text);

/// Filas y columnas, con comillas dobles como las escribe Excel ("a;b" es un
/// solo campo, "" es una comilla). Las lineas vacias se saltean.
[[nodiscard]] std::vector<std::vector<std::string>> parseCsv(std::string_view text, char separator);

enum class DateFormat { DiaMesAnio, AnioMesDia, MesDiaAnio };

struct BankProfile {
    std::string name;
    int dateColumn = 0;
    int descriptionColumn = 1;
    int amountColumn = 2;   ///< una sola columna con signo; -1 si hay dos
    int debitColumn = -1;   ///< cuando el banco separa cargos y abonos
    int creditColumn = -1;
    bool negativeIsExpense = true;
    DateFormat dateFormat = DateFormat::DiaMesAnio;
    int headerRows = 1;
};

struct BankRow {
    Date date;
    std::string description;
    std::int64_t amountMinor = 0;  ///< siempre positivo
    MovementKind kind = MovementKind::Gasto;
    std::string fingerprint;
};

struct BankRead {
    std::vector<BankRow> rows;
    std::vector<std::string> errors;  ///< "fila 7: la fecha '31/02/2026' no existe"
};

[[nodiscard]] BankRead readBankRows(const std::vector<std::vector<std::string>>& csv,
                                    const BankProfile& profile, Currency currency);

enum class BankStatus {
    Nuevo,        ///< se importa
    YaImportado,  ///< su huella ya esta: no hace nada
    YaAnotado     ///< coincide con uno anotado a mano: se enlaza
};

struct BankMatch {
    BankRow row;
    BankStatus status = BankStatus::Nuevo;
    Id matchedMovementId;
    std::string suggestedCategory;
};

/// Que hacer con cada fila, contra lo que ya hay.
[[nodiscard]] std::vector<BankMatch> matchBankRows(const std::vector<BankRow>& rows,
                                                   const std::vector<Movement>& movements,
                                                   const std::vector<MovementMeta>& metas);

} // namespace dake::core
