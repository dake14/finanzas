//
// ui/closingpage.cpp — el cierre de un mes terminado.
//
// No calcula nada: todo sale de dake::core, que es lo que garantiza que este
// numero y el que muestra el telefono sean el mismo.
//
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "cards.hpp"
#include "dake/core/format.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

/// Ultimo dia del mes. `month` es 1..12.
[[nodiscard]] core::Date lastDayOf(int year, unsigned month) {
    static constexpr unsigned dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned ultimo = dias[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        ultimo = 29;
    }
    return core::Date::fromYmd(year, month, ultimo);
}

[[nodiscard]] QString money(const core::Money& amount) {
    return QString::fromStdString(core::formatAmount(amount));
}

} // namespace

ClosingPage::ClosingPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void ClosingPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    refill();
}

void ClosingPage::buildUi() {
    // Layout vertical con margenes (24, 20, 24, 20) y espaciado 16.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    // 1. Fila superior
    auto* topRow = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("CIERRE DE MES"), this);
    title->setFont(theme::displayFont(16, QFont::Bold));
    theme::setLabelColor(title, theme::kText);

    month_ = new QComboBox(this);
    month_->setMinimumWidth(160);

    topRow->addWidget(title);
    topRow->addStretch(1);
    topRow->addWidget(month_);
    layout->addLayout(topRow);

    // 2. versus_
    versus_ = new QLabel(this);
    versus_->setFont(theme::bodyFont(10));
    theme::setLabelColor(versus_, theme::kTextMuted);
    versus_->setWordWrap(true);
    layout->addWidget(versus_);

    // 3. Fila de KpiCards
    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(12);

    result_ = new KpiCard(QStringLiteral("RESULTADO DEL MES"), theme::kAccent, this);
    income_ = new KpiCard(QStringLiteral("FACTURADO"), theme::kPositive, this);
    cost_ = new KpiCard(QStringLiteral("COSTO IMPUTADO"), theme::kNegative, this);
    cash_ = new KpiCard(QStringLiteral("VARIACION DE CAJA"), theme::kInversion, this);
    pending_ = new KpiCard(QStringLiteral("QUEDO POR COBRAR"), theme::kAhorro, this);

    kpiRow->addWidget(result_);
    kpiRow->addWidget(income_);
    kpiRow->addWidget(cost_);
    kpiRow->addWidget(cash_);
    kpiRow->addWidget(pending_);
    layout->addLayout(kpiRow);

    // 4. Categorias
    auto* categoriesCard = new Card(QStringLiteral("EN QUE SE FUE"), this);
    categories_ = new QTableWidget(0, 2, categoriesCard);
    categories_->setHorizontalHeaderLabels({QStringLiteral("Categoria"), QStringLiteral("Costo")});
    // La categoria se estira y el costo se ajusta a su contenido. Al reves
    // —que es lo que hace setStretchLastSection— el encabezado "Costo" queda
    // flotando en el medio de una columna vacia y el numero pegado al borde
    // derecho, a diez centimetros de su propia etiqueta.
    categories_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    categories_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    categories_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    categories_->setSelectionMode(QAbstractItemView::NoSelection);
    categories_->verticalHeader()->setVisible(false);
    categories_->setMinimumHeight(220);

    categoriesCard->addContent(categories_);
    layout->addWidget(categoriesCard);

    // 5. addStretch
    layout->addStretch(1);

    // Conectar combobox
    connect(month_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refill();
    });
}

