#pragma once
//
// mobile/appbridge.hpp — lo unico que QML puede tocar.
//
// La interfaz del telefono no hace cuentas propias ni abre la base: pide y
// muestra. Toda cifra que aparezca en pantalla salio de una funcion de
// dake::core que tiene una prueba escrita, igual que en la de escritorio. Que
// las dos interfaces compartan el nucleo es lo que garantiza que digan lo
// mismo; si cada una calculara lo suyo, tarde o temprano no coincidirian y no
// habria forma de saber cual miente.
//
// Los datos van a QML como QVariantList/QVariantMap y no como modelos propios:
// son cientos de filas al año, no millones, y un QAbstractListModel por cada
// lista seria complejidad comprada de mas.
//
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "dake/core/hlc.hpp"
#include "dake/storage/repository.hpp"
#include "dake/sync/supabase_client.hpp"
#include "dake/sync/sync_engine.hpp"

namespace dake::mobile {

class AppBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList pockets READ pockets NOTIFY dataChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY dataChanged)
    Q_PROPERTY(QVariantList recent READ recent NOTIFY dataChanged)
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY dataChanged)
    Q_PROPERTY(QString today READ today CONSTANT)
    Q_PROPERTY(QString dbPath READ dbPath CONSTANT)

    // --- Nube --------------------------------------------------------------
    //
    // Tres propiedades y dos acciones. El telefono no configura nada: la URL y
    // la clave vienen incrustadas al compilar, porque en Android no hay forma
    // comoda de dejarle un archivo a la aplicacion. Lo unico que no se puede
    // saber solo es la contrasena.
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY cloudChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY cloudChanged)
    Q_PROPERTY(QString cloudStatus READ cloudStatus NOTIFY cloudChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY cloudChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY cloudChanged)

public:
    explicit AppBridge(QObject* parent = nullptr);
    ~AppBridge() override;

    [[nodiscard]] QVariantList pockets() const { return pockets_; }
    [[nodiscard]] QVariantList jobs() const { return jobs_; }
    [[nodiscard]] QVariantList recent() const { return recent_; }
    [[nodiscard]] QVariantList alerts() const { return alerts_; }
    [[nodiscard]] QVariantMap summary() const { return summary_; }
    [[nodiscard]] QString today() const;
    [[nodiscard]] QString dbPath() const;

    /// Guarda un movimiento. Devuelve "" si salio bien, o el motivo para
    /// mostrar. Las claves esperadas son las que arma Anotar.qml:
    /// name, amount (texto), kind (0 gasto, 1 ingreso, 2 traspaso), date (ISO),
    /// pocketId, targetPocketId, category, jobId, spreadMonths, settled.
    Q_INVOKABLE QString saveMovement(const QVariantMap& draft);

    Q_INVOKABLE QString removeMovement(const QString& id);

    /// Las categorias ya usadas para ese tipo, de la mas usada a la menos. Se
    /// aprenden de los datos: una lista fija se queda vieja el primer dia.
    Q_INVOKABLE QStringList categoriesFor(int kind) const;

    Q_INVOKABLE QString addPocket(const QString& name, int kind, const QString& opening);
    Q_INVOKABLE QString addJob(const QString& name, const QString& client);

    /// Anota la diferencia entre lo que dice la app y lo que hay de verdad,
    /// como un movimiento visible y no como un saldo corregido por debajo.
    Q_INVOKABLE QString reconcile(const QString& pocketId, const QString& realAmount);

    Q_INVOKABLE QString exportTo(const QUrl& url);
    Q_INVOKABLE QString importFrom(const QUrl& url);
    /// Nombre sugerido para el archivo, con la fecha: dos exportaciones del
    /// mismo dia no se pisan sin que nadie lo note.
    Q_INVOKABLE QString suggestedFileName() const;

    // --- Nube --------------------------------------------------------------

    [[nodiscard]] bool signedIn() const;
    [[nodiscard]] QString userEmail() const;
    [[nodiscard]] QString cloudStatus() const { return cloudStatus_; }
    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] bool syncing() const;

    /// Inicia sesion. La contrasena se usa y se descarta: no se guarda en
    /// disco, ni en un miembro, ni en un registro. Lo que se persiste es el
    /// token de refresco, que se puede revocar desde Supabase.
    Q_INVOKABLE void signIn(const QString& email, const QString& password);
    Q_INVOKABLE void signOut();

    /// Sube lo pendiente y baja lo que cambio. No hace nada si no hay sesion.
    Q_INVOKABLE void sync();

    Q_INVOKABLE QString formatMinor(qlonglong minor) const;
    /// "hoy", "ayer" o la fecha, para las listas.
    Q_INVOKABLE QString relativeDate(const QString& isoDate) const;

signals:
    void dataChanged();
    void cloudChanged();

private:
    void reload();
    void stamp(std::string& hlc, std::string& deviceId);

    std::unique_ptr<storage::Database> db_;
    std::unique_ptr<storage::Repository> repository_;
    std::unique_ptr<core::HlcClock> clock_;
    std::unique_ptr<sync::SupabaseClient> supabase_;
    std::unique_ptr<sync::SyncEngine> syncEngine_;
    QString cloudStatus_;
    QString deviceId_;
    core::Currency currency_;
    core::Date today_;

    QVariantList pockets_;
    QVariantList jobs_;
    QVariantList recent_;
    QVariantList alerts_;
    QVariantMap summary_;
};

} // namespace dake::mobile
