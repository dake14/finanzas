//
// dake/storage/wire.cpp — la forma JSON de un registro.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Esto es una MUDANZA, no codigo nuevo. Las seis funciones ya existen, escritas
// y probadas, dentro del namespace anonimo de `storage/src/exchange.cpp`:
//
//     toJson(const core::Pocket&)       pocketFrom(const QJsonObject&)
//     toJson(const core::Job&)          jobFrom(const QJsonObject&)
//     toJson(const core::Movement&)     movementFrom(const QJsonObject&)
//
// Moverlas aca TAL CUAL, al namespace dake::storage, con las firmas que declara
// `dake/storage/wire.hpp`. Traer tambien los helpers que usen (`qs`, `ss` y
// cualquier otro del namespace anonimo de exchange.cpp del que dependan).
//
// PROHIBIDO cambiar el comportamiento:
//   - Ni una clave del JSON cambia de nombre ni de tipo.
//   - Ni un valor por defecto cambia.
//   - No se agrega "user_id": el modelo local no tiene usuarios; esa clave la
//     agrega el motor de sincronizacion al subir.
//
// Si algo del original parece mejorable, se deja como esta. Este archivo tiene
// que producir byte por byte el mismo JSON que hoy, porque los archivos ya
// exportados tienen que seguir importandose igual.
//
// DEPENDENCIAS PERMITIDAS: QJsonObject, QJsonValue, QString, dake/core/model.hpp
// y wire.hpp. Las mismas que ya usaba exchange.cpp para estas funciones.
//
// ACEPTACION: compila sin advertencias con /W4; dake_storagetest sigue pasando
// —es el que ejercita la ida y vuelta a disco— y su salida no cambia.
//
#include "dake/storage/wire.hpp"

namespace dake::storage {

// TODO(agy): mover las seis funciones desde exchange.cpp segun el contrato.

} // namespace dake::storage
