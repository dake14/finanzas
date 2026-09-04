//
// dake/sync/supabase_client.cpp — cliente HTTPS de Supabase.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `finaldake-labs/sync/src/supabase_client.cpp` (229 lineas) TAL CUAL,
// con una sola diferencia: la tabla que sondea `probe()`.
//
// Implementar exactamente las firmas declaradas en supabase_client.hpp, sin
// agregar ni quitar ninguna:
//
//   ctor, config(), setConfig(), isSignedIn(), userEmail(), userId(),
//   refreshToken(), restGet(), restPost(), probe(), signInWithPassword(),
//   restoreSession(), refreshSession(), signOut(),
//   buildRequest(), handleTokenReply()
//
// LA UNICA DIFERENCIA CON EL ORIGINAL:
//
//   probe() consulta la tabla `v2_movements`, no `movements`. La vieja es de la
//   aplicacion que se descontinua y tiene otro esquema; encontrarla no prueba
//   nada sobre esta. La ruta queda:
//
//       /rest/v1/v2_movements?select=id&limit=1
//
//   `probeFinished(reachable, tableReady, detail)` sigue separando los dos
//   fallos: `reachable=false` es URL o clave mal; `reachable=true` con
//   `tableReady=false` es que falta correr supabase_v2.sql en el panel.
//
// TODO LO DEMAS SE PORTA SIN CAMBIOS, incluido:
//   - El token de acceso vive solo en memoria; el de refresco lo devuelve
//     refreshToken() para que otro lo persista. La contrasena no se guarda
//     nunca, ni en memoria despues del login.
//   - Todas las llamadas son asincronas. Nada de QEventLoop anidado ni de
//     esperas bloqueantes: con mala senal eso traba la ventana entera.
//   - Quien llama es dueno del QNetworkReply y le hace deleteLater().
//   - Las cabeceras (apikey + Authorization) se arman en buildRequest() y en
//     ningun otro lado.
//
// DEPENDENCIAS PERMITIDAS: las mismas que el archivo original —QNetworkAccess-
// Manager, QNetworkRequest, QNetworkReply, QJsonDocument, QJsonObject, QUrl,
// QUrlQuery, QString, QByteArray— mas config.hpp y supabase_client.hpp. No
// agregar ninguna dependencia nueva.
//
// ACEPTACION: compila sin advertencias con /W4; `grep -c "movements"` no
// encuentra ninguna aparicion sin el prefijo v2_; no aparece la palabra
// QEventLoop en el archivo.
//
#include "dake/sync/supabase_client.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace dake::sync {
namespace {

/// Extrae el mensaje que manda Supabase. La API usa varios nombres segun el
/// subsistema (`msg` en autenticacion, `message` en PostgREST), asi que se
/// prueban todos antes de caer en el texto crudo.
[[nodiscard]] QString errorMessage(const QByteArray& body, const QString& fallback) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        for (const char* field : {"error_description", "msg", "message", "error"}) {
            const QString value = object[QLatin1String(field)].toString();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return fallback;
}

} // namespace

SupabaseClient::SupabaseClient(SupabaseConfig config, QObject* parent)
    : QObject(parent), config_(std::move(config)) {}

const SupabaseConfig& SupabaseClient::config() const noexcept {
    return config_;
}

void SupabaseClient::setConfig(SupabaseConfig config) {
    config_ = std::move(config);
    // Cambiar de proyecto invalida cualquier sesion: los tokens estan firmados
    // por el proyecto anterior y no valen en el nuevo.
    signOut();
}

bool SupabaseClient::isSignedIn() const noexcept {
    return !accessToken_.isEmpty();
}

QString SupabaseClient::userEmail() const noexcept {
    return userEmail_;
}

QString SupabaseClient::userId() const noexcept {
    return userId_;
}

QString SupabaseClient::refreshToken() const noexcept {
    return refreshToken_;
}

