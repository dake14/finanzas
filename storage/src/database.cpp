#include "dake/storage/database.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

namespace dake::storage {
namespace {

constexpr int kTargetVersion = 2;

// Pasos que NO alcanza con volver a correr el esquema.
//
// kSchemaV1 usa CREATE TABLE IF NOT EXISTS, asi que sobre una base que ya
// existe no agrega ni una columna: la tabla esta, y el IF NOT EXISTS la deja
// como esta. Una columna nueva necesita su ALTER, y por eso existe esta lista.
//
// El indice es la version DESDE la que se sube: kUpgrades[0] lleva de la 1 a
// la 2. Cada paso tiene que poder correr sobre una base ya usada, con datos
// adentro, sin perder ninguno.
constexpr const char* kUpgrades[] = {
    // 1 -> 2: cuando se cobro o se pago. Vacia en todo lo ya anotado, que es
    // la verdad: de esos movimientos no sabemos la fecha.
    "ALTER TABLE movements ADD COLUMN settled_date TEXT NOT NULL DEFAULT ''",
};

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
    -- Cuando se cobro o se pago. Cadena vacia = no se sabe, que es lo que vale
    -- para todo lo anotado antes de que existiera esta columna. El promedio de
    -- dias de cobro saltea las vacias en vez de inventarles una fecha.
    settled_date     TEXT NOT NULL DEFAULT '',
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

-- La cola de salida. Cada cambio local deja una fila aca, dentro de la misma
-- transaccion que lo escribio, y recien se marca `sent` cuando el servidor lo
-- confirmo. Es lo que hace que un corte de red no pierda nada.
--
-- `table_name` existe porque aca hay tres tablas que sincronizar y no una:
-- pockets, jobs y movements. El motor las recorre siempre en ese orden.
--
-- No hay tabla `sync_meta`: el estado del sincronizador vive en `settings` con
-- el prefijo "sync.", que es la misma forma clave/valor y ya la usa deviceId().
CREATE TABLE IF NOT EXISTS outbox (
    rowid_pk   INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name TEXT NOT NULL,
    record_id  TEXT NOT NULL,
    op         TEXT NOT NULL,
    payload    TEXT NOT NULL,
    hlc        TEXT NOT NULL,
    sent       INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS outbox_pending ON outbox(sent, rowid_pk);
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
    return dir + QStringLiteral("/" DAKE_DB_FILE);
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

    // Los ALTER van DESPUES del esquema: sobre una base nueva las tablas recien
    // se acaban de crear con la columna incluida, asi que el ALTER falla con
    // "duplicate column name" y hay que dejarlo pasar. Sobre una base vieja es
    // al reves y el ALTER es justamente lo que hace falta.
    // Desde 1 y no desde `current`: una base recien creada viene con
    // user_version 0, y kUpgrades[0 - 1] lee fuera del arreglo. Ademas no le
    // hace falta ningun ALTER, porque el esquema que se acaba de correr ya
    // trae todas las columnas.
    for (int from = std::max(current, 1); from < kTargetVersion; ++from) {
        QSqlQuery step(*db_);
        const QString sql = QString::fromUtf8(kUpgrades[from - 1]);
        if (!step.exec(sql) && !step.lastError().text().contains(QStringLiteral("duplicate column"),
                                                                 Qt::CaseInsensitive)) {
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
