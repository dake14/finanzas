// mobile/main.cpp — punto de entrada de la aplicacion del telefono.
//
// Acepta dos argumentos que no son para el uso diario sino para poder verificar
// la interfaz desde la computadora:
//
//   --captura <archivo.png>   dibuja la pantalla y se va
//   --pagina <0..3>           con cual de las cuatro pestañas arrancar
//   --alto <px>               alto de la ventana al capturar
//
// `--alto` existe porque la pantalla Hoy no entra en un telefono: se recorre
// deslizando, y una captura del tamaño real muestra solo el primer tercio.
// Verificarla entera exige poder estirar la ventana. NO cambia como se ve en el
// telefono, donde el alto lo pone Android.
//
// Existen por lo mismo que `dake_uipreview` en la version de escritorio: la
// unica forma de comprobar que una pantalla se arma bien es mirarla, y mirarla
// en un telefono conectado exige tener el telefono a mano. Con esto la interfaz
// del movil se revisa en Windows, antes de armar un APK.

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStringList>
#include <QTimer>

#include <cstdio>

#include "appbridge.hpp"

namespace {

[[nodiscard]] QString valorDe(const QStringList& args, const QString& bandera) {
    const int i = args.indexOf(bandera);
    return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : QString();
}

} // namespace

int main(int argc, char** argv) {
    // El modo captura se decide ANTES de construir la aplicacion: el
    // complemento de plataforma se elige al arrancar y despues ya es tarde.
    QStringList crudos;
    for (int i = 0; i < argc; ++i) {
        crudos << QString::fromLocal8Bit(argv[i]);
    }
    const QString captura = valorDe(crudos, QStringLiteral("--captura"));

    if (!captura.isEmpty()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // Sin el backend por software el offscreen no tiene contexto grafico y
        // la captura sale en negro.
        qputenv("QT_QUICK_BACKEND", "software");
#ifdef Q_OS_WIN
        if (qgetenv("QT_QPA_FONTDIR").isEmpty()) {
            qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
        }
#endif
    }

    QGuiApplication app(argc, argv);

    // El nombre manda sobre la carpeta de datos: QStandardPaths la arma con
    // applicationName. En Android esa carpeta es privada del paquete, asi que
    // aunque comparta nombre con la de escritorio no se pisan.
    QGuiApplication::setApplicationName(QStringLiteral(DAKE_APP_NAME));
    QGuiApplication::setOrganizationName(QStringLiteral("DakeLabs"));

    // El estilo se fija una vez aca y no con imports por archivo: mezclar
    // `import QtQuick.Controls` con `import QtQuick.Controls.Material` en
    // distintos archivos vuelve ambiguo que tipo se instancia.
    QQuickStyle::setStyle(QStringLiteral("Material"));

    // El puente se construye ANTES de cargar el QML: abre la base y siembra si
    // hace falta, y si eso falla es mejor enterarse por consola que con una
    // ventana en blanco.
    dake::mobile::AppBridge bridge;

    // Va como SINGLETON del modulo y no como propiedad de contexto. El QML de
    // un modulo se compila por adelantado, y el compilador no sabe nada de las
    // propiedades de contexto: las resuelve a null y cada `App.loQueSea` falla
    // en tiempo de ejecucion sin que nada avise al compilar. Registrado asi, el
    // `import DakeMobile` que ya tienen los archivos alcanza.
    qmlRegisterSingletonInstance("DakeMobile", 1, 0, "App", &bridge);

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() {
            std::fprintf(stderr, "No se pudo crear la interfaz.\n");
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("DakeMobile", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window == nullptr) {
        std::fprintf(stderr, "La raiz del QML no es una ventana.\n");
        return 1;
    }

    const QString pagina = valorDe(crudos, QStringLiteral("--pagina"));
    if (!pagina.isEmpty()) {
        window->setProperty("paginaInicial", pagina.toInt());
    }

    const QString alto = valorDe(crudos, QStringLiteral("--alto"));
    if (!captura.isEmpty() && !alto.isEmpty()) {
        bool ok = false;
        const int px = alto.toInt(&ok);
        if (ok && px > 0) {
            window->setHeight(px);
        }
    }

    if (!captura.isEmpty()) {
        // Una vuelta larga por el bucle de eventos antes de dibujar: los
        // layouts de QML se resuelven de forma diferida y capturar enseguida
        // sale con todo apilado en la esquina.
        QTimer::singleShot(1500, &app, [window, captura]() {
            const QImage shot = window->grabWindow();
            if (shot.isNull() || !shot.save(captura)) {
                std::fprintf(stderr, "No se pudo guardar %s\n", qPrintable(captura));
                QCoreApplication::exit(1);
                return;
            }
            std::printf("Guardado %s (%dx%d)\n", qPrintable(captura), shot.width(),
                        shot.height());
            QCoreApplication::quit();
        });
    }

    return QGuiApplication::exec();
}
