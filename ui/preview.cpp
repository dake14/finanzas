//
// ui/preview.cpp — banco de pruebas visual.
//
// Renderiza una pantalla a un PNG sin abrir ninguna ventana, con el complemento
// "offscreen" de Qt. Existe por una razon concreta: verificar la interfaz
// tomando capturas del escritorio obliga a traer la aplicacion al frente y a
// hacer clics sinteticos, y cualquier utilidad que se ponga encima (una
// notificacion, un panel del fabricante) desvia esos clics a la ventana
// equivocada.
//
// Aca no hay foco que robar ni clic que desviar: se arma el widget, se le dan
// datos y se guarda lo que pinta.
//
// Uso:  dake_uipreview <pantalla|todas> <salida.png|carpeta> [ancho] [alto]
//
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

#include <iostream>
#include <memory>

#include "dake/core/demo.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace {

[[nodiscard]] dake::ui::Snapshot demoSnapshot() {
    const auto currency = dake::core::Currency::usd();
    const auto data = dake::core::realCaseAugust2026(currency);

    dake::ui::Snapshot snapshot;
    snapshot.pockets = data.pockets;
    snapshot.jobs = data.jobs;
    snapshot.movements = data.movements;
    snapshot.currency = currency;
    // Se mira desde el 31 de agosto: es el corte que hace comparables los
    // numeros de esta pantalla con los que muestra hoy la aplicacion real.
    snapshot.today = dake::core::Date::fromYmd(2026, 8, 31);
    return snapshot;
}

[[nodiscard]] std::unique_ptr<QWidget> build(const QString& screen,
                                             const dake::ui::Snapshot& snapshot) {
    if (screen == QLatin1String("hoy")) {
        auto page = std::make_unique<dake::ui::TodayPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("trabajos")) {
        auto page = std::make_unique<dake::ui::JobsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("movimientos")) {
        auto page = std::make_unique<dake::ui::MovementsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("bolsillos")) {
        auto page = std::make_unique<dake::ui::PocketsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("cierre")) {
        auto page = std::make_unique<dake::ui::ClosingPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    return nullptr;
}

bool render(const QString& screen, const QString& output, int width, int height,
            const dake::ui::Snapshot& snapshot) {
    std::unique_ptr<QWidget> widget = build(screen, snapshot);
    if (widget == nullptr) {
        std::cout << "Pantalla desconocida: " << screen.toStdString() << "\n";
        return false;
    }

    widget->resize(width, height);
    widget->show();
    // Sin esta vuelta al bucle de eventos el layout todavia no se aplico y la
    // captura sale con todos los widgets apilados en la esquina.
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap shot = widget->grab();
    if (!shot.save(output)) {
        std::cout << "No se pudo guardar: " << output.toStdString() << "\n";
        return false;
    }
    std::cout << "Guardado " << output.toStdString() << " (" << shot.width() << "x"
              << shot.height() << ")\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    // El complemento se fija antes de construir QApplication; despues ya es
    // tarde.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // El offscreen no hereda las fuentes del sistema como si hace el de
    // Windows: sin esto cada letra se dibuja como un rectangulo vacio y la
    // captura no sirve para leer nada.
#ifdef Q_OS_WIN
    if (qgetenv("QT_QPA_FONTDIR").isEmpty()) {
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    }
#endif

    QApplication app(argc, argv);
    app.setStyleSheet(dake::ui::theme::styleSheet());

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() < 3) {
        std::cout << "Uso: dake_uipreview <pantalla|todas> <salida.png|carpeta> "
                     "[ancho] [alto]\n"
                  << "Pantallas: hoy | trabajos | movimientos | bolsillos | cierre | todas\n";
        return 2;
    }

    const QString screen = arguments.at(1);
    const QString output = arguments.at(2);
    const int width = arguments.size() > 3 ? arguments.at(3).toInt() : 1280;
    const int height = arguments.size() > 4 ? arguments.at(4).toInt() : 1400;

    const dake::ui::Snapshot snapshot = demoSnapshot();

    if (screen == QLatin1String("todas")) {
        QDir().mkpath(output);
        bool ok = true;
        for (const QString& one : {QStringLiteral("hoy"), QStringLiteral("trabajos"),
                                   QStringLiteral("movimientos"), QStringLiteral("bolsillos"),
                                   QStringLiteral("cierre")}) {
            ok = render(one, output + QLatin1Char('/') + one + QStringLiteral(".png"), width,
                        height, snapshot) &&
                 ok;
        }
        return ok ? 0 : 1;
    }

    QDir().mkpath(QFileInfo(output).absolutePath());
    return render(screen, output, width, height, snapshot) ? 0 : 1;
}
