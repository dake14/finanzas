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

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

namespace dake::sync {

bool SupabaseConfig::isValid() const {
    return url.startsWith(QStringLiteral("https://")) && !anonKey.isEmpty();
}

QString SupabaseConfig::defaultPath() {
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QStringLiteral("/.dakelabs");
    }
    return directory + QStringLiteral("/supabase.json");
}

SupabaseConfig SupabaseConfig::compiledIn() {
    SupabaseConfig config;
    config.url = QString::fromUtf8(DAKE_SUPABASE_URL).trimmed();
    config.anonKey = QString::fromUtf8(DAKE_SUPABASE_ANON_KEY).trimmed();
    while (config.url.endsWith(QLatin1Char('/'))) {
        config.url.chop(1);
    }
    return config;
}

SupabaseConfig SupabaseConfig::load() {
    QFile file(defaultPath());
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        // Tolerar BOM de UTF-8 (0xEF, 0xBB, 0xBF)
        if (data.size() >= 3 &&
            static_cast<unsigned char>(data[0]) == 0xEF &&
            static_cast<unsigned char>(data[1]) == 0xBB &&
            static_cast<unsigned char>(data[2]) == 0xBF) {
            data.remove(0, 3);
        }

        const QJsonDocument document = QJsonDocument::fromJson(data);
        if (document.isObject()) {
            const QJsonObject object = document.object();
            SupabaseConfig config;
            config.url = object[QStringLiteral("url")].toString().trimmed();
            config.anonKey = object[QStringLiteral("anon_key")].toString().trimmed();

            while (config.url.endsWith(QLatin1Char('/'))) {
                config.url.chop(1);
            }

            if (config.isValid()) {
                return config;
            }
        }
    }

    return compiledIn();
}

bool SupabaseConfig::save(QString& error) const {
    const QString path = defaultPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject object;
    object[QStringLiteral("url")] = url;
    object[QStringLiteral("anon_key")] = anonKey;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("No se pudo escribir %1: %2").arg(path, file.errorString());
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

} // namespace dake::sync
