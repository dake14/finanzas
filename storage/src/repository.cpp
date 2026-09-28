#include "dake/storage/repository.hpp"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

#include "dake/core/demo.hpp"
#include "dake/core/uuid.hpp"
#include "dake/storage/wire.hpp"

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
        "SELECT p.id, p.name, p.kind, p.opening_minor, p.archived, p.hlc, p.device_id, "
        "p.deleted, m.account "
        "FROM pockets p LEFT JOIN pocket_meta m ON m.pocket_id = p.id "
        "WHERE (p.deleted = 0 OR ?) ORDER BY p.rowid"));
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
        if (!query.value(8).isNull()) {
            pocket.accountOverride = core::accountFromString(ss(query.value(8)));
        }
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
        "job_id, spread_months, settled, settled_date, recurrence, hlc, device_id, "
        "deleted "
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
        // Cadena vacia = no se sabe cuando se cobro. Se pregunta antes porque
        // fromIso lanza con una cadena vacia, y no saberlo es lo normal en todo
        // lo anotado antes de que existiera la columna.
        {
            const std::string cobro = ss(query.value(11));
            if (!cobro.empty()) {
                movement.settledDate = core::Date::fromIso(cobro);
            }
        }
        movement.recurrence = core::recurrenceFromString(ss(query.value(12)));
        movement.hlc = ss(query.value(13));
        movement.deviceId = ss(query.value(14));
        movement.deleted = query.value(15).toInt() != 0;
        out.push_back(std::move(movement));
    }
    return out;
}

// ----------------------------------------------------------------- Escrituras

