#pragma once
//
// dake/sync/sync_engine.hpp
//
// Motor de sincronizacion entre la base local y Supabase.
//
// Portado del motor de finaldake-labs, con una diferencia de fondo: alla habia
// una sola tabla (movements) y aca hay varias. Eso cambia el orden de todo.
//
// Una corrida hace dos cosas, en este orden:
//
//   1. SUBIR  la cola de salida  ->  upsert en el servidor
//   2. BAJAR  lo cambiado desde el cursor  ->  aplicar local
//
// Subir primero no es un detalle: si se bajara antes, un cambio local recien
// hecho podria quedar pisado por una version vieja del servidor que todavia no
// lo conoce.
//
// Las tablas se recorren siempre en el orden en que se le dan, tanto al subir
// como al bajar: bolsillos, trabajos, movimientos. Un movimiento referencia un
// bolsillo (`pocket_id`, `target_pocket_id`) y puede referenciar un trabajo
// (`job_id`), asi que bajarlo antes que ellos dejaria la referencia colgando.
// En el escritorio va al final `quotes`, lo esencial de Cotizaciones, que solo
// baja: Finanzas nunca la escribe, asi que nunca esta en la outbox.
//
// Nada se marca como enviado antes de que el servidor confirme. Si se corta la
// red a mitad de camino, esos cambios siguen pendientes y salen en la proxima
// corrida.
//
// Resiliencia, igual que en el motor original:
//   * Un fallo de red o un error 5xx del servidor se reintenta solo, con
//     espera creciente (2 s, 5 s, 10 s), hasta 3 veces por operacion.
//   * Un 401 por token vencido dispara UNA renovacion automatica con el
//     refresh token guardado y reintenta la misma operacion; si la renovacion
//     tambien falla, cierra la sesion para que la UI pida iniciar sesion de
//     nuevo en vez de quedar en un estado ambiguo.
//   * `progress` lleva cifras (actual/total) para que la UI muestre una barra
//     real y no solo un texto. `total == -1` significa "todavia no se sabe
//     cuanto falta".
//
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>

#include "dake/storage/repository.hpp"
#include "dake/sync/supabase_client.hpp"

namespace dake::sync {

/// Las tablas del escritorio, en orden de dependencia: bolsillos, trabajos y
/// movimientos, y al final lo esencial de Cotizaciones, que solo baja (nunca
/// hay cotizaciones en la outbox). El nombre es el de la tabla local, el que
/// usa la columna `table_name` de la outbox, y el de la remota sin `v2_`.
[[nodiscard]] QStringList desktopTables();

/// Las del telefono de Finanzas: no importa cotizaciones, asi que no las baja.
[[nodiscard]] QStringList phoneTables();

/// Nombre de la tabla remota que corresponde a una local.
[[nodiscard]] QString remoteTableFor(const QString& localTable);

class SyncEngine : public QObject {
    Q_OBJECT

public:
    /// `tables`: desktopTables() o phoneTables().
    SyncEngine(SupabaseClient& client,
               storage::Repository& repository,
               QStringList tables,
               QObject* parent = nullptr);

    [[nodiscard]] const QStringList& tables() const noexcept;

    [[nodiscard]] bool isRunning() const noexcept;

    /// Cuantos cambios locales esperan subir, sumando todas las tablas.
    [[nodiscard]] int pendingCount() const;

public slots:
    /// Arranca una corrida completa. Si ya hay una en curso, no hace nada:
    /// dos corridas simultaneas se pisarian el cursor.
    void sync();

signals:
    void progress(const QString& message, int current, int total);
    /// `error` vacio significa que salio bien.
    void finished(int pushed, int pulled, const QString& error);

private:
    void pushNextBatch();
    void startPull();
    void pullNextPage();
    void fail(const QString& message);
    void succeed();

    /// Pide un token nuevo con el refresh token guardado y, si sale bien,
    /// reintenta `retry`. Si la renovacion falla, cierra la sesion: quedarse
    /// con un token muerto es peor que pedir loguearse de nuevo.
    void handleAuthExpiry(std::function<void()> retry);

    /// Programa `operation` con espera creciente si quedan reintentos.
    /// Devuelve false (sin programar nada) cuando ya se agotaron.
    bool scheduleRetry(const QString& reasonForUser, std::function<void()> operation);

    SupabaseClient& client_;
    storage::Repository& repository_;

    bool running_ = false;
    bool authRefreshAttempted_ = false;
    int retryCount_ = 0;

    int pushed_ = 0;
    int pulled_ = 0;
    int totalToPush_ = 0;

    /// Indice dentro de `tables_` de la tabla que se esta bajando. La subida no
    /// lo necesita porque la outbox ya viene ordenada por tabla.
    std::size_t pullTableIndex_ = 0;

    /// Filas de la cola que el servidor ya acepto y faltan marcar.
    std::vector<qint64> inFlight_;

    /// Un cursor POR TABLA, no uno solo. Con un cursor compartido, bajar
    /// movimientos adelantaria el reloj de bolsillos y los cambios de bolsillo
    /// hechos en el medio no bajarian nunca.
    std::vector<QString> cursors_;

    QStringList tables_;
};

} // namespace dake::sync
