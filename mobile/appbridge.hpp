#pragma once
//
// mobile/appbridge.hpp — lo unico que QML puede tocar.
//
// La interfaz del telefono no hace cuentas propias ni abre la base: pide y
// muestra. Toda cifra que aparezca en pantalla salio de una funcion de
// dake::core que tiene una prueba escrita, igual que en la de escritorio. Que
// las dos interfaces compartan el nucleo es lo que garantiza que digan lo
// mismo; si cada una calculara lo suyo, tarde o temprano no coincidirian y no
// habria forma de saber cual miente.
//
// Los datos van a QML como QVariantList/QVariantMap y no como modelos propios:
// son cientos de filas al año, no millones, y un QAbstractListModel por cada
// lista seria complejidad comprada de mas.
//
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class QTimer;

#include "dake/core/hlc.hpp"
#include "dake/core/model.hpp"
#include "dake/storage/repository.hpp"
#include "dake/sync/supabase_client.hpp"
#include "dake/sync/sync_engine.hpp"

namespace dake::mobile {

class AppBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList pockets READ pockets NOTIFY dataChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY dataChanged)

    /// TODOS los trabajos, cerrados incluidos, con ingreso, costo, margen y
    /// porcentaje. `jobs` trae solo los abiertos porque es lo que alimenta el
    /// selector de Anotar; esta es la pantalla de Trabajos entera, la misma que
    /// muestra el escritorio.
    Q_PROPERTY(QVariantList allJobs READ allJobs NOTIFY dataChanged)

    /// Lo que va arriba de la lista de Bolsillos: total, reserva y la frase que
    /// explica por que ese numero es el unico que se puede contar a mano.
    Q_PROPERTY(QVariantMap pocketsSummary READ pocketsSummary NOTIFY dataChanged)

    /// Lo que va arriba de la lista de Trabajos: margen total, estructura del
    /// mes y cuanto quedo entregado sin cobrar.
    Q_PROPERTY(QVariantMap jobsSummary READ jobsSummary NOTIFY dataChanged)
    Q_PROPERTY(QVariantList recent READ recent NOTIFY dataChanged)
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY dataChanged)

    // --- Lo que alimenta las graficas y los indicadores de Hoy -------------
    //
    // Van como listas de mapas y no como modelos propios, igual que el resto
    // del puente: son doce meses y un punado de categorias, no millones de
    // filas, y un QAbstractListModel por cada una seria complejidad comprada
    // de mas.
    //
    // Los numeros para dibujar van en `valor` como double; el texto ya
    // formateado va aparte, en `texto`. QML NO formatea plata: el formato vive
    // en dake::core::format y en ningun otro lado, para que el telefono y la
    // computadora no puedan escribir el mismo importe distinto.

    /// Un punto por mes con actividad, del mas viejo al mas nuevo. Claves:
    /// etiqueta, resultado, ingresos, costos, caja (acumulada), y sus textos
    /// resultadoTexto / cajaTexto.
    Q_PROPERTY(QVariantList months READ months NOTIFY dataChanged)

    /// Gastos del mes por categoria, de mayor a menor. Claves: etiqueta,
    /// valor, texto.
    Q_PROPERTY(QVariantList categories READ categories NOTIFY dataChanged)

    /// Margen por trabajo, del que mas dejo al que menos. Claves: etiqueta,
    /// valor, texto (importe y porcentaje).
    Q_PROPERTY(QVariantList jobMargins READ jobMargins NOTIFY dataChanged)

    /// Los cuatro indicadores nuevos, ya formateados y listos para mostrar.
    /// Claves: equilibrio, equilibrioNota, reserva, reservaNota, ticket,
    /// ticketNota, cobro, cobroNota, estructura, estructuraNota.
    ///
    /// Cuando un numero no se puede saber, su clave trae "—" y la nota explica
    /// por que. Un dato ausente disfrazado de cifra es peor que un guion.
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY dataChanged)
    Q_PROPERTY(QString today READ today CONSTANT)
    Q_PROPERTY(QString dbPath READ dbPath CONSTANT)

    /// Por que no se pudo arrancar. Vacio cuando todo esta bien.
    Q_PROPERTY(QString fatalError READ fatalError CONSTANT)
    Q_PROPERTY(bool ready READ ready CONSTANT)

    // --- Nube --------------------------------------------------------------
    //
    // Tres propiedades y dos acciones. El telefono no configura nada: la URL y
    // la clave vienen incrustadas al compilar, porque en Android no hay forma
    // comoda de dejarle un archivo a la aplicacion. Lo unico que no se puede
    // saber solo es la contrasena.
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY cloudChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY cloudChanged)
    Q_PROPERTY(QString cloudStatus READ cloudStatus NOTIFY cloudChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY cloudChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY cloudChanged)

