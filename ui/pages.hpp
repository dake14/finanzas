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

class QLabel;
class QLineEdit;
class QTableWidget;
class QVBoxLayout;

#include "charts.hpp"

namespace dake::ui {

class Card;
class KpiCard;
class QuickEntry;

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
    [[nodiscard]] QuickEntry* quickEntry() const noexcept { return quickEntry_; }

private:
    void buildUi();

    QuickEntry* quickEntry_ = nullptr;
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

class PocketsPage : public QWidget {
    Q_OBJECT

public:
    explicit PocketsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

signals:
    void newPocketRequested();
    void reconcileRequested(const dake::core::Id& pocketId);

private:
    void buildUi();

    Snapshot snapshot_;
    QLabel* total_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVBoxLayout* monthsLayout_ = nullptr;
};

} // namespace dake::ui