void ClosingPage::refill() {
    // 1. Obtener meses
    const std::vector<core::MonthSummary> meses = core::summarizeByMonth(
        snapshot_.pockets, snapshot_.movements, snapshot_.currency);

    // 2. Si no hay datos
    if (meses.empty()) {
        result_->setValue(QStringLiteral("—"));
        income_->setValue(QStringLiteral("—"));
        cost_->setValue(QStringLiteral("—"));
        cash_->setValue(QStringLiteral("—"));
        pending_->setValue(QStringLiteral("—"));
        versus_->setText(QStringLiteral("Todavia no hay ningun mes con movimientos."));
        categories_->setRowCount(0);
        return;
    }

    // 3. Rearmar el selector conservando el mes que estabas mirando.
    //
    // Se rearma siempre y no solo cuando cambia la cantidad: borrar el unico
    // movimiento de un mes y anotar en otro deja la MISMA cantidad de meses
    // con etiquetas distintas, y comparar por cantidad dejaria el selector
    // mostrando un mes que ya no existe. La seleccion se recupera por
    // etiqueta, que es lo que el usuario eligio; si ese mes ya no esta, cae en
    // el mas nuevo, que es el que abre la pantalla.
    {
        const QString elegidaAntes = month_->currentText();
        const QSignalBlocker blocker(month_);
        month_->clear();
        // Del mas nuevo al mas viejo: el cierre que se mira es el ultimo.
        for (size_t i = meses.size(); i-- > 0;) {
            month_->addItem(QString::fromStdString(meses[i].label()), static_cast<int>(i));
        }
        const int previa = month_->findText(elegidaAntes);
        month_->setCurrentIndex(previa >= 0 ? previa : 0);
    }

    // 4. Extraer indice del mes elegido
    bool ok = false;
    int indexElegido = month_->currentData().toInt(&ok);
    if (!ok || indexElegido < 0 || static_cast<size_t>(indexElegido) >= meses.size()) {
        indexElegido = static_cast<int>(meses.size() - 1);
    }
    const core::MonthSummary& elegido = meses[static_cast<size_t>(indexElegido)];

    // 5. Fechas
    const core::Date desde = core::Date::fromYmd(elegido.year, elegido.month, 1);
    const core::Date hasta = lastDayOf(elegido.year, elegido.month);

    // 6. Flujo del mes
    const core::CashFlow flujo = core::cashFlow(snapshot_.movements, snapshot_.currency, desde, hasta);
    result_->setValue(money(flujo.result));
    income_->setValue(money(flujo.incomeAccrued));
    cost_->setValue(money(flujo.cost));
    cash_->setValue(money(flujo.cashDelta));

    // 7. Por cobrar
    const auto balances = core::pocketBalances(snapshot_.pockets, snapshot_.movements, snapshot_.currency, hasta);
    core::Money totalPending = core::Money::zero(snapshot_.currency);
    for (const core::PocketBalance& balance : balances) {
        totalPending += balance.pendingIn;
    }
    pending_->setValue(money(totalPending));

    // 8. Versus
    if (indexElegido > 0) {
        const core::MonthSummary& anterior = meses[static_cast<size_t>(indexElegido) - 1];
        const core::Money diferencia = elegido.result - anterior.result;
        QString diffText = money(diferencia);
        if (!diferencia.isNegative() && !diferencia.isZero()) {
            diffText.prepend(QStringLiteral("+"));
        }
        versus_->setText(QStringLiteral("Contra %1: %2 de resultado.")
                             .arg(QString::fromStdString(anterior.label()), diffText));
    } else {
        versus_->setText(QStringLiteral("Es el primer mes con movimientos: no hay contra que compararlo."));
    }

    // 9. Categorias
    const std::vector<core::CategoryTotal> cats = core::costByCategory(
        snapshot_.movements, snapshot_.currency, desde, hasta);
    categories_->setRowCount(0);
    categories_->setRowCount(static_cast<int>(cats.size()));
    for (size_t i = 0; i < cats.size(); ++i) {
        const core::CategoryTotal& ct = cats[i];
        const int row = static_cast<int>(i);

        QString catName = QString::fromStdString(ct.category);
        if (catName.isEmpty()) {
            catName = QStringLiteral("Sin categoria");
        }

        auto* nameItem = new QTableWidgetItem(catName);
        categories_->setItem(row, 0, nameItem);

        auto* costItem = new QTableWidgetItem(money(ct.total));
        costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        categories_->setItem(row, 1, costItem);
    }
}

} // namespace dake::ui
