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

#include "dake/storage/database.hpp"
#include "dake/storage/repository.hpp"
#include "dake/sync/supabase_client.hpp"
#include "dake/sync/sync_engine.hpp"
#include "snapshot.hpp"

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace dake::ui {

class TodayPage;
class JobsPage;
class MovementsPage;
class PocketsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& dbPath, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void addMovement(const dake::core::Movement& draft);
    void newPocket();
    void newJob();
    void reconcile(const dake::core::Id& pocketId);
    void editMovement(const dake::core::Id& movementId);
    void toggleJob(const dake::core::Id& jobId);
    void resetToSeed();

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
    void reload();

    /// Lo que sigue a TODO cambio hecho en esta maquina: recarga la pantalla y
    /// programa la sincronizacion. Los `reload()` que quedan sueltos son los
    /// dos que no son cambios locales —el del arranque y el de despues de
    /// bajar del servidor—, y por eso mismo no pueden pasar por aca: bajar
    /// algo no debe disparar una subida.
    void afterLocalChange();

    /// Vence el temporizador. Sincroniza si hay sesion y el motor esta libre.
    void runAutoSync();
    void showPage(int index);
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

    Snapshot snapshot_;

    QStackedWidget* stack_ = nullptr;
    QList<QPushButton*> navButtons_;
    TodayPage* today_ = nullptr;
    JobsPage* jobs_ = nullptr;
    MovementsPage* movements_ = nullptr;
    PocketsPage* pockets_ = nullptr;
    QLabel* footer_ = nullptr;
};

} // namespace dake::ui
