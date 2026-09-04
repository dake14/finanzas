#include "quickentry.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QList>
#include <QPair>
#include <QSignalBlocker>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <stdexcept>

#include "categorybox.hpp"
#include "dake/core/money.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QDate toQDate(const core::Date& date) {
    return QDate(date.year, static_cast<int>(date.month), static_cast<int>(date.day));
}

[[nodiscard]] core::Date fromQDate(const QDate& date) {
    return core::Date::fromYmd(date.year(), static_cast<unsigned>(date.month()),
                               static_cast<unsigned>(date.day()));
}

/// Devuelve el dato seleccionado conservandolo entre recargas. Volver a llenar
/// un desplegable pierde la seleccion, y perderla en cada refresco obliga a
/// reelegir el bolsillo en cada movimiento: la friccion mas cara de todas,
/// porque aparece una vez por fila cargada.
void refill(QComboBox* box, const QList<QPair<QString, QString>>& items) {
    const QString previous = box->currentData().toString();
    QSignalBlocker blocker(box);
    box->clear();
    for (const auto& [label, id] : items) {
        box->addItem(label, id);
    }
    const int index = box->findData(previous);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

[[nodiscard]] QPushButton* chip(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setFont(theme::bodyFont(9));
    button->setFixedHeight(34);
    button->setFixedWidth(56);
    return button;
}

} // namespace

