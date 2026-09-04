#include "dake/storage/repository.hpp"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "dake/core/demo.hpp"
#include "dake/core/uuid.hpp"

namespace dake::storage {
namespace {

[[nodiscard]] QString describe(const QSqlQuery& query) {
    return query.lastError().text() + QStringLiteral(" [") + query.lastQuery() +
           QStringLiteral("]");
}

void run(QSqlQuery& query) {
    if (!query.exec()) {
        throw StorageError(QStringLiteral("Fallo la consulta: ") + describe(query));
    }
}

[[nodiscard]] QString qs(const std::string& text) {
    return QString::fromStdString(text);
}

[[nodiscard]] std::string ss(const QVariant& value) {
    return value.toString().toStdString();
}

} // namespace

core::Id newId() {
    static core::SystemUuidGenerator generator;
    return generator.next();
}

QString deviceId(Repository& repository) {
    if (const auto stored = repository.setting(QStringLiteral("device_id"))) {
        return *stored;
    }
    const QString fresh = qs(newId());
    repository.setSetting(QStringLiteral("device_id"), fresh);
    return fresh;
}

Repository::Repository(Database& db) : db_(db) {}

// ------------------------------------------------------------------ Lecturas

std::vector<core::Pocket> Repository::loadPockets(bool includeDeleted) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, name, kind, opening_minor, archived, hlc, device_id, deleted "
        "FROM pockets WHERE (deleted = 0 OR ?) ORDER BY rowid"));
    query.addBindValue(includeDeleted ? 1 : 0);
    run(query);

    std::vector<core::Pocket> out;
    while (query.next()) {
        core::Pocket pocket;
        pocket.id = ss(query.value(0));
        pocket.name = ss(query.value(1));
        pocket.kind = core::pocketKindFromString(ss(query.value(2)));
        pocket.openingMinor = query.value(3).toLongLong();
        pocket.archived = query.value(4).toInt() != 0;
        pocket.hlc = ss(query.value(5));
        pocket.deviceId = ss(query.value(6));
        pocket.deleted = query.value(7).toInt() != 0;
        out.push_back(std::move(pocket));
    }
    return out;
}

std::vector<core::Job> Repository::loadJobs(bool includeDeleted) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, name, client, opened, closed, hlc, device_id, deleted "
        "FROM jobs WHERE (deleted = 0 OR ?) ORDER BY opened DESC, id DESC"));
    query.addBindValue(includeDeleted ? 1 : 0);
    run(query);

    std::vector<core::Job> out;
    while (query.next()) {
        core::Job job;
        job.id = ss(query.value(0));
        job.name = ss(query.value(1));
        job.client = ss(query.value(2));
        job.opened = core::Date::fromIso(ss(query.value(3)));
        job.closed = query.value(4).toInt() != 0;
        job.hlc = ss(query.value(5));
        job.deviceId = ss(query.value(6));
        job.deleted = query.value(7).toInt() != 0;
        out.push_back(std::move(job));
    }
    return out;
}

std::vector<core::Movement> Repository::loadMovements(bool includeDeleted) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, date, name, kind, amount_minor, pocket_id, target_pocket_id, category, "
        "job_id, spread_months, settled, recurrence, hlc, device_id, deleted "
        "FROM movements WHERE (deleted = 0 OR ?) ORDER BY date ASC, id ASC"));
    query.addBindValue(includeDeleted ? 1 : 0);
    run(query);

    std::vector<core::Movement> out;
    while (query.next()) {
        core::Movement movement;
        movement.id = ss(query.value(0));
        movement.date = core::Date::fromIso(ss(query.value(1)));
        movement.name = ss(query.value(2));
        movement.kind = core::movementKindFromString(ss(query.value(3)));
        movement.amountMinor = query.value(4).toLongLong();
        movement.pocketId = ss(query.value(5));
        movement.targetPocketId = ss(query.value(6));
        movement.category = ss(query.value(7));
        movement.jobId = ss(query.value(8));
        movement.spreadMonths = query.value(9).toInt();
        movement.settled = query.value(10).toInt() != 0;
        movement.recurrence = core::recurrenceFromString(ss(query.value(11)));
        movement.hlc = ss(query.value(12));
        movement.deviceId = ss(query.value(13));
        movement.deleted = query.value(14).toInt() != 0;
        out.push_back(std::move(movement));
    }
    return out;
}

// ----------------------------------------------------------------- Escrituras

void Repository::save(const core::Pocket& pocket) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO pockets "
        "(id, name, kind, opening_minor, archived, hlc, device_id, deleted) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(pocket.id));
    query.addBindValue(qs(pocket.name));
    query.addBindValue(QString::fromUtf8(core::toString(pocket.kind).data(),
                                         static_cast<int>(core::toString(pocket.kind).size())));
    query.addBindValue(static_cast<qlonglong>(pocket.openingMinor));
    query.addBindValue(pocket.archived ? 1 : 0);
    query.addBindValue(qs(pocket.hlc));
    query.addBindValue(qs(pocket.deviceId));
    query.addBindValue(pocket.deleted ? 1 : 0);
    run(query);
}

