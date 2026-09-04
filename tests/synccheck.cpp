//
// tests/synccheck.cpp — diagnostico de la conexion con Supabase.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Portar `../finaldake-labs/tests/synccheck.cpp` (148 lineas) adaptandolo a
// este proyecto. Leer ese archivo entero antes de escribir.
//
// NO es una prueba automatica y NO entra en ctest: toca la red y depende de que
// el proyecto de Supabase este arriba. Una prueba que falla porque se cayo
// internet ensena a ignorar las pruebas que fallan.
//
// Uso:
//   dake_synccheck                      solo comprueba proyecto y tablas
//   dake_synccheck correo contrasena    ademas intenta iniciar sesion
//
// QUE TIENE QUE HACER, en este orden, imprimiendo una linea por paso:
//
//   1. Decir de donde salieron las credenciales: si de
//      SupabaseConfig::defaultPath() o si de las incrustadas al compilar.
//      Imprimir la URL. NUNCA imprimir la clave entera: como mucho su largo.
//   2. probe(): informar si el proyecto responde y si la tabla v2_movements
//      existe, con el detalle que trae la señal.
//   3. Si vinieron correo y contrasena, signInWithPassword() y decir si entro,
//      con que correo y con que user_id.
//   4. Si la sesion quedo abierta, comprobar las TRES tablas —v2_pockets,
//      v2_jobs, v2_movements— con un restGet de
//         /rest/v1/<tabla>?select=id&limit=1
//      e informar el codigo HTTP de cada una. Las tres tienen que dar 200.
//
// Sale con codigo 0 si todo lo que se pudo comprobar salio bien, y 1 si algo
// fallo. Ese codigo es el criterio de aceptacion de toda la sincronizacion.
//
// IMPORTANTE sobre el nombre: setApplicationName tiene que ser exactamente el
// de la aplicacion —viene en la macro DAKE_APP_NAME— porque es lo que decide
// donde QStandardPaths busca supabase.json. Con otro nombre leeria otro archivo
// y el diagnostico mentiria.
//
// Todo asincrono, con QTimer para el vencimiento; nada de QEventLoop anidado.
// Si algo no responde en 30 segundos, imprimir que se agoto la espera y salir
// con 1 en vez de quedarse colgado para siempre.
//
// DEPENDENCIAS PERMITIDAS: QCoreApplication, QTimer, QNetworkReply,
// QJsonDocument, QJsonObject, QJsonArray, <iostream>, dake/sync/config.hpp,
// dake/sync/supabase_client.hpp. Ninguna otra.
//
// ACEPTACION: compila con MSVC /W4 sin advertencias; corrido sin argumentos
// contra un proyecto con las tablas creadas, imprime que las encontro y sale 0.
//
#include <QCoreApplication>
#include <QSslSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>

#include <iostream>

#include "dake/sync/config.hpp"
#include "dake/sync/supabase_client.hpp"

