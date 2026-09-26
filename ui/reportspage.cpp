// ui/reportspage.cpp — los reportes, uno por pestana.
//
// Ningun numero se calcula aca: todos salen de dake::core. La pantalla solo
// decide en que orden se leen y con que color.

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "cards.hpp"
#include "charts.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QLabel* headline(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setFont(theme::displayFont(13, QFont::DemiBold));
    label->setWordWrap(true);
    theme::setLabelColor(label, theme::kText);
    return label;
}

/// Una pestana con desplazamiento vertical. Devuelve el layout donde va el
/// contenido.
[[nodiscard]] QVBoxLayout* scrollingTab(QWidget* tab) {
    auto* outer = new QVBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);
    auto* body = new QWidget(scroll);
    scroll->setWidget(body);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 16, 4, 16);
    layout->setSpacing(14);
    return layout;
}

/// Los meses con algun movimiento, del mas nuevo al mas viejo, hasta hoy.
[[nodiscard]] std::vector<core::Date> activeMonths(const Snapshot& snapshot) {
    std::vector<core::Date> months;
    core::Date first = snapshot.today.firstDayOfMonth();
    for (const core::Movement& m : snapshot.movements) {
        if (!m.deleted && m.date.firstDayOfMonth() < first) {
            first = m.date.firstDayOfMonth();
        }
    }
    for (core::Date month = snapshot.today.firstDayOfMonth(); month >= first;
         month = month.addMonths(-1)) {
        months.push_back(month);
    }
    return months;
}

[[nodiscard]] std::vector<CompareRow> compareRows(const core::SpendingReport& report) {
    std::vector<CompareRow> rows;
    for (const core::CategorySpend& spend : report.rows) {
        CompareRow row;
        row.label = QString::fromStdString(spend.category);
        row.current = static_cast<double>(spend.current.minor());
        row.previous = static_cast<double>(spend.previous.minor());
        row.currentText = theme::formatMoney(spend.current);
        row.changeText = theme::changeText(spend.current, spend.previous);
        row.rose = spend.change().minor() > 0;
        rows.push_back(row);
    }
    return rows;
}

[[nodiscard]] QString spendingHeadline(const QString& who, const core::SpendingReport& report,
                                       core::Date month) {
    if (report.totalPrevious.isZero()) {
        return QStringLiteral("%1 %2 en %3. En %4 no hubo gastos para comparar.")
            .arg(who, theme::formatMoney(report.totalCurrent), theme::monthName(month),
                 theme::monthName(month.addMonths(-1)));
    }
    return QStringLiteral("%1 %2 en %3 · %4 contra %5")
        .arg(who, theme::formatMoney(report.totalCurrent), theme::monthName(month),
             theme::changeText(report.totalCurrent, report.totalPrevious),
             theme::monthName(month.addMonths(-1)));
}

} // namespace

ReportsPage::ReportsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void ReportsPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 12);
    layout->setSpacing(10);

    auto* heading = new QLabel(QStringLiteral("Reportes"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    layout->addWidget(heading);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->setFont(theme::bodyFont(10, QFont::DemiBold));
    tabs_->addTab(buildSpendingTab(), QStringLiteral("Gastos por categoría"));
    layout->addWidget(tabs_, 1);
}

QWidget* ReportsPage::buildSpendingTab() {
    auto* tab = new QWidget(this);
    QVBoxLayout* layout = scrollingTab(tab);
    QWidget* body = layout->parentWidget();

    auto* monthRow = new QHBoxLayout();
    auto* monthLabel = new QLabel(QStringLiteral("Mes"), body);
    monthLabel->setFont(theme::bodyFont(10));
    theme::setLabelColor(monthLabel, theme::kTextMuted);
    spendingMonth_ = new QComboBox(body);
    spendingMonth_->setMinimumWidth(180);
    spendingMonth_->setFont(theme::bodyFont(10));
    connect(spendingMonth_, &QComboBox::currentIndexChanged, this,
            [this] { refillSpending(); });
    monthRow->addWidget(monthLabel);
    monthRow->addWidget(spendingMonth_);
    monthRow->addStretch(1);
    layout->addLayout(monthRow);

    // Negocio y personal van en dos tarjetas separadas y nunca se suman: son
    // dos cuentas distintas, y un total que las mezcla no contesta nada.
    auto* business = new Card(QStringLiteral("NEGOCIO"), body);
    businessHeadline_ = headline(business);
    business->addContent(businessHeadline_);
    businessChart_ = new CompareChart(business);
    business->addContent(businessChart_);
    layout->addWidget(business);

    auto* personal = new Card(QStringLiteral("PERSONAL"), body);
    personalHeadline_ = headline(personal);
    personal->addContent(personalHeadline_);
    personalChart_ = new CompareChart(personal);
    personal->addContent(personalChart_);
    layout->addWidget(personal);

    auto* legend = new QLabel(
        QStringLiteral("La barra es el mes elegido y la raya blanca, el mes anterior. Primero "
                       "van las categorías que más subieron. El sueldo no aparece: pasar plata "
                       "del negocio a lo personal no es un gasto."),
        body);
    legend->setWordWrap(true);
    legend->setFont(theme::bodyFont(9));
    theme::setLabelColor(legend, theme::kTextFaint);
    layout->addWidget(legend);
    layout->addStretch(1);
    return tab;
}

void ReportsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;

    // El selector se rearma entero y recupera la seleccion por etiqueta, igual
    // que en el cierre: comparar por cantidad de meses deja el selector
    // mostrando un mes que ya no existe.
    const QString previous = spendingMonth_->currentText();
    {
        const QSignalBlocker blocker(spendingMonth_);
        spendingMonth_->clear();
        for (const core::Date month : activeMonths(snapshot)) {
            spendingMonth_->addItem(theme::monthName(month), QString::fromStdString(month.toIso()));
        }
        const int keep = spendingMonth_->findText(previous);
        spendingMonth_->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    refillSpending();
}

void ReportsPage::refillSpending() {
    const QString iso = spendingMonth_->currentData().toString();
    if (iso.isEmpty()) {
        return;
    }
    const core::Date month = core::Date::fromIso(iso.toStdString());

    const auto business = core::spendingByCategory(snapshot_.movements, snapshot_.pockets,
                                                   core::Account::Negocio, month,
                                                   snapshot_.currency);
    const auto personal = core::spendingByCategory(snapshot_.movements, snapshot_.pockets,
                                                   core::Account::Personal, month,
                                                   snapshot_.currency);

    businessHeadline_->setText(spendingHeadline(QStringLiteral("El negocio gastó"), business, month));
    personalHeadline_->setText(spendingHeadline(QStringLiteral("Gastaste"), personal, month));
    businessChart_->setData(compareRows(business), theme::kOperacion);
    personalChart_->setData(compareRows(personal), theme::kPersonal);
}

} // namespace dake::ui
