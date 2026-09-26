#pragma once
//
// dake/storage/quotefolder.hpp — leer la carpeta de DakeLabs Cotizaciones.
//
// Solo lectura: Finanzas nunca escribe ahi. Cotizaciones guarda un JSON por
// documento en documentos\ y los escribe con renombrado atomico, asi que un
// archivo se lee entero o no se lee. Uno que no se puede leer se anota como
// error y se saltea: un documento roto no puede frenar a los demas.
//
#include <QString>
#include <QStringList>

#include <vector>

#include "dake/core/quotes.hpp"

namespace dake::storage {

struct QuoteFolderRead {
    std::vector<core::QuoteDoc> docs;
    QStringList errors;  ///< "INF-2026-009.json: <por que>"
    bool folderFound = false;
};

/// Documentos\DakeLabs Cotizaciones, que es donde la guarda Cotizaciones por
/// defecto.
[[nodiscard]] QString defaultQuoteFolder();

[[nodiscard]] QuoteFolderRead readQuoteFolder(const QString& folder);

/// Un documento desde su JSON. Lanza std::runtime_error si no tiene la forma
/// esperada.
[[nodiscard]] core::QuoteDoc quoteFromJson(const QByteArray& json);

} // namespace dake::storage
