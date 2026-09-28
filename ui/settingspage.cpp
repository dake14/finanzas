// ui/settingspage.cpp — lo que se configura una vez.
//
// Nada de esto hace falta para empezar: cada valor tiene su default, y la
// pantalla existe para corregir el que no sirva.

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QSignalBlocker>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "cards.hpp"
#include "fields.hpp"
#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString classLabel(core::CategoryClass cls) {
    switch (cls) {
        case core::CategoryClass::General: return QStringLiteral("General");
        case core::CategoryClass::Fija: return QStringLiteral("Gasto fijo");
        case core::CategoryClass::Variable: return QStringLiteral("Consumible");
        case core::CategoryClass::Activo: return QStringLiteral("Herramienta");
    }
    return {};
}

constexpr core::CategoryClass kClasses[] = {core::CategoryClass::General,
                                            core::CategoryClass::Fija,
                                            core::CategoryClass::Variable,
                                            core::CategoryClass::Activo};

} // namespace

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void SettingsPage::buildUi() {
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

    auto* heading = new QLabel(QStringLiteral("Ajustes"), page);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    layout->addWidget(heading);

    auto* lookCard = new Card(QStringLiteral("APARIENCIA"), page);
    lookCard->setSubtitle(QStringLiteral("Claro u oscuro. Cambia al instante, tambien en la ventana de anotar."));
    tema_ = new QComboBox(lookCard);
    tema_->setObjectName(QStringLiteral("TemaSelector"));
    tema_->setFont(theme::bodyFont(10));
    tema_->addItems({QStringLiteral("Claro"), QStringLiteral("Oscuro")});
    tema_->setMaximumWidth(220);
    connect(tema_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit themeChanged(index == 1 ? theme::Tema::Oscuro : theme::Tema::Claro);
    });
    lookCard->addContent(tema_);
    layout->addWidget(lookCard);

    auto* captureCard = new Card(QStringLiteral("CAPTURA"), page);
    captureCard->setSubtitle(
        QStringLiteral("El atajo abre la ventana de anotar desde cualquier programa. Para que "
                       "funcione, la aplicación tiene que estar abierta: por eso arranca con "
                       "Windows y al cerrarla queda en la bandeja del sistema."));
    auto* hotkeyRow = new QWidget(captureCard);
    auto* hotkeyLayout = new QHBoxLayout(hotkeyRow);
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->setSpacing(10);
    auto* hotkeyLabel = new QLabel(QStringLiteral("Atajo"), hotkeyRow);
    hotkeyLabel->setFont(theme::bodyFont(10));
    theme::setLabelColor(hotkeyLabel, theme::kTextMuted);
    hotkey_ = new QKeySequenceEdit(hotkeyRow);
    hotkey_->setMaximumSequenceLength(1);
    hotkey_->setFixedWidth(200);
    connect(hotkey_, &QKeySequenceEdit::editingFinished, this, [this] {
        if (!hotkey_->keySequence().isEmpty()) {
            emit hotkeyChanged(hotkey_->keySequence());
        }
    });
    hotkeyStatus_ = new QLabel(hotkeyRow);
    hotkeyStatus_->setFont(theme::bodyFont(9));
    hotkeyLayout->addWidget(hotkeyLabel);
    hotkeyLayout->addWidget(hotkey_);
    hotkeyLayout->addWidget(hotkeyStatus_, 1);
    captureCard->addContent(hotkeyRow);

    autostart_ = new QCheckBox(QStringLiteral("Arrancar con Windows, en la bandeja del sistema"),
                               captureCard);
    autostart_->setFont(theme::bodyFont(10));
    connect(autostart_, &QCheckBox::toggled, this, &SettingsPage::autostartChanged);
    captureCard->addContent(autostart_);

    layout->addWidget(captureCard);

    // --- Costos ---------------------------------------------------------------
    auto* costsCard = new Card(QStringLiteral("COSTOS"), page);
    costsCard->setSubtitle(
        QStringLiteral("La tarifa por hora es lo que quieres ganar por cada hora de trabajo: entra "
                       "al costo de cada reparación, así que un margen de 0% es cobrar justo tu "
                       "tarifa. El margen objetivo es lo que quieres dejar por encima."));
    auto* costsRow = new QWidget(costsCard);
    auto* costsLayout = new QHBoxLayout(costsRow);
    costsLayout->setContentsMargins(0, 0, 0, 0);
    costsLayout->setSpacing(10);
    auto label = [costsRow](const QString& text) {
        auto* l = new QLabel(text, costsRow);
        l->setFont(theme::bodyFont(10));
        theme::setLabelColor(l, theme::kTextMuted);
        return l;
    };
    hourlyRate_ = new QLineEdit(costsRow);
    hourlyRate_->setFixedWidth(110);
    hourlyRate_->setPlaceholderText(QStringLiteral("15,00"));
    targetMargin_ = new QLineEdit(costsRow);
    targetMargin_->setFixedWidth(70);
    targetMargin_->setPlaceholderText(QStringLiteral("30"));
    costsLayout->addWidget(label(QStringLiteral("Tarifa por hora")));
    costsLayout->addWidget(hourlyRate_);
    costsLayout->addSpacing(18);
    costsLayout->addWidget(label(QStringLiteral("Margen objetivo %")));
    costsLayout->addWidget(targetMargin_);
    costsLayout->addStretch(1);
    costsCard->addContent(costsRow);
    costsNote_ = new QLabel(costsCard);
    costsNote_->setWordWrap(true);
    costsNote_->setFont(theme::bodyFont(9));
    costsCard->addContent(costsNote_);
    connect(hourlyRate_, &QLineEdit::editingFinished, this, &SettingsPage::emitCosts);

    // El reparto de la utilidad: de aca sale el sueldo recomendado.
    auto* splitRow = new QWidget(costsCard);
    auto* splitLayout = new QHBoxLayout(splitRow);
    splitLayout->setContentsMargins(0, 0, 0, 0);
    splitLayout->setSpacing(8);
    auto splitField = [splitRow, splitLayout, this](const QString& name) {
        auto* l = new QLabel(name, splitRow);
        l->setFont(theme::bodyFont(10));
        theme::setLabelColor(l, theme::kTextMuted);
        auto* edit = new QLineEdit(splitRow);
        edit->setFixedWidth(52);
        connect(edit, &QLineEdit::editingFinished, this, &SettingsPage::emitSplit);
        splitLayout->addWidget(l);
        splitLayout->addWidget(edit);
        splitLayout->addSpacing(10);
        return edit;
    };
    auto* splitTitle = new QLabel(QStringLiteral("Reparto de la utilidad %:"), splitRow);
    splitTitle->setFont(theme::bodyFont(10));
    theme::setLabelColor(splitTitle, theme::kTextMuted);
    splitLayout->addWidget(splitTitle);
    splitSalary_ = splitField(QStringLiteral("sueldo"));
    splitTaxes_ = splitField(QStringLiteral("impuestos"));
    splitReinvest_ = splitField(QStringLiteral("reinversión"));
    splitEmergency_ = splitField(QStringLiteral("emergencia"));
    splitLayout->addStretch(1);
    costsCard->addContent(splitRow);
    splitNote_ = new QLabel(costsCard);
    splitNote_->setFont(theme::bodyFont(9));
    splitNote_->setWordWrap(true);
    costsCard->addContent(splitNote_);
    connect(targetMargin_, &QLineEdit::editingFinished, this, &SettingsPage::emitCosts);
    layout->addWidget(costsCard);

    // --- Plantillas -------------------------------------------------------------
    auto* templatesCard = new Card(QStringLiteral("PLANTILLAS DE REPARACIÓN"), page);
    templatesCard->setSubtitle(
        QStringLiteral("Lo que trae precargado una reparación nueva. Los repuestos típicos van "
                       "separados por punto y coma, con su costo al final: \"Esferas BGA 4,00; "
                       "Flux 2,50\". Doble clic para editar."));
    templates_ = makeTable({QStringLiteral("Nombre"), QStringLiteral("Tipo"), QStringLiteral("Precio"),
                            QStringLiteral("Horas"), QStringLiteral("Repuestos típicos"), QString()},
                           4);
    templates_->setMinimumHeight(200);
    templates_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                QAbstractItemView::AnyKeyPressed);
    fixColumn(templates_, 1, 130);
    fixColumn(templates_, 5, 76);
    connect(templates_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!filling_) emitTemplate(item->row());
    });
    templatesCard->addContent(templates_);
    auto* addTemplate = new QPushButton(QStringLiteral("Nueva plantilla"), templatesCard);
    addTemplate->setObjectName(QStringLiteral("GhostButton"));
    addTemplate->setCursor(Qt::PointingHandCursor);
    addTemplate->setFont(theme::bodyFont(9));
    connect(addTemplate, &QPushButton::clicked, this, &SettingsPage::templateAdded);
    templatesCard->addContent(addTemplate);
    layout->addWidget(templatesCard);

    // --- Gastos fijos y herramientas ---------------------------------------------
    auto* fixedCard = new Card(QStringLiteral("GASTOS FIJOS DEL TALLER"), page);
    fixedCard->setSubtitle(
        QStringLiteral("Se anotan solos cada mes con el monto estimado y quedan por confirmar en los "
                       "pendientes de Hoy. Si pagas algo fijo que no está acá, agrégalo: sin eso, la tasa de "
                       "fijos por hora sale baja."));
    fixedSummary_ = new QLabel(fixedCard);
    fixedSummary_->setWordWrap(true);
    fixedSummary_->setFont(theme::bodyFont(10));
    fixedCard->addContent(fixedSummary_);
    recurring_ = makeTable({QStringLiteral("Nombre"), QStringLiteral("Monto"), QStringLiteral("Día"),
                            QStringLiteral("Bolsillo"), QStringLiteral("Activo"), QString()},
                           0);
    recurring_->setMinimumHeight(150);
    recurring_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                QAbstractItemView::AnyKeyPressed);
    fixColumn(recurring_, 3, 170);
    fixColumn(recurring_, 4, 70);
    fixColumn(recurring_, 5, 76);
    connect(recurring_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!filling_) emitRecurring(item->row());
    });
    fixedCard->addContent(recurring_);
    auto* addRecurring = new QPushButton(QStringLiteral("Nuevo gasto fijo"), fixedCard);
    addRecurring->setObjectName(QStringLiteral("GhostButton"));
    addRecurring->setCursor(Qt::PointingHandCursor);
    connect(addRecurring, &QPushButton::clicked, this, &SettingsPage::recurringAdded);
    fixedCard->addContent(addRecurring);

    auto* toolsLabel = new QLabel(QStringLiteral("HERRAMIENTAS"), fixedCard);
    toolsLabel->setFont(theme::bodyFont(8, QFont::DemiBold));
    theme::setLabelColor(toolsLabel, theme::kTextFaint);
    fixedCard->addContent(toolsLabel);
    tools_ = makeTable({QStringLiteral("Herramienta"), QStringLiteral("Costo"), QStringLiteral("Compra"),
                        QStringLiteral("Meses de vida"), QStringLiteral("Por mes"), QString()},
                       0);
    tools_->setMinimumHeight(120);
    tools_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    fixColumn(tools_, 5, 76);
    connect(tools_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!filling_) emitTool(item->row());
    });
    fixedCard->addContent(tools_);
    auto* addTool = new QPushButton(QStringLiteral("Nueva herramienta"), fixedCard);
    addTool->setObjectName(QStringLiteral("GhostButton"));
    addTool->setCursor(Qt::PointingHandCursor);
    connect(addTool, &QPushButton::clicked, this, &SettingsPage::toolAdded);
    fixedCard->addContent(addTool);

    auto* hoursRow = new QWidget(fixedCard);
    auto* hoursLayout = new QHBoxLayout(hoursRow);
    hoursLayout->setContentsMargins(0, 0, 0, 0);
    auto* hoursLabel = new QLabel(QStringLiteral("Horas de trabajo por mes, mientras no haya historia"), hoursRow);
    hoursLabel->setFont(theme::bodyFont(10));
    theme::setLabelColor(hoursLabel, theme::kTextMuted);
    hoursPerMonth_ = new QLineEdit(hoursRow);
    hoursPerMonth_->setFixedWidth(80);
    connect(hoursPerMonth_, &QLineEdit::editingFinished, this, [this] {
        if (const auto minutes = parseHoursText(hoursPerMonth_->text())) {
            if (*minutes > 0 && *minutes != snapshot_.fallbackMinutesPerMonth) emit fallbackHoursChanged(*minutes);
        }
    });
    hoursLayout->addWidget(hoursLabel);
    hoursLayout->addWidget(hoursPerMonth_);
    hoursLayout->addStretch(1);
    fixedCard->addContent(hoursRow);
    layout->addWidget(fixedCard);

    // --- DakeLabs Cotizaciones ------------------------------------------------
    auto* quotesCard = new Card(QStringLiteral("DAKELABS COTIZACIONES"), page);
    quotesCard->setSubtitle(
        QStringLiteral("Finanzas lee lo esencial de cada documento desde la nube y nunca lo cambia. "
                       "Un informe entregado crea el ingreso por cobrar; al marcarlo pagado —en la "
                       "PC o en el teléfono—, queda cobrado con esa fecha. Una cotización aceptada "
                       "abre la reparación."));
    auto* folderRow = new QWidget(quotesCard);
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(8);
    quoteReview_ = new QPushButton(QStringLiteral("Revisar"), folderRow);
    quoteReview_->setObjectName(QStringLiteral("PrimaryButton"));
    quoteReview_->setCursor(Qt::PointingHandCursor);
    quoteReview_->setFont(theme::bodyFont(9, QFont::DemiBold));
    quoteReview_->setFixedHeight(32);
    connect(quoteReview_, &QPushButton::clicked, this, &SettingsPage::quoteReviewRequested);
    folderLayout->addStretch(1);
    folderLayout->addWidget(quoteReview_);
    quotesCard->addContent(folderRow);
    quoteStatus_ = new QLabel(quotesCard);
    quoteStatus_->setWordWrap(true);
    quoteStatus_->setFont(theme::bodyFont(10));
    quotesCard->addContent(quoteStatus_);
    layout->addWidget(quotesCard);

    auto* categoriesCard = new Card(QStringLiteral("CATEGORÍAS"), page);
    categoriesCard->setSubtitle(
        QStringLiteral("Se crean solas al escribirlas. La cuenta dice si un gasto es del negocio "
                       "o tuyo; la clase, cómo entra al costo de las reparaciones: los gastos "
                       "fijos se reparten por hora trabajada y las herramientas, por su vida "
                       "útil."));
    categories_ = makeTable({QStringLiteral("Categoría"), QStringLiteral("Tipo"),
                             QStringLiteral("Cuenta"), QStringLiteral("Clase")},
                            0);
    categories_->setMinimumHeight(260);
    fixColumn(categories_, 2, 130);
    fixColumn(categories_, 3, 150);
    categoriesCard->addContent(categories_);
    layout->addWidget(categoriesCard);
    layout->addStretch(1);
}

void SettingsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;

    if (!hourlyRate_->hasFocus()) {
        hourlyRate_->setText(moneyFieldText(snapshot.costs.hourlyRateMinor, snapshot.currency));
    }
    if (!targetMargin_->hasFocus()) {
        targetMargin_->setText(QString::number(snapshot.costs.targetMarginBps / 100.0, 'f', 0));
    }
    if (snapshot.costs.hourlyRateMinor == 0) {
        costsNote_->setText(QStringLiteral("Sin tarifa, tus horas no cuestan nada y todos los "
                                           "márgenes salen inflados."));
        theme::setLabelColor(costsNote_, theme::kAviso);
    } else {
        costsNote_->setText(
            QStringLiteral("Cada hora de reparación tiene que dejar %1: la tarifa más %2 de fijos "
                           "del taller.")
                .arg(theme::formatMoney(core::hourlyNeeded(snapshot.costs, snapshot.currency)),
                     theme::formatMoney(core::Money::fromMinor(snapshot.costs.fixedPerHourMinor,
                                                               snapshot.currency))));
        theme::setLabelColor(costsNote_, theme::kTextMuted);
    }
    refillTemplates();
    refillFixed();

    const std::pair<QLineEdit*, int> splits[] = {{splitSalary_, snapshot.split.salaryBps},
                                                 {splitTaxes_, snapshot.split.taxesBps},
                                                 {splitReinvest_, snapshot.split.reinvestBps},
                                                 {splitEmergency_, snapshot.split.emergencyBps}};
    for (const auto& [edit, bps] : splits) {
        if (!edit->hasFocus()) edit->setText(QString::number(bps / 100.0, 'g', 4).replace(QLatin1Char('.'), QLatin1Char(',')));
    }
    splitNote_->setText(QStringLiteral("Los cuatro suman 100. El sueldo recomendado es la utilidad neta "
                                       "promedio por el porcentaje de sueldo."));
    theme::setLabelColor(splitNote_, theme::kTextFaint);

    const int holds = snapshot.quoteHolds();
    quoteReview_->setText(holds > 0 ? QStringLiteral("Revisar (%1)").arg(holds) : QStringLiteral("Revisar"));
    quoteReview_->setEnabled(holds > 0);
    if (snapshot.quoteDocs.empty()) {
        quoteStatus_->setText(QStringLiteral("Todavía no bajó ningún documento. En DakeLabs "
                                             "Cotizaciones: Datos y respaldo → Nube → Conectar, "
                                             "con esta misma cuenta, y después Sincronizar."));
        theme::setLabelColor(quoteStatus_, theme::kTextMuted);
    } else {
        int imported = 0;
        for (const core::QuotePlan& plan : snapshot.quotePlans) {
            if (plan.decision == core::QuoteDecision::Importar) ++imported;
        }
        QString text = QStringLiteral("%1 documentos desde la nube · %2 al día en Finanzas")
                           .arg(snapshot.quoteDocs.size())
                           .arg(imported);
        if (holds > 0) {
            text += QStringLiteral(" · %1 esperan que decidas").arg(holds);
        }
        if (!snapshot.quoteErrors.isEmpty()) {
            text += QStringLiteral("\nNo se pudieron leer: ") + snapshot.quoteErrors.join(QStringLiteral("; "));
        }
        quoteStatus_->setText(text);
        theme::setLabelColor(quoteStatus_, holds > 0 ? theme::kAviso : theme::kTextMuted);
    }

    {
        const QSignalBlocker blockHotkey(hotkey_);
        hotkey_->setKeySequence(QKeySequence(snapshot.hotkey));
        const QSignalBlocker blockAutostart(autostart_);
        autostart_->setChecked(snapshot.autostart);
        const QSignalBlocker blockTema(tema_);
        tema_->setCurrentIndex(theme::currentTheme() == theme::Tema::Oscuro ? 1 : 0);
    }
    if (snapshot.hotkeyRegistered) {
        hotkeyStatus_->setText(QStringLiteral("activo"));
        theme::setLabelColor(hotkeyStatus_, theme::kPositive);
    } else {
        hotkeyStatus_->setText(QStringLiteral("no se pudo registrar: otro programa lo usa o la "
                                              "tecla no se admite; elige otra."));
        theme::setLabelColor(hotkeyStatus_, theme::kNegative);
    }
    categories_->setRowCount(static_cast<int>(snapshot.categories.size()));
    for (int row = 0; row < static_cast<int>(snapshot.categories.size()); ++row) {
        const core::Category category = snapshot.categories[static_cast<std::size_t>(row)];
        setText(categories_, row, 0, QString::fromStdString(category.name));
        setText(categories_, row, 1,
                category.kind == core::MovementKind::Ingreso ? QStringLiteral("Ingreso")
                                                             : QStringLiteral("Gasto"),
                theme::kTextMuted);

        auto* account = cellCombo(categories_);
        account->addItems({QStringLiteral("Negocio"), QStringLiteral("Personal")});
        account->setCurrentIndex(category.account == core::Account::Personal ? 1 : 0);
        connect(account, &QComboBox::currentIndexChanged, this, [this, category](int index) {
            core::Category changed = category;
            changed.account = index == 1 ? core::Account::Personal : core::Account::Negocio;
            emit categoryChanged(changed);
        });
        categories_->setCellWidget(row, 2, account);

        auto* cls = cellCombo(categories_);
        for (const core::CategoryClass value : kClasses) {
            cls->addItem(classLabel(value));
        }
        cls->setCurrentIndex(static_cast<int>(category.cls));
        cls->setEnabled(category.kind == core::MovementKind::Gasto);
        connect(cls, &QComboBox::currentIndexChanged, this, [this, category](int index) {
            core::Category changed = category;
            changed.cls = kClasses[index];
            emit categoryChanged(changed);
        });
        categories_->setCellWidget(row, 3, cls);
    }
}

