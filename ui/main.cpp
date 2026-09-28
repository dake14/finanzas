// ui/main.cpp — punto de entrada.
//
//   --bandeja   arranca escondida en la bandeja del sistema (asi arranca con
//               Windows: el atajo queda listo sin abrir ninguna ventana)
//   --anotar    abre directo la ventana mini de anotar
//
// Una sola instancia: si la aplicacion ya esta abierta, abrirla de nuevo le
// manda el pedido a la que esta andando y sale. Dos instancias sobre la misma
// base pelearian por el atajo global y por la cola de sincronizacion.

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

#include "dake/storage/database.hpp"
#include "mainwindow.hpp"

namespace {

/// El nombre del canal entre instancias. Lleva el usuario de Windows para que
/// dos personas en la misma maquina no se crucen.
[[nodiscard]] QString instanceChannel() {
    return QStringLiteral("dakelabs-finanzas-") + QStringLiteral(DAKE_APP_NAME).simplified().replace(
                                                      QLatin1Char(' '), QLatin1Char('-')) +
           QLatin1Char('-') + qEnvironmentVariable("USERNAME");
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // El nombre manda sobre la carpeta de datos: QStandardPaths la arma con
    // applicationName.
    QApplication::setApplicationName(QStringLiteral(DAKE_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral("DakeLabs"));

    const QStringList arguments = QCoreApplication::arguments();
    const bool toTray = arguments.contains(QStringLiteral("--bandeja"));
    const bool capture = arguments.contains(QStringLiteral("--anotar"));

    // ¿Ya hay una abierta? Se le pasa el pedido y listo.
    {
        QLocalSocket probe;
        probe.connectToServer(instanceChannel());
        if (probe.waitForConnected(300)) {
            probe.write(capture ? "anotar" : toTray ? "nada" : "mostrar");
            probe.flush();
            probe.waitForBytesWritten(300);
            return 0;
        }
    }

    // La ventana se esconde en la bandeja al cerrarla: cerrar la ultima
    // ventana no puede terminar el programa, o el atajo dejaria de andar.
    QApplication::setQuitOnLastWindowClosed(false);

    try {
        dake::ui::MainWindow window(dake::storage::Database::defaultPath());

        QLocalServer::removeServer(instanceChannel());
        QLocalServer server;
        server.listen(instanceChannel());
        QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window] {
            QLocalSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QLocalSocket::readyRead, &window, [socket, &window] {
                const QString message = QString::fromUtf8(socket->readAll()).trimmed();
                if (message != QStringLiteral("nada")) {
                    window.handleInstanceMessage(message);
                }
                socket->deleteLater();
            });
        });

        if (!toTray) {
            window.show();
        }
        if (capture) {
            window.showCapture();
        }
        window.askSignInIfNeeded();
        return QApplication::exec();
    } catch (const std::exception& error) {
        // Un fallo al abrir o migrar la base tiene que verse, no morir en
        // silencio dejando una ventana que nunca aparece.
        QMessageBox::critical(nullptr, QStringLiteral("Finanzas DakeLabs"),
                              QStringLiteral("No se pudo abrir la base:\n\n%1")
                                  .arg(QString::fromUtf8(error.what())));
        return 1;
    }
}
