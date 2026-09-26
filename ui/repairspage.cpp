// ui/repairspage.cpp — la lista de reparaciones y la ficha de la elegida.
//
// La ficha se guarda sola al salir de cada campo: no hay boton Guardar que
// olvidar. Lo que viene de DakeLabs Cotizaciones (cliente, equipo, precio) se
// ve bloqueado: se corrige alla, que es donde esta el papel que tiene el
// cliente.

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include "cards.hpp"
#include "fields.hpp"
#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

enum StatusFilter { kAbiertas = 0, kEnProceso, kPorCobrar, kCobradas, kTodas };

[[nodiscard]] QString typeLabel(core::RepairType type) {
    switch (type) {
        case core::RepairType::GPU: return QStringLiteral("GPU");
        case core::RepairType::Laptop: return QStringLiteral("Laptop");
        case core::RepairType::PlacaMadre: return QStringLiteral("Placa madre");
        case core::RepairType::Otro: return QStringLiteral("Otro");
    }
    return {};
}

[[nodiscard]] QString statusLabel(core::RepairStatus status) {
    switch (status) {
        case core::RepairStatus::EnProceso: return QStringLiteral("En proceso");
        case core::RepairStatus::Entregada: return QStringLiteral("Por cobrar");
        case core::RepairStatus::Cobrada: return QStringLiteral("Cobrada");
    }
    return {};
}

[[nodiscard]] QColor statusColor(core::RepairStatus status) {
    switch (status) {
        case core::RepairStatus::EnProceso: return theme::kAccent;
        case core::RepairStatus::Entregada: return theme::kInversion;
        case core::RepairStatus::Cobrada: return theme::kPositive;
    }
    return theme::kText;
}

[[nodiscard]] bool passes(const core::Repair& repair, int filter) {
    switch (filter) {
        case kAbiertas: return repair.status != core::RepairStatus::Cobrada;
        case kEnProceso: return repair.status == core::RepairStatus::EnProceso;
        case kPorCobrar: return repair.status == core::RepairStatus::Entregada;
        case kCobradas: return repair.status == core::RepairStatus::Cobrada;
        default: return true;
    }
}

[[nodiscard]] QLabel* small(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::bodyFont(8));
    theme::setLabelColor(label, theme::kTextFaint);
    return label;
}

[[nodiscard]] QString money(const core::Money& m) {
    return theme::formatMoney(m);
}

/// Una fila de la cuenta: concepto a la izquierda, monto a la derecha.
[[nodiscard]] QString line(const QString& label, const QString& amount, const QColor& color,
                           bool strong = false) {
    const QString weight = strong ? QStringLiteral("font-weight:600;") : QString();
    return QStringLiteral("<tr><td style='padding:2px 18px 2px 0;color:%1;%4'>%2</td>"
                          "<td align='right' style='padding:2px 0;color:%1;%4'>%3</td></tr>")
        .arg(color.name(), label, amount, weight);
}

} // namespace

