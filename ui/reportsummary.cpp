// ui/reportsummary.cpp — las cifras que antes estaban en Hoy y en Bolsillos.
//
// Desde el recorte, Hoy es para anotar y resolver pendientes; todos los
// numeros viven en Informes, ordenados por la pregunta que contestan. Este
// archivo arma las tarjetas que llegaron de Hoy (las tres preguntas, caja y
// reservas, con que se pago el mes, caja contra resultado, estructura, avisos,
// indicadores del negocio) y de Bolsillos (mes a mes). El resto de Informes
// esta en reportspage.cpp.
//
// Ningun numero se calcula aca: todos salen de dake::core.

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "cards.hpp"
#include "dake/core/accounts.hpp"
#include "dake/core/repairs.hpp"
#include "dake/core/report.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

/// Meses que cubre el rango, contando el primero y el ultimo.
[[nodiscard]] int monthsBetween(const core::Date& from, const core::Date& to) {
    const int months = (to.year - from.year) * 12 + static_cast<int>(to.month) -
                       static_cast<int>(from.month) + 1;
    return months < 1 ? 1 : months;
}

[[nodiscard]] QLabel* muted(const QString& text, QWidget* parent, int size = 9) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::bodyFont(size));
    label->setWordWrap(true);
    theme::setLabelColor(label, theme::kTextMuted);
    return label;
}

/// Vacia un layout que se rellena en cada recarga.
void clearLayout(QVBoxLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

[[nodiscard]] QHBoxLayout* kpiRow() {
    auto* row = new QHBoxLayout();
    row->setSpacing(12);
    return row;
}

} // namespace

// ------------------------------------------------------------- FundingBar

FundingBar::FundingBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(52);
}

void FundingBar::setValues(const core::Money& earned, const core::Money& fromReserves) {
    earned_ = earned;
    fromReserves_ = fromReserves;
    update();
}

void FundingBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qint64 earned = std::max<qint64>(earned_.minor(), 0);
    const qint64 reserves = std::max<qint64>(fromReserves_.minor(), 0);
    const qint64 total = earned + reserves;

    const QRectF track(0, 0, width(), 14);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::color(theme::kSurfaceRaised));
    painter.drawRoundedRect(track, 7, 7);

    if (total > 0) {
        const double earnedWidth =
            static_cast<double>(width()) * static_cast<double>(earned) /
            static_cast<double>(total);

        painter.setBrush(theme::color(theme::kPositive));
        painter.drawRoundedRect(QRectF(0, 0, std::max(earnedWidth, 14.0), 14), 7, 7);

        if (reserves > 0) {
            painter.setBrush(theme::color(theme::kNegative));
            const double x = earnedWidth;
            painter.drawRoundedRect(QRectF(x, 0, std::max(width() - x, 14.0), 14), 7, 7);
            // El redondeo de la derecha del tramo verde queda tapado por el
            // naranja, que es lo que hace que se lean como una sola barra.
            painter.setBrush(theme::color(theme::kPositive));
            painter.drawRect(QRectF(std::max(earnedWidth - 8.0, 0.0), 0, 8, 14));
        }
    }

    // --- Leyenda ---
    painter.setFont(theme::bodyFont(9));
    const int textTop = 22;

    painter.setPen(theme::color(theme::kPositive));
    painter.drawText(QRect(0, textTop, width() / 2, 26), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("%1 lo cobraste").arg(theme::formatMoney(earned_)));

    painter.setPen(theme::color(theme::kNegative));
    painter.drawText(QRect(width() / 2, textTop, width() / 2, 26), Qt::AlignRight | Qt::AlignTop,
                     QStringLiteral("%1 salio de tus reservas")
                         .arg(theme::formatMoney(fromReserves_)));
}

// ---------------------------------------------------------------- Resumen

