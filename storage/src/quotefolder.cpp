#include "dake/storage/quotefolder.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

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

QString defaultQuoteFolder() {
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
           QStringLiteral("/DakeLabs Cotizaciones");
}

core::QuoteDoc quoteFromJson(const QByteArray& json) {
    QJsonParseError error{};
    const QJsonDocument parsed = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
        throw std::runtime_error("no es un JSON valido: " + error.errorString().toStdString());
    }
    const QJsonObject o = parsed.object();

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
    doc.issued = date(o.value(QStringLiteral("fechaEmision")));
    doc.delivered = date(o.value(QStringLiteral("fechaEntrega")));
    doc.originId = text(o.value(QStringLiteral("origenId")));

    const QJsonObject client = o.value(QStringLiteral("clienteCongelado")).toObject();
    doc.client = text(client.value(QStringLiteral("nombre")));
    if (doc.client.empty()) {
        doc.client = text(o.value(QStringLiteral("clienteNombre")));
    }

    // Servicio: el equipo. Proyecto: su nombre, si tiene.
    const QJsonObject device = o.value(QStringLiteral("equipo")).toObject();
    doc.device = text(device.value(QStringLiteral("descripcion")));
    doc.received = date(device.value(QStringLiteral("fechaIngreso")));
    if (doc.device.empty()) {
        const QJsonObject project = o.value(QStringLiteral("proyecto")).toObject();
        doc.device = text(project.value(QStringLiteral("nombre")));
    }
    if (doc.device.empty()) {
        doc.device = text(o.value(QStringLiteral("titulo")));
    }

    // Los totales, con las mismas reglas que calcularTotales de Cotizaciones.
    std::int64_t subtotal = 0;
    for (const QJsonValue& categoryValue : o.value(QStringLiteral("categorias")).toArray()) {
        const QJsonObject category = categoryValue.toObject();
        const std::string section = text(category.value(QStringLiteral("nombre")));
        for (const QJsonValue& lineValue : category.value(QStringLiteral("lineas")).toArray()) {
            const QJsonObject line = lineValue.toObject();
            core::QuoteLine out;
            out.section = section;
            out.item = text(line.value(QStringLiteral("concepto")));
            out.totalMinor = core::quoteLineTotal(
                line.value(QStringLiteral("cantidad")).toDouble(),
                static_cast<std::int64_t>(line.value(QStringLiteral("valorUnitario")).toDouble()));
            subtotal += out.totalMinor;
            doc.lines.push_back(out);
        }
    }
    const QJsonObject discount = o.value(QStringLiteral("descuento")).toObject();
    const std::string discountKind = text(discount.value(QStringLiteral("tipo")));
    const auto discountValue =
        static_cast<std::int64_t>(discount.value(QStringLiteral("valor")).toDouble());
    doc.baseMinor = core::quoteBase(subtotal,
                                    discountKind == "porcentaje" ? core::DiscountKind::Porcentaje
                                    : discountKind == "monto"    ? core::DiscountKind::Monto
                                                                 : core::DiscountKind::Ninguno,
                                    discountValue);
    doc.depositMinor = static_cast<std::int64_t>(o.value(QStringLiteral("abono")).toDouble());

    // La fecha de pago es la del ultimo evento "Pagado" del historial.
    for (const QJsonValue& eventValue : o.value(QStringLiteral("historial")).toArray()) {
        const QJsonObject event = eventValue.toObject();
        if (QString::fromStdString(text(event.value(QStringLiteral("detalle"))))
                .startsWith(QStringLiteral("Pagado"))) {
            doc.paid = date(event.value(QStringLiteral("fecha")));
        }
    }
    return doc;
}

QuoteFolderRead readQuoteFolder(const QString& folder) {
    QuoteFolderRead read;
    const QDir documents(folder + QStringLiteral("/documentos"));
    read.folderFound = QDir(folder).exists() && documents.exists();
    if (!read.folderFound) {
        return read;
    }
    const QStringList files =
        documents.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        QFile file(documents.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            read.errors << name + QStringLiteral(": no se pudo abrir");
            continue;
        }
        try {
            read.docs.push_back(quoteFromJson(file.readAll()));
        } catch (const std::exception& error) {
            read.errors << name + QStringLiteral(": ") + QString::fromUtf8(error.what());
        }
    }
    return read;
}

} // namespace dake::storage
