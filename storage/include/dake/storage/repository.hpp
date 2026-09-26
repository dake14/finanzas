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

#include "dake/core/accounts.hpp"
#include "dake/core/model.hpp"
#include "dake/core/repairs.hpp"
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

    // --- Datos locales ----------------------------------------------------
    //
    // Todo lo que sigue vive en tablas que NO se sincronizan. El motor sube el
    // JSON de cada fila tal cual, y el servidor rechaza la fila entera si trae
    // una columna que no conoce: agregarle campos a pockets, jobs o movements
    // frenaria la sincronizacion del telefono. Por eso los datos nuevos van
    // al costado, unidos por id, y sobreviven a que la fila sincronizada se
    // reemplace al bajar del servidor.

    /// Fija a mano la cuenta de un bolsillo. `std::nullopt` la devuelve a la
    /// que corresponde por tipo.
    void setPocketAccount(const core::Id& pocketId, std::optional<core::Account> account);

    /// Las categorias con cuenta y clase, por nombre.
    [[nodiscard]] std::vector<core::Category> loadCategories();

    /// Crea o reemplaza. El nombre no distingue mayusculas: guardar "luz"
    /// cuando existe "Luz" la reemplaza en vez de crear otra.
    void saveCategory(const core::Category& category);

    void removeCategory(const std::string& name);

    // --- Reparaciones -----------------------------------------------------

    /// Una ficha por cada trabajo vivo. Un trabajo sin ficha —los de antes de
    /// la version 3, los que llegan del telefono— vuelve como reparacion de
    /// tipo Otro, con el nombre del trabajo como equipo, recibida el dia que
    /// se abrio, y entregada si el trabajo estaba cerrado.
    [[nodiscard]] std::vector<core::Repair> loadRepairs();
    void saveRepair(const core::Repair& repair);

    [[nodiscard]] std::vector<core::RepairPart> loadRepairParts();
    void saveRepairPart(const core::RepairPart& part);
    void removeRepairPart(const core::Id& partId);

    /// La primera vez siembra defaultTemplates(). Una sola vez: si despues se
    /// borran todas, no vuelven.
    [[nodiscard]] std::vector<core::RepairTemplate> loadTemplates();
    void saveTemplate(const core::RepairTemplate& tpl);
    void removeTemplate(const core::Id& templateId);

    /// Tarifa y margen objetivo, de los ajustes. La tasa de fijos por hora no
    /// se guarda: se calcula.
    [[nodiscard]] core::CostSettings loadCostSettings();
    void saveCostSettings(const core::CostSettings& settings);

    /// Anota cuanto tardo una captura ("captura", "reparacion", "revision").
    void addTiming(const QString& what, qint64 millis);

    /// La mediana de las ultimas `last` mediciones de `what`, en milisegundos.
    /// -1 si no hay ninguna.
    [[nodiscard]] qint64 timingMedian(const QString& what, int last = 30);

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

    /// Vacia la base entera con DELETE, sin lapidas y sin encolar nada.
    ///
    /// NO LA LLAME LA INTERFAZ. Un borrado que no encola no viaja: el otro
    /// aparato sigue teniendo todo, y como los cursores viven en `settings`
    /// —que esta funcion tampoco toca— lo borrado aca tampoco vuelve a bajar,
    /// porque el servidor lo tiene con fecha anterior al cursor.
    ///
    /// Existe para las pruebas, que necesitan una base vacia de verdad y sin
    /// filas marcadas. Para borrar lo del usuario esta deleteEverything().
    void wipe();

    /// Borra todo lo del usuario dejando lapida de cada fila, para que el
    /// borrado llegue a los otros aparatos.
    ///
    /// Pasa por remove() en cada registro, que marca `deleted = 1`, sube el
    /// hlc y encola una fila `Delete`. Es la unica forma de borrar que puede
    /// llamar la interfaz.
    ///
    /// Borra en este orden: movimientos, despues trabajos, despues bolsillos.
    /// Un movimiento apunta a un bolsillo y a un trabajo; empezar por el
    /// contenedor dejaria movimientos colgados durante el intervalo, y si la
    /// transaccion falla en el medio esa es exactamente la base que queda.
    ///
    /// Todo dentro de una transaccion: se van todos o no se va ninguno.
    ///
    /// @return cuantos registros se borraron, sumando los tres tipos.
    /// @throws StorageError si la transaccion no se puede abrir o confirmar.
    std::size_t deleteEverything();

private:
    Database& db_;
};

/// UUIDv7 nuevo, con el reloj del sistema.
[[nodiscard]] core::Id newId();

/// Identificador estable de este equipo, guardado en `settings`.
[[nodiscard]] QString deviceId(Repository& repository);

} // namespace dake::storage
