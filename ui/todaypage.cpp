// ui/todaypage.cpp — la pantalla que contesta la pregunta.
//
// El tablero actual muestra ingreso, gasto y "utilidad". Con los datos reales
// de agosto eso da −20,02 y tres secciones en rojo, y no dice nada sobre lo
// unico que de verdad paso ese mes: que la plata del material salio del
// ahorro. Esta pantalla pone eso primero y todo lo demas despues.

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include "cards.hpp"
#include "dake/core/accounts.hpp"
#include "dake/core/report.hpp"
#include "dake/core/repairs.hpp"
#include "pages.hpp"
#include "capturewidget.hpp"
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

// -------------------------------------------------------------- TodayPage

TodayPage::TodayPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void TodayPage::buildUi() {
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
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(16);

    heading_ = new QLabel(QStringLiteral("Hoy"), page);
    heading_->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading_, theme::kText);
    layout->addWidget(heading_);

    subheading_ = muted(QString(), page, 10);
    layout->addWidget(subheading_);

    // --- Carga rapida, arriba de todo -------------------------------------
    // Va primero y no al final: es lo que se viene a hacer. Un formulario al
    // que hay que bajar con la rueda es un formulario que se usa menos.
    entryCard_ = new Card(QStringLiteral("ANOTAR"), page);
    capture_ = new CaptureWidget(entryCard_);
    entryCard_->addContent(capture_);
    layout->addWidget(entryCard_);

    // --- Las tres preguntas ----------------------------------------------------
    auto* questions = new QHBoxLayout();
    questions->setSpacing(12);
    qPrices_ = new KpiCard(QStringLiteral("¿SUBIR PRECIOS?"), theme::kAviso, page);
    qSpend_ = new KpiCard(QStringLiteral("¿CUÁNTO GASTO? · ESTE MES"), theme::kNegative, page);
    qSalary_ = new KpiCard(QStringLiteral("¿CUÁNTO ME PUEDO PAGAR?"), theme::kPositive, page);
    questions->addWidget(qPrices_);
    questions->addWidget(qSpend_);
    questions->addWidget(qSalary_);
    layout->addLayout(questions);

    // --- Cuatro cifras ----------------------------------------------------
    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(12);
    kpiCash_ = new KpiCard(QStringLiteral("CAJA DEL NEGOCIO"), theme::kOperacion, page);
    kpiReserves_ = new KpiCard(QStringLiteral("AHORRO E INVERSION"), theme::kAhorro, page);
    kpiPending_ = new KpiCard(QStringLiteral("HECHO Y SIN COBRAR"), theme::kAviso, page);
    kpiPrepaid_ = new KpiCard(QStringLiteral("MATERIAL POR DELANTE"), theme::kTextMuted, page);
    for (KpiCard* card : {kpiCash_, kpiReserves_, kpiPending_, kpiPrepaid_}) {
        kpiRow->addWidget(card);
    }
    layout->addLayout(kpiRow);

    // --- Segunda fila de indicadores --------------------------------------
    auto* kpiRow2 = new QHBoxLayout();
    kpiRow2->setSpacing(12);
    kpiBreakEven_ = new KpiCard(QStringLiteral("PARA NO PERDER"), theme::kTextMuted, page);
    kpiRunway_ = new KpiCard(QStringLiteral("MESES DE RESERVA"), theme::kTextMuted, page);
    kpiTicket_ = new KpiCard(QStringLiteral("TICKET PROMEDIO"), theme::kTextMuted, page);
    kpiCollection_ = new KpiCard(QStringLiteral("DIAS EN COBRAR"), theme::kTextMuted, page);
    for (KpiCard* card : {kpiBreakEven_, kpiRunway_, kpiTicket_, kpiCollection_}) {
        kpiRow2->addWidget(card);
    }
    layout->addLayout(kpiRow2);

    // --- La pregunta ------------------------------------------------------
    auto* fundingCard = new Card(QStringLiteral("CON QUE SE PAGO ESTE MES"), page);
    fundingCard->setSubtitle(
        QStringLiteral("La aplicacion actual no puede contestar esto: sin traspasos, sacar "
                       "del ahorro se anota como un gasto mas."));

    fundingHeadline_ = new QLabel(fundingCard);
    fundingHeadline_->setFont(theme::figureFont(19));
    fundingHeadline_->setWordWrap(true);
    fundingCard->addContent(fundingHeadline_);

    fundingBar_ = new FundingBar(fundingCard);
    fundingCard->addContent(fundingBar_);

    fundingDetail_ = muted(QString(), fundingCard, 10);
    fundingCard->addContent(fundingDetail_);
    layout->addWidget(fundingCard);

    // --- Los dos numeros --------------------------------------------------
    auto* twoCard = new Card(QStringLiteral("CAJA Y RESULTADO NO SON EL MISMO NUMERO"), page);
    auto* twoBody = new QWidget(twoCard);
    auto* twoGrid = new QGridLayout(twoBody);
    twoGrid->setContentsMargins(0, 6, 0, 0);
    twoGrid->setHorizontalSpacing(28);

    auto* cashCaption = muted(QStringLiteral("LO QUE BAJO O SUBIO LA CAJA"), twoBody);
    auto* resultCaption = muted(QStringLiteral("LO QUE DEJO EL MES"), twoBody);

    cashValue_ = new QLabel(twoBody);
    cashValue_->setFont(theme::figureFont(24));
    resultValue_ = new QLabel(twoBody);
    resultValue_->setFont(theme::figureFont(24));

    twoGrid->addWidget(cashCaption, 0, 0);
    twoGrid->addWidget(resultCaption, 0, 1);
    twoGrid->addWidget(cashValue_, 1, 0);
    twoGrid->addWidget(resultValue_, 1, 1);
    // Ancho minimo explicito: sin el, los dos rotulos se parten en dos lineas y
    // las cifras quedan pegadas una a la otra, que es justo lo contrario de lo
    // que esta tarjeta quiere mostrar.
    twoGrid->setColumnMinimumWidth(0, 240);
    twoGrid->setColumnMinimumWidth(1, 240);
    twoGrid->setColumnStretch(2, 1);
    twoCard->addContent(twoBody);

    twoNumbersDetail_ = muted(QString(), twoCard, 10);
    twoCard->addContent(twoNumbersDetail_);
    layout->addWidget(twoCard);

    // --- Costo de estructura ----------------------------------------------
    auto* overheadCard = new Card(QStringLiteral("COSTO DE ESTRUCTURA"), page);
    
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

    // --- Avisos -----------------------------------------------------------
    auto* alertsCard = new Card(QStringLiteral("LO QUE HAY QUE MIRAR"), page);
    alertsCard->setSubtitle(
        QStringLiteral("Un libro de cuentas solo devuelve lo que se le mete. Esto es lo que "
                       "la aplicacion dice sin que se lo pregunten."));
    auto* alertsBody = new QWidget(alertsCard);
    alertsLayout_ = new QVBoxLayout(alertsBody);
    alertsLayout_->setContentsMargins(0, 4, 0, 0);
    alertsLayout_->setSpacing(12);
    alertsCard->addContent(alertsBody);
    layout->addWidget(alertsCard);

    layout->addStretch(1);
}

