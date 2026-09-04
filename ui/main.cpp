// ui/main.cpp — punto de entrada del banco de pruebas.

#include <QApplication>
#include <QMessageBox>

#include "dake/storage/database.hpp"
#include "mainwindow.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // El nombre manda sobre la carpeta de datos: QStandardPaths la arma con
    // applicationName. Es distinto del de la aplicacion real a proposito, para
    // que este programa no pueda tocar la base con los movimientos de verdad.
    QApplication::setApplicationName(QStringLiteral(DAKE_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral("DakeLabs"));

    try {
        dake::ui::MainWindow window(dake::storage::Database::defaultPath());
        window.show();
        return QApplication::exec();
    } catch (const std::exception& error) {
        // Un fallo al abrir o migrar la base tiene que verse, no morir en
        // silencio dejando una ventana que nunca aparece.
        QMessageBox::critical(nullptr, QStringLiteral("Banco de pruebas"),
                              QStringLiteral("No se pudo abrir la base:\n\n%1")
                                  .arg(QString::fromUtf8(error.what())));
        return 1;
    }
}
