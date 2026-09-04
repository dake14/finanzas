//
// dake/sync/sync_engine.cpp — el motor.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `finaldake-labs/sync/src/sync_engine.cpp` (394 lineas) generalizandolo
// de UNA tabla a TRES. Toda la resiliencia del original se conserva sin tocar:
// la escalera de reintentos (2 s, 5 s, 10 s, hasta 3), la renovacion unica de
// sesion ante un 401, y que nada se marque enviado antes de la confirmacion.
//
// Implementar exactamente las firmas de sync_engine.hpp. Ni una mas.
//
// EL ORDEN DE LAS TABLAS
//
//   `kTables` = {"pockets", "jobs", "movements"}. Ese orden se respeta al subir
//   y al bajar. Un movimiento referencia bolsillos y trabajos: si bajara antes
//   que ellos, la referencia quedaria colgando.
//
//   remoteTableFor("pockets") devuelve "v2_pockets". Es prefijo "v2_" y nada
//   mas. Las tablas sin prefijo son de la aplicacion vieja y no se tocan.
//
// SUBIDA
//
//   1. repository_.pendingOutbox(50). Ya viene ordenada por tabla y por orden
//      de insercion.
//   2. Agrupar por `tableName` y mandar un POST por grupo a
//        /rest/v1/<tabla remota>
//      con cabecera  Prefer: resolution=merge-duplicates  (upsert).
//   3. A CADA objeto del cuerpo, ANTES de mandarlo:
//        - QUITARLE la clave "t". La pone toJson() como discriminador de tipo
//          para el archivo de importar y exportar, donde las tres entidades
//          conviven en un mismo renglon. En Postgres no existe esa columna y
//          PostgREST rechaza el upsert entero con un 400 si la ve. Aca el tipo
//          ya lo dice la tabla a la que se manda.
//        - AGREGARLE "user_id" con client_.userId(). El payload de la outbox
//          no lo trae: la base local no sabe de usuarios.
//      Al bajar, la operacion inversa no hace falta: pocketFrom y compania
//      ignoran las claves que no conocen, asi que "user_id" y "updated_at" que
//      vienen del servidor se descartan solos.
//   4. Con la respuesta 2xx, repository_.markOutboxSent(los rowId del grupo).
//      Nunca antes.
//   5. Repetir hasta que pendingOutbox venga vacia.
//
// BAJADA
//
//   Para cada tabla, en el orden de kTables:
//     GET /rest/v1/<tabla remota>
//         ?user_id=eq.<userId>
//         &updated_at=gt.<cursor de esa tabla>
//         &order=updated_at.asc
//         &limit=200
//     Cada fila se convierte con storage::pocketFrom / jobFrom / movementFrom
//     y se aplica con repository_.applyRemote(...). Contar como `pulled` solo
//     las que devolvieron true.
//     Al terminar la pagina, guardar el `updated_at` de la ultima fila como
//     cursor de ESA tabla y seguir paginando hasta que vuelva vacia.
//
//   HAY UN CURSOR POR TABLA, no uno compartido. Con uno solo, bajar movimientos
//   adelantaria el reloj de los bolsillos y los cambios de bolsillo ocurridos
//   en el medio no bajarian nunca. Se guardan en `settings`:
//
//       sync.cursor.pockets    sync.cursor.jobs    sync.cursor.movements
//
//   via repository_.setting() / setSetting().
//
// SESION
//
//   El refresh token se persiste en la clave `sync.refresh_token` y el correo
//   en `sync.user_email`, con las mismas llamadas a setting()/setSetting().
//   No hay tabla sync_meta en este proyecto.
//
// SEÑALES
//
//   progress(mensaje, actual, total) durante todo. En la bajada total es -1
//   mientras no se sepa cuanto falta.
//   finished(pushed, pulled, error) una sola vez por corrida, con error vacio
//   si salio bien.
//
// PROHIBIDO
//
//   - Cualquier espera bloqueante: nada de QEventLoop ni de waitForReadyRead.
//     Una corrida con mala senal trabaria la ventana entera.
//   - Tocar las tablas sin prefijo v2_.
//   - Escribir en la outbox: eso es de applyRemote, y applyRemote justamente no
//     lo hace, para no crear un bucle de eco.
//
// DEPENDENCIAS PERMITIDAS: QObject, QTimer, QNetworkReply, QJsonDocument,
// QJsonObject, QJsonArray, QString, QUrlQuery, <array>, <functional>, <vector>,
// mas sync_engine.hpp, supabase_client.hpp, dake/storage/repository.hpp y
// dake/storage/wire.hpp. Ninguna otra.
//
// ACEPTACION: compila sin advertencias con /W4; no aparece "QEventLoop" en el
// archivo; no aparece ninguna cadena "movements" que no venga precedida de
// "v2_" salvo dentro de kTables y de las claves sync.cursor.*; una corrida sin
// credenciales termina con finished(0, 0, <error no vacio>) y sin colgarse.
//
#include "dake/sync/sync_engine.hpp"

namespace dake::sync {

// TODO(agy): portar y generalizar segun el contrato de arriba.

} // namespace dake::sync