// --------------------------------------------------------------- Costos

void SettingsPage::emitCosts() {
    core::CostSettings settings = snapshot_.costs;
    if (const auto rate = parseMoneyText(hourlyRate_->text(), snapshot_.currency)) {
        settings.hourlyRateMinor = *rate;
    } else if (hourlyRate_->text().trimmed().isEmpty()) {
        settings.hourlyRateMinor = 0;
    }
    bool ok = false;
    const double margin = targetMargin_->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.'))
                              .toDouble(&ok);
    if (ok && margin >= 0 && margin < 100) {
        settings.targetMarginBps = static_cast<int>(margin * 100.0 + 0.5);
    }
    if (settings.hourlyRateMinor != snapshot_.costs.hourlyRateMinor ||
        settings.targetMarginBps != snapshot_.costs.targetMarginBps) {
        emit costSettingsChanged(settings);
    }
}

void SettingsPage::emitSplit() {
    auto bps = [](const QLineEdit* edit) {
        bool ok = false;
        const double value = edit->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        return ok ? static_cast<int>(value * 100.0 + 0.5) : -1;
    };
    core::ProfitSplit split;
    split.salaryBps = bps(splitSalary_);
    split.taxesBps = bps(splitTaxes_);
    split.reinvestBps = bps(splitReinvest_);
    split.emergencyBps = bps(splitEmergency_);
    if (!split.valid()) {
        splitNote_->setText(QStringLiteral("No suman 100: se sigue usando el reparto anterior."));
        theme::setLabelColor(splitNote_, theme::kNegative);
        return;
    }
    if (split.salaryBps != snapshot_.split.salaryBps || split.taxesBps != snapshot_.split.taxesBps ||
        split.reinvestBps != snapshot_.split.reinvestBps || split.emergencyBps != snapshot_.split.emergencyBps) {
        emit splitChanged(split);
    }
}