QuickEntry::QuickEntry(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void QuickEntry::buildUi() {
    grid_ = new QGridLayout(this);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(8);

    // --- Fila 1: lo minimo indispensable ----------------------------------
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("Que fue"));
    nameEdit_->setFont(theme::bodyFont(10));
    nameEdit_->setMinimumWidth(180);

    amountEdit_ = new QLineEdit(this);
    amountEdit_->setPlaceholderText(QStringLiteral("Monto"));
    amountEdit_->setFont(theme::numericFont(10));
    amountEdit_->setFixedWidth(110);
    amountEdit_->setAlignment(Qt::AlignRight);

    expenseButton_ = new QPushButton(QStringLiteral("Gasto"), this);
    incomeButton_ = new QPushButton(QStringLiteral("Ingreso"), this);
    // El tercer boton es el que hoy no existe. Sin el, "saque del ahorro para
    // comprar material" no se puede anotar de ninguna forma, y termina anotado
    // como un gasto que no explica nada.
    transferButton_ = new QPushButton(QStringLiteral("Traspaso"), this);

    for (QPushButton* button : {expenseButton_, incomeButton_, transferButton_}) {
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFont(theme::bodyFont(9, QFont::DemiBold));
        button->setFixedHeight(34);
        button->setMinimumWidth(80);
    }
    expenseButton_->setObjectName(QStringLiteral("KindExpense"));
    incomeButton_->setObjectName(QStringLiteral("KindIncome"));
    transferButton_->setObjectName(QStringLiteral("KindTransfer"));

    auto* kindGroup = new QButtonGroup(this);
    kindGroup->setExclusive(true);
    for (QPushButton* button : {expenseButton_, incomeButton_, transferButton_}) {
        kindGroup->addButton(button);
    }
    // El estado inicial va DESPUES de armar el grupo: hacerlo antes deja que el
    // grupo reacomode las marcas al incorporar los botones.
    expenseButton_->setChecked(true);

    addButton_ = new QPushButton(QStringLiteral("Agregar"), this);
    addButton_->setObjectName(QStringLiteral("PrimaryButton"));
    addButton_->setCursor(Qt::PointingHandCursor);
    addButton_->setFont(theme::bodyFont(10, QFont::DemiBold));
    addButton_->setFixedHeight(34);
    addButton_->setMinimumWidth(96);

    // Los tres botones de tipo van dentro de un contenedor propio y no en tres
    // celdas de la grilla. En la grilla, las columnas se comparten con las filas
    // de abajo: el ancho de "Traspaso" le abriria un hueco permanente a la fila
    // siguiente cada vez que el campo de destino esta oculto.
    auto* kindRow = new QWidget(this);
    auto* kindLayout = new QHBoxLayout(kindRow);
    kindLayout->setContentsMargins(0, 0, 0, 0);
    kindLayout->setSpacing(6);
    for (QPushButton* button : {expenseButton_, incomeButton_, transferButton_}) {
        kindLayout->addWidget(button);
    }

    grid_->addWidget(nameEdit_, 0, 0, 1, 3);
    grid_->addWidget(amountEdit_, 0, 3);
    grid_->addWidget(kindRow, 0, 4, 1, 3);
    grid_->addWidget(addButton_, 0, 7);

    // --- Fila 2: el contexto que hoy hay que corregir despues -------------
    todayButton_ = chip(QStringLiteral("Hoy"), this);
    yesterdayButton_ = chip(QStringLiteral("Ayer"), this);

    dateEdit_ = new QDateEdit(this);
    dateEdit_->setCalendarPopup(true);
    dateEdit_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    dateEdit_->setFont(theme::numericFont(9));
    dateEdit_->setFixedHeight(34);
    dateEdit_->setFixedWidth(112);

    pocketLabel_ = new QLabel(QStringLiteral("de"), this);
    pocketLabel_->setFont(theme::bodyFont(9));
    theme::setLabelColor(pocketLabel_, theme::kTextMuted);

    pocketBox_ = new QComboBox(this);
    pocketBox_->setFont(theme::bodyFont(9));
    pocketBox_->setFixedHeight(34);
    pocketBox_->setMinimumWidth(140);

    targetLabel_ = new QLabel(QStringLiteral("a"), this);
    targetLabel_->setFont(theme::bodyFont(9));
    theme::setLabelColor(targetLabel_, theme::kTextMuted);

    targetBox_ = new QComboBox(this);
    targetBox_->setFont(theme::bodyFont(9));
    targetBox_->setFixedHeight(34);
    targetBox_->setMinimumWidth(140);

    categoryBox_ = new CategoryBox(this);
    categoryBox_->setFont(theme::bodyFont(9));
    categoryBox_->setFixedHeight(34);
    categoryBox_->setMinimumWidth(140);
    categoryBox_->lineEdit()->setPlaceholderText(QStringLiteral("Categoria"));

    jobBox_ = new QComboBox(this);
    jobBox_->setFont(theme::bodyFont(9));
    jobBox_->setFixedHeight(34);
    jobBox_->setMinimumWidth(150);

    spreadBox_ = new QComboBox(this);
    spreadBox_->setFont(theme::bodyFont(9));
    spreadBox_->setFixedHeight(34);
    spreadBox_->setMinimumWidth(150);
    spreadBox_->addItem(QStringLiteral("se gasta este mes"), 1);
    for (int months : {2, 3, 4, 6, 12}) {
        spreadBox_->addItem(QStringLiteral("me dura %1 meses").arg(months), months);
    }
    spreadBox_->setToolTip(
        QStringLiteral("Cuatro kilos de filamento no son un gasto de este mes.\n"
                       "La plata sale hoy igual: lo que se reparte es el costo,\n"
                       "para que reponer stock no haga parecer que el mes fue malo."));

    unsettledCheck_ = new QCheckBox(QStringLiteral("todavia no se pago"), this);
    unsettledCheck_->setFont(theme::bodyFont(9));
    unsettledCheck_->setCursor(Qt::PointingHandCursor);
    unsettledCheck_->setToolTip(
        QStringLiteral("Un trabajo entregado y sin cobrar no esta en el bolsillo.\n"
                       "Cuenta para el resultado del mes, no para el saldo."));

    // Fila 1: cuando y de donde. Fila 2: de que se trata. Agrupadas asi, cada
    // fila se lee de un vistazo sin tener que saltar entre las dos.
    grid_->addWidget(todayButton_, 1, 0);
    grid_->addWidget(yesterdayButton_, 1, 1);
    grid_->addWidget(dateEdit_, 1, 2);
    grid_->addWidget(pocketLabel_, 1, 3, Qt::AlignRight);
    grid_->addWidget(pocketBox_, 1, 4);
    grid_->addWidget(targetLabel_, 1, 5, Qt::AlignRight);
    grid_->addWidget(targetBox_, 1, 6);

    grid_->addWidget(categoryBox_, 2, 0, 1, 2);
    grid_->addWidget(jobBox_, 2, 2, 1, 2);
    grid_->addWidget(spreadBox_, 2, 4);
    grid_->addWidget(unsettledCheck_, 2, 5, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);

    errorLabel_ = new QLabel(this);
    errorLabel_->setFont(theme::bodyFont(9));
    theme::setLabelColor(errorLabel_, theme::kNegative);
    errorLabel_->setVisible(false);
    grid_->addWidget(errorLabel_, 3, 0, 1, 8);

    // Solo la primera columna crece: el resto tiene ancho propio y repartir el
    // sobrante entre todas dejaria los desplegables estirados y el campo del
    // nombre —el unico que se llena con texto largo— igual de angosto.
    grid_->setColumnStretch(0, 1);

    // Enter en cualquiera de los dos campos agrega: cargar varios movimientos
    // seguidos no deberia obligar a ir al boton con el mouse.
    connect(nameEdit_, &QLineEdit::returnPressed, this, &QuickEntry::submit);
    connect(amountEdit_, &QLineEdit::returnPressed, this, &QuickEntry::submit);
    connect(addButton_, &QPushButton::clicked, this, &QuickEntry::submit);
    connect(nameEdit_, &QLineEdit::textEdited, this, &QuickEntry::clearError);
    connect(amountEdit_, &QLineEdit::textEdited, this, &QuickEntry::clearError);

    connect(todayButton_, &QPushButton::clicked, this,
            [this] { dateEdit_->setDate(toQDate(snapshot_.today)); });
    connect(yesterdayButton_, &QPushButton::clicked, this,
            [this] { dateEdit_->setDate(toQDate(snapshot_.today.addDays(-1))); });

    for (QPushButton* button : {expenseButton_, incomeButton_, transferButton_}) {
        connect(button, &QPushButton::toggled, this, [this](bool checked) {
            if (checked) {
                applyKind();
            }
        });
    }

    applyKind();
}

