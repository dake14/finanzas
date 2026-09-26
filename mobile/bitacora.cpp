#include "bitacora.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>

namespace dake::mobile::bitacora {
namespace {

/// Arriba de esto el archivo se recorta a la mitad mas nueva. 128 KB son unas
/// mil corridas: mas que suficiente para investigar cualquier cosa, y nada al
/// lado del tamano de la propia aplicacion.
constexpr qint64 kTamanoMaximo = 128 * 1024;

/// El manejador de mensajes de Qt puede llamarse desde el hilo de red o desde
/// el de dibujo, no solo desde el principal. Sin el candado, dos mensajes a la
/// vez se entrelazan en el archivo y la linea que interesa sale partida.
QMutex& candado() {
    static QMutex mutex;
    return mutex;
}

QtMessageHandler anterior = nullptr;

void escribir(const QString& linea) {
    const QString destino = ruta();
    if (destino.isEmpty()) {
        return;
    }

    QMutexLocker cerrojo(&candado());

    QDir().mkpath(QFileInfo(destino).absolutePath());

    // El recorte va ANTES de abrir para agregar: leer el archivo entero
    // mientras se le escribe encima deja una copia a medias.
    if (QFileInfo(destino).size() > kTamanoMaximo) {
        QFile viejo(destino);
        if (viejo.open(QIODevice::ReadOnly)) {
            const QByteArray todo = viejo.readAll();
            viejo.close();
            QFile nuevo(destino);
            if (nuevo.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                nuevo.write(todo.right(static_cast<int>(kTamanoMaximo / 2)));
            }
        }
    }

    QFile archivo(destino);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    archivo.write(linea.toUtf8());
    archivo.write("\n");
    // Sin esto, lo ultimo escrito se queda en el buffer del sistema y se pierde
    // justo cuando mas hace falta: cuando el proceso muere de golpe. El costo
    // es irrelevante —son unas decenas de lineas por corrida— y el beneficio es
    // toda la razon de que el archivo exista.
    archivo.flush();
}

void manejador(QtMsgType tipo, const QMessageLogContext& contexto, const QString& mensaje) {
    // Los mensajes de depuracion no entran: son cientos por segundo cuando Qt
    // Quick tiene algo que decir, y llenarian el archivo tapando lo que
    // importa. Los avisos y los criticos si, que son los que dejan la ventana
    // vacia.
    if (tipo != QtDebugMsg) {
        const char* etiqueta = tipo == QtWarningMsg    ? "AVISO"
                               : tipo == QtCriticalMsg ? "GRAVE"
                               : tipo == QtFatalMsg    ? "FATAL"
                                                       : "INFO";
        escribir(QStringLiteral("%1  %2  %3")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                          QString::fromUtf8(etiqueta), mensaje));
    }

    // Y despues, lo de siempre: en la computadora los mensajes tienen que
    // seguir saliendo por consola, que es donde se los mira mientras se
    // trabaja. Este archivo agrega, no reemplaza.
    if (anterior != nullptr) {
        anterior(tipo, contexto, mensaje);
    }
}

} // namespace

QString ruta() {
    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (carpeta.isEmpty()) {
        return {};
    }
    return carpeta + QStringLiteral("/arranque.log");
}

void instalar() {
    escribir(QString());
    escribir(QStringLiteral("=== %1 ===")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    anterior = qInstallMessageHandler(manejador);
}

void anotar(const QString& texto) {
    escribir(QStringLiteral("%1  paso   %2")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), texto));
}

QStringList ultimas(int cuantas) {
    QMutexLocker cerrojo(&candado());

    QFile archivo(ruta());
    if (!archivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {QStringLiteral("Todavia no hay nada anotado.")};
    }
    const QStringList lineas =
        QString::fromUtf8(archivo.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    return lineas.mid(std::max(0, static_cast<int>(lineas.size()) - cuantas));
}

} // namespace dake::mobile::bitacora