// ----------------------------------------------------------- Plantillas

namespace {

[[nodiscard]] QString templateTypeLabel(core::RepairType type) {
    switch (type) {
        case core::RepairType::GPU: return QStringLiteral("GPU");
        case core::RepairType::Laptop: return QStringLiteral("Laptop");
        case core::RepairType::PlacaMadre: return QStringLiteral("Placa madre");
        case core::RepairType::Otro: return QStringLiteral("Otro");
    }
    return {};
}

[[nodiscard]] QString partsText(const std::vector<core::TemplatePart>& parts, core::Currency currency) {
    QStringList out;
    for (const core::TemplatePart& part : parts) {
        QString text = QString::fromStdString(part.name);
        if (part.costMinor > 0) {
            text += QLatin1Char(' ') + moneyFieldText(part.costMinor, currency);
        }
        out << text;
    }
    return out.join(QStringLiteral("; "));
}

/// "Esferas BGA 4,00; Flux 2,50" -> {{"Esferas BGA", 400}, {"Flux", 250}}.
[[nodiscard]] std::vector<core::TemplatePart> parsePartsText(const QString& text,
                                                             core::Currency currency) {
    std::vector<core::TemplatePart> out;
    for (const QString& chunk : text.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        QStringList words = chunk.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.isEmpty()) continue;
        core::TemplatePart part;
        if (words.size() > 1) {
            if (const auto cost = parseMoneyText(words.last(), currency)) {
                part.costMinor = *cost;
                words.removeLast();
            }
        }
        part.name = words.join(QLatin1Char(' ')).toStdString();
        out.push_back(part);
    }
    return out;
}

} // namespace

