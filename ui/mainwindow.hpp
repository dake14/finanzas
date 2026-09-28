#pragma once
//
// ui/mainwindow.hpp — la ventana.
//
// Es el unico lugar que escribe en la base. Las pantallas piden y muestran;
// guardar pasa siempre por aca, que es lo que hace que la verificacion de la
// forma del movimiento no se pueda esquivar por un camino lateral.
//
#include <QMainWindow>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "dake/storage/database.hpp"
#include "dake/storage/repository.hpp"
#include "dake/sync/supabase_client.hpp"
#include "dake/sync/sync_engine.hpp"
#include "snapshot.hpp"

class QLabel;
class QPushButton;
class QFileSystemWatcher;
class QSystemTrayIcon;
class QStackedWidget;
class QTimer;

namespace dake::ui {

class TodayPage;
class RepairsPage;
class ReviewPage;
class MovementsPage;
class PocketsPage;
class CaptureWindow;
class GlobalHotkey;
class ReportsPage;
class SettingsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& dbPath, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Abre la ventana mini de anotar, encima de lo que se este haciendo.
    void showCapture();

    /// Trae la ventana principal al frente, este escondida o minimizada.
    void showMainWindow();

    /// Lo que manda una segunda instancia al arrancar: "anotar" o "mostrar".
    /// Abrir la aplicacion dos veces no abre dos aplicaciones: le avisa a la
    /// que ya esta andando.
    void handleInstanceMessage(const QString& message);

protected:
    /// Cerrar la ventana la esconde en la bandeja: el atajo global solo
    /// funciona mientras la aplicacion este abierta.
    void closeEvent(QCloseEvent* event) override;

private slots:
    /// Guarda lo que llega de una captura. `newCategory` trae nombre solo si
    /// la categoria no existia, con la cuenta elegida en la vista previa.
    /// false si no se pudo guardar (ya se aviso al usuario).
    bool addMovement(const dake::core::Movement& draft, const dake::core::Category& newCategory);
    void newPocket();
    // --- Reparaciones -------------------------------------------------------
    void newRepair();
    void editRepair(const dake::core::Repair& repair, const QString& client);
    void deliverRepair(const dake::core::Id& jobId);
    void chargeRepair(const dake::core::Id& jobId);
    void addPart(const dake::core::Id& jobId, const QString& name, qint64 costMinor,
                 bool costKnown, bool bought);
    void changePart(const dake::core::RepairPart& part);
    void removePart(const dake::core::RepairPart& part);

    // --- DakeLabs Cotizaciones ----------------------------------------------
    //
    // Solo lectura: Finanzas lee la carpeta y nunca escribe en ella. Se lee al
    // arrancar, cada vez que cambia un archivo y con "Leer ahora".

    /// Lee la carpeta y aplica el plan. `notify`: avisar en la bandeja lo que
    /// entro (no al arrancar: seria un aviso por cada documento viejo).
    void importQuotes(bool notify);

    /// Abre la revision de los documentos que esperan una decision.
    void reviewQuotes();

    // --- Fijos y bandeja ------------------------------------------------------

    /// Anota los recurrentes que vencieron y faltan. Al arrancar, cada hora (por
    /// si cambio el dia con la aplicacion abierta) y al editar un recurrente.
    void generateRecurring();

    /// Si ya paso la hora del recordatorio de esta semana y hay pendientes,
    /// avisa en la bandeja del sistema. Una vez por semana.
    void checkReminder();

    void setMovementCategory(const dake::core::Id& movementId, const QString& category);
    void confirmRecurring(const dake::core::Id& movementId, qint64 amountMinor);
    void setToolLife(const dake::core::Id& movementId, int months);
    void setRealHours(const dake::core::Id& jobId, int minutes);
    void setPartCost(const dake::core::Id& partId, qint64 costMinor);
    void snooze(const std::string& id, int days);
    void deleteMovementById(const dake::core::Id& movementId);