void TodayPage::setSnapshot(const Snapshot& snapshot) {
    const core::Currency currency = snapshot.currency;
    const core::Date from = snapshot.monthStart();
    const core::Date to = snapshot.monthEnd();

    const auto balances =
        core::pocketBalances(snapshot.pockets, snapshot.movements, currency, snapshot.today);
    const core::CashFlow flow = core::cashFlow(snapshot.movements, currency, from, to);
    const core::Funding fund =
        core::funding(snapshot.pockets, snapshot.movements, currency, from, to);

    heading_->setText(QStringLiteral("Hoy"));
    subheading_->setText(
        QStringLiteral("%1 · todo lo de abajo mira el mes en curso")
            .arg(QString::fromStdString(snapshot.today.toIso())));

    // --- Cuatro cifras ----------------------------------------------------
    const core::Money cash = core::totalFor(balances, core::PocketKind::Operacion, currency);
    kpiCash_->setValue(theme::formatMoney(cash));
    kpiCash_->setNote(QStringLiteral("lo que hay para trabajar"),
                      cash.isNegative() ? theme::kNegative : theme::kTextMuted);

    kpiReserves_->setValue(theme::formatMoney(fund.reserveBalance));
    kpiReserves_->setNote(fund.eatingReserves()
                              ? QStringLiteral("bajando: %1 este mes")
                                    .arg(theme::formatMoney(fund.net))
                              : QStringLiteral("sin tocar este mes"),
                          fund.eatingReserves() ? theme::kNegative : theme::kPositive);

    core::Money pending = core::Money::zero(currency);
    for (const core::PocketBalance& balance : balances) {
        pending += balance.pendingIn;
    }
    kpiPending_->setValue(theme::formatMoney(pending));
    kpiPending_->setNote(QStringLiteral("trabajo entregado, plata no"), theme::kTextMuted);

    const core::Money prepaid =
        core::unusedPrepaid(snapshot.movements, currency, snapshot.today);
    kpiPrepaid_->setValue(theme::formatMoney(prepaid));
    kpiPrepaid_->setNote(QStringLiteral("material ya pagado, sin consumir"), theme::kTextMuted);

    // --- Segunda fila de indicadores --------------------------------------
    const core::BreakEven be = core::breakEven(snapshot.jobs, snapshot.movements, currency, from, to);
    if (be.unknown()) {
        kpiBreakEven_->setValue(QStringLiteral("—"));
        kpiBreakEven_->setNote(QStringLiteral("hacen falta trabajos con margen para saberlo"), theme::kTextMuted);
    } else {
        kpiBreakEven_->setValue(theme::formatMoney(be.revenueNeeded));
        kpiBreakEven_->setNote(QStringLiteral("facturacion minima"), theme::kTextMuted);
    }

    const int runway = fund.monthsOfRunway(monthsBetween(from, to));
    if (runway == -1) {
        kpiRunway_->setValue(QStringLiteral("—"));
        kpiRunway_->setNote(QStringLiteral("no estas consumiendo reservas"), theme::kTextMuted);
    } else {
        kpiRunway_->setValue(QString::number(runway));
        kpiRunway_->setNote(QStringLiteral("a este ritmo"), theme::kTextMuted);
    }

    const core::TicketStats ts = core::ticketStats(snapshot.jobs, snapshot.movements, currency, from, to);
    kpiTicket_->setValue(theme::formatMoney(ts.averageIncome));
    kpiTicket_->setNote(QStringLiteral("sobre %1 trabajos").arg(ts.jobCount), theme::kTextMuted);

    const core::CollectionStats cs = core::collectionStats(snapshot.movements, from, to);
    if (cs.sampled == 0) {
        kpiCollection_->setValue(QStringLiteral("—"));
        kpiCollection_->setNote(QStringLiteral("todavia no hay cobros con fecha"), theme::kTextMuted);
    } else {
        kpiCollection_->setValue(QString::number(cs.averageDays));
        kpiCollection_->setNote(QStringLiteral("sobre %1 cobros").arg(cs.sampled), theme::kTextMuted);
    }

    // --- La pregunta ------------------------------------------------------
    fundingBar_->setValues(flow.incomeCash, fund.fromReserves);

    if (fund.eatingReserves()) {
        fundingHeadline_->setText(
            QStringLiteral("%1 de lo que gastaste este mes salio de tus ahorros.")
                .arg(theme::formatMoney(fund.net)));
        theme::setLabelColor(fundingHeadline_, theme::kNegative);

        // El de la linea 324, sin recalcular: es el mismo numero, y tenerlo
        // dos veces invita a que un dia uno cambie y el otro no.
        QString detail =
            QStringLiteral("Eso no es una perdida ni una ganancia: es capital tuyo tapando "
                           "un hueco, y por eso no aparece en ningun estado de resultados. "
                           "Si vuelve a pasar el mes que viene, el problema no es el mes: "
                           "es el precio de los trabajos.");
        if (runway >= 0) {
            detail += QStringLiteral(" A este ritmo te quedan cerca de %1 meses de reserva.")
                          .arg(runway);
        }
        fundingDetail_->setText(detail);
    } else if (!fund.toReserves.isZero()) {
        fundingHeadline_->setText(QStringLiteral("Este mes guardaste %1.")
                                      .arg(theme::formatMoney(fund.toReserves - fund.fromReserves)));
        theme::setLabelColor(fundingHeadline_, theme::kPositive);
        fundingDetail_->setText(
            QStringLiteral("El mes se pago solo y ademas sobro. Es el unico caso en que el "
                           "ahorro crece de verdad: cuando la plata sale de lo cobrado."));
    } else {
        fundingHeadline_->setText(QStringLiteral("Este mes se pago con lo que cobraste."));
        theme::setLabelColor(fundingHeadline_, theme::kPositive);
        fundingDetail_->setText(QStringLiteral("Ni entro ni salio plata de las reservas."));
    }

    // --- Los dos numeros --------------------------------------------------
    cashValue_->setText(theme::formatMoney(flow.cashDelta));
    theme::setLabelColor(cashValue_,
                         flow.cashDelta.isNegative() ? theme::kNegative : theme::kPositive);
    resultValue_->setText(theme::formatMoney(flow.result));
    theme::setLabelColor(resultValue_,
                         flow.result.isNegative() ? theme::kNegative : theme::kPositive);

    const core::Money difference = flow.result - flow.cashDelta;
    if (difference.isZero()) {
        twoNumbersDetail_->setText(
            QStringLiteral("Este mes los dos numeros coinciden: nada quedo por cobrar y nada "
                           "de lo comprado dura mas de un mes."));
    } else {
        twoNumbersDetail_->setText(
            QStringLiteral("Se llevan %1 de diferencia, y ninguno de los dos esta mal: la "
                           "caja cuenta la plata que se movio, el resultado cuenta el mes "
                           "que trabajaste. La aplicacion actual calcula solo el primero y "
                           "lo llama utilidad, y con esa utilidad reparte ahorro e "
                           "inversion.")
                .arg(theme::formatMoney(difference)));
    }

    // --- Costo de estructura ----------------------------------------------
    const core::Money ov = core::overhead(snapshot.movements, currency, from, to);
    overheadValue_->setText(theme::formatMoney(ov));
    if (flow.incomeAccrued.isZero()) {
        overheadDetail_->setText(QStringLiteral("Este mes no hubo ingresos para absorber la estructura."));
    } else {
        const double pct = static_cast<double>(ov.minor()) * 100.0 / static_cast<double>(flow.incomeAccrued.minor());
        // Con formatBps y no con arg(double): esa funcion ya cambia el punto por
        // coma, que es el separador decimal del resto de la aplicacion. Un
        // "13.3%" al lado de un "27,91" delata que dos partes del programa no
        // se hablan.
        overheadDetail_->setText(QStringLiteral("Se come el %1 de los ingresos del mes.")
                                     .arg(theme::formatBps(static_cast<int>(pct * 100.0))));
    }

    // --- Avisos -----------------------------------------------------------
    while (QLayoutItem* item = alertsLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

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

        auto* detail = muted(QString::fromStdString(alert.detail), block, 9);

        blockLayout->addWidget(title);
        blockLayout->addWidget(detail);
        alertsLayout_->addWidget(block);
    }

    capture_->setSnapshot(snapshot);

    // ¿Subir precios? El peor tipo de los ultimos seis meses, si hay alguno
    // bajo el objetivo.
    {
        const auto stats = core::statsByType(snapshot.repairs, snapshot.parts, snapshot.movements,
                                             snapshot.costs, snapshot.currency,
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
    // ¿Cuanto gasto? Negocio y personal, nunca sumados.
    {
        const auto business = core::spendingByCategory(snapshot.movements, snapshot.pockets,
                                                       core::Account::Negocio, snapshot.today, snapshot.currency);
        const auto personal = core::spendingByCategory(snapshot.movements, snapshot.pockets,
                                                       core::Account::Personal, snapshot.today, snapshot.currency);
        qSpend_->setValue(theme::formatMoney(personal.totalCurrent));
        qSpend_->setNote(QStringLiteral("gasto personal (%1) · del negocio %2 (%3)")
                             .arg(theme::changeText(personal.totalCurrent, personal.totalPrevious),
                                  theme::formatMoney(business.totalCurrent),
                                  theme::changeText(business.totalCurrent, business.totalPrevious)),
                         theme::kTextMuted);
    }
    // ¿Cuanto me puedo pagar?
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
    const QString anywhere =
        snapshot.hotkeyRegistered
            ? QStringLiteral(" Desde cualquier programa: %1.")
                  .arg(QKeySequence(snapshot.hotkey, QKeySequence::PortableText)
                           .toString(QKeySequence::NativeText))
            : QString();
    entryCard_->setSubtitle(
        QStringLiteral("Monto y descripción, y Enter. La categoría, el bolsillo y la reparación "
                       "se deducen; lo que no, queda para la revisión del domingo.") +
        anywhere);
}

} // namespace dake::ui