namespace {

void print(const QString& text) {
    std::cout << text.toStdString() << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    
    QCoreApplication::setOrganizationName(QStringLiteral("DakeLabs"));
    QCoreApplication::setApplicationName(QString::fromUtf8(DAKE_APP_NAME));

    print(QStringLiteral("=== Diagnostico de Supabase ==="));

    // Lo primero, antes que las credenciales: sin respaldo TLS no hay HTTPS, y
    // todo lo que venga despues fallaria con errores que apuntan al lugar
    // equivocado. En Windows sobra —Qt usa Schannel— pero en Android es LA
    // pregunta.
    print(QStringLiteral("TLS disponible: ") +
          (QSslSocket::supportsSsl() ? QStringLiteral("si (") + QSslSocket::sslLibraryVersionString() +
                                           QStringLiteral(")")
                                     : QStringLiteral("NO -- ninguna peticion https va a funcionar")));

    const dake::sync::SupabaseConfig config = dake::sync::SupabaseConfig::load();
    if (!config.isValid()) {
        print(QStringLiteral("FALLO  No hay credenciales validas."));
        return 1;
    }

    const dake::sync::SupabaseConfig compiled = dake::sync::SupabaseConfig::compiledIn();
    if (config.url == compiled.url && config.anonKey == compiled.anonKey) {
        print(QStringLiteral("Config: incrustadas al compilar"));
    } else {
        print(QStringLiteral("Config: ") + dake::sync::SupabaseConfig::defaultPath());
    }

    print(QStringLiteral("URL:    ") + config.url);
    print(QStringLiteral("Clave:  ") + QString::number(config.anonKey.size()) + QStringLiteral(" caracteres"));
    print(QString());

    auto* client = new dake::sync::SupabaseClient(config, &app);

    const QStringList arguments = QCoreApplication::arguments();
    const bool tryLogin = arguments.size() >= 3;

    int exitCode = 0;

    QObject::connect(client, &dake::sync::SupabaseClient::probeFinished, &app,
                     [&](bool reachable, bool tableReady, const QString& detail) {
                         print(QStringLiteral("Proyecto responde:   ") +
                               (reachable ? QStringLiteral("si") : QStringLiteral("NO")));
                         print(QStringLiteral("Tabla v2_movements:  ") +
                               (tableReady ? QStringLiteral("si") : QStringLiteral("NO")));
                         print(QStringLiteral("Detalle: ") + detail);

                         if (!reachable || !tableReady) {
                             exitCode = 1;
                         }

                         if (!tryLogin) {
                             QTimer::singleShot(0, &app, &QCoreApplication::quit);
                             return;
                         }

                         print(QString());
                         print(QStringLiteral("Intentando iniciar sesion como ") + arguments[1]);
                         client->signInWithPassword(arguments[1], arguments[2]);
                     });

    QObject::connect(client, &dake::sync::SupabaseClient::signedIn, &app,
                     [&](const QString& email) {
                         print(QStringLiteral("OK     Sesion iniciada como ") + email);
                         print(QStringLiteral("       user_id: ") + client->userId());
                         print(QString());
                         print(QStringLiteral("Leyendo las tablas con la sesion…"));

                         int* pending = new int(3);
                         const char* const tables[] = {"v2_pockets", "v2_jobs", "v2_movements"};
                         
                         for (const char* tableName : tables) {
                             const QString table = QString::fromUtf8(tableName);
                             QNetworkReply* reply = client->restGet(
                                 QStringLiteral("/rest/v1/") + table + QStringLiteral("?select=id&limit=1"));

                             QObject::connect(reply, &QNetworkReply::finished, &app, [&app, &exitCode, reply, table, pending]() {
                                 reply->deleteLater();
                                 const int status =
                                     reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                                 const QByteArray body = reply->readAll();

                                 if (status != 200) {
                                     print(QStringLiteral("FALLO  Lectura HTTP %1 en %2: %3")
                                               .arg(status)
                                               .arg(table)
                                               .arg(QString::fromUtf8(body.left(300))));
                                     exitCode = 1;
                                 } else {
                                     print(QStringLiteral("OK     Lectura HTTP 200 en ") + table);
                                 }

                                 (*pending)--;
                                 if (*pending == 0) {
                                     delete pending;
                                     if (exitCode == 0) {
                                         print(QString());
                                         print(QStringLiteral("Todo listo: la app puede sincronizar."));
                                     }
                                     QTimer::singleShot(0, &app, &QCoreApplication::quit);
                                 }
                             });
                         }
                     });

    QObject::connect(client, &dake::sync::SupabaseClient::authFailed, &app,
                     [&](const QString& message) {
                         print(QStringLiteral("FALLO  ") + message);
                         exitCode = 1;
                         QTimer::singleShot(0, &app, &QCoreApplication::quit);
                     });

    QTimer::singleShot(30000, &app, [&]() {
        print(QStringLiteral("FALLO  Se agoto el tiempo de espera (30 s)."));
        exitCode = 1;
        app.quit();
    });

    client->probe();
    app.exec();
    return exitCode;
}