    void reconcile(const dake::core::Id& pocketId);
    void editMovement(const dake::core::Id& movementId);
    void togglePocketAccount(const dake::core::Id& pocketId);
    void saveCategory(const dake::core::Category& category);

    /// Borra bolsillos, trabajos y movimientos, con lapida y encolado, previo
    /// aviso que nombra el archivo y dice que tambien desaparecen del telefono.
    void deleteEverything();

    /// Deshace el ultimo cambio hecho en esta maquina: un movimiento anotado se
    /// borra, uno editado o borrado vuelve como estaba.
    ///
    /// Un solo nivel. Encadenar deshaceres obliga a llevar una pila que
    /// tambien habria que reconciliar con lo que baja del servidor, y el error
    /// que se comete de verdad es el ultimo, no el quinto hacia atras.
    void undoLast();

    // --- Nube --------------------------------------------------------------
    //
    // El escritorio se queda con lo minimo: conectarse y sincronizar. Nada de
    // una pantalla de nube con configuracion adentro: la URL y la clave ya
    // vienen incrustadas o en supabase.json, y lo unico que la aplicacion no
    // puede saber sola es la contrasena.

    /// Pide correo y contrasena y trata de iniciar sesion. Si ya hay sesion,
    /// desconecta. La contrasena no se guarda en ningun lado.
    void toggleSignIn();

    /// Corre una sincronizacion. Si no hay sesion, primero pide conectarse.
    void syncNow();

private:
    void buildUi();
    void buildSidebar(QWidget* parent);
    void buildTray();

    /// Registra el atajo guardado en los ajustes. false si Windows no lo acepto.
    bool applyHotkey(const QKeySequence& sequence);

    /// Escribe o borra la entrada de arranque con Windows.
    void applyAutostart(bool enabled);
    void reload();

    /// Lo que sigue a TODO cambio hecho en esta maquina: recarga la pantalla y
    /// programa la sincronizacion. Los `reload()` que quedan sueltos son los
    /// dos que no son cambios locales —el del arranque y el de despues de
    /// bajar del servidor—, y por eso mismo no pueden pasar por aca: bajar
    /// algo no debe disparar una subida.
    void afterLocalChange();

    /// Vence el temporizador. Sincroniza si hay sesion y el motor esta libre.
    void runAutoSync();

    /// Pone hlc y deviceId de este equipo sobre un registro que YA tiene id.
    /// stamp() sirve para lo que nace; esto, para lo que vuelve o se corrige.
    void restamp(std::string& hlc, std::string& deviceId);

    /// Guarda que deshacer. `previo` vacio significa que el registro no existia
    /// antes del cambio, y entonces deshacer es borrarlo.
    void rememberUndo(const std::optional<dake::core::Movement>& previo,
                      const dake::core::Movement& despues, const QString& que);
    void showPage(int index);

    /// Guarda el bolsillo usado para ese tipo: la proxima vez arranca ahi.
    void rememberPockets(const core::Movement& movement);

    /// "Gasto 25,00 · Comida · Caja del negocio": lo que se confirma al anotar.
    [[nodiscard]] QString savedSummary(const core::Movement& movement) const;

    /// Guarda la ficha y deja el trabajo sincronizado a tono: cerrado si la
    /// reparacion esta cobrada, abierto si no. Asi el telefono, que solo ve
    /// trabajos, no muestra como pendiente algo que ya se cobro.
    void persistRepair(const core::Repair& repair);

    /// Guarda un movimiento que nace de una reparacion (el cobro, la compra de
    /// un repuesto): id y reloj nuevos si es nuevo, reloj nuevo si ya existia,
    /// y la categoria creada si hace falta.
    void persistGenerated(core::Movement movement);

    /// El bolsillo del negocio donde entran los cobros: el ultimo usado de la
    /// cuenta negocio.
    [[nodiscard]] core::Id businessPocket() const;

