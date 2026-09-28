#include "dake/storage/database.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
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

constexpr int kTargetVersion = 4;

/// Copia la base a un `.bak` fechado antes de tocarle el esquema.
///
/// Una migracion corre una sola vez sobre datos que no estan en ningun otro
/// lado, y si sale mal en la mitad no hay a que volver: la transaccion protege
/// de una migracion incompleta, no de una migracion completa y equivocada.
/// Veinte lineas de copia valen mas que cualquier cuidado al escribir el ALTER.
///
/// El nombre lleva la version DESDE la que se sube y la fecha, para que dos
/// migraciones el mismo dia no se pisen:
///     finanzas-v2.db  ->  finanzas-v2.db.v1-20260904-193000.bak
///
/// Si la copia falla, la migracion NO sigue: preferimos una base vieja y
/// entera a una nueva sin respaldo.
///
/// CONTRATO — el cuerpo va aca y nada mas que aca.
///   - Si `dbPath` esta vacio o el archivo no existe, volver sin hacer nada:
///     una base recien creada no tiene nada que respaldar.
///   - Armar el destino como se describe arriba, con
///     QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss").
///   - Copiar con QFile::copy. Si el destino ya existe, borrarlo antes con
///     QFile::remove (dos migraciones en el mismo segundo).
///   - Si QFile::copy devuelve false, lanzar StorageError con el texto
///     "No se pudo respaldar la base antes de migrar: " + el destino.
///
/// Dependencias permitidas: QFile, QFileInfo, QDateTime, QString. QFile y
/// QDateTime hay que incluirlos; QFileInfo ya esta.
void backupBeforeUpgrade(const QString& dbPath, int fromVersion) {
    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString dest = QStringLiteral("%1.v%2-%3.bak").arg(dbPath).arg(fromVersion).arg(timestamp);

    if (QFile::exists(dest)) {
        QFile::remove(dest);
    }

    if (!QFile::copy(dbPath, dest)) {
        throw StorageError(QStringLiteral("No se pudo respaldar la base antes de migrar: ") + dest);
    }
}

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
    // 2 -> 3: las tablas locales. Las crea el esquema, que se vuelve a correr
    // entero en cada subida con CREATE TABLE IF NOT EXISTS, asi que este paso
    // solo tiene que existir para que el bucle tenga que correr. Se aprovecha
    // para algo inofensivo que igual hace falta.
    "CREATE INDEX IF NOT EXISTS movement_meta_by_ref ON movement_meta(external_ref)",
    // 3 -> 4: lo esencial de Cotizaciones, que baja de v2_quotes. Igual que el
    // paso anterior, la tabla ya la crea el esquema; el paso existe para que
    // el bucle corra.
    "CREATE TABLE IF NOT EXISTS quotes (id TEXT PRIMARY KEY, updated_at TEXT NOT NULL DEFAULT '', "
    "deleted INTEGER NOT NULL DEFAULT 0, data TEXT NOT NULL)",
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

-- ===================================================================== v3
-- Tablas LOCALES. No tienen hlc ni pasan por la cola de salida: el motor de
-- sincronizacion sube el JSON de cada fila tal cual, y el servidor rechaza la
-- fila entera si trae una columna que no conoce. Agregar campos a pockets,
-- jobs o movements frenaria la sincronizacion del telefono, asi que lo nuevo
-- va al costado, unido por id. Al bajar una fila del servidor se reemplaza la
-- de pockets, jobs o movements, y lo de aca sobrevive porque es otra tabla.
--
-- En estos comentarios no puede haber punto y coma: el guion se parte por ese
-- caracter antes de ejecutarse.

