#pragma once
//
// ui/pages.hpp — las cuatro pantallas.
//
// Ninguna toca la base ni hace cuentas propias: reciben un Snapshot, piden los
// numeros a dake::core y emiten señales. Toda cifra que aparezca en pantalla
// salio de una funcion que tiene una prueba escrita.
//
#include <QElapsedTimer>
#include <QWidget>

#include <set>
#include <string>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/report.hpp"
#include "snapshot.hpp"

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;

#include "charts.hpp"

namespace dake::ui {

class Card;
class CategoryBox;
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

    // --- Las tres preguntas ------------------------------------------------
    // Arriba de todo, despues de anotar: son la razon de la aplicacion.
    KpiCard* qPrices_ = nullptr;  ///< ¿subir precios?
    KpiCard* qSpend_ = nullptr;   ///< ¿cuanto gasto?
    KpiCard* qSalary_ = nullptr;  ///< ¿cuanto me pago?
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

/// Las reparaciones: la lista a la izquierda y la ficha de la elegida a la
/// derecha. La ficha se guarda sola al salir de cada campo.
class RepairsPage : public QWidget {
    Q_OBJECT

public:
    explicit RepairsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);
    void focusList();
    /// Muestra esa reparacion, aunque el filtro la escondiera.
    void selectRepair(const dake::core::Id& jobId);

signals:
    void newRepairRequested();
    void repairEdited(const dake::core::Repair& repair, const QString& client);
    void deliverRequested(const dake::core::Id& jobId);
    void chargeRequested(const dake::core::Id& jobId);
    /// `bought`: se compro ahora para esta reparacion, y hay que anotar el gasto.
    void partAdded(const dake::core::Id& jobId, const QString& name, qint64 costMinor,
                   bool costKnown, bool bought);
    void partChanged(const dake::core::RepairPart& part);
    void partRemoved(const dake::core::RepairPart& part);

private:
    void buildUi();
    [[nodiscard]] QWidget* buildPanel();
    void refillList();
    void showRepair(const core::Id& jobId);
    void saveEdits();

    Snapshot snapshot_;
    core::Id current_;
    bool filling_ = false;

    QComboBox* statusFilter_ = nullptr;
    QComboBox* typeFilter_ = nullptr;
    QLineEdit* search_ = nullptr;
    QTableWidget* list_ = nullptr;

    QLabel* empty_ = nullptr;
    Card* card_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* locked_ = nullptr;
    QLineEdit* client_ = nullptr;
    QLineEdit* device_ = nullptr;
    QComboBox* type_ = nullptr;
    QLineEdit* price_ = nullptr;
    QLineEdit* shipping_ = nullptr;
    QLineEdit* consumables_ = nullptr;
    QLineEdit* estHours_ = nullptr;
    QLineEdit* realHours_ = nullptr;
    QPushButton* deliver_ = nullptr;
    QPushButton* charge_ = nullptr;
    QTableWidget* parts_ = nullptr;
    QLineEdit* partName_ = nullptr;
    QLineEdit* partCost_ = nullptr;
    QCheckBox* partBought_ = nullptr;
    QLabel* costing_ = nullptr;
    QLabel* warnings_ = nullptr;
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

/// Los reportes, en pestanas. Cada uno contesta una pregunta y la primera
/// linea de cada uno es la respuesta.
class ReportsPage : public QWidget {
    Q_OBJECT

public:
    explicit ReportsPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);
    void showTab(int index);

private:
    void buildUi();
    [[nodiscard]] QWidget* buildSpendingTab();
    [[nodiscard]] QWidget* buildRepairsTab();
    [[nodiscard]] QWidget* buildTypesTab();
    [[nodiscard]] QWidget* buildCashTab();
    [[nodiscard]] QWidget* buildSalaryTab();
    void refillCash();
    void refillSalary();
    void refillSpending();
    void refillRepairs();
    void refillTypes();

    Snapshot snapshot_;
    QTabWidget* tabs_ = nullptr;

    // --- Gastos por categoria ----------------------------------------------
    QComboBox* spendingMonth_ = nullptr;
    QLabel* businessHeadline_ = nullptr;
    QLabel* personalHeadline_ = nullptr;
    CompareChart* businessChart_ = nullptr;
    CompareChart* personalChart_ = nullptr;

    // --- Rentabilidad por reparacion -------------------------------------
    QLabel* repairsHeadline_ = nullptr;
    QTableWidget* repairsTable_ = nullptr;

    // --- Rentabilidad por tipo --------------------------------------------
    QLabel* typesHeadline_ = nullptr;
    QLabel* typesNote_ = nullptr;
    TypeChart* typesChart_ = nullptr;