namespace {

void insertOrReplace(Database& db, const core::Pocket& pocket) {
    QSqlQuery query(db.handle());
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

void insertOrReplace(Database& db, const core::Job& job) {
    QSqlQuery query(db.handle());
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

void insertOrReplace(Database& db, const core::Movement& movement) {
    const auto kindText = core::toString(movement.kind);
    const auto recurrenceText = core::toString(movement.recurrence);

    QSqlQuery query(db.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO movements "
        "(id, date, name, kind, amount_minor, pocket_id, target_pocket_id, category, job_id, "
        " spread_months, settled, settled_date, recurrence, hlc, device_id, deleted) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
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
    query.addBindValue(movement.settledDate ? qs(movement.settledDate->toIso()) : QString());
    query.addBindValue(
        QString::fromUtf8(recurrenceText.data(), static_cast<int>(recurrenceText.size())));
    query.addBindValue(qs(movement.hlc));
    query.addBindValue(qs(movement.deviceId));
    query.addBindValue(movement.deleted ? 1 : 0);
    run(query);
}

template <typename T>
void enqueueOutbox(Database& db, const QString& tableName, const QString& op, const T& record) {
    QSqlQuery outbox(db.handle());
    outbox.prepare(QStringLiteral(
        "INSERT INTO outbox (table_name, record_id, op, payload, hlc, sent) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    outbox.addBindValue(tableName);
    outbox.addBindValue(qs(record.id));
    outbox.addBindValue(op);
    outbox.addBindValue(QString::fromUtf8(QJsonDocument(toJson(record)).toJson(QJsonDocument::Compact)));
    outbox.addBindValue(qs(record.hlc));
    outbox.addBindValue(0);
    run(outbox);
}

void tombstone(Database& db, const QString& table, const std::string& id) {
    QSqlQuery query(db.handle());
    query.prepare(QStringLiteral("UPDATE %1 SET deleted = 1 WHERE id = ?").arg(table));
    query.addBindValue(qs(id));
    run(query);
}

} // namespace

void Repository::save(const core::Pocket& pocket) {
    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, pocket);
        enqueueOutbox(db_, QStringLiteral("pockets"), QStringLiteral("Upsert"), pocket);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

void Repository::save(const core::Job& job) {
    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, job);
        enqueueOutbox(db_, QStringLiteral("jobs"), QStringLiteral("Upsert"), job);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

void Repository::save(const core::Movement& movement) {
    if (!movement.isWellFormed()) {
        throw StorageError(QStringLiteral("El movimiento '") + qs(movement.name) +
                           QStringLiteral("' no tiene la forma que su tipo exige."));
    }

    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, movement);
        enqueueOutbox(db_, QStringLiteral("movements"), QStringLiteral("Upsert"), movement);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

void Repository::remove(const core::Movement& movement) {
    const bool ownTx = db_.handle().transaction();
    try {
        tombstone(db_, QStringLiteral("movements"), movement.id);
        core::Movement tomb = movement;
        tomb.deleted = true;
        enqueueOutbox(db_, QStringLiteral("movements"), QStringLiteral("Delete"), tomb);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

void Repository::remove(const core::Job& job) {
    const bool ownTx = db_.handle().transaction();
    try {
        tombstone(db_, QStringLiteral("jobs"), job.id);
        core::Job tomb = job;
        tomb.deleted = true;
        enqueueOutbox(db_, QStringLiteral("jobs"), QStringLiteral("Delete"), tomb);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

void Repository::remove(const core::Pocket& pocket) {
    const bool ownTx = db_.handle().transaction();
    try {
        tombstone(db_, QStringLiteral("pockets"), pocket.id);
        core::Pocket tomb = pocket;
        tomb.deleted = true;
        enqueueOutbox(db_, QStringLiteral("pockets"), QStringLiteral("Delete"), tomb);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
}

// --------------------------------------------------------------- Sincronizacion

bool Repository::applyRemote(const core::Pocket& incoming) {
    QSqlQuery check(db_.handle());
    check.prepare(QStringLiteral("SELECT hlc FROM pockets WHERE id = ?"));
    check.addBindValue(qs(incoming.id));
    run(check);

    if (check.next()) {
        QString localHlc = check.value(0).toString();
        if (qs(incoming.hlc) <= localHlc) {
            return false;
        }
    }

    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, incoming);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
    return true;
}

bool Repository::applyRemote(const core::Job& incoming) {
    QSqlQuery check(db_.handle());
    check.prepare(QStringLiteral("SELECT hlc FROM jobs WHERE id = ?"));
    check.addBindValue(qs(incoming.id));
    run(check);

    if (check.next()) {
        QString localHlc = check.value(0).toString();
        if (qs(incoming.hlc) <= localHlc) {
            return false;
        }
    }

    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, incoming);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
    return true;
}

bool Repository::applyRemote(const core::Movement& incoming) {
    QSqlQuery check(db_.handle());
    check.prepare(QStringLiteral("SELECT hlc FROM movements WHERE id = ?"));
    check.addBindValue(qs(incoming.id));
    run(check);

    if (check.next()) {
        QString localHlc = check.value(0).toString();
        if (qs(incoming.hlc) <= localHlc) {
            return false;
        }
    }

    const bool ownTx = db_.handle().transaction();
    try {
        insertOrReplace(db_, incoming);
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
    return true;
}

bool Repository::applyRemoteQuote(const QString& id, const QString& updatedAt, bool deleted,
                                  const QString& json) {
    if (id.isEmpty()) {
        return false;
    }
    QSqlQuery check(db_.handle());
    check.prepare(QStringLiteral("SELECT updated_at, data FROM quotes WHERE id = ?"));
    check.addBindValue(id);
    run(check);
    if (check.next()) {
        const QString localUpdated = check.value(0).toString();
        if (updatedAt < localUpdated ||
            (updatedAt == localUpdated && check.value(1).toString() == json)) {
            return false;
        }
    }
    QSqlQuery write(db_.handle());
    write.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO quotes (id, updated_at, deleted, data) VALUES (?, ?, ?, ?)"));
    write.addBindValue(id);
    write.addBindValue(updatedAt);
    write.addBindValue(deleted ? 1 : 0);
    write.addBindValue(json);
    run(write);
    return true;
}

std::vector<QString> Repository::loadQuoteRows() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("SELECT data FROM quotes WHERE deleted = 0 ORDER BY id"));
    run(query);
    std::vector<QString> out;
    while (query.next()) {
        out.push_back(query.value(0).toString());
    }
    return out;
}

std::vector<Repository::OutboxEntry> Repository::pendingOutbox(int limit) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT rowid_pk, table_name, record_id, op, payload, hlc "
        "FROM outbox "
        "WHERE sent = 0 "
        "ORDER BY CASE table_name "
        "  WHEN 'pockets' THEN 1 "
        "  WHEN 'jobs' THEN 2 "
        "  WHEN 'movements' THEN 3 "
        "  ELSE 4 END, rowid_pk ASC "
        "LIMIT ?"
    ));
    query.addBindValue(limit);
    run(query);

    std::vector<OutboxEntry> out;
    while (query.next()) {
        OutboxEntry entry;
        entry.rowId = query.value(0).toLongLong();
        entry.tableName = query.value(1).toString();
        entry.recordId = query.value(2).toString();
        entry.op = query.value(3).toString();
        entry.payload = query.value(4).toString();
        entry.hlc = query.value(5).toString();
        out.push_back(std::move(entry));
    }
    return out;
}

int Repository::pendingOutboxCount() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM outbox WHERE sent = 0"));
    run(query);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

void Repository::markOutboxSent(const std::vector<qint64>& rowIds) {
    if (rowIds.empty()) {
        return;
    }
    const bool ownTx = db_.handle().transaction();
    try {
        QSqlQuery query(db_.handle());
        query.prepare(QStringLiteral("UPDATE outbox SET sent = 1 WHERE rowid_pk = ?"));
        for (qint64 id : rowIds) {
            query.addBindValue(id);
            run(query);
        }
    } catch (...) {
        if (ownTx) db_.handle().rollback();
        throw;
    }
    if (ownTx && !db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
    }
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

// ------------------------------------------------------------ Datos locales

void Repository::setPocketAccount(const core::Id& pocketId,
                                  std::optional<core::Account> account) {
    QSqlQuery query(db_.handle());
    if (account) {
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO pocket_meta (pocket_id, account) VALUES (?, ?)"));
        query.addBindValue(qs(pocketId));
        query.addBindValue(qs(std::string(core::toString(*account))));
    } else {
        query.prepare(QStringLiteral("DELETE FROM pocket_meta WHERE pocket_id = ?"));
        query.addBindValue(qs(pocketId));
    }
    run(query);
}

std::vector<core::Category> Repository::loadCategories() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT name, account, class, kind FROM categories ORDER BY name COLLATE NOCASE"));
    run(query);

    std::vector<core::Category> out;
    while (query.next()) {
        core::Category category;
        category.name = ss(query.value(0));
        category.account = core::accountFromString(ss(query.value(1)));
        category.cls = core::categoryClassFromString(ss(query.value(2)));
        category.kind = core::movementKindFromString(ss(query.value(3)));
        out.push_back(std::move(category));
    }
    return out;
}

void Repository::saveCategory(const core::Category& category) {
    // La clave primaria es NOCASE, asi que INSERT OR REPLACE con "luz" pisa la
    // fila de "Luz". El nombre que queda es el ultimo escrito.
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO categories (name, account, class, kind) VALUES (?, ?, ?, ?)"));
    query.addBindValue(qs(category.name));
    query.addBindValue(qs(std::string(core::toString(category.account))));
    query.addBindValue(qs(std::string(core::toString(category.cls))));
    query.addBindValue(qs(std::string(core::toString(category.kind))));
    run(query);
}

void Repository::removeCategory(const std::string& name) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("DELETE FROM categories WHERE name = ?"));
    query.addBindValue(qs(name));
    run(query);
}

// ------------------------------------------------------------ Reparaciones

namespace {

[[nodiscard]] QString text(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

[[nodiscard]] QString isoOrEmpty(const std::optional<core::Date>& date) {
    return date ? QString::fromStdString(date->toIso()) : QString();
}

[[nodiscard]] std::optional<core::Date> dateOrNothing(const QVariant& value) {
    const std::string iso = value.toString().toStdString();
    if (iso.empty()) {
        return std::nullopt;
    }
    return core::Date::fromIso(iso);
}

/// Los repuestos de una plantilla, uno por linea: "nombre<TAB>centavos". Un
/// tabulador o un salto de linea dentro del nombre se vuelven espacio: no
/// vale la pena escapar lo que nadie va a escribir a proposito.
[[nodiscard]] QString encodeParts(const std::vector<core::TemplatePart>& parts) {
    QStringList lines;
    for (const core::TemplatePart& part : parts) {
        QString name = QString::fromStdString(part.name);
        name.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
        lines << name + QLatin1Char('\t') + QString::number(part.costMinor);
    }
    return lines.join(QLatin1Char('\n'));
}

[[nodiscard]] std::vector<core::TemplatePart> decodeParts(const QString& encoded) {
    std::vector<core::TemplatePart> out;
    for (const QString& line : encoded.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        core::TemplatePart part;
        part.name = fields.value(0).toStdString();
        part.costMinor = fields.value(1).toLongLong();
        out.push_back(part);
    }
    return out;
}

} // namespace

std::vector<core::Repair> Repository::loadRepairs() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT j.id, j.name, j.opened, j.closed, r.job_id, r.order_no, r.device, r.repair_type, "
        "r.template_id, r.received, r.delivered, r.status, r.price_minor, r.shipping_minor, "
        "r.consumables_minor, r.est_minutes, r.real_minutes, r.source_ref "
        "FROM jobs j LEFT JOIN repairs r ON r.job_id = j.id "
        "WHERE j.deleted = 0 ORDER BY j.opened DESC, j.id DESC"));
    run(query);

