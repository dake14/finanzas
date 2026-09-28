#include "entryform.hpp"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "categorybox.hpp"
#include "dake/core/capture.hpp"
#include "fields.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

/// El ultimo item de la lista de categorias: vacia el campo para escribir una.
const QString kNewCategory = QStringLiteral("+ Nueva…");

[[nodiscard]] QDate toQDate(const core::Date& date) {
    return QDate(date.year, static_cast<int>(date.month), static_cast<int>(date.day));
}

[[nodiscard]] core::Date fromQDate(const QDate& date) {
    return core::Date::fromYmd(date.year(), static_cast<unsigned>(date.month()),
                               static_cast<unsigned>(date.day()));
}

[[nodiscard]] QLabel* fieldLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::bodyFont(8));
    theme::setLabelColor(label, theme::kTextFaint);
    return label;
}

[[nodiscard]] QPushButton* kindButton(const QString& text, const QString& name, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(name);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFont(theme::bodyFont(9, QFont::DemiBold));
    button->setFixedHeight(30);
    button->setMinimumWidth(92);
    button->setProperty("entryKind", true);  // la hoja marca el elegido
    return button;
}

[[nodiscard]] core::MovementKind movementKind(EntryForm::Kind kind) {
    switch (kind) {
        case EntryForm::Kind::Gasto: return core::MovementKind::Gasto;
        case EntryForm::Kind::Ingreso: return core::MovementKind::Ingreso;
        case EntryForm::Kind::Traspaso: return core::MovementKind::Traspaso;
    }
    return core::MovementKind::Gasto;
}

} // namespace