RepairsPage::RepairsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void RepairsPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 18);
    layout->setSpacing(12);

    auto* header = new QHBoxLayout();
    auto* heading = new QLabel(QStringLiteral("Reparaciones"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    header->addWidget(heading);
    header->addStretch(1);
    auto* create = new QPushButton(QStringLiteral("Nueva  ·  Ctrl+R"), this);
    create->setObjectName(QStringLiteral("PrimaryButton"));
    create->setCursor(Qt::PointingHandCursor);
    create->setFont(theme::bodyFont(9, QFont::DemiBold));
    create->setFixedHeight(34);
    connect(create, &QPushButton::clicked, this, &RepairsPage::newRepairRequested);
    header->addWidget(create);
    layout->addLayout(header);

    auto* filters = new QHBoxLayout();
    filters->setSpacing(8);
    statusFilter_ = new QComboBox(this);
    statusFilter_->addItems({QStringLiteral("Abiertas"), QStringLiteral("En proceso"),
                             QStringLiteral("Por cobrar"), QStringLiteral("Cobradas"),
                             QStringLiteral("Todas")});
    typeFilter_ = new QComboBox(this);
    typeFilter_->addItem(QStringLiteral("Todos los tipos"));
    for (const core::RepairType type : core::allRepairTypes()) {
        typeFilter_->addItem(typeLabel(type));
    }
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(QStringLiteral("Buscar por orden, equipo o cliente"));
    search_->setClearButtonEnabled(true);
    for (QWidget* w : {static_cast<QWidget*>(statusFilter_), static_cast<QWidget*>(typeFilter_),
                       static_cast<QWidget*>(search_)}) {
        w->setFont(theme::bodyFont(10));
    }
    filters->addWidget(statusFilter_);
    filters->addWidget(typeFilter_);
    filters->addWidget(search_, 1);
    layout->addLayout(filters);
    connect(statusFilter_, &QComboBox::currentIndexChanged, this, [this] { refillList(); });
    connect(typeFilter_, &QComboBox::currentIndexChanged, this, [this] { refillList(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { refillList(); });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    list_ = makeTable({QStringLiteral("Orden"), QStringLiteral("Equipo"), QStringLiteral("Cliente"),
                       QStringLiteral("Tipo"), QStringLiteral("Estado"), QStringLiteral("Precio"),
                       QStringLiteral("Margen")},
                      1);
    list_->setMinimumWidth(460);
    connect(list_, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (filling_ || row < 0) return;
        const QTableWidgetItem* item = list_->item(row, 0);
        if (item != nullptr) {
            showRepair(item->data(Qt::UserRole).toString().toStdString());
        }
    });
    splitter->addWidget(list_);
    splitter->addWidget(buildPanel());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);
}

QWidget* RepairsPage::buildPanel() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumWidth(420);
    auto* panel = new QWidget(scroll);
    scroll->setWidget(panel);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 0, 0, 0);
    layout->setSpacing(10);

    empty_ = new QLabel(QStringLiteral("Elige una reparación de la lista, o crea una con Ctrl+R."),
                        panel);
    empty_->setWordWrap(true);
    empty_->setFont(theme::bodyFont(10));
    theme::setLabelColor(empty_, theme::kTextMuted);
    layout->addWidget(empty_);

    card_ = new Card(QString(), panel);
    auto* body = new QWidget(card_);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);

    auto* titleRow = new QHBoxLayout();
    title_ = new QLabel(body);
    title_->setFont(theme::displayFont(14, QFont::Bold));
    title_->setWordWrap(true);
    theme::setLabelColor(title_, theme::kText);
    status_ = new QLabel(body);
    status_->setFont(theme::bodyFont(9, QFont::DemiBold));
    titleRow->addWidget(title_, 1);
    titleRow->addWidget(status_);
    bodyLayout->addLayout(titleRow);

    locked_ = small(QString(), body);
    locked_->setWordWrap(true);
    bodyLayout->addWidget(locked_);

    // --- Campos -------------------------------------------------------------
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(2);
    client_ = new QLineEdit(body);
    device_ = new QLineEdit(body);
    type_ = new QComboBox(body);
    for (const core::RepairType type : core::allRepairTypes()) {
        type_->addItem(typeLabel(type));
    }
    price_ = new QLineEdit(body);
    shipping_ = new QLineEdit(body);
    consumables_ = new QLineEdit(body);
    estHours_ = new QLineEdit(body);
    realHours_ = new QLineEdit(body);
    realHours_->setPlaceholderText(QStringLiteral("sin cargar"));

    int row = 0;
    auto place = [&](const QString& label, QWidget* field, int column) {
        grid->addWidget(small(label, body), row, column);
        grid->addWidget(field, row + 1, column);
    };
    place(QStringLiteral("CLIENTE"), client_, 0);
    place(QStringLiteral("EQUIPO"), device_, 1);
    row += 2;
    place(QStringLiteral("TIPO"), type_, 0);
    place(QStringLiteral("PRECIO"), price_, 1);
    row += 2;
    place(QStringLiteral("CONSUMIBLES"), consumables_, 0);
    place(QStringLiteral("ENVÍO"), shipping_, 1);
    row += 2;
    place(QStringLiteral("HORAS ESTIMADAS"), estHours_, 0);
    place(QStringLiteral("HORAS REALES"), realHours_, 1);
    bodyLayout->addLayout(grid);

    for (QLineEdit* edit : {client_, device_, price_, shipping_, consumables_, estHours_, realHours_}) {
        edit->setFont(theme::bodyFont(10));
        connect(edit, &QLineEdit::editingFinished, this, &RepairsPage::saveEdits);
    }
    type_->setFont(theme::bodyFont(10));
    connect(type_, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_) saveEdits();
    });

    // --- Acciones -------------------------------------------------------------
    auto* actions = new QHBoxLayout();
    deliver_ = new QPushButton(QStringLiteral("Entregar…  Ctrl+E"), body);
    deliver_->setObjectName(QStringLiteral("PrimaryButton"));
    charge_ = new QPushButton(QStringLiteral("Cobrar"), body);
    charge_->setObjectName(QStringLiteral("GhostButton"));
    for (QPushButton* b : {deliver_, charge_}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFont(theme::bodyFont(9, QFont::DemiBold));
        b->setFixedHeight(32);
        actions->addWidget(b);
    }
    actions->addStretch(1);
    connect(deliver_, &QPushButton::clicked, this, [this] { emit deliverRequested(current_); });
    connect(charge_, &QPushButton::clicked, this, [this] { emit chargeRequested(current_); });

    // Sin mouse: Ctrl+E entrega y Ctrl+B cobra la reparacion abierta en la
    // ficha, con el foco donde sea dentro de la ventana, mientras esta pagina
    // esta a la vista.
    auto* deliverKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this);
    deliverKey->setContext(Qt::WindowShortcut);
    connect(deliverKey, &QShortcut::activated, this, [this] {
        if (!isVisible()) return;
        const core::Repair* repair = snapshot_.repair(current_);
        if (repair != nullptr && repair->sourceRef.empty() &&
            repair->status == core::RepairStatus::EnProceso) {
            emit deliverRequested(current_);
        }
    });
    auto* chargeKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    chargeKey->setContext(Qt::WindowShortcut);
    connect(chargeKey, &QShortcut::activated, this, [this] {
        if (!isVisible()) return;
        const core::Repair* repair = snapshot_.repair(current_);
        if (repair != nullptr && repair->sourceRef.empty() &&
            repair->status != core::RepairStatus::Cobrada) {
            emit chargeRequested(current_);
        }
    });
    bodyLayout->addLayout(actions);

    // --- Repuestos --------------------------------------------------------------
    bodyLayout->addWidget(small(QStringLiteral("REPUESTOS"), body));
    parts_ = makeTable({QStringLiteral("Repuesto"), QStringLiteral("Costo"), QString()}, 0);
    parts_->setMinimumHeight(110);
    parts_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    fixColumn(parts_, 2, 80);
    connect(parts_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (filling_ || item->column() != 1) return;
        const core::Id id = item->data(Qt::UserRole).toString().toStdString();
        for (core::RepairPart part : snapshot_.parts) {
            if (part.id != id) continue;
            const auto cost = parseMoneyText(item->text(), snapshot_.currency);
            if (!cost) return;
            part.costMinor = *cost;
            part.costKnown = true;
            emit partChanged(part);
            return;
        }
    });
    bodyLayout->addWidget(parts_);

    auto* addRow = new QHBoxLayout();
    partName_ = new QLineEdit(body);
    partName_->setPlaceholderText(QStringLiteral("Repuesto"));
    partCost_ = new QLineEdit(body);
    partCost_->setPlaceholderText(QStringLiteral("Costo"));
    partCost_->setFixedWidth(90);
    partBought_ = new QCheckBox(QStringLiteral("lo compré ahora"), body);
    partBought_->setToolTip(QStringLiteral("Anota además el gasto, enlazado a esta reparación.\n"
                                           "Sin marcar: salió de lo que ya tenías."));
    partBought_->setChecked(true);
    auto* add = new QPushButton(QStringLiteral("Agregar"), body);
    add->setObjectName(QStringLiteral("GhostButton"));
    add->setCursor(Qt::PointingHandCursor);
    for (QWidget* w : {static_cast<QWidget*>(partName_), static_cast<QWidget*>(partCost_),
                       static_cast<QWidget*>(partBought_), static_cast<QWidget*>(add)}) {
        w->setFont(theme::bodyFont(9));
    }
    addRow->addWidget(partName_, 1);
    addRow->addWidget(partCost_);
    addRow->addWidget(partBought_);
    addRow->addWidget(add);
    bodyLayout->addLayout(addRow);
    auto addPart = [this] {
        const QString name = partName_->text().trimmed();
        if (name.isEmpty() || current_.empty()) return;
        const auto cost = parseMoneyText(partCost_->text(), snapshot_.currency);
        emit partAdded(current_, name, cost.value_or(0), cost.has_value(),
                       partBought_->isChecked() && cost.has_value());
        partName_->clear();
        partCost_->clear();
        partName_->setFocus();
    };
    connect(add, &QPushButton::clicked, this, addPart);
    connect(partCost_, &QLineEdit::returnPressed, this, addPart);
    connect(partName_, &QLineEdit::returnPressed, partCost_, qOverload<>(&QWidget::setFocus));

    // --- La cuenta ----------------------------------------------------------------
    bodyLayout->addWidget(small(QStringLiteral("CUÁNTO DEJA"), body));
    costing_ = new QLabel(body);
    costing_->setTextFormat(Qt::RichText);
    costing_->setFont(theme::bodyFont(10));
    bodyLayout->addWidget(costing_);
    warnings_ = new QLabel(body);
    warnings_->setWordWrap(true);
    warnings_->setFont(theme::bodyFont(9));
    theme::setLabelColor(warnings_, theme::kInversion);
    bodyLayout->addWidget(warnings_);

    card_->addContent(body);
    layout->addWidget(card_);
    layout->addStretch(1);
    card_->hide();
    return scroll;
}

void RepairsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    refillList();
    showRepair(current_);
}

void RepairsPage::focusList() {
    list_->setFocus();
}

void RepairsPage::selectRepair(const core::Id& jobId) {
    current_ = jobId;
    statusFilter_->setCurrentIndex(kTodas);
    refillList();
    showRepair(jobId);
}

void RepairsPage::refillList() {
    filling_ = true;
    const QString needle = search_->text().trimmed();
    const int typeIndex = typeFilter_->currentIndex();

    std::vector<const core::Repair*> rows;
    for (const core::Repair& repair : snapshot_.repairs) {
        if (!passes(repair, statusFilter_->currentIndex())) continue;
        if (typeIndex > 0 && repair.type != core::allRepairTypes()[typeIndex - 1]) continue;
        const core::Job* job = snapshot_.job(repair.jobId);
        const QString client = job != nullptr ? QString::fromStdString(job->client) : QString();
        if (!needle.isEmpty() &&
            !QString::fromStdString(repair.orderNo).contains(needle, Qt::CaseInsensitive) &&
            !QString::fromStdString(repair.device).contains(needle, Qt::CaseInsensitive) &&
            !client.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        rows.push_back(&repair);
    }

    list_->setRowCount(static_cast<int>(rows.size()));
    int selectedRow = -1;
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const core::Repair& repair = *rows[static_cast<std::size_t>(row)];
        const core::Job* job = snapshot_.job(repair.jobId);
        const auto costing = core::costRepair(repair, snapshot_.parts, snapshot_.movements,
                                              snapshot_.costs, snapshot_.currency);

        setText(list_, row, 0, QString::fromStdString(repair.orderNo), theme::kTextMuted);
        list_->item(row, 0)->setData(Qt::UserRole, QString::fromStdString(repair.jobId));
        setText(list_, row, 1, QString::fromStdString(repair.device));
        setText(list_, row, 2, job != nullptr ? QString::fromStdString(job->client) : QString(),
                theme::kTextMuted);
        setText(list_, row, 3, typeLabel(repair.type), theme::kTextMuted);
        setText(list_, row, 4, statusLabel(repair.status), statusColor(repair.status));
        setNumber(list_, row, 5, costing.price.isZero() ? QStringLiteral("—") : money(costing.price));
        if (costing.marginBps) {
            const bool low = costing.belowTarget(snapshot_.costs.targetMarginBps);
            setNumber(list_, row, 6, theme::formatBps(*costing.marginBps),
                      low ? theme::kNegative : theme::kPositive);
        } else {
            setNumber(list_, row, 6, QStringLiteral("—"), theme::kTextFaint);
        }
        if (repair.jobId == current_) selectedRow = row;
    }
    if (selectedRow >= 0) {
        list_->setCurrentCell(selectedRow, list_->currentColumn() < 0 ? 0 : list_->currentColumn());
    }
    filling_ = false;
}