    std::vector<core::Repair> out;
    while (query.next()) {
        core::Repair repair;
        repair.jobId = ss(query.value(0));
        if (query.value(4).isNull()) {
            // Sin ficha: lo que se sabe del trabajo.
            repair.device = ss(query.value(1));
            repair.received = core::Date::fromIso(ss(query.value(2)));
            repair.status = query.value(3).toInt() != 0 ? core::RepairStatus::Entregada
                                                         : core::RepairStatus::EnProceso;
            out.push_back(std::move(repair));
            continue;
        }
        repair.orderNo = ss(query.value(5));
        repair.device = ss(query.value(6));
        repair.type = core::repairTypeFromString(ss(query.value(7)));
        repair.templateId = ss(query.value(8));
        repair.received = dateOrNothing(query.value(9));
        repair.delivered = dateOrNothing(query.value(10));
        repair.status = core::repairStatusFromString(ss(query.value(11)));
        repair.priceMinor = query.value(12).toLongLong();
        repair.shippingMinor = query.value(13).toLongLong();
        repair.consumablesMinor = query.value(14).toLongLong();
        repair.estMinutes = query.value(15).toInt();
        const int real = query.value(16).toInt();
        if (real >= 0) {
            repair.realMinutes = real;
        }
        repair.sourceRef = ss(query.value(17));
        out.push_back(std::move(repair));
    }
    return out;
}