EntryForm::EntryForm(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void EntryForm::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // --- Tipo ---------------------------------------------------------------
    // Con mnemonico: Alt+G, Alt+I, Alt+T. Ctrl+numero ya cambia de seccion.
    auto* kinds = new QHBoxLayout();
    kinds->setSpacing(6);
    gasto_ = kindButton(QStringLiteral("&Gasto"), QStringLiteral("EntryGasto"), this);
    ingreso_ = kindButton(QStringLiteral("&Ingreso"), QStringLiteral("EntryIngreso"), this);
    traspaso_ = kindButton(QStringLiteral("&Traspaso"), QStringLiteral("EntryTraspaso"), this);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (QPushButton* b : {gasto_, ingreso_, traspaso_}) {
        group->addButton(b);
        kinds->addWidget(b);
    }
    kinds->addStretch(1);
    gasto_->setChecked(true);
    connect(gasto_, &QPushButton::clicked, this, [this] { setKind(Kind::Gasto); });
    connect(ingreso_, &QPushButton::clicked, this, [this] { setKind(Kind::Ingreso); });
    connect(traspaso_, &QPushButton::clicked, this, [this] { setKind(Kind::Traspaso); });
    layout->addLayout(kinds);

    // --- Campos -------------------------------------------------------------
    auto* grid = new QGridLayout();
    grid_ = grid;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(2);

    amount_ = new QLineEdit(this);
    amount_->setObjectName(QStringLiteral("EntryAmount"));
    amount_->setPlaceholderText(QStringLiteral("0,00"));
    amount_->setFont(theme::figureFont(16));
    amount_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    amount_->setMinimumWidth(130);

    category_ = new CategoryBox(this);
    category_->setObjectName(QStringLiteral("EntryCategory"));
    category_->setMinimumWidth(190);
    categoryLabel_ = fieldLabel(QStringLiteral("CATEGORÍA"), this);

    pocket_ = new QComboBox(this);
    pocket_->setObjectName(QStringLiteral("EntryPocket"));
    pocket_->setMinimumWidth(170);
    pocketLabel_ = fieldLabel(QStringLiteral("SALE DE"), this);

    target_ = new QComboBox(this);
    target_->setObjectName(QStringLiteral("EntryTarget"));
    target_->setMinimumWidth(170);
    targetLabel_ = fieldLabel(QStringLiteral("A"), this);

    date_ = new QDateEdit(this);
    date_->setObjectName(QStringLiteral("EntryDate"));
    date_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    date_->setCalendarPopup(true);

    submit_ = new QPushButton(QStringLiteral("Anotar"), this);
    submit_->setObjectName(QStringLiteral("PrimaryButton"));
    submit_->setCursor(Qt::PointingHandCursor);
    submit_->setFont(theme::bodyFont(10, QFont::DemiBold));
    submit_->setFixedHeight(34);
    connect(submit_, &QPushButton::clicked, this, [this] { submit(false); });

    for (QWidget* field : {static_cast<QWidget*>(category_), static_cast<QWidget*>(pocket_),
                           static_cast<QWidget*>(target_), static_cast<QWidget*>(date_)}) {
        field->setFont(theme::bodyFont(10));
    }

    grid->addWidget(fieldLabel(QStringLiteral("MONTO"), this), 0, 0);
    grid->addWidget(categoryLabel_, 0, 1);
    grid->addWidget(pocketLabel_, 0, 2);
    grid->addWidget(targetLabel_, 0, 3);
    grid->addWidget(fieldLabel(QStringLiteral("FECHA"), this), 0, 4);
    grid->addWidget(amount_, 1, 0);
    grid->addWidget(category_, 1, 1);
    grid->addWidget(pocket_, 1, 2);
    grid->addWidget(target_, 1, 3);
    grid->addWidget(date_, 1, 4);
    grid->addWidget(submit_, 1, 5);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 2);
    grid->setColumnStretch(3, 2);
    layout->addLayout(grid);

    auto* status = new QHBoxLayout();
    error_ = new QLabel(this);
    error_->setObjectName(QStringLiteral("EntryError"));
    error_->setFont(theme::bodyFont(9, QFont::DemiBold));
    theme::setLabelColor(error_, theme::kNegative);
    done_ = new QLabel(this);
    done_->setObjectName(QStringLiteral("EntryDone"));
    done_->setFont(theme::bodyFont(9));
    theme::setLabelColor(done_, theme::kPositive);
    status->addWidget(error_);
    status->addWidget(done_);
    status->addStretch(1);
    layout->addLayout(status);

    setTabOrder(amount_, category_);
    setTabOrder(category_, pocket_);
    setTabOrder(pocket_, target_);
    setTabOrder(target_, date_);
    setTabOrder(date_, submit_);

    for (QWidget* w : {static_cast<QWidget*>(amount_), static_cast<QWidget*>(category_),
                       static_cast<QWidget*>(category_->lineEdit()), static_cast<QWidget*>(pocket_),
                       static_cast<QWidget*>(target_), static_cast<QWidget*>(date_),
                       static_cast<QWidget*>(submit_), static_cast<QWidget*>(gasto_),
                       static_cast<QWidget*>(ingreso_), static_cast<QWidget*>(traspaso_)}) {
        w->installEventFilter(this);
    }

    // "+ Nueva…" no es una categoria: vacia el campo para escribirla.
    connect(category_, &QComboBox::activated, this, [this](int index) {
        if (category_->itemText(index) == kNewCategory) {
            category_->setCategory(QString());
            category_->setFocus();
        }
    });
    connect(amount_, &QLineEdit::textEdited, this, [this] {
        error_->clear();
        done_->clear();
    });

    applyKind();
}

void EntryForm::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    // La fecha sigue a hoy mientras nadie la cambie: arranca en hoy y, si
    // cambia el dia con la aplicacion abierta, se mueve con el. Una fecha
    // elegida a mano se respeta.
    const QDate today = toQDate(snapshot.today);
    const bool followsToday = !shownToday_.isValid() || date_->date() == shownToday_;
    date_->setMaximumDate(today);
    if (followsToday || date_->date() > today) {
        date_->setDate(today);
    }
    shownToday_ = today;
    fillPockets();
    refreshCategories();
}

