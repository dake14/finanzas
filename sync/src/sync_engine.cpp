//
// dake/sync/sync_engine.cpp — el motor.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `finaldake-labs/sync/src/sync_engine.cpp` (394 lineas) generalizandolo
// de UNA tabla a TRES. Toda la resiliencia del original se conserva sin tocar:
// la escalera de reintentos (2 s, 5 s, 10 s, hasta 3), la renovacion unica de
// sesion ante un 401, y que nada se marque enviado antes de la confirmacion.
//
// Implementar exactamente las firmas de sync_engine.hpp. Ni una mas.
//
// EL ORDEN DE LAS TABLAS
//
//   `kTables` = {"pockets", "jobs", "movements"}. Ese orden se respeta al subir
//   y al bajar. Un movimiento referencia bolsillos y trabajos: si bajara antes
//   que ellos, la referencia quedaria colgando.
//
//   remoteTableFor("pockets") devuelve "v2_pockets". Es prefijo "v2_" y nada
//   mas. Las tablas sin prefijo son de la aplicacion vieja y no se tocan.
//
// SUBIDA
//
//   1. repository_.pendingOutbox(50). Ya viene ordenada por tabla y por orden
//      de insercion.
//   2. Agrupar por `tableName` y mandar un POST por grupo a
//        /rest/v1/<tabla remota>
//      con cabecera  Prefer: resolution=merge-duplicates  (upsert).
//   3. A CADA objeto del cuerpo, ANTES de mandarlo:
//        - QUITARLE la clave "t". La pone toJson() como discriminador de tipo
//          para el archivo de importar y exportar, donde las tres entidades
//          conviven en un mismo renglon. En Postgres no existe esa columna y
//          PostgREST rechaza el upsert entero con un 400 si la ve. Aca el tipo
//          ya lo dice la tabla a la que se manda.
//        - AGREGARLE "user_id" con client_.userId(). El payload de la outbox
//          no lo trae: la base local no sabe de usuarios.
//      Al bajar, la operacion inversa no hace falta: pocketFrom y compania
//      ignoran las claves que no conocen, asi que "user_id" y "updated_at" que
//      vienen del servidor se descartan solos.
//   4. Con la respuesta 2xx, repository_.markOutboxSent(los rowId del grupo).
//      Nunca antes.
//   5. Repetir hasta que pendingOutbox venga vacia.
//
// BAJADA
//
//   Para cada tabla, en el orden de kTables:
//     GET /rest/v1/<tabla remota>
//         ?user_id=eq.<userId>
//         &updated_at=gt.<cursor de esa tabla>
//         &order=updated_at.asc
//         &limit=200
//     Cada fila se convierte con storage::pocketFrom / jobFrom / movementFrom
//     y se aplica con repository_.applyRemote(...). Contar como `pulled` solo
//     las que devolvieron true.
//     Al terminar la pagina, guardar el `updated_at` de la ultima fila como
//     cursor de ESA tabla y seguir paginando hasta que vuelva vacia.
//
//   HAY UN CURSOR POR TABLA, no uno compartido. Con uno solo, bajar movimientos
//   adelantaria el reloj de los bolsillos y los cambios de bolsillo ocurridos
//   en el medio no bajarian nunca. Se guardan en `settings`:
//
//       sync.cursor.pockets    sync.cursor.jobs    sync.cursor.movements
//
//   via repository_.setting() / setSetting().
//
// SESION
//
//   El refresh token se persiste en la clave `sync.refresh_token` y el correo
//   en `sync.user_email`, con las mismas llamadas a setting()/setSetting().
//   No hay tabla sync_meta en este proyecto.
//
// SEÑALES
//
//   progress(mensaje, actual, total) durante todo. En la bajada total es -1
//   mientras no se sepa cuanto falta.
//   finished(pushed, pulled, error) una sola vez por corrida, con error vacio
//   si salio bien.
//
// PROHIBIDO
//
//   - Cualquier espera bloqueante: nada de QEventLoop ni de waitForReadyRead.
//     Una corrida con mala senal trabaria la ventana entera.
//   - Tocar las tablas sin prefijo v2_.
//   - Escribir en la outbox: eso es de applyRemote, y applyRemote justamente no
//     lo hace, para no crear un bucle de eco.
//
// DEPENDENCIAS PERMITIDAS: QObject, QTimer, QNetworkReply, QJsonDocument,
// QJsonObject, QJsonArray, QString, QUrlQuery, <array>, <functional>, <vector>,
// mas sync_engine.hpp, supabase_client.hpp, dake/storage/repository.hpp y
// dake/storage/wire.hpp. Ninguna otra.
//
// ACEPTACION: compila sin advertencias con /W4; no aparece "QEventLoop" en el
// archivo; no aparece ninguna cadena "movements" que no venga precedida de
// "v2_" salvo dentro de kTables y de las claves sync.cursor.*; una corrida sin
// credenciales termina con finished(0, 0, <error no vacio>) y sin colgarse.
//
#include "dake/sync/sync_engine.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>