-- La cuenta elegida a mano. Sin fila = la que corresponde al tipo.
CREATE TABLE IF NOT EXISTS pocket_meta (
    pocket_id TEXT PRIMARY KEY NOT NULL,
    account   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS categories (
    name    TEXT PRIMARY KEY NOT NULL COLLATE NOCASE,
    account TEXT NOT NULL DEFAULT 'Negocio',
    class   TEXT NOT NULL DEFAULT 'General',
    kind    TEXT NOT NULL DEFAULT 'Gasto'
);

-- De donde vino cada movimiento y que falta revisar. external_ref es la
-- huella de lo que vino de afuera (un informe, una fila de un extracto), que
-- es lo que impide importar dos veces lo mismo.
CREATE TABLE IF NOT EXISTS movement_meta (
    movement_id  TEXT PRIMARY KEY NOT NULL,
    origin       TEXT NOT NULL DEFAULT 'Manual',
    review       TEXT NOT NULL DEFAULT '',
    recurring_id TEXT NOT NULL DEFAULT '',
    period       TEXT NOT NULL DEFAULT '',
    external_ref TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS movement_meta_by_ref ON movement_meta(external_ref);

-- La ficha de una reparacion. Un trabajo sin fila aca es una reparacion de
-- tipo Otro, que es lo que son los trabajos anotados antes de la version 3.
-- real_minutes = -1 quiere decir que no se sabe todavia, que no es lo mismo
-- que cero.
CREATE TABLE IF NOT EXISTS repairs (
    job_id            TEXT PRIMARY KEY NOT NULL,
    order_no          TEXT NOT NULL DEFAULT '',
    device            TEXT NOT NULL DEFAULT '',
    repair_type       TEXT NOT NULL DEFAULT 'Otro',
    template_id       TEXT NOT NULL DEFAULT '',
    received          TEXT NOT NULL DEFAULT '',
    delivered         TEXT NOT NULL DEFAULT '',
    status            TEXT NOT NULL DEFAULT 'EnProceso',
    price_minor       INTEGER NOT NULL DEFAULT 0,
    shipping_minor    INTEGER NOT NULL DEFAULT 0,
    consumables_minor INTEGER NOT NULL DEFAULT 0,
    est_minutes       INTEGER NOT NULL DEFAULT 0,
    real_minutes      INTEGER NOT NULL DEFAULT -1,
    source_ref        TEXT NOT NULL DEFAULT ''
);

-- Repuestos. cost_known = 0 para los que vinieron de un informe sin costo:
-- lo que el cliente pago por la pieza no es lo que costo.
CREATE TABLE IF NOT EXISTS repair_parts (
    id          TEXT PRIMARY KEY NOT NULL,
    job_id      TEXT NOT NULL,
    name        TEXT NOT NULL,
    cost_minor  INTEGER NOT NULL DEFAULT 0,
    cost_known  INTEGER NOT NULL DEFAULT 1,
    movement_id TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS repair_parts_by_job ON repair_parts(job_id);

-- parts: un repuesto tipico por linea, "nombre<TAB>centavos".
CREATE TABLE IF NOT EXISTS repair_templates (
    id                TEXT PRIMARY KEY NOT NULL,
    repair_type       TEXT NOT NULL,
    name              TEXT NOT NULL,
    price_minor       INTEGER NOT NULL DEFAULT 0,
    est_minutes       INTEGER NOT NULL DEFAULT 0,
    consumables_minor INTEGER NOT NULL DEFAULT 0,
    shipping_minor    INTEGER NOT NULL DEFAULT 0,
    parts             TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS recurring (
    id           TEXT PRIMARY KEY NOT NULL,
    name         TEXT NOT NULL,
    category     TEXT NOT NULL DEFAULT '',
    pocket_id    TEXT NOT NULL DEFAULT '',
    amount_minor INTEGER NOT NULL DEFAULT 0,
    day_of_month INTEGER NOT NULL DEFAULT 1,
    starts       TEXT NOT NULL,
    ends         TEXT NOT NULL DEFAULT '',
    active       INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS tools (
    id          TEXT PRIMARY KEY NOT NULL,
    name        TEXT NOT NULL,
    cost_minor  INTEGER NOT NULL,
    bought      TEXT NOT NULL,
    life_months INTEGER NOT NULL DEFAULT 24,
    retired     TEXT NOT NULL DEFAULT '',
    movement_id TEXT NOT NULL DEFAULT ''
);

-- Cuanto tarda cada captura, para medir los criterios de aceptacion con el uso
-- de verdad y no con una demostracion.
CREATE TABLE IF NOT EXISTS timings (
    id     INTEGER PRIMARY KEY AUTOINCREMENT,
    what   TEXT NOT NULL,
    millis INTEGER NOT NULL,
    at     TEXT NOT NULL
);

-- Lo esencial de cada documento de DakeLabs Cotizaciones, tal como bajo de
-- v2_quotes. Finanzas solo lo lee: nunca escribe ni encola nada aca. Se
-- guarda el JSON entero de la fila: si Cotizaciones suma una columna, no hay
-- que migrar esta tabla.
CREATE TABLE IF NOT EXISTS quotes (
    id         TEXT PRIMARY KEY,
    updated_at TEXT NOT NULL DEFAULT '',
    deleted    INTEGER NOT NULL DEFAULT 0,
    data       TEXT NOT NULL
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

    // El respaldo va aca y no adentro de la transaccion: copiar el archivo
    // mientras SQLite tiene una transaccion abierta encima copiaria un estado
    // a medias. `current >= 1` deja afuera la base recien creada, que no tiene
    // nada que respaldar.
    if (current >= 1) {
        backupBeforeUpgrade(path(), current);
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