void EntryForm::fillPockets() {
    for (QComboBox* box : {pocket_, target_}) {
        const QString previous = box->currentData().toString();
        const QSignalBlocker blocker(box);
        box->clear();
        for (const core::Pocket& p : snapshot_.pockets) {
            if (p.archived) {
                continue;
            }
            const QString account = core::accountOf(p) == core::Account::Personal
                                        ? QStringLiteral("personal")
                                        : QStringLiteral("negocio");
            box->addItem(QString::fromStdString(p.name) + QStringLiteral(" · ") + account,
                         QString::fromStdString(p.id));
        }
        box->setCurrentIndex(box->findData(previous));
    }
    // Lo que ya estaba elegido se respeta; lo que no, toma el de siempre.
    if (pocket_->currentIndex() < 0 || target_->currentIndex() < 0) {
        selectDefaultPockets();
    }
}

void EntryForm::selectDefaultPockets() {
    auto pick = [](QComboBox* box, const QString& id) {
        const int index = box->findData(id);
        if (index >= 0) {
            box->setCurrentIndex(index);
            return true;
        }
        return false;
    };
    auto suggested = [this](std::optional<core::Account> account) {
        return QString::fromStdString(
            core::suggestedPocket(snapshot_.pockets, snapshot_.movements, account));
    };
    switch (kind_) {
        case Kind::Gasto:
            if (!pick(pocket_, snapshot_.lastExpensePocket)) pick(pocket_, suggested(std::nullopt));
            break;
        case Kind::Ingreso:
            if (!pick(pocket_, snapshot_.lastIncomePocket)) {
                pick(pocket_, suggested(core::Account::Negocio));
            }
            break;
        case Kind::Traspaso:
            if (!pick(pocket_, snapshot_.lastTransferFrom)) {
                pick(pocket_, suggested(core::Account::Negocio));
            }
            if (!pick(target_, snapshot_.lastTransferTo)) {
                pick(target_, QString::fromStdString(snapshot_.personalPocket()));
            }
            break;
    }
    if (pocket_->currentIndex() < 0 && pocket_->count() > 0) pocket_->setCurrentIndex(0);
    if (target_->currentIndex() < 0 && target_->count() > 0) {
        target_->setCurrentIndex(std::min(1, target_->count() - 1));
    }
}

void EntryForm::refreshCategories() {
    QStringList names;
    if (kind_ != Kind::Traspaso) {
        for (const std::string& name :
             core::categoriesByUse(snapshot_.categories, snapshot_.movements, movementKind(kind_),
                                   snapshot_.today)) {
            if (name == core::kUncategorized || name == core::kAdjustment) {
                continue;
            }
            names << QString::fromStdString(name);
        }
    }
    category_->setSuggestions(names);
    {
        const QSignalBlocker blocker(category_);
        const QString current = category_->currentText();
        category_->addItem(kNewCategory);
        category_->setCurrentText(current);
    }
}

void EntryForm::setKind(Kind kind) {
    const bool changed = kind != kind_;
    kind_ = kind;
    gasto_->setChecked(kind == Kind::Gasto);
    ingreso_->setChecked(kind == Kind::Ingreso);
    traspaso_->setChecked(kind == Kind::Traspaso);
    if (changed) {
        category_->setCategory(QString());
        selectDefaultPockets();
    }
    applyKind();
    refreshCategories();
    error_->clear();
}

void EntryForm::applyKind() {
    const bool transfer = kind_ == Kind::Traspaso;
    categoryLabel_->setVisible(!transfer);
    category_->setVisible(!transfer);
    targetLabel_->setVisible(transfer);
    target_->setVisible(transfer);
    // Una columna escondida no puede quedarse con su parte del ancho.
    grid_->setColumnStretch(1, transfer ? 0 : 2);
    grid_->setColumnStretch(3, transfer ? 2 : 0);
    pocketLabel_->setText(transfer                 ? QStringLiteral("DE")
                          : kind_ == Kind::Ingreso ? QStringLiteral("ENTRA A")
                                                   : QStringLiteral("SALE DE"));
    theme::setLabelColor(amount_, kind_ == Kind::Ingreso  ? theme::kPositive
                                  : kind_ == Kind::Gasto ? theme::kNegative
                                                         : theme::kAccent);
}