QNetworkRequest SupabaseClient::buildRequest(const QString& path, bool withAuth) const {
    QNetworkRequest request(QUrl(config_.url + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    // `apikey` identifica al proyecto y va SIEMPRE. `Authorization` lleva la
    // sesion del usuario cuando existe; sin ella, PostgREST evalua las
    // politicas RLS como anonimo y no devuelve nada del usuario.
    request.setRawHeader("apikey", config_.anonKey.toUtf8());

    const QString bearer = (withAuth && !accessToken_.isEmpty()) ? accessToken_ : config_.anonKey;
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + bearer.toUtf8());

    return request;
}

QNetworkReply* SupabaseClient::restGet(const QString& pathWithQuery) {
    return network_.get(buildRequest(pathWithQuery, true));
}

QNetworkReply* SupabaseClient::restPost(const QString& pathWithQuery,
                                        const QByteArray& body,
                                        const QByteArray& prefer) {
    QNetworkRequest request = buildRequest(pathWithQuery, true);
    if (!prefer.isEmpty()) {
        request.setRawHeader("Prefer", prefer);
    }
    return network_.post(request, body);
}

void SupabaseClient::probe() {
    if (!config_.isValid()) {
        emit probeFinished(false, false,
                           QStringLiteral("Faltan la URL o la clave del proyecto."));
        return;
    }

    // Se pide una sola fila: alcanza para distinguir "no existe la tabla" de
    // "el proyecto no responde" sin traerse datos.
    QNetworkReply* reply =
        network_.get(buildRequest(QStringLiteral("/rest/v1/v2_movements?select=id&limit=1"), true));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (status == 0) {
            emit probeFinished(false, false,
                               QStringLiteral("No se pudo conectar: ") + reply->errorString());
            return;
        }

        if (status == 200) {
            emit probeFinished(true, true, QStringLiteral("Proyecto y tabla listos."));
            return;
        }

        if (status == 401 || status == 403) {
            emit probeFinished(true, false,
                               QStringLiteral("La clave no fue aceptada: ") +
                                   errorMessage(body, QStringLiteral("credenciales invalidas")));
            return;
        }

        if (status == 404) {
            // El proyecto contesto, asi que la clave sirve; lo que falta es el
            // esquema. Es un fallo con una solucion muy concreta.
            emit probeFinished(true, false,
                               QStringLiteral("El proyecto responde, pero falta la tabla "
                                              "'v2_movements'. Corre supabase_v2.sql en el editor "
                                              "SQL del panel de Supabase."));
            return;
        }

        emit probeFinished(true, false,
                           QStringLiteral("Respuesta inesperada (HTTP %1): %2")
                               .arg(status)
                               .arg(errorMessage(body, QString::fromUtf8(body.left(200)))));
    });
}

void SupabaseClient::handleTokenReply(QNetworkReply* reply, const QString& failureContext) {
    connect(reply, &QNetworkReply::finished, this, [this, reply, failureContext]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (status != 200) {
            const QString detail =
                status == 0 ? reply->errorString()
                            : errorMessage(body, QStringLiteral("HTTP %1").arg(status));
            emit authFailed(failureContext + QStringLiteral(": ") + detail);
            return;
        }

        const QJsonObject object = QJsonDocument::fromJson(body).object();
        accessToken_ = object[QStringLiteral("access_token")].toString();
        refreshToken_ = object[QStringLiteral("refresh_token")].toString();

        const QJsonObject user = object[QStringLiteral("user")].toObject();
        userEmail_ = user[QStringLiteral("email")].toString();
        userId_ = user[QStringLiteral("id")].toString();

        if (accessToken_.isEmpty()) {
            emit authFailed(failureContext +
                            QStringLiteral(": la respuesta no trajo un token de acceso."));
            return;
        }

        emit signedIn(userEmail_);
    });
}

void SupabaseClient::signInWithPassword(const QString& email, const QString& password) {
    if (!config_.isValid()) {
        emit authFailed(QStringLiteral("Faltan la URL o la clave del proyecto."));
        return;
    }

    QJsonObject payload;
    payload[QStringLiteral("email")] = email;
    payload[QStringLiteral("password")] = password;

    QNetworkReply* reply = network_.post(
        buildRequest(QStringLiteral("/auth/v1/token?grant_type=password"), false),
        QJsonDocument(payload).toJson(QJsonDocument::Compact));

    handleTokenReply(reply, QStringLiteral("No se pudo iniciar sesion"));
}

void SupabaseClient::restoreSession(const QString& token) {
    if (token.isEmpty() || !config_.isValid()) {
        return;
    }

    QJsonObject payload;
    payload[QStringLiteral("refresh_token")] = token;

    QNetworkReply* reply = network_.post(
        buildRequest(QStringLiteral("/auth/v1/token?grant_type=refresh_token"), false),
        QJsonDocument(payload).toJson(QJsonDocument::Compact));

    handleTokenReply(reply, QStringLiteral("No se pudo reanudar la sesion"));
}

void SupabaseClient::refreshSession() {
    if (refreshToken_.isEmpty()) {
        emit authFailed(QStringLiteral("No hay una sesion guardada para renovar."));
        return;
    }
    restoreSession(refreshToken_);
}

void SupabaseClient::signOut() {
    const bool had = isSignedIn();
    accessToken_.clear();
    refreshToken_.clear();
    userEmail_.clear();
    userId_.clear();
    if (had) {
        emit signedOut();
    }
}

} // namespace dake::sync
