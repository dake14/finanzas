// ui/pocketspage.cpp — donde esta la plata, y si la app le acerta.
//
// La aplicacion actual no tiene esta pantalla porque no tiene el concepto: sus
// "secciones" son un porcentaje de una resta, no un lugar con saldo. Nada de
// lo que muestra se puede contar a mano y comparar, y un numero que no se
// puede verificar nunca llega a creerse del todo.
//
// El boton "Cuadrar" es la otra mitad: se escribe lo que hay DE VERDAD y la
// aplicacion anota la diferencia como un movimiento visible en vez de mover el
// saldo por debajo. Un ajuste escondido es un agujero que se repite.

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "dake/core/accounts.hpp"
#include "cards.hpp"
#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString kindLabel(core::PocketKind kind) {
    switch (kind) {
        case core::PocketKind::Operacion: return QStringLiteral("Operacion");
        case core::PocketKind::Ahorro: return QStringLiteral("Ahorro");
        case core::PocketKind::Inversion: return QStringLiteral("Inversion");
        case core::PocketKind::Personal: return QStringLiteral("Personal");
        case core::PocketKind::Emergencia: return QStringLiteral("Emergencia");
    }
    return {};
}

} // namespace

PocketsPage::PocketsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void PocketsPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* page = new QWidget(scroll);
    scroll->setWidget(page);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto* headerRow = new QHBoxLayout();
    auto* heading = new QLabel(QStringLiteral("Bolsillos"), page);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    headerRow->addWidget(heading);
    headerRow->addStretch(1);

    auto* newButton = new QPushButton(QStringLiteral("Nuevo bolsillo"), page);
    newButton->setObjectName(QStringLiteral("PrimaryButton"));
    newButton->setCursor(Qt::PointingHandCursor);
    newButton->setFont(theme::bodyFont(9, QFont::DemiBold));
    newButton->setFixedHeight(34);
    connect(newButton, &QPushButton::clicked, this, &PocketsPage::newPocketRequested);
    headerRow->addWidget(newButton);
    layout->addLayout(headerRow);

    total_ = new QLabel(page);
    total_->setFont(theme::bodyFont(10));
    total_->setWordWrap(true);
    theme::setLabelColor(total_, theme::kTextMuted);
    layout->addWidget(total_);

    table_ = makeTable({QStringLiteral("Bolsillo"), QStringLiteral("Para que"),
                        QStringLiteral("Cuenta"), QStringLiteral("Sin cobrar"),
                        QStringLiteral("Saldo"), QStringLiteral("")},
                       0);
    table_->setMinimumHeight(200);
    fixColumn(table_, 2, 96);
    fixColumn(table_, 5, 96);
    layout->addWidget(table_);

    auto* monthsCard = new Card(QStringLiteral("MES A MES"), page);
    monthsCard->setSubtitle(
        QStringLiteral("Lo que dejo cada mes y cuanto de eso salio de las reservas. Dos meses "
                       "seguidos en rojo en la segunda columna es el aviso que importa."));
    auto* monthsBody = new QWidget(monthsCard);
    monthsLayout_ = new QVBoxLayout(monthsBody);
    monthsLayout_->setContentsMargins(0, 4, 0, 0);
    monthsLayout_->setSpacing(6);
    monthsCard->addContent(monthsBody);
    layout->addWidget(monthsCard);

    layout->addStretch(1);
}

void PocketsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    const core::Currency currency = snapshot.currency;

    const auto balances =
        core::pocketBalances(snapshot.pockets, snapshot.movements, currency, snapshot.today);

    table_->setRowCount(static_cast<int>(balances.size()));
    for (int row = 0; row < static_cast<int>(balances.size()); ++row) {
        const core::PocketBalance& balance = balances[static_cast<std::size_t>(row)];

        setText(table_, row, 0, QString::fromStdString(balance.name),
                theme::pocketColor(balance.kind));
        setText(table_, row, 1, kindLabel(balance.kind), theme::kTextMuted);

        // La cuenta es un boton y no un texto: cambiarla es un clic, y es lo
        // que hace falta el dia que se abre un ahorro personal.
        const core::Pocket* pocket = snapshot.pocket(balance.pocketId);
        const bool personal =
            pocket != nullptr && core::accountOf(*pocket) == core::Account::Personal;
        auto* accountButton = new QPushButton(
            personal ? QStringLiteral("Personal") : QStringLiteral("Negocio"), table_);
        accountButton->setObjectName(QStringLiteral("GhostButton"));
        accountButton->setFixedHeight(24);
        accountButton->setCursor(Qt::PointingHandCursor);
        accountButton->setFont(theme::bodyFont(8));
        accountButton->setToolTip(
            QStringLiteral("Pasarlo a %1. Lo que va de un bolsillo del negocio a uno personal\n"
                           "cuenta como sueldo.")
                .arg(personal ? QStringLiteral("negocio") : QStringLiteral("personal")));
        const core::Id pocketId = balance.pocketId;
        connect(accountButton, &QPushButton::clicked, this,
                [this, pocketId] { emit accountToggled(pocketId); });
        table_->setCellWidget(row, 2, accountButton);

        setNumber(table_, row, 3,
                  balance.pendingIn.isZero() ? QStringLiteral("—")
                                             : theme::formatMoney(balance.pendingIn),
                  theme::kAviso);
        setNumber(table_, row, 4, theme::formatMoney(balance.balance),
                  balance.balance.isNegative() ? theme::kNegative : theme::kText);

        auto* button = new QPushButton(QStringLiteral("Cuadrar"), table_);
        button->setObjectName(QStringLiteral("GhostButton"));
        button->setFixedHeight(24);
        button->setCursor(Qt::PointingHandCursor);
        button->setFont(theme::bodyFont(8));
        button->setToolTip(
            QStringLiteral("Contá lo que hay de verdad en este bolsillo y escribilo.\n"
                           "La diferencia queda anotada como un movimiento visible,\n"
                           "no como un saldo corregido por debajo."));
        const core::Id id = balance.pocketId;
        connect(button, &QPushButton::clicked, this,
                [this, id] { emit reconcileRequested(id); });
        table_->setCellWidget(row, 5, button);
    }

    const core::Money all = core::totalAll(balances, currency);
    const core::Money reserves = core::reserveTotal(balances, currency);
    total_->setText(
        QStringLiteral("En total tenés %1, de los cuales %2 son reserva. Este es el unico "
                       "numero de toda la aplicacion que se puede contar a mano y comparar; "
                       "si no cuadra, cuadralo hoy y no en tres meses.")
            .arg(theme::formatMoney(all), theme::formatMoney(reserves)));

    // --- Mes a mes ---------------------------------------------------------
    while (QLayoutItem* item = monthsLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const auto months = core::summarizeByMonth(snapshot.pockets, snapshot.movements, currency);
    if (months.empty()) {
        auto* empty = new QLabel(QStringLiteral("Todavia no hay meses con actividad."), this);
        empty->setFont(theme::bodyFont(9));
        theme::setLabelColor(empty, theme::kTextMuted);
        monthsLayout_->addWidget(empty);
    }

    for (const core::MonthSummary& month : months) {
        auto* row = new QWidget(this);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto* label = new QLabel(QString::fromStdString(month.label()), row);
        label->setFont(theme::bodyFont(10, QFont::DemiBold));
        label->setFixedWidth(84);
        theme::setLabelColor(label, theme::kText);

        auto* result = new QLabel(
            QStringLiteral("dejo %1").arg(theme::formatMoney(month.result)), row);
        result->setFont(theme::numericFont(10));
        result->setFixedWidth(160);
        theme::setLabelColor(result,
                             month.result.isNegative() ? theme::kNegative : theme::kPositive);

        auto* fundingLabel = new QLabel(row);
        fundingLabel->setFont(theme::numericFont(10));
        if (month.netFunding.minor() > 0) {
            fundingLabel->setText(QStringLiteral("y saco %1 de las reservas")
                                      .arg(theme::formatMoney(month.netFunding)));
            theme::setLabelColor(fundingLabel, theme::kNegative);
        } else if (month.netFunding.minor() < 0) {
            fundingLabel->setText(QStringLiteral("y guardo %1")
                                      .arg(theme::formatMoney(-month.netFunding)));
            theme::setLabelColor(fundingLabel, theme::kPositive);
        } else {
            fundingLabel->setText(QStringLiteral("sin tocar las reservas"));
            theme::setLabelColor(fundingLabel, theme::kTextMuted);
        }

        rowLayout->addWidget(label);
        rowLayout->addWidget(result);
        rowLayout->addWidget(fundingLabel);
        rowLayout->addStretch(1);
        monthsLayout_->addWidget(row);
    }
}

} // namespace dake::ui