void Repository::saveRepair(const core::Repair& repair) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO repairs (job_id, order_no, device, repair_type, template_id, "
        "received, delivered, status, price_minor, shipping_minor, consumables_minor, "
        "est_minutes, real_minutes, source_ref) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(repair.jobId));
    query.addBindValue(qs(repair.orderNo));
    query.addBindValue(qs(repair.device));
    query.addBindValue(text(core::toString(repair.type)));
    query.addBindValue(qs(repair.templateId));
    query.addBindValue(isoOrEmpty(repair.received));
    query.addBindValue(isoOrEmpty(repair.delivered));
    query.addBindValue(text(core::toString(repair.status)));
    query.addBindValue(static_cast<qlonglong>(repair.priceMinor));
    query.addBindValue(static_cast<qlonglong>(repair.shippingMinor));
    query.addBindValue(static_cast<qlonglong>(repair.consumablesMinor));
    query.addBindValue(repair.estMinutes);
    query.addBindValue(repair.realMinutes.value_or(-1));
    query.addBindValue(qs(repair.sourceRef));
    run(query);
}

std::vector<core::RepairPart> Repository::loadRepairParts() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, job_id, name, cost_minor, cost_known, movement_id FROM repair_parts "
        "ORDER BY rowid"));
    run(query);
    std::vector<core::RepairPart> out;
    while (query.next()) {
        core::RepairPart part;
        part.id = ss(query.value(0));
        part.jobId = ss(query.value(1));
        part.name = ss(query.value(2));
        part.costMinor = query.value(3).toLongLong();
        part.costKnown = query.value(4).toInt() != 0;
        part.movementId = ss(query.value(5));
        out.push_back(std::move(part));
    }
    return out;
}

void Repository::saveRepairPart(const core::RepairPart& part) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO repair_parts (id, job_id, name, cost_minor, cost_known, "
        "movement_id) VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(part.id));
    query.addBindValue(qs(part.jobId));
    query.addBindValue(qs(part.name));
    query.addBindValue(static_cast<qlonglong>(part.costMinor));
    query.addBindValue(part.costKnown ? 1 : 0);
    query.addBindValue(qs(part.movementId));
    run(query);
}

