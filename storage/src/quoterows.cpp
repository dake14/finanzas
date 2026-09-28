#include "dake/storage/quoterows.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include <stdexcept>

namespace dake::storage {
namespace {

[[nodiscard]] std::string text(const QJsonValue& value) {
    return value.isString() ? value.toString().toStdString() : std::string();
}

/// Una fecha "aaaa-mm-dd", o nada si no hay o no se entiende.
[[nodiscard]] std::optional<core::Date> date(const QJsonValue& value) {
    const std::string iso = text(value);
    if (iso.size() < 10) {
        return std::nullopt;
    }
    try {
        return core::Date::fromIso(iso.substr(0, 10));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

core::QuoteDoc quoteFromRow(const QJsonObject& o) {
    core::QuoteDoc doc;
    doc.id = text(o.value(QStringLiteral("id")));
    if (doc.id.empty()) {
        throw std::runtime_error("no tiene id");
    }
    doc.number = text(o.value(QStringLiteral("numero")));
    if (doc.number.empty()) {
        doc.number = doc.id;  // un borrador todavia no tiene numero
    }
    doc.kind = text(o.value(QStringLiteral("tipo"))) == "cotizacion" ? core::QuoteKind::Cotizacion
                                                                      : core::QuoteKind::Informe;
    doc.status = text(o.value(QStringLiteral("estado")));
    doc.client = text(o.value(QStringLiteral("cliente")));
    doc.device = text(o.value(QStringLiteral("equipo")));
    doc.originId = text(o.value(QStringLiteral("origen_id")));
    doc.issued = date(o.value(QStringLiteral("fecha_emision")));
    doc.received = date(o.value(QStringLiteral("fecha_ingreso")));
    doc.delivered = date(o.value(QStringLiteral("fecha_entrega")));
    doc.paid = date(o.value(QStringLiteral("fecha_pago")));

    // Los totales, con las mismas reglas que calcularTotales de Cotizaciones.
    std::int64_t subtotal = 0;
    for (const QJsonValue& lineValue : o.value(QStringLiteral("lineas")).toArray()) {
        const QJsonObject line = lineValue.toObject();
        core::QuoteLine out;
        out.section = text(line.value(QStringLiteral("seccion")));
        out.item = text(line.value(QStringLiteral("concepto")));
        out.totalMinor = core::quoteLineTotal(
            line.value(QStringLiteral("cantidad")).toDouble(),
            static_cast<std::int64_t>(line.value(QStringLiteral("valorUnitario")).toDouble()));
        subtotal += out.totalMinor;
        doc.lines.push_back(out);
    }
    const std::string discountKind = text(o.value(QStringLiteral("descuento_tipo")));
    const auto discountValue =
        static_cast<std::int64_t>(o.value(QStringLiteral("descuento_valor")).toDouble());
    doc.baseMinor = core::quoteBase(subtotal,
                                    discountKind == "porcentaje" ? core::DiscountKind::Porcentaje
                                    : discountKind == "monto"    ? core::DiscountKind::Monto
                                                                 : core::DiscountKind::Ninguno,
                                    discountValue);
    doc.depositMinor = static_cast<std::int64_t>(o.value(QStringLiteral("abono_minor")).toDouble());
    return doc;
}

QuoteRowsRead readQuoteRows(Repository& repository) {
    QuoteRowsRead read;
    for (const QString& json : repository.loadQuoteRows()) {
        const QJsonObject row = QJsonDocument::fromJson(json.toUtf8()).object();
        try {
            read.docs.push_back(quoteFromRow(row));
        } catch (const std::exception& error) {
            read.errors << row.value(QStringLiteral("id")).toString() + QStringLiteral(": ") +
                               QString::fromUtf8(error.what());
        }
    }
    return read;
}

} // namespace dake::storage
