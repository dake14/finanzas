#pragma once
//
// dake/sync/config.hpp
//
// Credenciales del proyecto de Supabase.
//
// Hay dos origenes, y el orden importa:
//
//   1. El archivo JSON de la carpeta de datos del usuario, si existe:
//      %APPDATA%\DakeLabs\Finanzas DakeLabs\supabase.json
//   2. Si no hay archivo, los valores incrustados en la compilacion
//      (DAKE_SUPABASE_URL y DAKE_SUPABASE_ANON_KEY).
//
// El archivo gana porque permite apuntar a otro proyecto sin recompilar. Los
// valores de compilacion existen para Android, donde no hay forma comoda de
// dejarle un archivo a la aplicacion; CMake los lee del mismo supabase.json al
// configurar, asi que NUNCA entran al repositorio.
//
// La seguridad real no la da el secreto de la clave sino las politicas RLS del
// servidor. La clave `service_role` las saltea y por eso no se guarda aca ni
// en ningun otro lugar de la aplicacion.
//
// La seguridad real no la da el secreto de la clave sino las politicas RLS del
// servidor. La clave `service_role` las saltea y por eso no se guarda aca ni
// en ningun otro lugar de la aplicacion.
//
#include <QString>

namespace dake::sync {

struct SupabaseConfig {
    QString url;      // https://xxxx.supabase.co
    QString anonKey;  // clave publicable (sb_publishable_... o el JWT antiguo)

    [[nodiscard]] bool isValid() const;

    /// Ruta del archivo de configuracion, junto a la base local.
    [[nodiscard]] static QString defaultPath();

    /// Lee el archivo y, si no hay o esta malformado, cae a los valores de
    /// compilacion. Devuelve una configuracion vacia si tampoco hay de esos.
    /// No lanza: la app tiene que arrancar igual y trabajar solo en local
    /// mientras no haya credenciales.
    [[nodiscard]] static SupabaseConfig load();

    /// Los valores incrustados al compilar. Vacios si se compilo sin ellos.
    [[nodiscard]] static SupabaseConfig compiledIn();

    /// Escribe el archivo creando el directorio si hace falta.
    /// Devuelve false y deja `error` explicado si no pudo.
    [[nodiscard]] bool save(QString& error) const;
};

} // namespace dake::sync