void Repository::save(const core::Job& job) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO jobs (id, name, client, opened, closed, hlc, device_id, deleted) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(job.id));
    query.addBindValue(qs(job.name));
    query.addBindValue(qs(job.client));
    query.addBindValue(qs(job.opened.toIso()));
    query.addBindValue(job.closed ? 1 : 0);
    query.addBindValue(qs(job.hlc));
    query.addBindValue(qs(job.deviceId));
    query.addBindValue(job.deleted ? 1 : 0);
    run(query);
}

void Repository::save(const core::Movement& movement) {
    // La forma se verifica ANTES de escribir. Una fila con un traspaso sin
    // destino, o un importe en cero, no se puede arreglar despues mirandola:
    // ya no se sabe que quiso decir.
    if (!movement.isWellFormed()) {
        throw StorageError(QStringLiteral("El movimiento '") + qs(movement.name) +
                           QStringLiteral("' no tiene la forma que su tipo exige."));
    }

    const auto kindText = core::toString(movement.kind);
    const auto recurrenceText = core::toString(movement.recurrence);

    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO movements "
        "(id, date, name, kind, amount_minor, pocket_id, target_pocket_id, category, job_id, "
        " spread_months, settled, recurrence, hlc, device_id, deleted) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(movement.id));
    query.addBindValue(qs(movement.date.toIso()));
    query.addBindValue(qs(movement.name));
    query.addBindValue(QString::fromUtf8(kindText.data(), static_cast<int>(kindText.size())));
    query.addBindValue(static_cast<qlonglong>(movement.amountMinor));
    query.addBindValue(qs(movement.pocketId));
    query.addBindValue(qs(movement.targetPocketId));
    query.addBindValue(qs(movement.category));
    query.addBindValue(qs(movement.jobId));
    query.addBindValue(movement.spreadMonths);
    query.addBindValue(movement.settled ? 1 : 0);
    query.addBindValue(
        QString::fromUtf8(recurrenceText.data(), static_cast<int>(recurrenceText.size())));
    query.addBindValue(qs(movement.hlc));
    query.addBindValue(qs(movement.deviceId));
    query.addBindValue(movement.deleted ? 1 : 0);
    run(query);
}

namespace {

void tombstone(Database& db, const QString& table, const std::string& id) {
    QSqlQuery query(db.handle());
    query.prepare(QStringLiteral("UPDATE %1 SET deleted = 1 WHERE id = ?").arg(table));
    query.addBindValue(qs(id));
    run(query);
}

} // namespace

void Repository::remove(const core::Movement& movement) {
    tombstone(db_, QStringLiteral("movements"), movement.id);
}

void Repository::remove(const core::Job& job) {
    tombstone(db_, QStringLiteral("jobs"), job.id);
}

void Repository::remove(const core::Pocket& pocket) {
    tombstone(db_, QStringLiteral("pockets"), pocket.id);
}

// ------------------------------------------------------------------- Ajustes

std::optional<QString> Repository::setting(const QString& key) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    run(query);
    if (!query.next()) {
        return std::nullopt;
    }
    return query.value(0).toString();
}

void Repository::setSetting(const QString& key, const QString& value) {
    QSqlQuery query(db_.handle());
    query.prepare(
        QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    run(query);
}

// -------------------------------------------------------------------- Siembra

bool Repository::isEmpty() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM movements"));
    run(query);
    return query.next() && query.value(0).toLongLong() == 0;
}

bool Repository::seedIfEmpty(core::Currency currency) {
    if (!isEmpty()) {
        return false;
    }

    const core::DemoData data = core::realCaseAugust2026(currency);
    if (!db_.handle().transaction()) {
        throw StorageError(QStringLiteral("No se pudo abrir la transaccion de siembra."));
    }
    try {
        for (const core::Pocket& pocket : data.pockets) {
            save(pocket);
        }
        for (const core::Job& job : data.jobs) {
            save(job);
        }
        for (const core::Movement& movement : data.movements) {
            save(movement);
        }
    } catch (...) {
        db_.handle().rollback();
        throw;
    }
    if (!db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la siembra."));
    }
    return true;
}

void Repository::wipe() {
    if (!db_.handle().transaction()) {
        throw StorageError(QStringLiteral("No se pudo abrir la transaccion de borrado."));
    }
    for (const QString& table :
         {QStringLiteral("movements"), QStringLiteral("jobs"), QStringLiteral("pockets")}) {
        QSqlQuery query(db_.handle());
        query.prepare(QStringLiteral("DELETE FROM %1").arg(table));
        if (!query.exec()) {
            db_.handle().rollback();
            throw StorageError(QStringLiteral("No se pudo vaciar: ") + describe(query));
        }
    }
    if (!db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar el borrado."));
    }
}

} // namespace dake::storage
