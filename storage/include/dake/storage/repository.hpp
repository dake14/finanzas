#pragma once
//
// dake/storage/repository.hpp — leer y escribir el modelo.
//
// La interfaz carga todo en memoria al arrancar y vuelve a leer despues de
// cada cambio. Con los volumenes de un taller —cientos de movimientos al año,
// no millones— una consulta por pantalla seria complejidad comprada de mas: la
// misma logica que se prueba sin base de datos es la que corre en produccion.
//
#include <QString>
#include <optional>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/storage/database.hpp"

namespace dake::storage {

class Repository {
public:
    explicit Repository(Database& db);

    /// `includeDeleted` trae tambien las lapidas. La interfaz nunca las quiere;
    /// el intercambio de archivos SI, porque sin ellas algo borrado en un
    /// equipo reaparece en el otro.
    [[nodiscard]] std::vector<core::Pocket> loadPockets(bool includeDeleted = false);
    [[nodiscard]] std::vector<core::Job> loadJobs(bool includeDeleted = false);
    /// Ordenados por fecha ascendente y, dentro del mismo dia, por id: el mismo
    /// orden estable que espera la interfaz.
    [[nodiscard]] std::vector<core::Movement> loadMovements(bool includeDeleted = false);

    /// Guardar encola ademas el cambio en la outbox, DENTRO DE LA MISMA
    /// TRANSACCION que el INSERT OR REPLACE. Si se cayera entre las dos, el
    /// cambio quedaria en la base y nunca en el servidor, y nadie se enteraria.
    void save(const core::Pocket& pocket);
    void save(const core::Job& job);
    void save(const core::Movement& movement);

    /// Borrado logico. Nada se borra fisicamente: una lapida se puede deshacer
    /// y se puede sincronizar; un DELETE no.
    void remove(const core::Movement& movement);
    void remove(const core::Job& job);
    void remove(const core::Pocket& pocket);

    [[nodiscard]] bool isEmpty();

    /// Carga el caso de agosto si la base esta vacia. Devuelve true si sembro.
    bool seedIfEmpty(core::Currency currency);

    [[nodiscard]] std::optional<QString> setting(const QString& key);
    void setSetting(const QString& key, const QString& value);

    // --- Sincronizacion ----------------------------------------------------
    //
    // El estado del sincronizador (cursores, refresh token, correo) vive en
    // `settings` con el prefijo "sync.", y no en una tabla `sync_meta` aparte
    // como en finaldake-labs. Es la misma forma clave/valor, la tabla ya
    // existe y `deviceId()` ya la usa: una segunda tabla identica al lado no
    // agrega nada que valga su costo.

    /// Una fila pendiente de subir.
    struct OutboxEntry {
        qint64 rowId = 0;
        QString tableName;  ///< "pockets" | "jobs" | "movements"
        QString recordId;
        QString op;         ///< "Upsert" | "Delete"
        QString payload;    ///< el objeto JSON tal como viajara
        QString hlc;
    };

    /// Aplica un registro que llego del servidor.
    ///
    /// NO escribe en la outbox, y esa es la clave: si lo hiciera, cada cambio
    /// bajado se volveria a subir y los dos equipos se mandarian el mismo dato
    /// para siempre.
    ///
    /// Devuelve true si entro o piso a uno mas viejo; false si se descarto por
    /// traer un HLC menor o igual al que ya habia. Comparar por HLC y no por
    /// fecha es lo que hace que dos equipos con relojes distintos converjan.
    bool applyRemote(const core::Pocket& incoming);
    bool applyRemote(const core::Job& incoming);
    bool applyRemote(const core::Movement& incoming);

    /// Hasta `limit` filas sin enviar, ordenadas por tabla —bolsillos,
    /// trabajos, movimientos— y despues por orden de insercion.
    [[nodiscard]] std::vector<OutboxEntry> pendingOutbox(int limit);

    /// Cuantas filas esperan subir, sin traerlas.
    [[nodiscard]] int pendingOutboxCount();

    /// Marca filas como enviadas. Se llama SOLO despues de que el servidor
    /// confirmo: marcar antes es perder el cambio si la respuesta no llega.
    void markOutboxSent(const std::vector<qint64>& rowIds);

    /// Vacia la base entera. Es un banco de pruebas: poder volver a foja cero
    /// en un clic es la mitad del valor de tenerlo.
    void wipe();

private:
    Database& db_;
};

/// UUIDv7 nuevo, con el reloj del sistema.
[[nodiscard]] core::Id newId();

/// Identificador estable de este equipo, guardado en `settings`.
[[nodiscard]] QString deviceId(Repository& repository);

} // namespace dake::storage