    /// Aplica el plan de un documento: crea o actualiza el trabajo, la ficha,
    /// los repuestos y los ingresos, solo si algo cambio. Devuelve si cambio.
    bool applyQuotePlan(const core::QuotePlan& plan, const std::set<std::string>& tombstones);

    /// Borra la marca de revision de un movimiento (confirmado, revisado).
    void clearReview(const core::Id& movementId);


    [[nodiscard]] core::QuoteDecisions loadQuoteDecisions();
    void saveQuoteDecisions(const core::QuoteDecisions& decisions);
    [[nodiscard]] QString quoteFolder();
    void watchQuoteFolder(const QString& folder);
    [[nodiscard]] core::Id stamp(std::string& hlc, std::string& deviceId);

    /// Refleja el estado de la sesion en el boton y en el pie.
    void updateCloudUi(const QString& message);

    std::unique_ptr<storage::Database> db_;
    std::unique_ptr<storage::Repository> repository_;
    QString deviceId_;

    std::unique_ptr<sync::SupabaseClient> supabase_;
    std::unique_ptr<sync::SyncEngine> syncEngine_;
    QPushButton* cloudButton_ = nullptr;
    QPushButton* syncButton_ = nullptr;
    QLabel* cloudStatus_ = nullptr;

    /// Disparo unico. Cada cambio local lo reinicia, asi que cinco
    /// movimientos seguidos son una sola subida y no cinco.
    QTimer* autoSyncTimer_ = nullptr;

    /// Si la sincronizacion en curso la pidio el temporizador y no el usuario.
    /// Sirve para una sola cosa: que un fallo de sesion no abra un cuadro de
    /// dialogo encima de alguien que esta anotando y no pidio nada.
    bool lastSyncWasAutomatic_ = false;

    /// Repetitivo, cada diez minutos. El de arriba cubre lo que escribis vos;
    /// este cubre lo que escribiste en el TELEFONO, que de otro modo no
    /// aparece hasta que toques algo aca.
    QTimer* periodicSyncTimer_ = nullptr;

    /// El estado anterior del ultimo movimiento tocado. Vacio si no hay nada
    /// que deshacer, o si lo ultimo fue crearlo —eso lo dice undoWasCreate_—.
    std::optional<dake::core::Movement> undoBefore_;

    /// El movimiento tal como quedo. Es lo que hay que borrar si lo ultimo fue
    /// una creacion.
    std::optional<dake::core::Movement> undoAfter_;

    /// Que decir en el aviso: "Se deshizo: <esto>".
    QString undoLabel_;

    /// Lo que nacio junto con undoAfter_ y tiene que irse con el: el traspaso
    /// de sueldo de un gasto personal pagado con plata del negocio. Solo se
    /// usa cuando lo ultimo fue una creacion.
    std::vector<dake::core::Movement> undoAlso_;

    Snapshot snapshot_;

    QStackedWidget* stack_ = nullptr;
    QList<QPushButton*> navButtons_;
    TodayPage* today_ = nullptr;
    RepairsPage* repairs_ = nullptr;
    ReviewPage* review_ = nullptr;
    QTimer* hourlyTimer_ = nullptr;
    MovementsPage* movements_ = nullptr;
    PocketsPage* pockets_ = nullptr;
    ReportsPage* reports_ = nullptr;
    SettingsPage* settings_ = nullptr;
    QLabel* footer_ = nullptr;

    CaptureWindow* captureWindow_ = nullptr;
    GlobalHotkey* hotkey_ = nullptr;
    QSystemTrayIcon* tray_ = nullptr;

    QFileSystemWatcher* quoteWatcher_ = nullptr;
    /// Cotizaciones escribe un archivo temporal y lo renombra: una rafaga de
    /// avisos por cada guardado. Se espera un momento y se lee una vez.
    QTimer* quoteDebounce_ = nullptr;

    /// true solo cuando se eligio Salir en la bandeja. Cerrar la ventana con
    /// la X no sale.
    bool quitting_ = false;
};

} // namespace dake::ui