public:
    explicit AppBridge(QObject* parent = nullptr);
    ~AppBridge() override;

    /// Vacio si el arranque salio bien; si no, POR QUE no salio bien, escrito
    /// para que lo lea el dueno de la aplicacion y no un programador.
    ///
    /// El constructor NO lanza. Antes lo hacia, y en Android una excepcion sin
    /// atrapar mata el proceso antes de que exista una sola ventana: la
    /// aplicacion se quedaba en negro sin decir una palabra. Ahora el fallo se
    /// guarda aca y la interfaz lo muestra. Una aplicacion de finanzas que se
    /// abre en negro es una aplicacion que se desinstala.
    [[nodiscard]] QString fatalError() const { return fatalError_; }

    /// true cuando la base abrio y se puede trabajar.
    [[nodiscard]] bool ready() const { return repository_ != nullptr; }

    /// Las ultimas lineas del cuaderno de arranque, para mirarlas desde el
    /// propio telefono cuando algo no anda.
    Q_INVOKABLE QStringList startupLog() const;

    /// Donde esta ese cuaderno, por si hay que sacarlo del telefono.
    Q_INVOKABLE QString startupLogPath() const;

    [[nodiscard]] QVariantList pockets() const { return pockets_; }
    [[nodiscard]] QVariantList jobs() const { return jobs_; }
    [[nodiscard]] QVariantList allJobs() const { return allJobs_; }
    [[nodiscard]] QVariantMap pocketsSummary() const { return pocketsSummary_; }
    [[nodiscard]] QVariantMap jobsSummary() const { return jobsSummary_; }
    [[nodiscard]] QVariantList recent() const { return recent_; }
    [[nodiscard]] QVariantList alerts() const { return alerts_; }
    [[nodiscard]] QVariantMap summary() const { return summary_; }
    [[nodiscard]] QVariantList months() const { return months_; }
    [[nodiscard]] QVariantList categories() const { return categories_; }
    [[nodiscard]] QVariantList jobMargins() const { return jobMargins_; }
    [[nodiscard]] QVariantMap stats() const { return stats_; }
    [[nodiscard]] QString today() const;
    [[nodiscard]] QString dbPath() const;

    /// Guarda un movimiento. Devuelve "" si salio bien, o el motivo para
    /// mostrar. Las claves esperadas son las que arma Anotar.qml:
    /// name, amount (texto), kind (0 gasto, 1 ingreso, 2 traspaso), date (ISO),
    /// pocketId, targetPocketId, category, jobId, spreadMonths, settled.
    Q_INVOKABLE QString saveMovement(const QVariantMap& draft);

    Q_INVOKABLE QString removeMovement(const QString& id);

    /// Las categorias ya usadas para ese tipo, de la mas usada a la menos. Se
    /// aprenden de los datos: una lista fija se queda vieja el primer dia.
    Q_INVOKABLE QStringList categoriesFor(int kind) const;

    Q_INVOKABLE QString addPocket(const QString& name, int kind, const QString& opening);
    Q_INVOKABLE QString addJob(const QString& name, const QString& client);

    /// Cierra o reabre un trabajo. Un trabajo cerrado deja de aparecer en el
    /// selector de Anotar, que es todo lo que significa: sus numeros siguen
    /// contando en los reportes, porque el trabajo se hizo igual.
    Q_INVOKABLE QString setJobClosed(const QString& id, bool closed);

    // --- Cierre de mes -----------------------------------------------------
    //
    // Los meses van del MAS NUEVO al mas viejo: el cierre que se mira es el
    // ultimo. `closing` recibe el indice dentro de esa misma lista.

    /// Etiquetas de los meses con actividad, del mas nuevo al mas viejo.
    Q_INVOKABLE QStringList closingMonths() const;

    /// Los numeros del cierre de un mes. Claves: etiqueta, resultado,
    /// resultadoNegativo, facturado, costo, caja, cajaNegativa, porCobrar,
    /// contra (la comparacion con el mes anterior) y categorias (lista de
    /// {etiqueta, texto}).
    Q_INVOKABLE QVariantMap closing(int index) const;

    /// Borra bolsillos, trabajos y movimientos dejando lapida de cada uno, para
    /// que el borrado tambien viaje a la nube y a la computadora. Devuelve ""
    /// si salio bien.
    Q_INVOKABLE QString eraseAll();

    /// Anota la diferencia entre lo que dice la app y lo que hay de verdad,
    /// como un movimiento visible y no como un saldo corregido por debajo.
    Q_INVOKABLE QString reconcile(const QString& pocketId, const QString& realAmount);

    Q_INVOKABLE QString exportTo(const QUrl& url);
    Q_INVOKABLE QString importFrom(const QUrl& url);
    /// Nombre sugerido para el archivo, con la fecha: dos exportaciones del
    /// mismo dia no se pisan sin que nadie lo note.
    Q_INVOKABLE QString suggestedFileName() const;

    // --- Nube --------------------------------------------------------------

    [[nodiscard]] bool signedIn() const;
    [[nodiscard]] QString userEmail() const;
    [[nodiscard]] QString cloudStatus() const { return cloudStatus_; }
    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] bool syncing() const;

    /// Inicia sesion. La contrasena se usa y se descarta: no se guarda en
    /// disco, ni en un miembro, ni en un registro. Lo que se persiste es el
    /// token de refresco, que se puede revocar desde Supabase.
    Q_INVOKABLE void signIn(const QString& email, const QString& password);
    Q_INVOKABLE void signOut();

    /// Sube lo pendiente y baja lo que cambio. No hace nada si no hay sesion.
    Q_INVOKABLE void sync();

    Q_INVOKABLE QString formatMinor(qlonglong minor) const;
    /// "hoy", "ayer" o la fecha, para las listas.
    Q_INVOKABLE QString relativeDate(const QString& isoDate) const;