#include "dake/storage/wire.hpp"

namespace dake::sync {
namespace {

constexpr int kPushBatch = 50;
constexpr int kPullPage = 200;

constexpr int kRetryDelaysMs[] = {2000, 5000, 10000};
constexpr int kMaxRetries = 3;

[[nodiscard]] QString errorMessage(const QByteArray& body, const QString& fallback) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        for (const char* field : {"message", "msg", "error_description", "error", "hint"}) {
            const QString value = object[QLatin1String(field)].toString();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return fallback;
}

[[nodiscard]] bool isTransientStatus(int status) {
    return status == 0 || status >= 500;
}

[[nodiscard]] bool isExpiredTokenError(const QByteArray& body) {
    return body.contains("PGRST301") || body.contains("JWT expired") ||
           body.contains("jwt expired");
}

} // namespace

QString remoteTableFor(const QString& localTable) {
    return QStringLiteral("v2_") + localTable;
}

SyncEngine::SyncEngine(SupabaseClient& client,
                       storage::Repository& repository,
                       QObject* parent)
    : QObject(parent), client_(client), repository_(repository) {
    connect(&client_, &SupabaseClient::signedIn, this, [this]() {
        repository_.setSetting(QStringLiteral("sync.refresh_token"), client_.refreshToken());
        repository_.setSetting(QStringLiteral("sync.user_email"), client_.userEmail());
    });
    connect(&client_, &SupabaseClient::signedOut, this, [this]() {
        repository_.setSetting(QStringLiteral("sync.refresh_token"), QString());
        repository_.setSetting(QStringLiteral("sync.user_email"), QString());
    });
}

bool SyncEngine::isRunning() const noexcept {
    return running_;
}

int SyncEngine::pendingCount() const {
    return const_cast<storage::Repository&>(repository_).pendingOutboxCount();
}

void SyncEngine::sync() {
    if (running_) {
        return;
    }
    if (!client_.isSignedIn()) {
        emit finished(0, 0, QStringLiteral("No hay sesion iniciada."));
        return;
    }

    running_ = true;
    authRefreshAttempted_ = false;
    retryCount_ = 0;
    pushed_ = 0;
    pulled_ = 0;
    inFlight_.clear();

    for (std::size_t i = 0; i < kTables.size(); ++i) {
        const QString key = QStringLiteral("sync.cursor.") + QString::fromLatin1(kTables[i]);
        const std::optional<QString> c = repository_.setting(key);
        cursors_[i] = c.value_or(QString());
    }

    try {
        totalToPush_ = repository_.pendingOutboxCount();
    } catch (const std::exception&) {
        totalToPush_ = 0;
    }

    emit progress(QStringLiteral("Subiendo cambios locales…"), 0, totalToPush_);
    pushNextBatch();
}

void SyncEngine::fail(const QString& message) {
    running_ = false;
    emit finished(pushed_, pulled_, message);
}

void SyncEngine::succeed() {
    running_ = false;
    emit finished(pushed_, pulled_, QString());
}

bool SyncEngine::scheduleRetry(const QString& reasonForUser, std::function<void()> operation) {
    if (retryCount_ >= kMaxRetries) {
        return false;
    }
    const int delayMs = kRetryDelaysMs[retryCount_];
    ++retryCount_;

    emit progress(QStringLiteral("%1 Reintentando en %2 s… (intento %3 de %4)")
                      .arg(reasonForUser)
                      .arg(delayMs / 1000)
                      .arg(retryCount_)
                      .arg(kMaxRetries),
                  pushed_, totalToPush_ > 0 ? totalToPush_ : -1);

    QTimer::singleShot(delayMs, this, std::move(operation));
    return true;
}

void SyncEngine::handleAuthExpiry(std::function<void()> retry) {
    emit progress(QStringLiteral("La sesion expiro; renovandola…"), pushed_,
                 totalToPush_ > 0 ? totalToPush_ : -1);

    auto success = new QMetaObject::Connection;
    auto failure = new QMetaObject::Connection;

    *success = connect(&client_, &SupabaseClient::signedIn, this,
                       // Sin capturar `this`: el cuerpo no lo usa, y el
                       // compilador de Android lo avisa. Capturar de mas en una
                       // lambda que sobrevive a la llamada es como se alarga la
                       // vida de un puntero sin querer.
                       [retry, success, failure](const QString&) {
                           QObject::disconnect(*success);
                           QObject::disconnect(*failure);
                           delete success;
                           delete failure;
                           retry();
                       });
    *failure = connect(&client_, &SupabaseClient::authFailed, this,
                       [this, success, failure](const QString& message) {
                           QObject::disconnect(*success);
                           QObject::disconnect(*failure);
                           delete success;
                           delete failure;
                           client_.signOut();
                           fail(QStringLiteral("La sesion expiro y no se pudo renovar: ") +
                               message + QStringLiteral(" Inicia sesion de nuevo."));
                       });

    client_.refreshSession();
}

void SyncEngine::pushNextBatch() {
    std::vector<storage::Repository::OutboxEntry> batch;
    try {
        batch = repository_.pendingOutbox(kPushBatch);
    } catch (const std::exception& error) {
        fail(QString::fromUtf8(error.what()));
        return;
    }

    if (batch.empty()) {
        startPull();
        return;
    }

    const QString currentTable = batch.front().tableName;
    QJsonArray rows;
    inFlight_.clear();
    const QString userId = client_.userId();

    for (const storage::Repository::OutboxEntry& entry : batch) {
        if (entry.tableName != currentTable) {
            break;
        }
        QJsonObject row = QJsonDocument::fromJson(entry.payload.toUtf8()).object();
        if (row.isEmpty()) {
            inFlight_.push_back(entry.rowId);
            continue;
        }
        row.remove(QStringLiteral("t"));
        row[QStringLiteral("user_id")] = userId;

        rows.append(row);
        inFlight_.push_back(entry.rowId);
    }

    if (rows.isEmpty()) {
        try {
            repository_.markOutboxSent(inFlight_);
        } catch (const std::exception& error) {
            fail(QString::fromUtf8(error.what()));
            return;
        }
        pushNextBatch();
        return;
    }

    const QString remoteTable = remoteTableFor(currentTable);
    QNetworkReply* reply = client_.restPost(
        QStringLiteral("/rest/v1/") + remoteTable,
        QJsonDocument(rows).toJson(QJsonDocument::Compact),
        QByteArrayLiteral("resolution=merge-duplicates,return=minimal"));

    connect(reply, &QNetworkReply::finished, this, [this, reply, count = rows.size()]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (status < 200 || status >= 300) {
            if (status == 401 && isExpiredTokenError(body) && !authRefreshAttempted_) {
                authRefreshAttempted_ = true;
                handleAuthExpiry([this]() { pushNextBatch(); });
                return;
            }

            if (isTransientStatus(status) &&
                scheduleRetry(QStringLiteral("Sin conexion con el servidor."),
                             [this]() { pushNextBatch(); })) {
                return;
            }

            const QString detail =
                status == 0 ? reply->errorString()
                            : errorMessage(body, QStringLiteral("HTTP %1").arg(status));
            fail(QStringLiteral("No se pudieron subir los cambios: ") + detail);
            return;
        }

        retryCount_ = 0;

        try {
            repository_.markOutboxSent(inFlight_);
        } catch (const std::exception& error) {
            fail(QString::fromUtf8(error.what()));
            return;
        }

        pushed_ += static_cast<int>(count);
        emit progress(QStringLiteral("Subiendo cambios locales…"), pushed_, totalToPush_);
        pushNextBatch();
    });
}