    // --- Flujo de caja ---------------------------------------------------
    QLabel* cashHeadline_ = nullptr;
    BarChart* cashBars_ = nullptr;
    LineChart* cashLine_ = nullptr;
    QTableWidget* cashTable_ = nullptr;

    // --- Sueldo ------------------------------------------------------------
    QLabel* salaryHeadline_ = nullptr;
    SalaryScale* salaryScale_ = nullptr;
    QLabel* salaryBasis_ = nullptr;
    SplitBar* salarySplit_ = nullptr;
    QLabel* salaryMonths_ = nullptr;
};

/// La revision de la semana: un pendiente por vez, con la sugerencia puesta.
class ReviewPage : public QWidget {
    Q_OBJECT

public:
    explicit ReviewPage(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);
    /// Empieza una revision: olvida lo saltado y arranca el cronometro.
    void startReview();

signals:
    void categorySet(const dake::core::Id& movementId, const QString& category);
    void recurringConfirmed(const dake::core::Id& movementId, qint64 amountMinor);
    void toolLifeSet(const dake::core::Id& movementId, int months);
    void quotesReviewRequested();
    void chargeRequested(const dake::core::Id& jobId);
    void repairOpened(const dake::core::Id& jobId);
    void realHoursSet(const dake::core::Id& jobId, int minutes);
    void partCostSet(const dake::core::Id& partId, qint64 costMinor);
    void snoozed(const std::string& id, int days);
    void movementDeleted(const dake::core::Id& movementId);
    /// La bandeja quedo vacia: lo que tardo la revision, para el criterio de
    /// los cinco minutos.
    void reviewFinished(qint64 elapsedMs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void showCurrent();
    void focusInput();
    void acceptCurrent();
    void skipCurrent();
    void snoozeCurrent();
    void deleteCurrent();
    [[nodiscard]] const core::InboxItem* current() const;
    [[nodiscard]] static std::string keyOf(const core::InboxItem& item);

    Snapshot snapshot_;
    std::set<std::string> skipped_;
    QElapsedTimer clock_;

    QLabel* summary_ = nullptr;
    Card* card_ = nullptr;
    QLabel* kind_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* detail_ = nullptr;
    CategoryBox* category_ = nullptr;
    QLineEdit* value_ = nullptr;
    QPushButton* accept_ = nullptr;
    QLabel* upcoming_ = nullptr;
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
    void costSettingsChanged(const dake::core::CostSettings& settings);
    void templateChanged(const dake::core::RepairTemplate& tpl);
    void templateAdded();
    void templateRemoved(const dake::core::Id& templateId);
    void quoteFolderChanged(const QString& folder);
    void quoteReadRequested();
    void quoteReviewRequested();
    void recurringChanged(const dake::core::Recurring& recurring);
    void recurringAdded();
    void recurringRemoved(const dake::core::Id& id);
    void toolChanged(const dake::core::Tool& tool);
    void toolAdded();
    void toolRemoved(const dake::core::Id& id);
    void fallbackHoursChanged(int minutesPerMonth);
    void reminderChanged(int weekday, int hour);
    void splitChanged(const dake::core::ProfitSplit& split);
    void bankImportRequested();

private:
    void buildUi();
    void refillTemplates();
    void emitCosts();
    void emitSplit();
    void emitTemplate(int row);
    void refillFixed();
    void emitRecurring(int row);
    void emitTool(int row);

    Snapshot snapshot_;
    QTableWidget* categories_ = nullptr;
    QKeySequenceEdit* hotkey_ = nullptr;
    QLabel* hotkeyStatus_ = nullptr;
    QCheckBox* autostart_ = nullptr;
    QLabel* captureTiming_ = nullptr;
    QLineEdit* hourlyRate_ = nullptr;
    QLineEdit* targetMargin_ = nullptr;
    QLabel* costsNote_ = nullptr;
    QLineEdit* splitSalary_ = nullptr;
    QLineEdit* splitTaxes_ = nullptr;
    QLineEdit* splitReinvest_ = nullptr;
    QLineEdit* splitEmergency_ = nullptr;
    QLabel* splitNote_ = nullptr;
    QTableWidget* templates_ = nullptr;
    QLineEdit* quoteFolder_ = nullptr;
    QLabel* quoteStatus_ = nullptr;
    QPushButton* quoteReview_ = nullptr;
    QLabel* fixedSummary_ = nullptr;
    QTableWidget* recurring_ = nullptr;
    QTableWidget* tools_ = nullptr;
    QLineEdit* hoursPerMonth_ = nullptr;
    QComboBox* reminderDay_ = nullptr;
    QComboBox* reminderHour_ = nullptr;
    QLabel* timings_ = nullptr;
    bool filling_ = false;
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
