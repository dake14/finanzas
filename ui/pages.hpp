#pragma once
//
// ui/pages.hpp — las cuatro pantallas.
//
// Ninguna toca la base ni hace cuentas propias: reciben un Snapshot, piden los
// numeros a dake::core y emiten señales. Toda cifra que aparezca en pantalla
// salio de una funcion que tiene una prueba escrita.
//
#include <QWidget>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/report.hpp"
#include "snapshot.hpp"

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;

#include "charts.hpp"

namespace dake::ui {

class Card;
class KpiCard;
class CaptureWidget;

/// Barra apilada de una sola linea: con que se pago el mes. Se pinta a mano
/// porque son dos rectangulos y una leyenda, y traer una libreria de graficos
/// para eso seria mas codigo, no menos.
class FundingBar : public QWidget {
    Q_OBJECT

public:
    explicit FundingBar(QWidget* parent = nullptr);

    void setValues(const core::Money& earned, const core::Money& fromReserves);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::Money earned_;
    core::Money fromReserves_;
};

// ------------------------------------------------------------------- Hoy

class TodayPage : public QWidget {
    Q_OBJECT

public:
    explicit TodayPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);
    [[nodiscard]] CaptureWidget* capture() const noexcept { return capture_; }

private:
    void buildUi();

    CaptureWidget* capture_ = nullptr;
    Card* entryCard_ = nullptr;
    QLabel* heading_ = nullptr;
    QLabel* subheading_ = nullptr;

    KpiCard* kpiCash_ = nullptr;
    KpiCard* kpiReserves_ = nullptr;
    KpiCard* kpiPending_ = nullptr;
    KpiCard* kpiPrepaid_ = nullptr;

    QLabel* fundingHeadline_ = nullptr;
    QLabel* fundingDetail_ = nullptr;
    FundingBar* fundingBar_ = nullptr;

    QLabel* cashValue_ = nullptr;
    QLabel* resultValue_ = nullptr;
    QLabel* twoNumbersDetail_ = nullptr;

    // --- Segunda fila de indicadores -------------------------------------
    //
    // Los cuatro de arriba contestan "cuanta plata hay". Estos contestan "como
    // anda el negocio", que es otra pregunta y por eso van en su propia fila.
    KpiCard* kpiBreakEven_ = nullptr;  ///< cuanto facturar por mes para no perder
    KpiCard* kpiRunway_ = nullptr;     ///< meses que aguantan las reservas
    KpiCard* kpiTicket_ = nullptr;     ///< cuanto deja un trabajo, en promedio
    KpiCard* kpiCollection_ = nullptr; ///< dias que tardas en cobrar

    // --- Graficas ---------------------------------------------------------
    BarChart* chartResult_ = nullptr;    ///< resultado por mes, con signo
    BarChart* chartIncomeCost_ = nullptr;///< ingresos contra costos
    LineChart* chartCash_ = nullptr;     ///< caja acumulada
    RankChart* chartCategories_ = nullptr;///< en que se va la plata
    RankChart* chartJobs_ = nullptr;     ///< margen por trabajo

    QLabel* overheadValue_ = nullptr;    ///< costo de la estructura
    QLabel* overheadDetail_ = nullptr;   ///< y que porcentaje se come

    QVBoxLayout* alertsLayout_ = nullptr;
};

// -------------------------------------------------------------- Trabajos

class JobsPage : public QWidget {
    Q_OBJECT

public:
    explicit JobsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

signals:
    void newJobRequested();
    void jobActivated(const dake::core::Id& jobId);

private:
    void buildUi();

    Snapshot snapshot_;
    QLabel* summary_ = nullptr;
    QTableWidget* table_ = nullptr;
};

// ------------------------------------------------------------ Movimientos

// -------------------------------------------------------------- Cierre

/// Cierre de un mes terminado.
///
/// Las otras pantallas responden "como voy". Esta responde "como me fue", que
/// es una pregunta distinta y se hace una vez al mes: con el mes cerrado, sin
/// dias por delante que puedan cambiar el numero, y contra el mes anterior,
/// que es la unica comparacion que dice si algo mejoro.
class ClosingPage : public QWidget {
    Q_OBJECT

public:
    explicit ClosingPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

private:
    void buildUi();
    void refill();

    Snapshot snapshot_;
    QComboBox* month_ = nullptr;
    KpiCard* result_ = nullptr;
    KpiCard* income_ = nullptr;
    KpiCard* cost_ = nullptr;
    KpiCard* cash_ = nullptr;
    KpiCard* pending_ = nullptr;
    QLabel* versus_ = nullptr;
    QTableWidget* categories_ = nullptr;
};

class MovementsPage : public QWidget {
    Q_OBJECT

public:
    explicit MovementsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);
    void focusSearch();

signals:
    void movementActivated(const dake::core::Id& movementId);

private:
    void buildUi();
    void refill();

    Snapshot snapshot_;
    QLineEdit* search_ = nullptr;
    QLabel* summary_ = nullptr;
    QTableWidget* table_ = nullptr;
};

// -------------------------------------------------------------- Bolsillos

/// Los reportes, en pestanas. Cada uno contesta una pregunta y la primera
/// linea de cada uno es la respuesta.
class ReportsPage : public QWidget {
    Q_OBJECT

public:
    explicit ReportsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

private:
    void buildUi();
    [[nodiscard]] QWidget* buildSpendingTab();
    void refillSpending();

    Snapshot snapshot_;
    QTabWidget* tabs_ = nullptr;

    // --- Gastos por categoria ----------------------------------------------
    QComboBox* spendingMonth_ = nullptr;
    QLabel* businessHeadline_ = nullptr;
    QLabel* personalHeadline_ = nullptr;
    CompareChart* businessChart_ = nullptr;
    CompareChart* personalChart_ = nullptr;
};

/// Todo lo que se configura una vez y no se vuelve a mirar.
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

signals:
    void categoryChanged(const dake::core::Category& category);
    void hotkeyChanged(const QKeySequence& sequence);
    void autostartChanged(bool enabled);

private:
    void buildUi();

    Snapshot snapshot_;
    QTableWidget* categories_ = nullptr;
    QKeySequenceEdit* hotkey_ = nullptr;
    QLabel* hotkeyStatus_ = nullptr;
    QCheckBox* autostart_ = nullptr;
    QLabel* captureTiming_ = nullptr;
};

class PocketsPage : public QWidget {
    Q_OBJECT

public:
    explicit PocketsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

signals:
    void newPocketRequested();
    void reconcileRequested(const dake::core::Id& pocketId);
    /// Pasa el bolsillo de negocio a personal o al reves.
    void accountToggled(const dake::core::Id& pocketId);

private:
    void buildUi();

    Snapshot snapshot_;
    QLabel* total_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVBoxLayout* monthsLayout_ = nullptr;
};

} // namespace dake::ui