signals:
    void dataChanged();
    void cloudChanged();

private:
    /// Todo lo que antes estaba en el constructor. Devuelve el motivo del fallo
    /// o vacio. Separado para que el constructor pueda atraparlo entero.
    QString abrir();

    void reload();

    /// Lo que sigue a TODO cambio hecho en este telefono: recarga las
    /// pantallas, avisa a QML y programa la sincronizacion. Los `reload()`
    /// sueltos que quedan son el del arranque y el de despues de bajar del
    /// servidor; no pasan por aca para que bajar algo no dispare una subida.
    void afterLocalChange();

    /// Vence el temporizador. Sincroniza si hay sesion y el motor esta libre.
    void runAutoSync();

    void stamp(std::string& hlc, std::string& deviceId);

    std::unique_ptr<storage::Database> db_;
    std::unique_ptr<storage::Repository> repository_;
    std::unique_ptr<core::HlcClock> clock_;
    std::unique_ptr<sync::SupabaseClient> supabase_;
    std::unique_ptr<sync::SyncEngine> syncEngine_;
    QString cloudStatus_;
    QString fatalError_;

    /// Disparo unico. Cada cambio local lo reinicia, asi que una tanda de
    /// anotaciones seguidas es una sola subida —que en el telefono no es
    /// prolijidad sino bateria y datos moviles.
    QTimer* autoSyncTimer_ = nullptr;

    QString deviceId_;
    core::Currency currency_;
    core::Date today_;

    /// La ultima lectura de la base. Se refresca en reload() y la usan todas
    /// las consultas que antes volvian a leer la base entera cada vez
    /// —categoriesFor, removeMovement, reconcile, el cierre de mes—. En un
    /// telefono eso no es prolijidad: cada lectura de mas es bateria.
    std::vector<core::Pocket> pocketsData_;
    std::vector<core::Job> jobsData_;
    std::vector<core::Movement> movementsData_;

    QVariantList pockets_;
    QVariantList jobs_;
    QVariantList allJobs_;
    QVariantMap pocketsSummary_;
    QVariantMap jobsSummary_;
    QVariantList recent_;
    QVariantList alerts_;
    QVariantMap summary_;
    QVariantList months_;
    QVariantList categories_;
    QVariantList jobMargins_;
    QVariantMap stats_;
};

} // namespace dake::mobile