core::MovementKind QuickEntry::currentKind() const {
    if (incomeButton_->isChecked()) {
        return core::MovementKind::Ingreso;
    }
    if (transferButton_->isChecked()) {
        return core::MovementKind::Traspaso;
    }
    return core::MovementKind::Gasto;
}

void QuickEntry::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;

    QList<QPair<QString, QString>> pocketItems;
    for (const core::Pocket& pocket : snapshot_.pockets) {
        if (pocket.archived || pocket.deleted) {
            continue;
        }
        pocketItems.append({QString::fromStdString(pocket.name),
                            QString::fromStdString(pocket.id)});
    }
    refill(pocketBox_, pocketItems);
    refill(targetBox_, pocketItems);

    QList<QPair<QString, QString>> jobItems{{QStringLiteral("— sin trabajo —"), QString()}};
    for (const core::Job& job : snapshot_.jobs) {
        if (job.deleted || job.closed) {
            continue;
        }
        QString label = QString::fromStdString(job.name);
        if (!job.client.empty()) {
            label += QStringLiteral(" · ") + QString::fromStdString(job.client);
        }
        jobItems.append({label, QString::fromStdString(job.id)});
    }
    refill(jobBox_, jobItems);

    // El rango se fija ANTES de la fecha. Al reves, setDateRange recorta la
    // fecha recien puesta contra el minimo y el campo termina mostrando el
    // limite inferior del rango en vez de hoy.
    //
    // El rango existe porque sin limites un tipeo pone el movimiento en 1752 y
    // desaparece de todos los reportes sin que nadie sepa donde fue.
    dateEdit_->setDateRange(toQDate(snapshot_.today.addMonths(-24)),
                            toQDate(snapshot_.today.addMonths(12)));
    if (!dateInitialized_) {
        dateEdit_->setDate(toQDate(snapshot_.today));
        dateInitialized_ = true;
    }

    applyKind();
}