void SyncEngine::startPull() {
    pullTableIndex_ = 0;
    retryCount_ = 0;
    emit progress(QStringLiteral("Bajando cambios del servidor…"), pulled_, -1);
    pullNextPage();
}

void SyncEngine::pullNextPage() {
    if (pullTableIndex_ >= kTables.size()) {
        succeed();
        return;
    }

    const QString localTable = QString::fromLatin1(kTables[pullTableIndex_]);
    const QString remoteTable = remoteTableFor(localTable);

    QString path = QStringLiteral("/rest/v1/") + remoteTable +
                   QStringLiteral("?select=*&user_id=eq.") + client_.userId() +
                   QStringLiteral("&order=updated_at.asc&limit=%1").arg(kPullPage);

    const QString currentCursor = cursors_[pullTableIndex_];
    if (!currentCursor.isEmpty()) {
        QString encodedCursor = currentCursor;
        encodedCursor.replace(QLatin1Char('+'), QStringLiteral("%2B"));
        path += QStringLiteral("&updated_at=gt.") + encodedCursor;
    }

    QNetworkReply* reply = client_.restGet(path);

    connect(reply, &QNetworkReply::finished, this, [this, reply, localTable]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (status < 200 || status >= 300) {
            if (status == 401 && isExpiredTokenError(body) && !authRefreshAttempted_) {
                authRefreshAttempted_ = true;
                handleAuthExpiry([this]() { pullNextPage(); });
                return;
            }

            if (isTransientStatus(status) &&
                scheduleRetry(QStringLiteral("Sin conexion con el servidor."),
                             [this]() { pullNextPage(); })) {
                return;
            }

            const QString detail =
                status == 0 ? reply->errorString()
                            : errorMessage(body, QStringLiteral("HTTP %1").arg(status));
            fail(QStringLiteral("No se pudieron bajar los cambios: ") + detail);
            return;
        }

        retryCount_ = 0;

        const QJsonArray rows = QJsonDocument::fromJson(body).array();
        if (rows.isEmpty()) {
            ++pullTableIndex_;
            pullNextPage();
            return;
        }

        QString maxUpdatedAt = cursors_[pullTableIndex_];
        int appliedHere = 0;

        try {
            for (const QJsonValue& value : rows) {
                const QJsonObject row = value.toObject();

                const QString updatedAt = row[QStringLiteral("updated_at")].toString();
                if (updatedAt > maxUpdatedAt) {
                    maxUpdatedAt = updatedAt;
                }

                bool applied = false;
                if (localTable == QLatin1String(kTables[0])) {
                    core::Pocket pocket = storage::pocketFrom(row);
                    if (!pocket.id.empty()) {
                        applied = repository_.applyRemote(pocket);
                    }
                } else if (localTable == QLatin1String(kTables[1])) {
                    core::Job job = storage::jobFrom(row);
                    if (!job.id.empty()) {
                        applied = repository_.applyRemote(job);
                    }
                } else if (localTable == QLatin1String(kTables[2])) {
                    core::Movement movement = storage::movementFrom(row);
                    if (!movement.id.empty()) {
                        applied = repository_.applyRemote(movement);
                    }
                }

                if (applied) {
                    ++appliedHere;
                }
            }

            if (!maxUpdatedAt.isEmpty() && maxUpdatedAt != cursors_[pullTableIndex_]) {
                cursors_[pullTableIndex_] = maxUpdatedAt;
                const QString key = QStringLiteral("sync.cursor.") + localTable;
                repository_.setSetting(key, maxUpdatedAt);
            }
        } catch (const std::exception& error) {
            fail(QString::fromUtf8(error.what()));
            return;
        }

        pulled_ += appliedHere;

        if (rows.size() < kPullPage) {
            ++pullTableIndex_;
        }

        emit progress(QStringLiteral("Bajando cambios del servidor…"), pulled_, -1);
        pullNextPage();
    });
}

} // namespace dake::sync
