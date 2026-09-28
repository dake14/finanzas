// ui/reportspage.cpp — Informes: una pestana por pregunta.
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
#include "dake/core/report.hpp"
#include "fields.hpp"
#include "pages.hpp"
#include "tables.hpp"
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

    auto* heading = new QLabel(QStringLiteral("Informes"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    layout->addWidget(heading);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->setFont(theme::bodyFont(10, QFont::DemiBold));
    // Una pestana por pregunta: cuanto tengo, como fue el mes, en que se va la
    // plata, cuanto dejan los trabajos, cuanto me puedo pagar.
    const auto tab = [this](const QString& name, const QString& id) {
        auto* page = new QWidget(this);
        page->setObjectName(QStringLiteral("Informe:") + id);
        QVBoxLayout* body = scrollingTab(page);
        tabs_->addTab(page, name);
        return body;
    };

    QVBoxLayout* summary = tab(QStringLiteral("Resumen"), QStringLiteral("Resumen"));
    addSummarySection(summary);
    summary->addStretch(1);

    QVBoxLayout* month = tab(QStringLiteral("El mes"), QStringLiteral("ElMes"));
    addSpendKpi(month);
    addMonthCharts(month);
    addCashSection(month);
    addMonthsSection(month);
    addTwoNumbersSection(month);
    month->addStretch(1);

    QVBoxLayout* spending = tab(QStringLiteral("Gastos"), QStringLiteral("Gastos"));
    addCategoriesChart(spending);
    addSpendingSection(spending);
    addOverheadSection(spending);
    spending->addStretch(1);

    QVBoxLayout* jobs = tab(QStringLiteral("Trabajos"), QStringLiteral("Trabajos"));
    addJobsKpis(jobs);
    addJobsChart(jobs);
    addRepairsSection(jobs);
    addTypesSection(jobs);
    jobs->addStretch(1);

    QVBoxLayout* salary = tab(QStringLiteral("Sueldo"), QStringLiteral("Sueldo"));
    addSalaryKpi(salary);
    addSalarySection(salary);
    salary->addStretch(1);

    layout->addWidget(tabs_, 1);
}

// ------------------------------------------------------------- Graficas

void ReportsPage::addMonthCharts(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();

    auto* resultCard = new Card(QStringLiteral("RESULTADO POR MES"), body);
    chartResult_ = new BarChart(resultCard);
    chartResult_->setSigned(true);
    resultCard->addContent(chartResult_);
    layout->addWidget(resultCard);

    auto* incomeCostCard = new Card(QStringLiteral("INGRESOS CONTRA COSTOS"), body);
    chartIncomeCost_ = new BarChart(incomeCostCard);
    chartIncomeCost_->setSeries(QStringLiteral("Ingresos"), QStringLiteral("Costos"));
    incomeCostCard->addContent(chartIncomeCost_);
    layout->addWidget(incomeCostCard);

    auto* cashCard = new Card(QStringLiteral("CAJA ACUMULADA"), body);
    chartCash_ = new LineChart(cashCard);
    cashCard->addContent(chartCash_);
    layout->addWidget(cashCard);
}

void ReportsPage::addCategoriesChart(QVBoxLayout* layout) {
    auto* categoriesCard = new Card(QStringLiteral("GASTOS POR CATEGORÍA, ESTE MES"), layout->parentWidget());
    chartCategories_ = new RankChart(categoriesCard);
    categoriesCard->addContent(chartCategories_);
    layout->addWidget(categoriesCard);
}

void ReportsPage::addJobsChart(QVBoxLayout* layout) {
    auto* jobsCard = new Card(QStringLiteral("MARGEN POR TRABAJO"), layout->parentWidget());
    chartJobs_ = new RankChart(jobsCard);
    jobsCard->addContent(chartJobs_);
    layout->addWidget(jobsCard);
}

void ReportsPage::addSpendingSection(QVBoxLayout* layout) {
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
}

void ReportsPage::showTab(int index) {
    tabs_->setCurrentIndex(index);
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
    refillSummary();
    refillCharts();
    refillSpending();
    refillRepairs();
    refillTypes();
    refillCash();
    refillSalary();
}

void ReportsPage::refillCharts() {
    const core::Currency currency = snapshot_.currency;
    const core::Date from = snapshot_.monthStart();
    const core::Date to = snapshot_.monthEnd();
    const std::vector<core::MonthSummary> months = core::summarizeByMonth(snapshot_.pockets, snapshot_.movements, currency);
    std::vector<ChartPoint> resultPoints;
    std::vector<ChartPoint> incomeCostPoints;
    std::vector<ChartPoint> cashPoints;
    core::Money accumulatedCash = core::Money::zero(currency);

    for (const core::MonthSummary& ms : months) {
        ChartPoint ptResult;
        ptResult.label = QString::fromStdString(ms.label());
        ptResult.primary = ms.result.minor() / 100.0;
        ptResult.primaryText = theme::formatMoney(ms.result);
        resultPoints.push_back(ptResult);

        ChartPoint ptIC;
        ptIC.label = QString::fromStdString(ms.label());
        ptIC.primary = ms.incomeAccrued.minor() / 100.0;
        ptIC.secondary = ms.cost.minor() / 100.0;
        ptIC.primaryText = theme::formatMoney(ms.incomeAccrued);
        incomeCostPoints.push_back(ptIC);

        accumulatedCash += (ms.incomeAccrued - ms.cost);
        ChartPoint ptCash;
        ptCash.label = QString::fromStdString(ms.label());
        ptCash.primary = accumulatedCash.minor() / 100.0;
        ptCash.primaryText = theme::formatMoney(accumulatedCash);
        cashPoints.push_back(ptCash);
    }
    chartResult_->setData(resultPoints);
    chartIncomeCost_->setData(incomeCostPoints);
    chartCash_->setData(cashPoints);

    const std::vector<core::CategoryTotal> cats = core::costByCategory(snapshot_.movements, currency, from, to);
    std::vector<ChartPoint> catPoints;
    for (const core::CategoryTotal& ct : cats) {
        ChartPoint pt;
        pt.label = QString::fromStdString(ct.category);
        pt.primary = ct.total.minor() / 100.0;
        pt.primaryText = theme::formatMoney(ct.total);
        catPoints.push_back(pt);
    }
    chartCategories_->setData(catPoints);

    const std::vector<core::JobResult> jrs = core::jobResults(snapshot_.jobs, snapshot_.movements, currency);
    std::vector<ChartPoint> jobPoints;
    for (const core::JobResult& jr : jrs) {
        ChartPoint pt;
        pt.label = QString::fromStdString(jr.name);
        pt.primary = jr.margin.minor() / 100.0;
        pt.primaryText = QStringLiteral("%1 (%2)").arg(theme::formatMoney(jr.margin), theme::formatBps(jr.marginBps));
        jobPoints.push_back(pt);
    }
    chartJobs_->setData(jobPoints);
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

// ------------------------------------------------ Rentabilidad por reparacion

namespace {

[[nodiscard]] QString typeName(core::RepairType type) {
    switch (type) {
        case core::RepairType::GPU: return QStringLiteral("GPU");
        case core::RepairType::Laptop: return QStringLiteral("Laptop");
        case core::RepairType::PlacaMadre: return QStringLiteral("Placa madre");
        case core::RepairType::Otro: return QStringLiteral("Otro");
    }
    return {};
}

[[nodiscard]] QString ratioText(int permille) {
    return QString::number(static_cast<double>(permille) / 1000.0, 'f', 1)
               .replace(QLatin1Char('.'), QLatin1Char(',')) +
           QStringLiteral("×");
}

/// Las entregas de los ultimos seis meses: la ventana de los reportes de
/// rentabilidad. Mas corta y un mes malo pesa demasiado; mas larga y los
/// precios de hace un año tapan los de ahora.
[[nodiscard]] std::pair<core::Date, core::Date> lastSixMonths(core::Date today) {
    return {today.firstDayOfMonth().addMonths(-5), today};
}

} // namespace

void ReportsPage::addRepairsSection(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();
    repairsHeadline_ = headline(body);
    layout->addWidget(repairsHeadline_);
    repairsTable_ = makeTable({QStringLiteral("Orden"), QStringLiteral("Equipo"), QStringLiteral("Tipo"),
                               QStringLiteral("Entrega"), QStringLiteral("Precio"),
                               QStringLiteral("Directo"), QStringLiteral("Horas"),
                               QStringLiteral("Fijos"), QStringLiteral("Ganancia"),
                               QStringLiteral("Margen"), QStringLiteral("Por hora"),
                               QStringLiteral("Sugerido")},
                              1);
    repairsTable_->setMinimumHeight(420);
    layout->addWidget(repairsTable_);
    auto* legend = new QLabel(
        QStringLiteral("Horas es tu tiempo a la tarifa objetivo; fijos, la parte del taller por "
                       "hora trabajada. Un margen de 0% es cobrar justo tu tarifa. En rojo, las que "
                       "quedaron bajo el objetivo, con el precio que las habría llevado ahí."),
        body);
    legend->setWordWrap(true);
    legend->setFont(theme::bodyFont(9));
    theme::setLabelColor(legend, theme::kTextFaint);
    layout->addWidget(legend);
}

void ReportsPage::refillRepairs() {
    const auto [from, to] = lastSixMonths(snapshot_.today);
    std::vector<const core::Repair*> delivered;
    for (const core::Repair& repair : snapshot_.repairs) {
        if (repair.status == core::RepairStatus::EnProceso) continue;
        const core::Date date = repair.delivered.value_or(repair.received.value_or(core::Date{}));
        if (date < from || to < date) continue;
        delivered.push_back(&repair);
    }
    std::stable_sort(delivered.begin(), delivered.end(), [](const core::Repair* a, const core::Repair* b) {
        return a->delivered.value_or(core::Date{}) > b->delivered.value_or(core::Date{});
    });

    int below = 0;
    repairsTable_->setRowCount(static_cast<int>(delivered.size()));
    for (int row = 0; row < static_cast<int>(delivered.size()); ++row) {
        const core::Repair& repair = *delivered[static_cast<std::size_t>(row)];
        const auto c = core::costRepair(repair, snapshot_.parts, snapshot_.movements, snapshot_.costs,
                                        snapshot_.currency);
        const bool low = c.belowTarget(snapshot_.costs.targetMarginBps);
        below += low ? 1 : 0;
        setText(repairsTable_, row, 0, QString::fromStdString(repair.orderNo), theme::kTextMuted);
        setText(repairsTable_, row, 1, QString::fromStdString(repair.device));
        setText(repairsTable_, row, 2, typeName(repair.type), theme::kTextMuted);
        setText(repairsTable_, row, 3,
                repair.delivered ? QString::fromStdString(repair.delivered->toIso()) : QStringLiteral("—"),
                theme::kTextMuted);
        setNumber(repairsTable_, row, 4, theme::formatMoney(c.price));
        setNumber(repairsTable_, row, 5, theme::formatMoney(c.direct), theme::kTextMuted);
        setNumber(repairsTable_, row, 6,
                  theme::formatMoney(c.labor) + (c.hoursEstimated ? QStringLiteral(" *") : QString()),
                  theme::kTextMuted);
        setNumber(repairsTable_, row, 7, theme::formatMoney(c.fixedShare), theme::kTextMuted);
        setNumber(repairsTable_, row, 8, theme::formatMoney(c.profit),
                  c.profit.isNegative() ? theme::kNegative : theme::kText);
        setNumber(repairsTable_, row, 9, c.marginBps ? theme::formatBps(*c.marginBps) : QStringLiteral("—"),
                  low ? theme::kNegative : theme::kPositive);
        setNumber(repairsTable_, row, 10,
                  c.profitPerHour ? theme::formatMoney(*c.profitPerHour) : QStringLiteral("—"));
        setNumber(repairsTable_, row, 11,
                  low && c.suggestedPrice ? theme::formatMoney(*c.suggestedPrice) : QString(),
                  theme::kAviso);
    }

    if (delivered.empty()) {
        repairsHeadline_->setText(QStringLiteral("Todavía no hay reparaciones entregadas en los "
                                                 "últimos seis meses."));
    } else if (below == 0) {
        repairsHeadline_->setText(QStringLiteral("Las %1 reparaciones de los últimos seis meses "
                                                 "llegaron al margen objetivo.")
                                      .arg(delivered.size()));
    } else {
        repairsHeadline_->setText(QStringLiteral("%1 de %2 reparaciones quedaron bajo el margen "
                                                 "objetivo de %3.")
                                      .arg(below)
                                      .arg(delivered.size())
                                      .arg(theme::formatBps(snapshot_.costs.targetMarginBps)));
    }
}

// ---------------------------------------------------- Rentabilidad por tipo

void ReportsPage::addTypesSection(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();
    auto* card = new Card(QStringLiteral("¿TENGO QUE SUBIR PRECIOS?"), body);
    typesHeadline_ = headline(card);
    card->addContent(typesHeadline_);
    typesChart_ = new TypeChart(card);
    card->addContent(typesChart_);
    typesNote_ = new QLabel(card);
    typesNote_->setWordWrap(true);
    typesNote_->setFont(theme::bodyFont(9));
    theme::setLabelColor(typesNote_, theme::kTextFaint);
    card->addContent(typesNote_);
    layout->addWidget(card);
}

void ReportsPage::refillTypes() {
    const auto [from, to] = lastSixMonths(snapshot_.today);
    const auto stats = core::statsByType(snapshot_.repairs, snapshot_.parts, snapshot_.movements,
                                         snapshot_.costs, snapshot_.currency, from, to);
    const core::Money needed = core::hourlyNeeded(snapshot_.costs, snapshot_.currency);

    std::vector<TypeRow> rows;
    const core::TypeStats* worst = nullptr;
    for (const core::TypeStats& s : stats) {
        TypeRow row;
        row.label = typeName(s.type);
        row.hasMargin = s.marginBps.has_value();
        row.marginPercent = s.marginBps ? static_cast<double>(*s.marginBps) / 100.0 : 0.0;

        QStringList detail;
        detail << (s.marginBps ? theme::formatBps(*s.marginBps) : QStringLiteral("—"));
        if (s.profitPerHour) {
            detail << QStringLiteral("deja %1/h").arg(theme::formatMoney(*s.profitPerHour));
        }
        if (s.hoursRatioPermille) {
            detail << QStringLiteral("horas %1").arg(ratioText(*s.hoursRatioPermille));
        }
        row.detail = detail.join(QStringLiteral("  ·  "));

        switch (s.verdict) {
            case core::Verdict::Bien:
                row.color = theme::kPositive;
                row.action = QStringLiteral("✓ bien");
                break;
            case core::Verdict::Cerca:
                row.color = theme::kAviso;
                row.action = QStringLiteral("▲ sube a %1 (hoy %2)")
                                 .arg(theme::formatMoney(*s.suggestedPrice),
                                      theme::formatMoney(s.averagePrice));
                break;
            case core::Verdict::Bajo:
                row.color = theme::kNegative;
                row.action = QStringLiteral("✕ sube a %1 (hoy %2)")
                                 .arg(theme::formatMoney(*s.suggestedPrice),
                                      theme::formatMoney(s.averagePrice));
                break;
            case core::Verdict::PocosDatos:
                row.color = theme::kTextMuted;
                row.action = QStringLiteral("pocos datos (%1)").arg(s.count);
                break;
        }
        rows.push_back(row);

        const bool judged = s.verdict == core::Verdict::Bajo || s.verdict == core::Verdict::Cerca;
        if (judged && (worst == nullptr || *s.marginBps < *worst->marginBps)) {
            worst = &s;
        }
    }
    typesChart_->setData(rows, static_cast<double>(snapshot_.costs.targetMarginBps) / 100.0);

    // La primera linea es la respuesta. Si las horas son el problema (se
    // tarda 1,3 veces lo presupuestado o mas), eso va antes que el precio:
    // puede estar mal la plantilla y no la tarifa.
    if (stats.empty()) {
        typesHeadline_->setText(QStringLiteral("Todavía no hay reparaciones entregadas en los últimos "
                                               "seis meses."));
    } else if (worst == nullptr) {
        typesHeadline_->setText(QStringLiteral("Ningún tipo con datos suficientes queda bajo el objetivo. "
                                               "Por ahora, no hace falta subir precios."));
    } else if (worst->hoursRatioPermille && *worst->hoursRatioPermille >= 1300) {
        typesHeadline_->setText(
            QStringLiteral("%1 deja %2 por hora y necesitas %3. Antes que el precio, mira las "
                           "horas: tardas %4 lo que presupuestas.")
                .arg(typeName(worst->type),
                     worst->profitPerHour ? theme::formatMoney(*worst->profitPerHour) : QStringLiteral("—"),
                     theme::formatMoney(needed), ratioText(*worst->hoursRatioPermille)));
    } else {
        typesHeadline_->setText(
            QStringLiteral("%1 deja %2 por hora y necesitas %3. Sube el precio a %4: hoy cobras %5 "
                           "en promedio.")
                .arg(typeName(worst->type),
                     worst->profitPerHour ? theme::formatMoney(*worst->profitPerHour) : QStringLiteral("—"),
                     theme::formatMoney(needed), theme::formatMoney(*worst->suggestedPrice),
                     theme::formatMoney(worst->averagePrice)));
    }

    QString note = QStringLiteral("Últimos seis meses, solo reparaciones entregadas. Cada hora tiene "
                                  "que dejar %1: tu tarifa %2 más %3 de fijos del taller. Verde: en "
                                  "o sobre el objetivo. Ámbar: hasta 5 puntos debajo. Rojo: más abajo.")
                       .arg(theme::formatMoney(needed),
                            theme::formatMoney(core::Money::fromMinor(snapshot_.costs.hourlyRateMinor,
                                                                      snapshot_.currency)),
                            theme::formatMoney(core::Money::fromMinor(snapshot_.costs.fixedPerHourMinor,
                                                                      snapshot_.currency)));
    if (snapshot_.costs.hourlyRateMinor == 0) {
        note = QStringLiteral("Falta la tarifa por hora (Ajustes): sin ella tus horas no cuestan nada "
                              "y todos los márgenes salen inflados. ") +
               note;
    }
    typesNote_->setText(note);
}

// ---------------------------------------------------------- Flujo de caja

void ReportsPage::addCashSection(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();
    cashHeadline_ = headline(body);
    layout->addWidget(cashHeadline_);

    auto* barsCard = new Card(QStringLiteral("ENTRADAS Y SALIDAS DEL NEGOCIO, POR MES"), body);
    barsCard->setSubtitle(QStringLiteral("Lo que entró y salió de verdad de los bolsillos del negocio, en "
                                         "la fecha en que pasó. Las salidas incluyen el sueldo."));
    cashBars_ = new BarChart(barsCard);
    cashBars_->setSeries(QStringLiteral("entradas"), QStringLiteral("salidas"));
    cashBars_->setMinimumHeight(220);
    barsCard->addContent(cashBars_);
    layout->addWidget(barsCard);

    auto* lineCard = new Card(QStringLiteral("CAJA DEL NEGOCIO AL CIERRE DE CADA MES"), body);
    cashLine_ = new LineChart(lineCard);
    cashLine_->setMinimumHeight(180);
    lineCard->addContent(cashLine_);
    layout->addWidget(lineCard);

    cashTable_ = makeTable({QStringLiteral("Mes"), QStringLiteral("Entradas"), QStringLiteral("Gastos"),
                            QStringLiteral("Sueldo"), QStringLiteral("Saldo")},
                           0);
    cashTable_->setMinimumHeight(260);
    layout->addWidget(cashTable_);
}

void ReportsPage::refillCash() {
    const core::Date to = snapshot_.today;
    const core::Date from = to.firstDayOfMonth().addMonths(-11);
    const auto months = core::businessCashFlow(snapshot_.movements, snapshot_.pockets, from, to, snapshot_.currency);

    std::vector<ChartPoint> bars;
    std::vector<ChartPoint> line;
    for (const core::CashMonth& m : months) {
        const core::Money out = m.outExpenses + m.outSalary;
        ChartPoint bar;
        bar.label = theme::monthName(m.month).left(3) + QLatin1Char(' ') + QString::number(m.month.year % 100);
        bar.primary = static_cast<double>(m.in.minor()) / 100.0;
        bar.secondary = static_cast<double>(out.minor()) / 100.0;
        bar.primaryText = theme::formatMoney(m.in);
        bars.push_back(bar);
        ChartPoint point;
        point.label = bar.label;
        point.primary = static_cast<double>(m.balance.minor()) / 100.0;
        point.primaryText = theme::formatMoney(m.balance);
        line.push_back(point);
    }
    cashBars_->setData(bars);
    cashLine_->setData(line);

    cashTable_->setRowCount(static_cast<int>(months.size()));
    for (int row = 0; row < static_cast<int>(months.size()); ++row) {
        const core::CashMonth& m = months[months.size() - 1 - static_cast<std::size_t>(row)];
        setText(cashTable_, row, 0, theme::monthName(m.month));
        setNumber(cashTable_, row, 1, theme::formatMoney(m.in), theme::kPositive);
        setNumber(cashTable_, row, 2, theme::formatMoney(m.outExpenses), theme::kNegative);
        setNumber(cashTable_, row, 3, theme::formatMoney(m.outSalary), theme::kAccent);
        setNumber(cashTable_, row, 4, theme::formatMoney(m.balance),
                  m.balance.isNegative() ? theme::kNegative : theme::kText);
    }

    if (!months.empty()) {
        const core::CashMonth& now = months.back();
        cashHeadline_->setText(QStringLiteral("En %1 entraron %2; salieron %3 en gastos y %4 en sueldo. "
                                              "La caja del negocio está en %5.")
                                   .arg(theme::monthName(now.month), theme::formatMoney(now.in),
                                        theme::formatMoney(now.outExpenses), theme::formatMoney(now.outSalary),
                                        theme::formatMoney(now.balance)));
    }
}

// ----------------------------------------------------------------- Sueldo

void ReportsPage::addSalarySection(QVBoxLayout* layout) {
    QWidget* body = layout->parentWidget();
    auto* card = new Card(QStringLiteral("¿CUÁNTO ME PUEDO PAGAR?"), body);
    salaryHeadline_ = new QLabel(card);
    salaryHeadline_->setFont(theme::figureFont(16));
    salaryHeadline_->setWordWrap(true);
    card->addContent(salaryHeadline_);
    salaryScale_ = new SalaryScale(card);
    salaryScale_->setMinimumHeight(150);
    card->addContent(salaryScale_);
    salaryBasis_ = new QLabel(card);
    salaryBasis_->setWordWrap(true);
    salaryBasis_->setFont(theme::bodyFont(10));
    theme::setLabelColor(salaryBasis_, theme::kTextMuted);
    card->addContent(salaryBasis_);
    salarySplit_ = new SplitBar(card);
    salarySplit_->setMinimumHeight(64);
    card->addContent(salarySplit_);
    salaryMonths_ = new QLabel(card);
    salaryMonths_->setWordWrap(true);
    salaryMonths_->setFont(theme::bodyFont(9));
    theme::setLabelColor(salaryMonths_, theme::kTextFaint);
    card->addContent(salaryMonths_);
    layout->addWidget(card);
}

void ReportsPage::refillSalary() {
    const core::SalaryAdvice& a = snapshot_.salary;
    const auto money = [](const core::Money& m) { return theme::formatMoney(m); };

    if (a.closedMonths == 0) {
        salaryHeadline_->setText(QStringLiteral("Todavía no hay un mes cerrado: el sueldo sostenible se "
                                                "calcula con meses completos, no con uno a medias."));
        theme::setLabelColor(salaryHeadline_, theme::kTextMuted);
    } else if (!a.shortfall.isZero()) {
        salaryHeadline_->setText(QStringLiteral("Hoy no hay sueldo sostenible: el negocio pierde %1 por mes "
                                                "en promedio. Mira el reporte Por tipo para ver qué precio "
                                                "subir.")
                                     .arg(money(a.shortfall)));
        theme::setLabelColor(salaryHeadline_, theme::kNegative);
    } else if (a.margin() && a.margin()->isNegative()) {
        salaryHeadline_->setText(QStringLiteral("Te puedes pagar %1 y necesitas %2. Faltan %3 por mes.")
                                     .arg(money(a.salary), money(*a.personalSpend), money(-*a.margin())));
        theme::setLabelColor(salaryHeadline_, theme::kNegative);
    } else if (a.margin()) {
        salaryHeadline_->setText(QStringLiteral("Te puedes pagar %1 al mes. Necesitas %2. Te sobran %3.")
                                     .arg(money(a.salary), money(*a.personalSpend), money(*a.margin())));
        theme::setLabelColor(salaryHeadline_, theme::kPositive);
    }
    if (a.closedMonths > 0 && a.paidAverage > a.salary) {
        salaryHeadline_->setText(salaryHeadline_->text() +
                                 QStringLiteral(" Te pagaste %1 por mes: la diferencia sale de la caja del negocio.")
                                     .arg(money(a.paidAverage)));
    }

    salaryScale_->setValues(static_cast<double>(a.salary.minor()),
                            a.personalSpend ? static_cast<double>(a.personalSpend->minor()) : -1.0,
                            a.closedMonths > 0 ? static_cast<double>(a.paidAverage.minor()) : -1.0,
                            money(a.salary), a.personalSpend ? money(*a.personalSpend) : QString(),
                            money(a.paidAverage));

    if (a.average3 && a.average6) {
        salaryBasis_->setText(
            QStringLiteral("Utilidad neta promedio: %1 meses %2 · %3 meses %4 → se usa %5, la menor.%6")
                .arg(std::min(3, a.closedMonths))
                .arg(money(*a.average3))
                .arg(a.closedMonths)
                .arg(money(*a.average6), money(std::min(*a.average3, *a.average6)),
                     a.provisional ? QStringLiteral(" Provisional: hay %1 mes(es) cerrados, la cuenta pide 3.")
                                         .arg(a.closedMonths)
                                   : QString()));
    } else {
        salaryBasis_->clear();
    }

    const auto pct = [](int bps) { return QString::number(bps / 100.0, 'g', 4).replace(QLatin1Char('.'), QLatin1Char(',')); };
    salarySplit_->setSegments({
        {QStringLiteral("Sueldo %1%").arg(pct(snapshot_.split.salaryBps)), money(a.salary),
         static_cast<double>(a.salary.minor()), theme::kPositive},
        {QStringLiteral("Impuestos %1%").arg(pct(snapshot_.split.taxesBps)), money(a.taxes),
         static_cast<double>(a.taxes.minor()), theme::kAviso},
        {QStringLiteral("Reinversión %1%").arg(pct(snapshot_.split.reinvestBps)), money(a.reinvest),
         static_cast<double>(a.reinvest.minor()), theme::kSerie},
        {QStringLiteral("Emergencia %1%").arg(pct(snapshot_.split.emergencyBps)), money(a.emergency),
         static_cast<double>(a.emergency.minor()), theme::kAhorro},
    });

    QStringList months;
    for (const core::MonthNet& m : a.months) {
        months << QStringLiteral("%1: %2").arg(theme::monthName(m.month), money(m.net));
    }
    salaryMonths_->setText(months.isEmpty() ? QString()
                                            : QStringLiteral("Utilidad neta por mes (ingresos − costos del "
                                                             "negocio; el sueldo no resta): ") +
                                                  months.join(QStringLiteral(" · ")));
}

} // namespace dake::ui
