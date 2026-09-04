#pragma once
//
// dake/sync/supabase_client.hpp
//
// Cliente HTTPS de Supabase: autenticacion y acceso a la API REST.
//
// Todo es asincrono. Una llamada de red bloqueante congelaria la ventana el
// tiempo que tarde el servidor, y con mala senal eso es la app entera trabada.
//
// El token de acceso NO se guarda en disco; el de refresco SI, en la tabla
// `sync_meta` de la base local. Asi cerrar la aplicacion no obliga a escribir
// la contrasena de nuevo, y la contrasena en si nunca se almacena.
//
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include "dake/sync/config.hpp"

namespace dake::sync {

class SupabaseClient : public QObject {
    Q_OBJECT

public:
    explicit SupabaseClient(SupabaseConfig config, QObject* parent = nullptr);

    [[nodiscard]] const SupabaseConfig& config() const noexcept;
    void setConfig(SupabaseConfig config);

    /// true si hay un token de acceso vigente en memoria.
    [[nodiscard]] bool isSignedIn() const noexcept;

    /// Correo del usuario conectado, vacio si no hay sesion.
    [[nodiscard]] QString userEmail() const noexcept;

    /// Identificador del usuario en Supabase. Es el `user_id` que va en cada
    /// fila y contra el que se evaluan las politicas RLS.
    [[nodiscard]] QString userId() const noexcept;

    /// Token de refresco vigente, para que quien maneje la persistencia lo
    /// guarde. Vacio si no hay sesion.
    [[nodiscard]] QString refreshToken() const noexcept;

    // --- Acceso REST -------------------------------------------------------
    //
    // Se exponen aca y no en cada usuario del cliente para que el armado de
    // cabeceras (apikey + token de sesion) viva en un solo lugar. Quien llama
    // es dueno del QNetworkReply y debe hacerle deleteLater().

    [[nodiscard]] class QNetworkReply* restGet(const QString& pathWithQuery);

    /// `prefer` va en la cabecera Prefer de PostgREST; sirve para pedir
    /// upsert ("resolution=merge-duplicates").
    [[nodiscard]] class QNetworkReply* restPost(const QString& pathWithQuery,
                                                const QByteArray& body,
                                                const QByteArray& prefer = {});

public slots:
    /// Comprueba que la URL y la clave respondan, sin necesidad de sesion.
    /// Emite `probeFinished`.
    void probe();

    void signInWithPassword(const QString& email, const QString& password);

    /// Reanuda una sesion guardada. `token` sale de la base local.
    void restoreSession(const QString& token);

    /// Renueva la sesion actual con el refresh token que ya tiene el cliente.
    /// Emite `authFailed` de inmediato si no hay ninguno guardado: sin eso,
    /// quien pide la renovacion (SyncEngine) se quedaria esperando una señal
    /// que nunca llega.
    void refreshSession();

    void signOut();

signals:
    /// `reachable` dice si el proyecto respondio; `tableReady` si la tabla
    /// `v2_movements` ya existe. Se separan porque son dos fallos distintos
    /// con dos soluciones distintas: uno es la URL o la clave, el otro es que
    /// falta correr supabase_v2.sql en el panel.
    void probeFinished(bool reachable, bool tableReady, const QString& detail);

    void signedIn(const QString& email);
    void signedOut();
    void authFailed(const QString& message);

private:
    [[nodiscard]] QNetworkRequest buildRequest(const QString& path, bool withAuth) const;
    void handleTokenReply(class QNetworkReply* reply, const QString& failureContext);

    SupabaseConfig config_;
    QNetworkAccessManager network_;

    QString accessToken_;
    QString refreshToken_;
    QString userEmail_;
    QString userId_;
};

} // namespace dake::sync
