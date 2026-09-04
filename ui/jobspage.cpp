// ui/jobspage.cpp — cuanto dejo cada trabajo.
//
// Es la pantalla que la aplicacion actual no tiene y la que mas justifica
// cargar los datos. En la base real de agosto hay un ingreso de 120,00
// ("Trabajo Macbook Herman Galvan") y un gasto de 49,82 ("Teclado y backlight
// macbook y envio") que son evidentemente el mismo trabajo, y no hay forma de
// preguntarle a la aplicacion cuanto dejo: son dos filas sin relacion.
//
// Aca son un trabajo con margen: 70,18, el 58,5%. Ese numero es el que dice si
// conviene seguir aceptando reparaciones de Macbook, y es el unico motivo por
// el que alguien se sienta a anotar un gasto de 49,82.

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "cards.hpp"
#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {

JobsPage::JobsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void JobsPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto* headerRow = new QHBoxLayout();
    auto* heading = new QLabel(QStringLiteral("Trabajos"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    headerRow->addWidget(heading);
    headerRow->addStretch(1);

    auto* newButton = new QPushButton(QStringLiteral("Nuevo trabajo"), this);
    newButton->setObjectName(QStringLiteral("PrimaryButton"));
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setFont(theme::bodyFont(9, QFont::DemiBold));
    newButton->setFixedHeight(34);
    connect(newButton, &QPushButton::clicked, this, &JobsPage::newJobRequested);
    headerRow->addWidget(newButton);
    layout->addLayout(headerRow);

    summary_ = new QLabel(this);
    summary_->setFont(theme::bodyFont(10));
    summary_->setWordWrap(true);
    theme::setLabelColor(summary_, theme::kTextMuted);
    layout->addWidget(summary_);

    table_ = makeTable({QStringLiteral("Trabajo"), QStringLiteral("Cliente"),
                        QStringLiteral("Estado"), QStringLiteral("Cobrado y por cobrar"),
                        QStringLiteral("Costo"), QStringLiteral("Dejo"),
                        QStringLiteral("Margen")},
                       0);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = table_->item(row, 0);
        if (item != nullptr) {
            emit jobActivated(item->data(Qt::UserRole).toString().toStdString());
        }
    });
    layout->addWidget(table_, 1);
}

void JobsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    const core::Currency currency = snapshot.currency;

    const auto results = core::jobResults(snapshot.jobs, snapshot.movements, currency);

    table_->setRowCount(static_cast<int>(results.size()));

    core::Money totalMargin = core::Money::zero(currency);
    core::Money totalPending = core::Money::zero(currency);

    for (int row = 0; row < static_cast<int>(results.size()); ++row) {
        const core::JobResult& result = results[static_cast<std::size_t>(row)];
        totalMargin += result.margin;
        totalPending += result.pending;

        setText(table_, row, 0, QString::fromStdString(result.name));
        table_->item(row, 0)->setData(Qt::UserRole, QString::fromStdString(result.jobId));

        setText(table_, row, 1, QString::fromStdString(result.client), theme::kTextMuted);

        QString state = result.closed ? QStringLiteral("cerrado") : QStringLiteral("abierto");
        QColor stateColor = theme::kTextMuted;
        if (!result.pending.isZero()) {
            state = QStringLiteral("sin cobrar %1").arg(theme::formatMoney(result.pending));
            stateColor = theme::kInversion;
        }
        setText(table_, row, 2, state, stateColor);

        setNumber(table_, row, 3, theme::formatMoney(result.income), theme::kPositive);
        setNumber(table_, row, 4, theme::formatMoney(result.cost), theme::kNegative);
        setNumber(table_, row, 5, theme::formatMoney(result.margin),
                  result.margin.isNegative() ? theme::kNegative : theme::kPositive);
        setNumber(table_, row, 6,
                  result.income.isZero() ? QStringLiteral("—")
                                         : theme::formatBps(result.marginBps),
                  result.marginBps < 0 ? theme::kNegative : theme::kText);
    }

    const core::Date from = snapshot.monthStart();
    const core::Date to = snapshot.monthEnd();
    const core::Money structure = core::overhead(snapshot.movements, currency, from, to);

    summary_->setText(
        QStringLiteral("Los trabajos dejaron %1 en total. Aparte, la estructura del negocio "
                       "—lo que no cuelga de ningun trabajo— costo %2 este mes: ese es el "
                       "piso que los trabajos tienen que cubrir antes de dejar algo.%3")
            .arg(theme::formatMoney(totalMargin), theme::formatMoney(structure),
                 totalPending.isZero()
                     ? QString()
                     : QStringLiteral("  Hay %1 entregados y sin cobrar.")
                           .arg(theme::formatMoney(totalPending))));
}

} // namespace dake::ui