void ReportsPage::addSummarySection(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();

    auto* row = kpiRow();
    kpiCash_ = new KpiCard(QStringLiteral("CAJA DEL NEGOCIO"), theme::kOperacion, body);
    kpiReserves_ = new KpiCard(QStringLiteral("RESERVAS"), theme::kAhorro, body);
    kpiRunway_ = new KpiCard(QStringLiteral("MESES DE RESERVA"), theme::kTextMuted, body);
    for (KpiCard* card : {kpiCash_, kpiReserves_, kpiRunway_}) {
        row->addWidget(card);
    }
    layout->addLayout(row);

    auto* fundingCard = new Card(QStringLiteral("CON QUÉ SE PAGÓ ESTE MES"), body);
    fundingHeadline_ = new QLabel(fundingCard);
    fundingHeadline_->setFont(theme::figureFont(19));
    fundingHeadline_->setWordWrap(true);
    fundingCard->addContent(fundingHeadline_);
    fundingBar_ = new FundingBar(fundingCard);
    fundingCard->addContent(fundingBar_);
    fundingDetail_ = muted(QString(), fundingCard, 10);
    fundingCard->addContent(fundingDetail_);
    layout->addWidget(fundingCard);

    auto* alertsCard = new Card(QStringLiteral("LO QUE HAY QUE MIRAR"), body);
    alertsCard->setSubtitle(
        QStringLiteral("Un libro de cuentas solo devuelve lo que se le mete. Esto es lo que "
                       "la aplicación dice sin que se lo pregunten."));
    auto* alertsBody = new QWidget(alertsCard);
    alertsLayout_ = new QVBoxLayout(alertsBody);
    alertsLayout_->setContentsMargins(0, 4, 0, 0);
    alertsLayout_->setSpacing(12);
    alertsCard->addContent(alertsBody);
    layout->addWidget(alertsCard);
}

// ---------------------------------------------------------------- El mes

void ReportsPage::addSpendKpi(QVBoxLayout* layout) {
    auto* row = kpiRow();
    qSpend_ = new KpiCard(QStringLiteral("¿CUÁNTO GASTO? · ESTE MES"), theme::kNegative,
                          layout->parentWidget());
    row->addWidget(qSpend_);
    row->addStretch(2);
    layout->addLayout(row);
}

void ReportsPage::addMonthsSection(QVBoxLayout* layout) {
    auto* monthsCard = new Card(QStringLiteral("MES A MES"), layout->parentWidget());
    monthsCard->setSubtitle(
        QStringLiteral("Lo que dejó cada mes y cuánto de eso salió de las reservas. Dos meses "
                       "seguidos en rojo en la segunda columna es el aviso que importa."));
    auto* monthsBody = new QWidget(monthsCard);
    monthsLayout_ = new QVBoxLayout(monthsBody);
    monthsLayout_->setContentsMargins(0, 4, 0, 0);
    monthsLayout_->setSpacing(6);
    monthsCard->addContent(monthsBody);
    layout->addWidget(monthsCard);
}

void ReportsPage::addTwoNumbersSection(QVBoxLayout* layout) {
    auto* twoCard = new Card(QStringLiteral("CAJA Y RESULTADO NO SON EL MISMO NÚMERO"),
                             layout->parentWidget());
    auto* twoBody = new QWidget(twoCard);
    auto* twoGrid = new QGridLayout(twoBody);
    twoGrid->setContentsMargins(0, 6, 0, 0);
    twoGrid->setHorizontalSpacing(28);

    auto* cashCaption = muted(QStringLiteral("LO QUE BAJÓ O SUBIÓ LA CAJA"), twoBody);
    auto* resultCaption = muted(QStringLiteral("LO QUE DEJÓ EL MES"), twoBody);
    cashValue_ = new QLabel(twoBody);
    cashValue_->setFont(theme::figureFont(24));
    resultValue_ = new QLabel(twoBody);
    resultValue_->setFont(theme::figureFont(24));

    twoGrid->addWidget(cashCaption, 0, 0);
    twoGrid->addWidget(resultCaption, 0, 1);
    twoGrid->addWidget(cashValue_, 1, 0);
    twoGrid->addWidget(resultValue_, 1, 1);
    // Ancho minimo explicito: sin el, los dos rotulos se parten en dos lineas y
    // las cifras quedan pegadas una a la otra.
    twoGrid->setColumnMinimumWidth(0, 240);
    twoGrid->setColumnMinimumWidth(1, 240);
    twoGrid->setColumnStretch(2, 1);
    twoCard->addContent(twoBody);

    twoNumbersDetail_ = muted(QString(), twoCard, 10);
    twoCard->addContent(twoNumbersDetail_);
    layout->addWidget(twoCard);
}