void QuickEntry::applyKind() {
    const core::MovementKind kind = currentKind();
    const bool transfer = kind == core::MovementKind::Traspaso;
    const bool expense = kind == core::MovementKind::Gasto;

    pocketLabel_->setText(kind == core::MovementKind::Ingreso ? QStringLiteral("entra a")
                                                              : QStringLiteral("sale de"));
    targetLabel_->setVisible(transfer);
    targetBox_->setVisible(transfer);

    categoryBox_->setVisible(!transfer);
    jobBox_->setVisible(!transfer);
    spreadBox_->setVisible(expense);
    unsettledCheck_->setVisible(!transfer);
    unsettledCheck_->setText(expense ? QStringLiteral("todavia no lo pague")
                                     : QStringLiteral("todavia no me lo pagaron"));

    if (!transfer) {
        categoryBox_->setSuggestions(snapshot_.categoriesFor(kind));
    }
}

void QuickEntry::showError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(true);
}

void QuickEntry::clearError() {
    if (errorLabel_->isVisible()) {
        errorLabel_->setVisible(false);
    }
}

void QuickEntry::submit() {
    const QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        showError(QStringLiteral("Falta decir que fue."));
        nameEdit_->setFocus();
        return;
    }

    const QString amountText = amountEdit_->text().trimmed();
    if (amountText.isEmpty()) {
        showError(QStringLiteral("Falta el monto."));
        amountEdit_->setFocus();
        return;
    }

    // El parseo lo hace el nucleo, que es donde vive la regla de que un monto
    // nunca pasa por punto flotante.
    core::Money amount;
    try {
        amount = core::Money::parse(amountText.toStdString(), snapshot_.currency);
    } catch (const std::out_of_range&) {
        showError(QStringLiteral("Ese monto es demasiado grande."));
        amountEdit_->setFocus();
        return;
    } catch (const std::invalid_argument&) {
        showError(QStringLiteral("Monto invalido. Ejemplos: 46  ·  46,00  ·  1.250,50"));
        amountEdit_->setFocus();
        return;
    }
    if (amount.isNegative() || amount.isZero()) {
        showError(QStringLiteral("El monto va en positivo; el signo lo pone el tipo."));
        amountEdit_->setFocus();
        return;
    }

    if (pocketBox_->currentData().toString().isEmpty()) {
        showError(QStringLiteral("Primero crea un bolsillo: sin el no se sabe de donde "
                                 "salio la plata."));
        return;
    }

    const core::MovementKind kind = currentKind();

    core::Movement draft;
    draft.date = fromQDate(dateEdit_->date());
    draft.name = name.toStdString();
    draft.kind = kind;
    draft.amountMinor = amount.minor();
    draft.pocketId = pocketBox_->currentData().toString().toStdString();

    if (kind == core::MovementKind::Traspaso) {
        if (targetBox_->currentData().toString() == pocketBox_->currentData().toString()) {
            showError(QStringLiteral("Un traspaso necesita dos bolsillos distintos."));
            return;
        }
        draft.targetPocketId = targetBox_->currentData().toString().toStdString();
        draft.category = std::string(core::kUncategorized);
    } else {
        const QString category = categoryBox_->category().trimmed();
        draft.category = category.isEmpty() ? std::string(core::kUncategorized)
                                            : category.toStdString();
        draft.jobId = jobBox_->currentData().toString().toStdString();
        draft.settled = !unsettledCheck_->isChecked();
        if (kind == core::MovementKind::Gasto) {
            draft.spreadMonths = spreadBox_->currentData().toInt();
        }
    }

    clearError();
    emit submitted(draft);

    // Se limpian nombre y monto; el resto se conserva. Cargar cinco compras de
    // insumos seguidas no deberia costar cinco veces elegir el mismo bolsillo.
    nameEdit_->clear();
    amountEdit_->clear();
    nameEdit_->setFocus();
}

void QuickEntry::focusName() {
    nameEdit_->setFocus(Qt::ShortcutFocusReason);
    nameEdit_->selectAll();
}

} // namespace dake::ui
