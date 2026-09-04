//
// dake/sync/supabase_client.cpp — cliente HTTPS de Supabase.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `finaldake-labs/sync/src/supabase_client.cpp` (229 lineas) TAL CUAL,
// con una sola diferencia: la tabla que sondea `probe()`.
//
// Implementar exactamente las firmas declaradas en supabase_client.hpp, sin
// agregar ni quitar ninguna:
//
//   ctor, config(), setConfig(), isSignedIn(), userEmail(), userId(),
//   refreshToken(), restGet(), restPost(), probe(), signInWithPassword(),
//   restoreSession(), refreshSession(), signOut(),
//   buildRequest(), handleTokenReply()
//
// LA UNICA DIFERENCIA CON EL ORIGINAL:
//
//   probe() consulta la tabla `v2_movements`, no `movements`. La vieja es de la
//   aplicacion que se descontinua y tiene otro esquema; encontrarla no prueba
//   nada sobre esta. La ruta queda:
//
//       /rest/v1/v2_movements?select=id&limit=1
//
//   `probeFinished(reachable, tableReady, detail)` sigue separando los dos
//   fallos: `reachable=false` es URL o clave mal; `reachable=true` con
//   `tableReady=false` es que falta correr supabase_v2.sql en el panel.
//
// TODO LO DEMAS SE PORTA SIN CAMBIOS, incluido:
//   - El token de acceso vive solo en memoria; el de refresco lo devuelve
//     refreshToken() para que otro lo persista. La contrasena no se guarda
//     nunca, ni en memoria despues del login.
//   - Todas las llamadas son asincronas. Nada de QEventLoop anidado ni de
//     esperas bloqueantes: con mala senal eso traba la ventana entera.
//   - Quien llama es dueno del QNetworkReply y le hace deleteLater().
//   - Las cabeceras (apikey + Authorization) se arman en buildRequest() y en
//     ningun otro lado.
//
// DEPENDENCIAS PERMITIDAS: las mismas que el archivo original —QNetworkAccess-
// Manager, QNetworkRequest, QNetworkReply, QJsonDocument, QJsonObject, QUrl,
// QUrlQuery, QString, QByteArray— mas config.hpp y supabase_client.hpp. No
// agregar ninguna dependencia nueva.
//
// ACEPTACION: compila sin advertencias con /W4; `grep -c "movements"` no
// encuentra ninguna aparicion sin el prefijo v2_; no aparece la palabra
// QEventLoop en el archivo.
//
#include "dake/sync/supabase_client.hpp"

namespace dake::sync {

// TODO(agy): portar segun el contrato de arriba.

} // namespace dake::sync