// ---------------------------------------------------------------- Gastos

void ReportsPage::addOverheadSection(QVBoxLayout* layout) {
    auto* overheadCard = new Card(QStringLiteral("COSTO DE ESTRUCTURA"), layout->parentWidget());
    auto* overheadBody = new QWidget(overheadCard);
    auto* overheadLayout = new QVBoxLayout(overheadBody);
    overheadLayout->setContentsMargins(0, 6, 0, 0);
    overheadValue_ = new QLabel(overheadBody);
    overheadValue_->setFont(theme::figureFont(24));
    overheadLayout->addWidget(overheadValue_);
    overheadDetail_ = muted(QString(), overheadBody, 10);
    overheadLayout->addWidget(overheadDetail_);
    overheadCard->addContent(overheadBody);
    layout->addWidget(overheadCard);
}

// -------------------------------------------------------------- Trabajos

void ReportsPage::addJobsKpis(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();
    auto* first = kpiRow();
    qPrices_ = new KpiCard(QStringLiteral("¿SUBIR PRECIOS?"), theme::kAviso, body);
    kpiBreakEven_ = new KpiCard(QStringLiteral("PARA NO PERDER"), theme::kTextMuted, body);
    kpiTicket_ = new KpiCard(QStringLiteral("TICKET PROMEDIO"), theme::kTextMuted, body);
    for (KpiCard* card : {qPrices_, kpiBreakEven_, kpiTicket_}) {
        first->addWidget(card);
    }
    layout->addLayout(first);

    auto* second = kpiRow();
    kpiPending_ = new KpiCard(QStringLiteral("HECHO Y SIN COBRAR"), theme::kAviso, body);
    kpiCollection_ = new KpiCard(QStringLiteral("DÍAS EN COBRAR"), theme::kTextMuted, body);
    second->addWidget(kpiPending_);
    second->addWidget(kpiCollection_);
    second->addStretch(1);
    layout->addLayout(second);
}

// ---------------------------------------------------------------- Sueldo

void ReportsPage::addSalaryKpi(QVBoxLayout* layout) {
    auto* row = kpiRow();
    qSalary_ = new KpiCard(QStringLiteral("¿CUÁNTO ME PUEDO PAGAR?"), theme::kPositive,
                           layout->parentWidget());
    row->addWidget(qSalary_);
    row->addStretch(2);
    layout->addLayout(row);
}

// ------------------------------------------------------------- Rellenar

