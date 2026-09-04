#include "dake/storage/exchange.hpp"
#include "dake/storage/wire.hpp"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <unordered_map>

namespace dake::storage {
namespace {

constexpr const char* kFormat = "dake-pruebas-1";

[[nodiscard]] std::string ss(const QJsonValue& value) {
    return value.toString().toStdString();
}

[[nodiscard]] QString line(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

/// El HLC viene codificado con ancho fijo, asi que comparar el texto da el
/// mismo orden que comparar el reloj. Un registro sin HLC se trata como el mas
/// viejo posible: lo pisa cualquier cosa que si lo tenga.
[[nodiscard]] bool isNewer(const std::string& incoming, const std::string& existing) {
    return incoming > existing;
}

} // namespace

int exportAll(Repository& repository, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        throw StorageError(QStringLiteral("No se pudo escribir en '") + path +
                           QStringLiteral("': ") + file.errorString());
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << line(QJsonObject{
               {"t", "meta"},
               {"format", kFormat},
               {"exported", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
           })
        << "\n";

    int written = 0;
    for (const core::Pocket& pocket : repository.loadPockets(/*includeDeleted=*/true)) {
        out << line(dake::storage::toJson(pocket)) << "\n";
        ++written;
    }
    for (const core::Job& job : repository.loadJobs(/*includeDeleted=*/true)) {
        out << line(dake::storage::toJson(job)) << "\n";
        ++written;
    }
    for (const core::Movement& movement : repository.loadMovements(/*includeDeleted=*/true)) {
        out << line(dake::storage::toJson(movement)) << "\n";
        ++written;
    }

    out.flush();
    file.close();
    return written;
}

ExchangeReport importFile(Repository& repository, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw StorageError(QStringLiteral("No se pudo leer '") + path + QStringLiteral("': ") +
                           file.errorString());
    }

    // Los relojes de lo que ya hay, en memoria: preguntarle a SQLite una vez
    // por registro convertiria un archivo de mil lineas en mil consultas.
    std::unordered_map<std::string, std::string> known;
    for (const core::Pocket& pocket : repository.loadPockets(true)) {
        known.emplace(pocket.id, pocket.hlc);
    }
    for (const core::Job& job : repository.loadJobs(true)) {
        known.emplace(job.id, job.hlc);
    }
    for (const core::Movement& movement : repository.loadMovements(true)) {
        known.emplace(movement.id, movement.hlc);
    }

    ExchangeReport report;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        const QString raw = in.readLine().trimmed();
        if (raw.isEmpty()) {
            continue;
        }

        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(raw.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            ++report.malformed;
            continue;
        }

        const QJsonObject object = document.object();
        const QString type = object.value("t").toString();
        if (type == QLatin1String("meta")) {
            continue;
        }

        ++report.total;
        const std::string id = ss(object.value("id"));
        if (id.empty()) {
            ++report.malformed;
            continue;
        }

        const std::string incomingHlc = ss(object.value("hlc"));
        const auto existing = known.find(id);
        if (existing != known.end() && !isNewer(incomingHlc, existing->second)) {
            // Ya tenemos una version igual o mas nueva. Esto es lo que hace que
            // importar dos veces el mismo archivo no cambie nada.
            ++report.skippedOlder;
            continue;
        }

        try {
            if (type == QLatin1String("pocket")) {
                repository.save(dake::storage::pocketFrom(object));
            } else if (type == QLatin1String("job")) {
                repository.save(dake::storage::jobFrom(object));
            } else if (type == QLatin1String("movement")) {
                repository.save(dake::storage::movementFrom(object));
            } else {
                ++report.malformed;
                continue;
            }
        } catch (const std::exception&) {
            // Una linea rota no puede llevarse puesto el archivo entero: se
            // cuenta y se sigue con las demas.
            ++report.malformed;
            continue;
        }

        known[id] = incomingHlc;
        ++report.applied;
    }

    return report;
}

} // namespace dake::storage