void EntryForm::focusAmount() {
    amount_->setFocus();
    amount_->selectAll();
}

void EntryForm::showError(const QString& text) {
    done_->clear();
    error_->setText(text);
}

void EntryForm::submit(bool keepOpen) {
    const auto amount = parseMoneyText(amount_->text(), snapshot_.currency);
    if (!amount) {
        showError(QStringLiteral("El monto tiene que ser mayor que cero"));
        amount_->setFocus();
        return;
    }
    const QString categoryName = category_->category();
    if (kind_ != Kind::Traspaso && (categoryName.isEmpty() || categoryName == kNewCategory)) {
        showError(QStringLiteral("Falta la categoría"));
        category_->setFocus();
        return;
    }
    const QString from = pocket_->currentData().toString();
    const QString to = target_->currentData().toString();
    if (kind_ == Kind::Traspaso && !from.isEmpty() && from == to) {
        showError(QStringLiteral("De y A tienen que ser distintos"));
        target_->setFocus();
        return;
    }
    if (date_->date() > toQDate(snapshot_.today)) {
        showError(QStringLiteral("La fecha no puede ser futura"));
        date_->setFocus();
        return;
    }
    if (from.isEmpty() || (kind_ == Kind::Traspaso && to.isEmpty())) {
        showError(QStringLiteral("Falta un bolsillo"));
        pocket_->setFocus();
        return;
    }

    core::Movement movement;
    movement.kind = movementKind(kind_);
    movement.amountMinor = *amount;
    movement.date = fromQDate(date_->date());
    movement.pocketId = from.toStdString();
    movement.settled = true;
    movement.spreadMonths = 1;

    core::Category newCategory;
    if (kind_ == Kind::Traspaso) {
        movement.targetPocketId = to.toStdString();
        const core::Pocket* source = snapshot_.pocket(movement.pocketId);
        const core::Pocket* target = snapshot_.pocket(movement.targetPocketId);
        const bool salary = source != nullptr && target != nullptr &&
                            core::accountOf(*source) == core::Account::Negocio &&
                            core::accountOf(*target) == core::Account::Personal;
        movement.name = salary ? "Sueldo" : "Traspaso";
    } else {
        // Sin descripcion aparte: el nombre es la categoria, que es lo que se
        // lee en Movimientos.
        movement.category = categoryName.toStdString();
        movement.name = movement.category;
        if (core::findCategory(snapshot_.categories, movement.category) == nullptr) {
            newCategory.name = movement.category;
            newCategory.account = snapshot_.categoryAccount(movement.category, movement.pocketId);
            newCategory.cls = core::CategoryClass::General;
            newCategory.kind = movement.kind;
        }
    }

    if (!movement.isWellFormed()) {
        showError(QStringLiteral("Falta un bolsillo"));
        return;
    }
    error_->clear();
    emit submitted(movement, newCategory, keepOpen);
}

void EntryForm::confirmSaved(const QString& summary) {
    amount_->clear();
    error_->clear();
    done_->setText(QStringLiteral("✓ Anotado: ") + summary);
    amount_->setFocus();
}

bool EntryForm::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            // Con el completador abierto, Enter elige la sugerencia y no guarda:
            // guardar con la categoria a medio escribir es justo el error que el
            // completador existe para evitar. Vale para el campo y para el
            // combo: el Enter que el campo no usa sube al combo que lo contiene.
            QCompleter* completer = category_->lineEdit()->completer();
            if ((watched == category_->lineEdit() || watched == category_) && completer != nullptr &&
                completer->popup()->isVisible()) {
                return false;
            }
            submit(key->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            emit cancelled();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace dake::ui
