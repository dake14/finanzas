#include "dake/storage/database.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

namespace dake::storage {
namespace {

constexpr int kTargetVersion = 1;

[[nodiscard]] QString describe(const QSqlQuery& query) {
    return query.lastError().text() + QStringLiteral(" [") + query.lastQuery() +
           QStringLiteral("]");
}

/// Esquema v1. Escrito a mano y en un solo lugar: un esquema repartido entre
/// veinte `CREATE TABLE IF NOT EXISTS` sueltos deja de ser legible al tercero.
///
/// Todo id es TEXT (UUIDv7), todo importe es INTEGER en unidades minimas y no
/// hay una sola columna REAL: un solo redondeo binario descuadra los reportes
/// de forma permanente y sin dejar rastro.
const char* const kSchemaV1 = R"SQL(
CREATE TABLE IF NOT EXISTS pockets (
    id            TEXT PRIMARY KEY NOT NULL,
    name          TEXT NOT NULL,
    kind          TEXT NOT NULL,
    opening_minor INTEGER NOT NULL DEFAULT 0,
    archived      INTEGER NOT NULL DEFAULT 0,
    hlc           TEXT NOT NULL DEFAULT '',
    device_id     TEXT NOT NULL DEFAULT '',
    deleted       INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS jobs (
    id        TEXT PRIMARY KEY NOT NULL,
    name      TEXT NOT NULL,
    client    TEXT NOT NULL DEFAULT '',
    opened    TEXT NOT NULL,
    closed    INTEGER NOT NULL DEFAULT 0,
    hlc       TEXT NOT NULL DEFAULT '',
    device_id TEXT NOT NULL DEFAULT '',
    deleted   INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS movements (
    id               TEXT PRIMARY KEY NOT NULL,
    date             TEXT NOT NULL,
    name             TEXT NOT NULL,
    kind             TEXT NOT NULL,
    amount_minor     INTEGER NOT NULL,
    pocket_id        TEXT NOT NULL,
    target_pocket_id TEXT NOT NULL DEFAULT '',
    category         TEXT NOT NULL DEFAULT '',
    job_id           TEXT NOT NULL DEFAULT '',
    spread_months    INTEGER NOT NULL DEFAULT 1,
    settled          INTEGER NOT NULL DEFAULT 1,
    recurrence       TEXT NOT NULL DEFAULT 'Puntual',
    hlc              TEXT NOT NULL DEFAULT '',
    device_id        TEXT NOT NULL DEFAULT '',
    deleted          INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS movements_by_date ON movements(date);

CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL
);
)SQL";

} // namespace

StorageError::StorageError(const QString& message)
    : std::runtime_error(message.toStdString()) {}

QString Database::defaultPath() {
    const QByteArray override = qgetenv("DAKE_TEST_DB_PATH");
    if (!override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/pruebas.db");
}

Database::Database(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Nombre de conexion unico: dos Database vivos a la vez se pisarian si
    // compartieran el nombre por defecto de Qt.
    connectionName_ = QStringLiteral("pruebas-") + QUuid::createUuid().toString(QUuid::Id128);

    db_ = std::make_unique<QSqlDatabase>(
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_));
    db_->setDatabaseName(path);

    if (!db_->open()) {
        throw StorageError(QStringLiteral("No se pudo abrir la base en '") + path +
                           QStringLiteral("': ") + db_->lastError().text());
    }

    QSqlQuery pragma(*db_);
    for (const QString& statement : {QStringLiteral("PRAGMA journal_mode = WAL"),
                                     QStringLiteral("PRAGMA foreign_keys = ON"),
                                     QStringLiteral("PRAGMA busy_timeout = 5000")}) {
        if (!pragma.exec(statement)) {
            throw StorageError(QStringLiteral("Fallo un PRAGMA: ") + describe(pragma));
        }
    }

    applyMigrations();
}

Database::~Database() {
    // El QSqlDatabase se destruye ANTES de quitar la conexion; al reves, Qt
    // avisa que la conexion sigue en uso y la deja colgada en el registro.
    db_->close();
    db_.reset();
    QSqlDatabase::removeDatabase(connectionName_);
}

QSqlDatabase& Database::handle() noexcept {
    return *db_;
}

QString Database::path() const {
    return db_->databaseName();
}

void Database::applyMigrations() {
    QSqlQuery query(*db_);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        throw StorageError(QStringLiteral("No se pudo leer user_version: ") + describe(query));
    }
    const int current = query.value(0).toInt();

    if (current > kTargetVersion) {
        // Abrir una base de una version que este binario no conoce y escribir
        // igual es la forma mas rapida de perder datos sin enterarse.
        throw StorageError(QStringLiteral("La base es de una version mas nueva (%1) que este "
                                          "programa (%2).")
                               .arg(current)
                               .arg(kTargetVersion));
    }
    if (current == kTargetVersion) {
        return;
    }

    if (!db_->transaction()) {
        throw StorageError(QStringLiteral("No se pudo abrir la transaccion de migracion."));
    }

    // QSqlQuery ejecuta una sentencia por llamada, asi que el guion se parte
    // por punto y coma.
    const QStringList statements =
        QString::fromUtf8(kSchemaV1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& statement : statements) {
        const QString trimmed = statement.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        QSqlQuery step(*db_);
        if (!step.exec(trimmed)) {
            db_->rollback();
            throw StorageError(QStringLiteral("Fallo la migracion: ") + describe(step));
        }
    }

    QSqlQuery version(*db_);
    if (!version.exec(QStringLiteral("PRAGMA user_version = %1").arg(kTargetVersion))) {
        db_->rollback();
        throw StorageError(QStringLiteral("No se pudo fijar user_version: ") + describe(version));
    }

    if (!db_->commit()) {
        throw StorageError(QStringLiteral("No se pudo confirmar la migracion."));
    }
}

} // namespace dake::storage
