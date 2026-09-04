#include "dake/storage/exchange.hpp"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <unordered_map>

namespace dake::storage {
namespace {

constexpr const char* kFormat = "dake-pruebas-1";

[[nodiscard]] QString qs(const std::string& text) {
    return QString::fromStdString(text);
}

[[nodiscard]] std::string ss(const QJsonValue& value) {
    return value.toString().toStdString();
}

[[nodiscard]] QString line(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// ------------------------------------------------------------- a JSON ------

[[nodiscard]] QJsonObject toJson(const core::Pocket& pocket) {
    const auto kind = core::toString(pocket.kind);
    return QJsonObject{
        {"t", "pocket"},
        {"id", qs(pocket.id)},
        {"name", qs(pocket.name)},
        {"kind", QString::fromUtf8(kind.data(), static_cast<int>(kind.size()))},
        // El importe va como numero JSON, no como texto: cabe exacto en un
        // double hasta 2^53 unidades minimas, que son 90 mil millones de
        // dolares. Mas alla de eso el problema no seria el formato.
        {"opening_minor", static_cast<double>(pocket.openingMinor)},
        {"archived", pocket.archived},
        {"hlc", qs(pocket.hlc)},
        {"device_id", qs(pocket.deviceId)},
        {"deleted", pocket.deleted},
    };
}

[[nodiscard]] QJsonObject toJson(const core::Job& job) {
    return QJsonObject{
        {"t", "job"},
        {"id", qs(job.id)},
        {"name", qs(job.name)},
        {"client", qs(job.client)},
        {"opened", qs(job.opened.toIso())},
        {"closed", job.closed},
        {"hlc", qs(job.hlc)},
        {"device_id", qs(job.deviceId)},
        {"deleted", job.deleted},
    };
}

[[nodiscard]] QJsonObject toJson(const core::Movement& movement) {
    const auto kind = core::toString(movement.kind);
    const auto recurrence = core::toString(movement.recurrence);
    return QJsonObject{
        {"t", "movement"},
        {"id", qs(movement.id)},
        {"date", qs(movement.date.toIso())},
        {"name", qs(movement.name)},
        {"kind", QString::fromUtf8(kind.data(), static_cast<int>(kind.size()))},
        {"amount_minor", static_cast<double>(movement.amountMinor)},
        {"pocket_id", qs(movement.pocketId)},
        {"target_pocket_id", qs(movement.targetPocketId)},
        {"category", qs(movement.category)},
        {"job_id", qs(movement.jobId)},
        {"spread_months", movement.spreadMonths},
        {"settled", movement.settled},
        {"recurrence", QString::fromUtf8(recurrence.data(), static_cast<int>(recurrence.size()))},
        {"hlc", qs(movement.hlc)},
        {"device_id", qs(movement.deviceId)},
        {"deleted", movement.deleted},
    };
}

// ----------------------------------------------------------- desde JSON ----

[[nodiscard]] core::Pocket pocketFrom(const QJsonObject& object) {
    core::Pocket pocket;
    pocket.id = ss(object.value("id"));
    pocket.name = ss(object.value("name"));
    pocket.kind = core::pocketKindFromString(ss(object.value("kind")));
    pocket.openingMinor = static_cast<std::int64_t>(object.value("opening_minor").toDouble());
    pocket.archived = object.value("archived").toBool();
    pocket.hlc = ss(object.value("hlc"));
    pocket.deviceId = ss(object.value("device_id"));
    pocket.deleted = object.value("deleted").toBool();
    return pocket;
}

[[nodiscard]] core::Job jobFrom(const QJsonObject& object) {
    core::Job job;
    job.id = ss(object.value("id"));
    job.name = ss(object.value("name"));
    job.client = ss(object.value("client"));
    job.opened = core::Date::fromIso(ss(object.value("opened")));
    job.closed = object.value("closed").toBool();
    job.hlc = ss(object.value("hlc"));
    job.deviceId = ss(object.value("device_id"));
    job.deleted = object.value("deleted").toBool();
    return job;
}

[[nodiscard]] core::Movement movementFrom(const QJsonObject& object) {
    core::Movement movement;
    movement.id = ss(object.value("id"));
    movement.date = core::Date::fromIso(ss(object.value("date")));
    movement.name = ss(object.value("name"));
    movement.kind = core::movementKindFromString(ss(object.value("kind")));
    movement.amountMinor = static_cast<std::int64_t>(object.value("amount_minor").toDouble());
    movement.pocketId = ss(object.value("pocket_id"));
    movement.targetPocketId = ss(object.value("target_pocket_id"));
    movement.category = ss(object.value("category"));
    movement.jobId = ss(object.value("job_id"));
    movement.spreadMonths = object.value("spread_months").toInt(1);
    movement.settled = object.value("settled").toBool(true);
    movement.recurrence = core::recurrenceFromString(ss(object.value("recurrence")));
    movement.hlc = ss(object.value("hlc"));
    movement.deviceId = ss(object.value("device_id"));
    movement.deleted = object.value("deleted").toBool();
    return movement;
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
        out << line(toJson(pocket)) << "\n";
        ++written;
    }
    for (const core::Job& job : repository.loadJobs(/*includeDeleted=*/true)) {
        out << line(toJson(job)) << "\n";
        ++written;
    }
    for (const core::Movement& movement : repository.loadMovements(/*includeDeleted=*/true)) {
        out << line(toJson(movement)) << "\n";
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
                repository.save(pocketFrom(object));
            } else if (type == QLatin1String("job")) {
                repository.save(jobFrom(object));
            } else if (type == QLatin1String("movement")) {
                repository.save(movementFrom(object));
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
