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
