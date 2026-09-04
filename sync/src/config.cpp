//
// dake/sync/config.cpp — implementacion de SupabaseConfig.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `finaldake-labs/sync/src/config.cpp` con UNA diferencia: el origen de
// las credenciales deja de ser solo el archivo y pasa a ser archivo primero,
// valores de compilacion despues.
//
// Funciones a implementar, exactamente estas firmas (ver config.hpp):
//
//   bool SupabaseConfig::isValid() const
//       true si `url` empieza con "https://" y `anonKey` no esta vacia.
//
//   QString SupabaseConfig::defaultPath()
//       QStandardPaths::AppDataLocation + "/supabase.json".
//
//   SupabaseConfig SupabaseConfig::compiledIn()
//       Devuelve los valores de las macros DAKE_SUPABASE_URL y
//       DAKE_SUPABASE_ANON_KEY. Las dos se definen siempre desde CMake; cuando
//       no hay credenciales valen la cadena vacia, y entonces esto devuelve una
//       configuracion vacia. NO usar #ifdef para decidir si existen: existen
//       siempre, lo que varia es el contenido.
//
//   SupabaseConfig SupabaseConfig::load()
//       1. Si defaultPath() existe y trae un JSON con "url" y "anon_key"
//          validos, devolver eso.
//       2. Si no, devolver compiledIn().
//       No lanza nunca. Un archivo malformado no es un error fatal: se ignora
//       y se cae al paso 2, porque la aplicacion tiene que arrancar igual y
//       trabajar en local.
//
//   bool SupabaseConfig::save(QString& error) const
//       Escribe defaultPath() creando el directorio si hace falta. Devuelve
//       false y deja `error` con el motivo si no pudo.
//
// El JSON del archivo usa las claves "url" y "anon_key" —con guion bajo— y se
// lee con codificacion UTF-8 tolerando el BOM: el archivo que ya existe en el
// equipo lo tiene.
//
// DEPENDENCIAS PERMITIDAS: QFile, QDir, QFileInfo, QJsonDocument, QJsonObject,
// QStandardPaths, QString, QByteArray y la propia config.hpp. Ninguna otra.
//
// ACEPTACION: compila sin advertencias con /W4; load() devuelve la
// configuracion del archivo cuando existe y la de compilacion cuando no;
// isValid() es false para una url vacia y para una que no sea https.
//
#include "dake/sync/config.hpp"

namespace dake::sync {

// TODO(agy): implementar segun el contrato de arriba.

} // namespace dake::sync
