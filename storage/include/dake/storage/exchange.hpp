#pragma once
//
// dake/storage/exchange.hpp — mover los datos entre el telefono y la computadora.
//
// Un archivo JSON-lines: un registro por linea, primero los bolsillos, despues
// los trabajos y al final los movimientos. Ese orden importa poco para el
// resultado —los reportes toleran un movimiento cuyo bolsillo todavia no
// llego— pero hace que el archivo se lea de arriba abajo sin saltar.
//
// Reglas:
//
//   * Gana el HLC mas alto. Importar dos veces el mismo archivo deja
//     exactamente el mismo estado: la operacion es idempotente y se puede
//     repetir sin pensar.
//   * Las lapidas VIAJAN. Sin ellas, algo borrado en el telefono reaparece en
//     la computadora en la siguiente importacion, que es la forma mas molesta
//     de perder la confianza en una sincronizacion.
//   * Una linea invalida no aborta el archivo entero: se cuenta y se sigue.
//     Cortar en la primera deja al usuario sin los 200 registros buenos por
//     culpa de uno malo.
//
#include <QString>

#include "dake/storage/repository.hpp"

namespace dake::storage {

struct ExchangeReport {
    int total = 0;         ///< lineas de datos leidas
    int applied = 0;       ///< registros que entraron o pisaron a uno mas viejo
    int skippedOlder = 0;  ///< llegaron con un HLC menor al que ya habia
    int malformed = 0;     ///< lineas que no se pudieron entender
};

/// Escribe todo —incluidas las lapidas— en `path`. Devuelve cuantos registros
/// se escribieron. En Android `path` puede ser una URL `content://` del
/// selector del sistema: QFile las entiende.
int exportAll(Repository& repository, const QString& path);

/// Aplica el archivo sobre la base local.
[[nodiscard]] ExchangeReport importFile(Repository& repository, const QString& path);

} // namespace dake::storage