void SettingsPage::refillTemplates() {
    filling_ = true;
    templates_->setRowCount(static_cast<int>(snapshot_.templates.size()));
    for (int row = 0; row < static_cast<int>(snapshot_.templates.size()); ++row) {
        const core::RepairTemplate& tpl = snapshot_.templates[static_cast<std::size_t>(row)];
        auto* name = new QTableWidgetItem(QString::fromStdString(tpl.name));
        name->setData(Qt::UserRole, QString::fromStdString(tpl.id));
        templates_->setItem(row, 0, name);

        auto* type = cellCombo(templates_);
        for (const core::RepairType t : core::allRepairTypes()) {
            type->addItem(templateTypeLabel(t));
        }
        type->setCurrentIndex(static_cast<int>(tpl.type));
        connect(type, &QComboBox::currentIndexChanged, this, [this, row] {
            if (!filling_) emitTemplate(row);
        });
        templates_->setCellWidget(row, 1, type);

        setNumber(templates_, row, 2, moneyFieldText(tpl.priceMinor, snapshot_.currency));
        setNumber(templates_, row, 3, tpl.estMinutes > 0 ? hoursText(tpl.estMinutes) : QString());
        setText(templates_, row, 4, partsText(tpl.parts, snapshot_.currency));

        auto* remove = new QPushButton(QStringLiteral("Quitar"), templates_);
        remove->setObjectName(QStringLiteral("GhostButton"));
        remove->setFont(theme::bodyFont(8));
        const core::Id id = tpl.id;
        connect(remove, &QPushButton::clicked, this, [this, id] { emit templateRemoved(id); });
        templates_->setCellWidget(row, 5, remove);
    }
    filling_ = false;
}