void RepairsPage::showRepair(const core::Id& jobId) {
    current_ = jobId;
    const core::Repair* repair = snapshot_.repair(jobId);
    const core::Job* job = snapshot_.job(jobId);
    if (repair == nullptr || job == nullptr) {
        card_->hide();
        empty_->show();
        current_.clear();
        return;
    }
    empty_->hide();
    card_->show();
    filling_ = true;

    title_->setText(QStringLiteral("%1 · %2").arg(QString::fromStdString(repair->orderNo),
                                                 QString::fromStdString(repair->device)));
    status_->setText(statusLabel(repair->status).toUpper());
    theme::setLabelColor(status_, statusColor(repair->status));

    // Lo que vino de Cotizaciones se corrige alla: aca se ve pero no se toca.
    const bool fromQuotes = !repair->sourceRef.empty();
    locked_->setVisible(fromQuotes);
    locked_->setText(QStringLiteral("Viene de DakeLabs Cotizaciones: cliente, equipo y precio se "
                                    "corrigen allá, y la entrega y el cobro se marcan allá."));
    for (QLineEdit* edit : {client_, device_, price_}) {
        edit->setReadOnly(fromQuotes);
    }

    client_->setText(QString::fromStdString(job->client));
    device_->setText(QString::fromStdString(repair->device));
    type_->setCurrentIndex(static_cast<int>(repair->type));
    price_->setText(moneyFieldText(repair->priceMinor, snapshot_.currency));
    shipping_->setText(moneyFieldText(repair->shippingMinor, snapshot_.currency));
    consumables_->setText(moneyFieldText(repair->consumablesMinor, snapshot_.currency));
    estHours_->setText(repair->estMinutes > 0 ? hoursText(repair->estMinutes) : QString());
    realHours_->setText(repair->realMinutes ? hoursText(*repair->realMinutes) : QString());

    deliver_->setVisible(!fromQuotes && repair->status == core::RepairStatus::EnProceso);
    charge_->setVisible(!fromQuotes && repair->status != core::RepairStatus::Cobrada);
    charge_->setText(repair->status == core::RepairStatus::EnProceso
                         ? QStringLiteral("Entregar y cobrar  Ctrl+B")
                         : QStringLiteral("Cobrar  Ctrl+B"));

    // Repuestos
    const auto parts = snapshot_.partsOf(jobId);
    parts_->setRowCount(static_cast<int>(parts.size()));
    for (int row = 0; row < static_cast<int>(parts.size()); ++row) {
        const core::RepairPart& part = parts[static_cast<std::size_t>(row)];
        setText(parts_, row, 0, QString::fromStdString(part.name));
        parts_->item(row, 0)->setFlags(parts_->item(row, 0)->flags() & ~Qt::ItemIsEditable);
        auto* cost = new QTableWidgetItem(
            part.costKnown ? money(core::Money::fromMinor(part.costMinor, snapshot_.currency))
                           : QStringLiteral("¿costo?"));
        cost->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cost->setForeground(part.costKnown ? theme::kText : theme::kInversion);
        cost->setData(Qt::UserRole, QString::fromStdString(part.id));
        cost->setToolTip(QStringLiteral("Doble clic para cambiar el costo."));
        parts_->setItem(row, 1, cost);
        auto* remove = new QPushButton(QStringLiteral("Quitar"), parts_);
        remove->setObjectName(QStringLiteral("GhostButton"));
        remove->setFont(theme::bodyFont(8));
        remove->setToolTip(part.movementId.empty()
                               ? QStringLiteral("Quita el repuesto de la reparación.")
                               : QStringLiteral("Quita el repuesto y también el gasto que se "
                                                "anotó al comprarlo."));
        connect(remove, &QPushButton::clicked, this, [this, part] { emit partRemoved(part); });
        parts_->setCellWidget(row, 2, remove);
    }

    // La cuenta
    const core::RepairCosting c = core::costRepair(*repair, snapshot_.parts, snapshot_.movements,
                                                   snapshot_.costs, snapshot_.currency);
    const QString hours = hoursText(c.minutes) + QStringLiteral(" h") +
                          (c.hoursEstimated ? QStringLiteral(" estimadas") : QString());
    QString html = QStringLiteral("<table cellspacing='0'>");
    html += line(QStringLiteral("Repuestos"), money(c.parts), theme::kTextMuted);
    html += line(QStringLiteral("Consumibles"), money(c.consumables), theme::kTextMuted);
    html += line(QStringLiteral("Envío"), money(c.shipping), theme::kTextMuted);
    html += line(QStringLiteral("Costo directo"), money(c.direct), theme::kText, true);
    html += line(QStringLiteral("Tus horas (%1 × %2)")
                     .arg(hours, money(core::Money::fromMinor(snapshot_.costs.hourlyRateMinor,
                                                              snapshot_.currency))),
                 money(c.labor), theme::kTextMuted);
    html += line(QStringLiteral("Fijos del taller (%1 × %2)")
                     .arg(hours, money(core::Money::fromMinor(snapshot_.costs.fixedPerHourMinor,
                                                              snapshot_.currency))),
                 money(c.fixedShare), theme::kTextMuted);
    html += line(QStringLiteral("Costo total"), money(c.cost), theme::kText, true);
    html += line(QStringLiteral("Precio"), money(c.price), theme::kText);
    html += line(QStringLiteral("Ganancia"), money(c.profit),
                 c.profit.isNegative() ? theme::kNegative : theme::kPositive, true);
    const bool low = c.belowTarget(snapshot_.costs.targetMarginBps);
    html += line(QStringLiteral("Margen (objetivo %1)")
                     .arg(theme::formatBps(snapshot_.costs.targetMarginBps)),
                 c.marginBps ? theme::formatBps(*c.marginBps) : QStringLiteral("—"),
                 low ? theme::kNegative : theme::kPositive, true);
    const core::Money needed = core::hourlyNeeded(snapshot_.costs, snapshot_.currency);
    if (c.profitPerHour) {
        html += line(QStringLiteral("Deja por hora (necesitas %1)").arg(money(needed)),
                     money(*c.profitPerHour),
                     *c.profitPerHour < needed ? theme::kNegative : theme::kPositive);
    }
    if (c.suggestedPrice && low) {
        html += line(QStringLiteral("Precio para llegar al objetivo"), money(*c.suggestedPrice),
                     theme::kInversion, true);
    }
    html += QStringLiteral("</table>");
    costing_->setText(html);

    QStringList notes;
    if (snapshot_.costs.hourlyRateMinor == 0) {
        notes << QStringLiteral("Sin tarifa por hora: tus horas no cuestan nada todavía y el margen "
                                "sale inflado. Se configura en Ajustes.");
    }
    if (c.hoursEstimated && repair->status != core::RepairStatus::EnProceso) {
        notes << QStringLiteral("Faltan las horas reales: la cuenta usa las estimadas.");
    }
    if (c.partsIncomplete) {
        notes << QStringLiteral("Hay repuestos sin costo: la ganancia real es menor.");
    }
    warnings_->setText(notes.join(QStringLiteral("\n")));
    warnings_->setVisible(!notes.isEmpty());
    filling_ = false;
}