void Repository::removeRepairPart(const core::Id& partId) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("DELETE FROM repair_parts WHERE id = ?"));
    query.addBindValue(qs(partId));
    run(query);
}

std::vector<core::RepairTemplate> Repository::loadTemplates() {
    if (!setting(QStringLiteral("config.plantillas_sembradas"))) {
        setSetting(QStringLiteral("config.plantillas_sembradas"), QStringLiteral("1"));
        for (core::RepairTemplate tpl : core::defaultTemplates()) {
            tpl.id = newId();
            saveTemplate(tpl);
        }
    }

    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, repair_type, name, price_minor, est_minutes, consumables_minor, "
        "shipping_minor, parts FROM repair_templates ORDER BY rowid"));
    run(query);
    std::vector<core::RepairTemplate> out;
    while (query.next()) {
        core::RepairTemplate tpl;
        tpl.id = ss(query.value(0));
        tpl.type = core::repairTypeFromString(ss(query.value(1)));
        tpl.name = ss(query.value(2));
        tpl.priceMinor = query.value(3).toLongLong();
        tpl.estMinutes = query.value(4).toInt();
        tpl.consumablesMinor = query.value(5).toLongLong();
        tpl.shippingMinor = query.value(6).toLongLong();
        tpl.parts = decodeParts(query.value(7).toString());
        out.push_back(std::move(tpl));
    }
    return out;
}

void Repository::saveTemplate(const core::RepairTemplate& tpl) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO repair_templates (id, repair_type, name, price_minor, "
        "est_minutes, consumables_minor, shipping_minor, parts) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(tpl.id));
    query.addBindValue(text(core::toString(tpl.type)));
    query.addBindValue(qs(tpl.name));
    query.addBindValue(static_cast<qlonglong>(tpl.priceMinor));
    query.addBindValue(tpl.estMinutes);
    query.addBindValue(static_cast<qlonglong>(tpl.consumablesMinor));
    query.addBindValue(static_cast<qlonglong>(tpl.shippingMinor));
    query.addBindValue(encodeParts(tpl.parts));
    run(query);
}

void Repository::removeTemplate(const core::Id& templateId) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("DELETE FROM repair_templates WHERE id = ?"));
    query.addBindValue(qs(templateId));
    run(query);
}

core::CostSettings Repository::loadCostSettings() {
    core::CostSettings settings;
    settings.hourlyRateMinor =
        setting(QStringLiteral("config.tarifa_hora")).value_or(QStringLiteral("0")).toLongLong();
    settings.targetMarginBps =
        setting(QStringLiteral("config.margen_objetivo")).value_or(QStringLiteral("3000")).toInt();
    settings.fixedPerHourMinor = 0;
    return settings;
}

void Repository::saveCostSettings(const core::CostSettings& settings) {
    setSetting(QStringLiteral("config.tarifa_hora"), QString::number(settings.hourlyRateMinor));
    setSetting(QStringLiteral("config.margen_objetivo"), QString::number(settings.targetMarginBps));
}

// ------------------------------------------------------- Fijos y bandeja

std::vector<core::Recurring> Repository::loadRecurring() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, name, category, pocket_id, amount_minor, day_of_month, starts, ends, active "
        "FROM recurring ORDER BY rowid"));
    run(query);
    std::vector<core::Recurring> out;
    while (query.next()) {
        core::Recurring r;
        r.id = ss(query.value(0));
        r.name = ss(query.value(1));
        r.category = ss(query.value(2));
        r.pocketId = ss(query.value(3));
        r.amountMinor = query.value(4).toLongLong();
        r.dayOfMonth = query.value(5).toInt();
        r.starts = core::Date::fromIso(ss(query.value(6)));
        r.ends = dateOrNothing(query.value(7));
        r.active = query.value(8).toInt() != 0;
        out.push_back(std::move(r));
    }
    return out;
}

void Repository::saveRecurring(const core::Recurring& r) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO recurring (id, name, category, pocket_id, amount_minor, "
        "day_of_month, starts, ends, active) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(r.id));
    query.addBindValue(qs(r.name));
    query.addBindValue(qs(r.category));
    query.addBindValue(qs(r.pocketId));
    query.addBindValue(static_cast<qlonglong>(r.amountMinor));
    query.addBindValue(r.dayOfMonth);
    query.addBindValue(qs(r.starts.toIso()));
    query.addBindValue(isoOrEmpty(r.ends));
    query.addBindValue(r.active ? 1 : 0);
    run(query);
}