void SettingsPage::emitTemplate(int row) {
    if (row < 0 || row >= static_cast<int>(snapshot_.templates.size())) return;
    core::RepairTemplate tpl = snapshot_.templates[static_cast<std::size_t>(row)];
    auto text = [this, row](int column) {
        const QTableWidgetItem* item = templates_->item(row, column);
        return item == nullptr ? QString() : item->text();
    };
    const QString name = text(0).trimmed();
    if (!name.isEmpty()) tpl.name = name.toStdString();
    if (auto* type = qobject_cast<QComboBox*>(templates_->cellWidget(row, 1))) {
        tpl.type = core::allRepairTypes()[static_cast<std::size_t>(std::max(0, type->currentIndex()))];
    }
    tpl.priceMinor = parseMoneyText(text(2), snapshot_.currency).value_or(0);
    tpl.estMinutes = parseHoursText(text(3)).value_or(0);
    tpl.parts = parsePartsText(text(4), snapshot_.currency);
    emit templateChanged(tpl);
}

// ----------------------------------------------------- Fijos y herramientas

namespace {

[[nodiscard]] QString seconds(qint64 ms) {
    return QString::number(static_cast<double>(ms) / 1000.0, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(','));
}

} // namespace

void SettingsPage::refillFixed() {
    filling_ = true;
    const core::FixedRate& rate = snapshot_.fixedRate;
    fixedSummary_->setText(
        QStringLiteral("Fijos del mes: %1 (gastos fijos activos + depreciación). %2 h de trabajo por mes%3. "
                       "Cada hora de reparación carga %4 de fijos.")
            .arg(theme::formatMoney(rate.monthlyFixed), hoursText(rate.monthlyMinutes),
                 rate.minutesFromSettings ? QStringLiteral(" (las de abajo: todavía no hay horas reales)")
                                          : QStringLiteral(" según tus últimas entregas"),
                 theme::formatMoney(rate.perHour)));
    theme::setLabelColor(fixedSummary_, theme::kText);

    recurring_->setRowCount(static_cast<int>(snapshot_.recurring.size()));
    for (int row = 0; row < static_cast<int>(snapshot_.recurring.size()); ++row) {
        const core::Recurring& r = snapshot_.recurring[static_cast<std::size_t>(row)];
        auto* name = new QTableWidgetItem(QString::fromStdString(r.name));
        name->setData(Qt::UserRole, QString::fromStdString(r.id));
        recurring_->setItem(row, 0, name);
        setNumber(recurring_, row, 1, moneyFieldText(r.amountMinor, snapshot_.currency));
        setNumber(recurring_, row, 2, QString::number(r.dayOfMonth));
        auto* pocket = cellCombo(recurring_);
        for (const core::Pocket& p : snapshot_.pockets) {
            if (!p.archived) pocket->addItem(QString::fromStdString(p.name), QString::fromStdString(p.id));
        }
        pocket->setCurrentIndex(std::max(0, pocket->findData(QString::fromStdString(r.pocketId))));
        connect(pocket, &QComboBox::currentIndexChanged, this, [this, row] {
            if (!filling_) emitRecurring(row);
        });
        recurring_->setCellWidget(row, 3, pocket);
        auto* active = new QTableWidgetItem();
        active->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        active->setCheckState(r.active ? Qt::Checked : Qt::Unchecked);
        recurring_->setItem(row, 4, active);
        auto* remove = new QPushButton(QStringLiteral("Quitar"), recurring_);
        remove->setObjectName(QStringLiteral("GhostButton"));
        remove->setFont(theme::bodyFont(8));
        const core::Id id = r.id;
        connect(remove, &QPushButton::clicked, this, [this, id] { emit recurringRemoved(id); });
        recurring_->setCellWidget(row, 5, remove);
    }

    tools_->setRowCount(static_cast<int>(snapshot_.tools.size()));
    for (int row = 0; row < static_cast<int>(snapshot_.tools.size()); ++row) {
        const core::Tool& t = snapshot_.tools[static_cast<std::size_t>(row)];
        auto* name = new QTableWidgetItem(QString::fromStdString(t.name));
        name->setData(Qt::UserRole, QString::fromStdString(t.id));
        tools_->setItem(row, 0, name);
        setNumber(tools_, row, 1, moneyFieldText(t.costMinor, snapshot_.currency));
        setText(tools_, row, 2, QString::fromStdString(t.bought.toIso()));
        setNumber(tools_, row, 3, QString::number(t.lifeMonths));
        setNumber(tools_, row, 4,
                  theme::formatMoney(core::depreciationInMonth(t, snapshot_.today, snapshot_.currency)),
                  theme::kTextMuted);
        tools_->item(row, 4)->setFlags(Qt::ItemIsEnabled);
        auto* remove = new QPushButton(QStringLiteral("Quitar"), tools_);
        remove->setObjectName(QStringLiteral("GhostButton"));
        remove->setFont(theme::bodyFont(8));
        const core::Id id = t.id;
        connect(remove, &QPushButton::clicked, this, [this, id] { emit toolRemoved(id); });
        tools_->setCellWidget(row, 5, remove);
    }

    if (!hoursPerMonth_->hasFocus()) hoursPerMonth_->setText(hoursText(snapshot_.fallbackMinutesPerMonth));

    filling_ = false;
}

