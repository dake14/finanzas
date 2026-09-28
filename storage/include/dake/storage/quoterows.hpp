#pragma once
//
// dake/storage/quoterows.hpp — los documentos de DakeLabs Cotizaciones, desde
// lo que bajo de v2_quotes.
//
// Solo lectura: Finanzas nunca escribe cotizaciones. Una fila que no se puede
// leer se anota como error y se saltea: un documento roto no puede frenar a
// los demas.
//
#include <QJsonObject>
#include <QStringList>

#include <vector>

#include "dake/core/quotes.hpp"
#include "dake/storage/repository.hpp"

namespace dake::storage {

struct QuoteRowsRead {
    std::vector<core::QuoteDoc> docs;
    QStringList errors;  ///< "<id>: <por que>"
};

/// Un documento desde su fila. Lanza std::runtime_error si no tiene id. Lo que
/// falte o venga nulo se lee vacio.
[[nodiscard]] core::QuoteDoc quoteFromRow(const QJsonObject& row);

[[nodiscard]] QuoteRowsRead readQuoteRows(Repository& repository);

} // namespace dake::storage
