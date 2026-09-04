#pragma once
//
// dake/storage/wire.hpp — la forma JSON de un registro.
//
// Estas funciones ya existian dentro de `exchange.cpp`, en un namespace
// anonimo, para el archivo de importar y exportar. Salen aca porque ahora hay
// un segundo consumidor: la cola de salida de la sincronizacion guarda el
// mismo JSON, y el motor lee del servidor filas con esas mismas claves.
//
// Que sea UNA sola serializacion y no dos no es prolijidad: si el archivo y la
// nube divergieran, exportar y sincronizar producirian datos distintos para el
// mismo movimiento, y el error aparecería meses despues, en la maquina del
// otro lado.
//
// Las claves son snake_case a proposito: son las mismas que los nombres de
// columna en SQLite y en Postgres, asi que el JSON se lee al lado de una
// consulta sin traducir nada mentalmente.
//
// Lo que NO va aca: `user_id`. El modelo local no lo tiene y no debe tenerlo
// —una base local es de un solo usuario por definicion—; lo agrega el motor de
// sincronizacion al subir, tomandolo de la sesion.
//
#include <QJsonObject>

#include "dake/core/model.hpp"

namespace dake::storage {

[[nodiscard]] QJsonObject toJson(const core::Pocket& pocket);
[[nodiscard]] QJsonObject toJson(const core::Job& job);
[[nodiscard]] QJsonObject toJson(const core::Movement& movement);

/// Reconstruyen desde JSON. Un campo ausente toma el valor por defecto del
/// modelo; nunca lanzan. Un registro que llega incompleto es preferible a una
/// sincronizacion que se corta entera por una fila rara.
[[nodiscard]] core::Pocket pocketFrom(const QJsonObject& object);
[[nodiscard]] core::Job jobFrom(const QJsonObject& object);
[[nodiscard]] core::Movement movementFrom(const QJsonObject& object);

} // namespace dake::storage
