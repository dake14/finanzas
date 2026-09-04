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
#include "snapshot.hpp"

class QLabel;
class QPushButton;
class QStackedWidget;

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

private:
    void buildUi();
    void buildSidebar(QWidget* parent);
    void reload();
    void showPage(int index);
    [[nodiscard]] core::Id stamp(std::string& hlc, std::string& deviceId);

    std::unique_ptr<storage::Database> db_;
    std::unique_ptr<storage::Repository> repository_;
    QString deviceId_;

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