void Repository::removeRecurring(const core::Id& id) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("DELETE FROM recurring WHERE id = ?"));
    query.addBindValue(qs(id));
    run(query);
}

std::vector<core::Tool> Repository::loadTools() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT id, name, cost_minor, bought, life_months, retired, movement_id FROM tools "
        "ORDER BY bought, rowid"));
    run(query);
    std::vector<core::Tool> out;
    while (query.next()) {
        core::Tool t;
        t.id = ss(query.value(0));
        t.name = ss(query.value(1));
        t.costMinor = query.value(2).toLongLong();
        t.bought = core::Date::fromIso(ss(query.value(3)));
        t.lifeMonths = query.value(4).toInt();
        t.retired = dateOrNothing(query.value(5));
        t.movementId = ss(query.value(6));
        out.push_back(std::move(t));
    }
    return out;
}

void Repository::saveTool(const core::Tool& t) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO tools (id, name, cost_minor, bought, life_months, retired, "
        "movement_id) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(t.id));
    query.addBindValue(qs(t.name));
    query.addBindValue(static_cast<qlonglong>(t.costMinor));
    query.addBindValue(qs(t.bought.toIso()));
    query.addBindValue(t.lifeMonths);
    query.addBindValue(isoOrEmpty(t.retired));
    query.addBindValue(qs(t.movementId));
    run(query);
}

void Repository::removeTool(const core::Id& id) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("DELETE FROM tools WHERE id = ?"));
    query.addBindValue(qs(id));
    run(query);
}

std::vector<core::MovementMeta> Repository::loadMovementMeta() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT movement_id, origin, review, recurring_id, period, external_ref FROM movement_meta"));
    run(query);
    std::vector<core::MovementMeta> out;
    while (query.next()) {
        out.push_back({ss(query.value(0)), ss(query.value(1)), ss(query.value(2)), ss(query.value(3)),
                       ss(query.value(4)), ss(query.value(5))});
    }
    return out;
}

void Repository::saveMovementMeta(const core::MovementMeta& m) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO movement_meta (movement_id, origin, review, recurring_id, period, "
        "external_ref) VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(qs(m.movementId));
    query.addBindValue(qs(m.origin.empty() ? std::string("Manual") : m.origin));
    query.addBindValue(qs(m.review));
    query.addBindValue(qs(m.recurringId));
    query.addBindValue(qs(m.period));
    query.addBindValue(qs(m.externalRef));
    run(query);
}

void Repository::addTiming(const QString& what, qint64 millis) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("INSERT INTO timings (what, millis, at) VALUES (?, ?, ?)"));
    query.addBindValue(what);
    query.addBindValue(millis);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    run(query);
}

qint64 Repository::timingMedian(const QString& what, int last) {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral(
        "SELECT millis FROM timings WHERE what = ? ORDER BY id DESC LIMIT ?"));
    query.addBindValue(what);
    query.addBindValue(last);
    run(query);

    std::vector<qint64> values;
    while (query.next()) {
        values.push_back(query.value(0).toLongLong());
    }
    if (values.empty()) {
        return -1;
    }
    // Mediana y no promedio: una captura que quedo abierta mientras se atendia
    // a un cliente dura media hora y no dice nada de lo que tarda anotar.
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    return values.size() % 2 == 1 ? values[mid] : (values[mid - 1] + values[mid]) / 2;
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
         {QStringLiteral("quotes"), QStringLiteral("movements"), QStringLiteral("jobs"),
          QStringLiteral("pockets")}) {
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

std::size_t Repository::deleteEverything() {
    const auto movements = loadMovements();
    const auto jobs = loadJobs();
    const auto pockets = loadPockets();

    if (!db_.handle().transaction()) {
        throw StorageError(QStringLiteral("No se pudo abrir la transaccion de borrado."));
    }

    try {
        for (const auto& movement : movements) {
            remove(movement);
        }
        for (const auto& job : jobs) {
            remove(job);
        }
        for (const auto& pocket : pockets) {
            remove(pocket);
        }
    } catch (...) {
        db_.handle().rollback();
        throw;
    }

    if (!db_.handle().commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar el borrado."));
    }

    return movements.size() + jobs.size() + pockets.size();
}

} // namespace dake::storage