void SettingsPage::emitRecurring(int row) {
    if (row < 0 || row >= static_cast<int>(snapshot_.recurring.size())) return;
    core::Recurring r = snapshot_.recurring[static_cast<std::size_t>(row)];
    const auto text = [this, row](int column) {
        const QTableWidgetItem* item = recurring_->item(row, column);
        return item == nullptr ? QString() : item->text();
    };
    const QString name = text(0).trimmed();
    if (!name.isEmpty()) {
        r.name = name.toStdString();
        r.category = r.name;
    }
    r.amountMinor = parseMoneyText(text(1), snapshot_.currency).value_or(r.amountMinor);
    bool ok = false;
    const int day = text(2).trimmed().toInt(&ok);
    if (ok && day >= 1 && day <= 31) r.dayOfMonth = day;
    if (auto* pocket = qobject_cast<QComboBox*>(recurring_->cellWidget(row, 3))) {
        r.pocketId = pocket->currentData().toString().toStdString();
    }
    if (const QTableWidgetItem* active = recurring_->item(row, 4)) {
        r.active = active->checkState() == Qt::Checked;
    }
    emit recurringChanged(r);
}

void SettingsPage::emitTool(int row) {
    if (row < 0 || row >= static_cast<int>(snapshot_.tools.size())) return;
    core::Tool t = snapshot_.tools[static_cast<std::size_t>(row)];
    const auto text = [this, row](int column) {
        const QTableWidgetItem* item = tools_->item(row, column);
        return item == nullptr ? QString() : item->text();
    };
    const QString name = text(0).trimmed();
    if (!name.isEmpty()) t.name = name.toStdString();
    t.costMinor = parseMoneyText(text(1), snapshot_.currency).value_or(t.costMinor);
    try {
        t.bought = core::Date::fromIso(text(2).trimmed().toStdString());
    } catch (const std::exception&) {
        // Una fecha mal escrita no cambia la que habia.
    }
    bool ok = false;
    const int months = text(3).trimmed().toInt(&ok);
    if (ok && months > 0) t.lifeMonths = months;
    emit toolChanged(t);
}

} // namespace dake::ui