void RepairsPage::saveEdits() {
    if (filling_ || current_.empty()) return;
    const core::Repair* original = snapshot_.repair(current_);
    const core::Job* job = snapshot_.job(current_);
    if (original == nullptr || job == nullptr) return;

    core::Repair repair = *original;
    repair.device = device_->text().trimmed().toStdString();
    repair.type = core::allRepairTypes()[static_cast<std::size_t>(std::max(0, type_->currentIndex()))];
    repair.priceMinor = parseMoneyText(price_->text(), snapshot_.currency).value_or(0);
    repair.shippingMinor = parseMoneyText(shipping_->text(), snapshot_.currency).value_or(0);
    repair.consumablesMinor = parseMoneyText(consumables_->text(), snapshot_.currency).value_or(0);
    repair.estMinutes = parseHoursText(estHours_->text()).value_or(0);
    repair.realMinutes = parseHoursText(realHours_->text());
    const QString client = client_->text().trimmed();

    const bool changed = repair.device != original->device || repair.type != original->type ||
                         repair.priceMinor != original->priceMinor ||
                         repair.shippingMinor != original->shippingMinor ||
                         repair.consumablesMinor != original->consumablesMinor ||
                         repair.estMinutes != original->estMinutes ||
                         repair.realMinutes != original->realMinutes ||
                         client != QString::fromStdString(job->client);
    if (changed) {
        emit repairEdited(repair, client);
    }
}

} // namespace dake::ui