void ReportsPage::refillSummary() {
    const Snapshot& snapshot = snapshot_;
    const core::Currency currency = snapshot.currency;
    const core::Date from = snapshot.monthStart();
    const core::Date to = snapshot.monthEnd();

    const auto balances =
        core::pocketBalances(snapshot.pockets, snapshot.movements, currency, snapshot.today);
    const core::CashFlow flow = core::cashFlow(snapshot.movements, currency, from, to);
    const core::Funding fund =
        core::funding(snapshot.pockets, snapshot.movements, currency, from, to);

    // --- Caja y reservas --------------------------------------------------
    const core::Money cash = core::totalFor(balances, core::PocketKind::Operacion, currency);
    kpiCash_->setValue(theme::formatMoney(cash));
    kpiCash_->setNote(QStringLiteral("lo que hay para trabajar"),
                      cash.isNegative() ? theme::kNegative : theme::kTextMuted);

    kpiReserves_->setValue(theme::formatMoney(fund.reserveBalance));
    kpiReserves_->setNote(fund.eatingReserves()
                              ? QStringLiteral("bajando: %1 este mes").arg(theme::formatMoney(fund.net))
                              : QStringLiteral("ahorro, inversión y emergencia · sin tocar este mes"),
                          fund.eatingReserves() ? theme::kNegative : theme::kPositive);

    const int runway = fund.monthsOfRunway(monthsBetween(from, to));
    if (runway == -1) {
        kpiRunway_->setValue(QStringLiteral("—"));
        kpiRunway_->setNote(QStringLiteral("no estás consumiendo reservas"), theme::kTextMuted);
    } else {
        kpiRunway_->setValue(QString::number(runway));
        kpiRunway_->setNote(QStringLiteral("a este ritmo"), theme::kTextMuted);
    }

    // --- Con que se pago el mes -------------------------------------------
    fundingBar_->setValues(flow.incomeCash, fund.fromReserves);
    if (fund.eatingReserves()) {
        fundingHeadline_->setText(
            QStringLiteral("%1 de lo que gastaste este mes salió de tus ahorros.")
                .arg(theme::formatMoney(fund.net)));
        theme::setLabelColor(fundingHeadline_, theme::kNegative);
        QString detail =
            QStringLiteral("Eso no es una pérdida ni una ganancia: es capital tuyo tapando "
                           "un hueco. Si vuelve a pasar el mes que viene, el problema no es el mes: "
                           "es el precio de los trabajos.");
        if (runway >= 0) {
            detail += QStringLiteral(" A este ritmo te quedan cerca de %1 meses de reserva.").arg(runway);
        }
        fundingDetail_->setText(detail);
    } else if (!fund.toReserves.isZero()) {
        fundingHeadline_->setText(QStringLiteral("Este mes guardaste %1.")
                                      .arg(theme::formatMoney(fund.toReserves - fund.fromReserves)));
        theme::setLabelColor(fundingHeadline_, theme::kPositive);
        fundingDetail_->setText(
            QStringLiteral("El mes se pagó solo y además sobró. Es el único caso en que el "
                           "ahorro crece de verdad: cuando la plata sale de lo cobrado."));
    } else {
        fundingHeadline_->setText(QStringLiteral("Este mes se pagó con lo que cobraste."));
        theme::setLabelColor(fundingHeadline_, theme::kPositive);
        fundingDetail_->setText(QStringLiteral("Ni entró ni salió plata de las reservas."));
    }

    // --- Avisos -----------------------------------------------------------
    clearLayout(alertsLayout_);
    const auto list = core::alerts(snapshot.pockets, snapshot.movements, snapshot.jobs, currency,
                                   snapshot.today, [](const core::Money& amount) {
                                       return theme::formatMoney(amount).toStdString();
                                   });
    if (list.empty()) {
        alertsLayout_->addWidget(muted(QStringLiteral("Nada que reportar."), this, 10));
    }
    for (const core::Alert& alert : list) {
        auto* block = new QWidget(this);
        auto* blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(10, 0, 0, 0);
        blockLayout->setSpacing(2);
        auto* title = new QLabel(QString::fromStdString(alert.title), block);
        title->setFont(theme::bodyFont(11, QFont::DemiBold));
        title->setWordWrap(true);
        theme::setLabelColor(title, theme::alertColor(alert.level));
        blockLayout->addWidget(title);
        blockLayout->addWidget(muted(QString::fromStdString(alert.detail), block, 9));
        alertsLayout_->addWidget(block);
    }

    // --- ¿Cuanto gasto? Negocio y personal, nunca sumados ------------------
    {
        const auto business = core::spendingByCategory(snapshot.movements, snapshot.pockets,
                                                       core::Account::Negocio, snapshot.today, currency);
        const auto personal = core::spendingByCategory(snapshot.movements, snapshot.pockets,
                                                       core::Account::Personal, snapshot.today, currency);
        qSpend_->setValue(theme::formatMoney(personal.totalCurrent));
        qSpend_->setNote(QStringLiteral("gasto personal (%1) · del negocio %2 (%3)")
                             .arg(theme::changeText(personal.totalCurrent, personal.totalPrevious),
                                  theme::formatMoney(business.totalCurrent),
                                  theme::changeText(business.totalCurrent, business.totalPrevious)),
                         theme::kTextMuted);
    }

    // --- Mes a mes ----------------------------------------------------------
    clearLayout(monthsLayout_);
    const auto months = core::summarizeByMonth(snapshot.pockets, snapshot.movements, currency);
    if (months.empty()) {
        monthsLayout_->addWidget(muted(QStringLiteral("Todavía no hay meses con actividad."), this));
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

        auto* result = new QLabel(QStringLiteral("dejó %1").arg(theme::formatMoney(month.result)), row);
        result->setFont(theme::numericFont(10));
        result->setFixedWidth(160);
        theme::setLabelColor(result, month.result.isNegative() ? theme::kNegative : theme::kPositive);

        auto* fundingLabel = new QLabel(row);
        fundingLabel->setFont(theme::numericFont(10));
        if (month.netFunding.minor() > 0) {
            fundingLabel->setText(QStringLiteral("y sacó %1 de las reservas")
                                      .arg(theme::formatMoney(month.netFunding)));
            theme::setLabelColor(fundingLabel, theme::kNegative);
        } else if (month.netFunding.minor() < 0) {
            fundingLabel->setText(QStringLiteral("y guardó %1").arg(theme::formatMoney(-month.netFunding)));
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

    // --- Caja y resultado -------------------------------------------------
    cashValue_->setText(theme::formatMoney(flow.cashDelta));
    theme::setLabelColor(cashValue_, flow.cashDelta.isNegative() ? theme::kNegative : theme::kPositive);
    resultValue_->setText(theme::formatMoney(flow.result));
    theme::setLabelColor(resultValue_, flow.result.isNegative() ? theme::kNegative : theme::kPositive);
    const core::Money difference = flow.result - flow.cashDelta;
    if (difference.isZero()) {
        twoNumbersDetail_->setText(
            QStringLiteral("Este mes los dos números coinciden: nada quedó por cobrar."));
    } else {
        twoNumbersDetail_->setText(
            QStringLiteral("Se llevan %1 de diferencia, y ninguno de los dos está mal: la caja "
                           "cuenta la plata que se movió, el resultado cuenta el mes que trabajaste.")
                .arg(theme::formatMoney(difference)));
    }

    // --- Costo de estructura ------------------------------------------------
    const core::Money ov = core::overhead(snapshot.movements, currency, from, to);
    overheadValue_->setText(theme::formatMoney(ov));
    if (flow.incomeAccrued.isZero()) {
        overheadDetail_->setText(QStringLiteral("Este mes no hubo ingresos para absorber la estructura."));
    } else {
        const double pct =
            static_cast<double>(ov.minor()) * 100.0 / static_cast<double>(flow.incomeAccrued.minor());
        overheadDetail_->setText(QStringLiteral("Se come el %1 de los ingresos del mes.")
                                     .arg(theme::formatBps(static_cast<int>(pct * 100.0))));
    }

    // --- Trabajos -----------------------------------------------------------
    core::Money pending = core::Money::zero(currency);
    for (const core::PocketBalance& balance : balances) {
        pending += balance.pendingIn;
    }
    kpiPending_->setValue(theme::formatMoney(pending));
    kpiPending_->setNote(QStringLiteral("trabajo entregado, plata no"), theme::kTextMuted);

    const core::BreakEven be = core::breakEven(snapshot.jobs, snapshot.movements, currency, from, to);
    if (be.unknown()) {
        kpiBreakEven_->setValue(QStringLiteral("—"));
        kpiBreakEven_->setNote(QStringLiteral("hacen falta trabajos con margen para saberlo"), theme::kTextMuted);
    } else {
        kpiBreakEven_->setValue(theme::formatMoney(be.revenueNeeded));
        kpiBreakEven_->setNote(QStringLiteral("facturación mínima"), theme::kTextMuted);
    }

    const core::TicketStats ts = core::ticketStats(snapshot.jobs, snapshot.movements, currency, from, to);
    kpiTicket_->setValue(theme::formatMoney(ts.averageIncome));
    kpiTicket_->setNote(QStringLiteral("sobre %1 trabajos").arg(ts.jobCount), theme::kTextMuted);

    const core::CollectionStats cs = core::collectionStats(snapshot.movements, from, to);
    if (cs.sampled == 0) {
        kpiCollection_->setValue(QStringLiteral("—"));
        kpiCollection_->setNote(QStringLiteral("todavía no hay cobros con fecha"), theme::kTextMuted);
    } else {
        kpiCollection_->setValue(QString::number(cs.averageDays));
        kpiCollection_->setNote(QStringLiteral("sobre %1 cobros").arg(cs.sampled), theme::kTextMuted);
    }

    // ¿Subir precios? El peor tipo de los ultimos seis meses, si hay alguno
    // bajo el objetivo.
    {
        const auto stats = core::statsByType(snapshot.repairs, snapshot.parts, snapshot.movements,
                                             snapshot.costs, currency,
                                             snapshot.today.firstDayOfMonth().addMonths(-5), snapshot.today);
        const core::TypeStats* worst = nullptr;
        for (const auto& s : stats) {
            const bool judged = s.verdict == core::Verdict::Bajo || s.verdict == core::Verdict::Cerca;
            if (judged && (worst == nullptr || *s.marginBps < *worst->marginBps)) worst = &s;
        }
        if (stats.empty()) {
            qPrices_->setValue(QStringLiteral("—"));
            qPrices_->setNote(QStringLiteral("todavía no hay reparaciones entregadas"), theme::kTextMuted);
        } else if (worst == nullptr) {
            qPrices_->setValue(QStringLiteral("No"));
            qPrices_->setNote(QStringLiteral("ningún tipo con datos queda bajo el objetivo"), theme::kPositive);
        } else {
            static const char* const kNames[] = {"GPU", "Laptop", "Placa madre", "Otro"};
            qPrices_->setValue(QStringLiteral("%1 a %2")
                                   .arg(QString::fromLatin1(kNames[static_cast<int>(worst->type)]),
                                        theme::formatMoney(*worst->suggestedPrice)));
            qPrices_->setNote(QStringLiteral("margen %1, objetivo %2 · hoy cobras %3")
                                  .arg(theme::formatBps(*worst->marginBps),
                                       theme::formatBps(snapshot.costs.targetMarginBps),
                                       theme::formatMoney(worst->averagePrice)),
                              worst->verdict == core::Verdict::Bajo ? theme::kNegative : theme::kAviso);
        }
    }

    // --- ¿Cuanto me puedo pagar? ---------------------------------------------
    {
        const core::SalaryAdvice& a = snapshot.salary;
        if (a.closedMonths == 0) {
            qSalary_->setValue(QStringLiteral("—"));
            qSalary_->setNote(QStringLiteral("hace falta al menos un mes cerrado"), theme::kTextMuted);
        } else {
            qSalary_->setValue(theme::formatMoney(a.salary));
            if (!a.shortfall.isZero()) {
                qSalary_->setNote(QStringLiteral("el negocio pierde %1 por mes en promedio")
                                      .arg(theme::formatMoney(a.shortfall)),
                                  theme::kNegative);
            } else if (a.margin()) {
                const bool enough = !a.margin()->isNegative();
                qSalary_->setNote(QStringLiteral("necesitas %1 · %2 %3%4")
                                      .arg(theme::formatMoney(*a.personalSpend),
                                           enough ? QStringLiteral("sobran") : QStringLiteral("faltan"),
                                           theme::formatMoney(enough ? *a.margin() : -*a.margin()),
                                           a.provisional ? QStringLiteral(" · provisional") : QString()),
                                  enough ? theme::kPositive : theme::kNegative);
            }
        }
    }
}

} // namespace dake::ui
