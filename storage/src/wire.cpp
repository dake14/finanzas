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

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "dake/core/model.hpp"

namespace dake::storage {
namespace {

[[nodiscard]] QString qs(const std::string& text) {
    return QString::fromStdString(text);
}

[[nodiscard]] std::string ss(const QJsonValue& value) {
    return value.toString().toStdString();
}

} // namespace

// ------------------------------------------------------------- a JSON ------

[[nodiscard]] QJsonObject toJson(const core::Pocket& pocket) {
    const auto kind = core::toString(pocket.kind);
    return QJsonObject{
        {"t", "pocket"},
        {"id", qs(pocket.id)},
        {"name", qs(pocket.name)},
        {"kind", QString::fromUtf8(kind.data(), static_cast<int>(kind.size()))},
        // El importe va como numero JSON, no como texto: cabe exacto en un
        // double hasta 2^53 unidades minimas, que son 90 mil millones de
        // dolares. Mas alla de eso el problema no seria el formato.
        {"opening_minor", static_cast<double>(pocket.openingMinor)},
        {"archived", pocket.archived},
        {"hlc", qs(pocket.hlc)},
        {"device_id", qs(pocket.deviceId)},
        {"deleted", pocket.deleted},
    };
}

[[nodiscard]] QJsonObject toJson(const core::Job& job) {
    return QJsonObject{
        {"t", "job"},
        {"id", qs(job.id)},
        {"name", qs(job.name)},
        {"client", qs(job.client)},
        {"opened", qs(job.opened.toIso())},
        {"closed", job.closed},
        {"hlc", qs(job.hlc)},
        {"device_id", qs(job.deviceId)},
        {"deleted", job.deleted},
    };
}

[[nodiscard]] QJsonObject toJson(const core::Movement& movement) {
    const auto kind = core::toString(movement.kind);
    const auto recurrence = core::toString(movement.recurrence);
    return QJsonObject{
        {"t", "movement"},
        {"id", qs(movement.id)},
        {"date", qs(movement.date.toIso())},
        {"name", qs(movement.name)},
        {"kind", QString::fromUtf8(kind.data(), static_cast<int>(kind.size()))},
        {"amount_minor", static_cast<double>(movement.amountMinor)},
        {"pocket_id", qs(movement.pocketId)},
        {"target_pocket_id", qs(movement.targetPocketId)},
        {"category", qs(movement.category)},
        {"job_id", qs(movement.jobId)},
        {"spread_months", movement.spreadMonths},
        {"settled", movement.settled},
        {"recurrence", QString::fromUtf8(recurrence.data(), static_cast<int>(recurrence.size()))},
        {"hlc", qs(movement.hlc)},
        {"device_id", qs(movement.deviceId)},
        {"deleted", movement.deleted},
    };
}

// ----------------------------------------------------------- desde JSON ----

[[nodiscard]] core::Pocket pocketFrom(const QJsonObject& object) {
    core::Pocket pocket;
    pocket.id = ss(object.value("id"));
    pocket.name = ss(object.value("name"));
    pocket.kind = core::pocketKindFromString(ss(object.value("kind")));
    pocket.openingMinor = static_cast<std::int64_t>(object.value("opening_minor").toDouble());
    pocket.archived = object.value("archived").toBool();
    pocket.hlc = ss(object.value("hlc"));
    pocket.deviceId = ss(object.value("device_id"));
    pocket.deleted = object.value("deleted").toBool();
    return pocket;
}

[[nodiscard]] core::Job jobFrom(const QJsonObject& object) {
    core::Job job;
    job.id = ss(object.value("id"));
    job.name = ss(object.value("name"));
    job.client = ss(object.value("client"));
    job.opened = core::Date::fromIso(ss(object.value("opened")));
    job.closed = object.value("closed").toBool();
    job.hlc = ss(object.value("hlc"));
    job.deviceId = ss(object.value("device_id"));
    job.deleted = object.value("deleted").toBool();
    return job;
}

[[nodiscard]] core::Movement movementFrom(const QJsonObject& object) {
    core::Movement movement;
    movement.id = ss(object.value("id"));
    movement.date = core::Date::fromIso(ss(object.value("date")));
    movement.name = ss(object.value("name"));
    movement.kind = core::movementKindFromString(ss(object.value("kind")));
    movement.amountMinor = static_cast<std::int64_t>(object.value("amount_minor").toDouble());
    movement.pocketId = ss(object.value("pocket_id"));
    movement.targetPocketId = ss(object.value("target_pocket_id"));
    movement.category = ss(object.value("category"));
    movement.jobId = ss(object.value("job_id"));
    movement.spreadMonths = object.value("spread_months").toInt(1);
    movement.settled = object.value("settled").toBool(true);
    movement.recurrence = core::recurrenceFromString(ss(object.value("recurrence")));
    movement.hlc = ss(object.value("hlc"));
    movement.deviceId = ss(object.value("device_id"));
    movement.deleted = object.value("deleted").toBool();
    return movement;
}

} // namespace dake::storage
