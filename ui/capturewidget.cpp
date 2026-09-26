#include "capturewidget.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "categorybox.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

enum KindIndex { kGasto = 0, kIngreso = 1, kSueldo = 2 };

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

[[nodiscard]] QString sourceText(core::CategorySource source) {
    switch (source) {
        case core::CategorySource::Historial: return QStringLiteral("aprendida");
        case core::CategorySource::Nombre: return QStringLiteral("por el nombre");
        case core::CategorySource::Reparacion: return QStringLiteral("por la reparación");
        case core::CategorySource::Ninguna: return QStringLiteral("por completar");
    }
    return {};
}

} // namespace

CaptureWidget::CaptureWidget(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void CaptureWidget::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* top = new QHBoxLayout();
    top->setSpacing(12);
    input_ = new QLineEdit(this);
    input_->setObjectName(QStringLiteral("CaptureInput"));
    input_->setPlaceholderText(QStringLiteral("25 almuerzo  ·  120 cobro GPU 3080  ·  40 luz ayer"));
    input_->setFont(theme::bodyFont(12));
    input_->setMinimumWidth(320);
    input_->setClearButtonEnabled(true);
    amount_ = new QLabel(QStringLiteral("—"), this);
    amount_->setFont(theme::numericFont(16, QFont::Bold));
    amount_->setMinimumWidth(120);
    amount_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    theme::setLabelColor(amount_, theme::kTextFaint);
    top->addWidget(input_, 1);
    top->addWidget(amount_);
    layout->addLayout(top);

    // La vista previa. Cada campo es editable y Tab los recorre en orden.
    auto* grid = new QGridLayout();
    grid_ = grid;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(2);

    kind_ = new QComboBox(this);
    kind_->addItems({QStringLiteral("Gasto"), QStringLiteral("Ingreso"), QStringLiteral("Sueldo")});
    kind_->setToolTip(QStringLiteral("Sueldo es pasar plata del negocio a lo personal."));

    category_ = new CategoryBox(this);
    category_->setMinimumWidth(170);

    account_ = new QPushButton(this);
    account_->setObjectName(QStringLiteral("GhostButton"));
    account_->setCursor(Qt::PointingHandCursor);
    account_->setFont(theme::bodyFont(8));
    account_->setToolTip(QStringLiteral("La categoría es nueva: ¿es del negocio o tuya?\n"
                                        "Se pregunta una sola vez."));

    pocket_ = new QComboBox(this);
    pocket_->setMinimumWidth(150);

    date_ = new QDateEdit(this);
    date_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    date_->setCalendarPopup(true);

    job_ = new QComboBox(this);
    job_->setObjectName(QStringLiteral("CaptureJob"));
    job_->setMinimumWidth(170);

    for (QWidget* field : {static_cast<QWidget*>(kind_), static_cast<QWidget*>(category_),
                           static_cast<QWidget*>(pocket_), static_cast<QWidget*>(date_),
                           static_cast<QWidget*>(job_)}) {
        field->setFont(theme::bodyFont(10));
    }
    kind_->setMinimumWidth(100);

    source_ = new QLabel(this);
    source_->setFont(theme::bodyFont(8));
    theme::setLabelColor(source_, theme::kTextFaint);

    grid->addWidget(fieldLabel(QStringLiteral("TIPO"), this), 0, 0);
    grid->addWidget(fieldLabel(QStringLiteral("CATEGORÍA"), this), 0, 1);
    grid->addWidget(source_, 0, 2);
    grid->addWidget(fieldLabel(QStringLiteral("BOLSILLO"), this), 0, 3);
    grid->addWidget(fieldLabel(QStringLiteral("FECHA"), this), 0, 4);
    jobLabel_ = fieldLabel(QStringLiteral("REPARACIÓN"), this);
    grid->addWidget(jobLabel_, 0, 5);
    grid->addWidget(kind_, 1, 0);
    grid->addWidget(category_, 1, 1);
    grid->addWidget(account_, 1, 2);
    grid->addWidget(pocket_, 1, 3);
    grid->addWidget(date_, 1, 4);
    grid->addWidget(job_, 1, 5);
    grid->setColumnStretch(1, 2);
    layout->addLayout(grid);
    updateJobField();

    hint_ = new QLabel(this);
    hint_->setFont(theme::bodyFont(8));
    layout->addWidget(hint_);
    showHint(QString(), false);

    setTabOrder(input_, kind_);
    setTabOrder(kind_, category_);
    setTabOrder(category_, account_);
    setTabOrder(account_, pocket_);
    setTabOrder(pocket_, date_);
    setTabOrder(date_, job_);

    for (QWidget* field : {static_cast<QWidget*>(input_), static_cast<QWidget*>(kind_),
                           static_cast<QWidget*>(category_), static_cast<QWidget*>(account_),
                           static_cast<QWidget*>(pocket_), static_cast<QWidget*>(date_),
                           static_cast<QWidget*>(job_)}) {
        field->installEventFilter(this);
    }
    category_->lineEdit()->installEventFilter(this);
    date_->installEventFilter(this);

    connect(input_, &QLineEdit::textEdited, this, [this] {
        if (!clock_.isValid()) {
            clock_.start();
        }
        reparse();
    });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_) manualKind_ = true;
        refreshSuggestions();
        updateAccountButton();
        updateJobField();
    });
    connect(category_, &QComboBox::currentTextChanged, this, [this] {
        if (filling_) {
            updateAccountButton();
            return;
        }
        manualCategory_ = true;
        source_->setText(QStringLiteral("a mano"));
        // La categoria decide la cuenta, y la cuenta el bolsillo: si el
        // bolsillo no se toco a mano, sigue a la categoria.
        if (!manualPocket_) {
            const QSignalBlocker blocker(pocket_);
            const core::Id suggested = core::suggestedPocket(context_, currentCategoryAccount());
            pocket_->setCurrentIndex(std::max(0, pocket_->findData(QString::fromStdString(suggested))));
        }
        updateAccountButton();
    });
    connect(account_, &QPushButton::clicked, this, [this] {
        manualAccount_ = true;
        newCategoryAccount_ = newCategoryAccount_ == core::Account::Personal
                                  ? core::Account::Negocio
                                  : core::Account::Personal;
        if (!manualPocket_) {
            const QSignalBlocker blocker(pocket_);
            const core::Id suggested = core::suggestedPocket(context_, newCategoryAccount_);
            pocket_->setCurrentIndex(std::max(0, pocket_->findData(QString::fromStdString(suggested))));
        }
        updateAccountButton();
    });
    connect(pocket_, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_) manualPocket_ = true;
        updateAccountButton();
    });
    connect(date_, &QDateEdit::dateChanged, this, [this] {
        if (!filling_) manualDate_ = true;
    });
    connect(job_, &QComboBox::currentIndexChanged, this, [this] {
        if (!filling_) manualJob_ = true;
    });
}

void CaptureWidget::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;

    context_ = core::CaptureContext{};
    context_.today = snapshot.today;
    context_.currency = snapshot.currency;
    context_.pockets = snapshot.pockets;
    context_.categories = snapshot.categories;
    context_.history = snapshot.movements;
    context_.openRepairs = snapshot.openRepairRefs();

    filling_ = true;
    fillPockets();
    {
        const QString previous = job_->currentData().toString();
        const QSignalBlocker blocker(job_);
        job_->clear();
        job_->addItem(QStringLiteral("Sin reparación"), QString());
        for (const core::RepairRef& repair : context_.openRepairs) {
            QString label = QString::fromStdString(repair.device);
            if (!repair.orderNo.empty()) {
                label = QString::fromStdString(repair.orderNo) + QStringLiteral(" · ") + label;
            }
            if (!repair.client.empty()) {
                label += QStringLiteral(" · ") + QString::fromStdString(repair.client);
            }
            job_->addItem(label, QString::fromStdString(repair.jobId));
        }
        job_->setCurrentIndex(std::max(0, job_->findData(previous)));
    }
    refreshSuggestions();
    filling_ = false;
    reparse();
}

void CaptureWidget::refreshSuggestions() {
    const bool salary = kind_->currentIndex() == kSueldo;
    category_->setEnabled(!salary);
    const auto kind = kind_->currentIndex() == kIngreso ? core::MovementKind::Ingreso
                                                        : core::MovementKind::Gasto;
    QStringList names;
    if (!salary) {
        for (const std::string& name :
             core::categoriesByUse(snapshot_.categories, snapshot_.movements, kind, snapshot_.today)) {
            names << QString::fromStdString(name);
        }
    }
    const bool wasFilling = filling_;
    filling_ = true;
    category_->setSuggestions(names);
    filling_ = wasFilling;
}

void CaptureWidget::fillPockets() {
    const QString previous = pocket_->currentData().toString();
    const QSignalBlocker blocker(pocket_);
    pocket_->clear();
    for (const core::Pocket& pocket : snapshot_.pockets) {
        if (pocket.archived) {
            continue;
        }
        const QString account = core::accountOf(pocket) == core::Account::Personal
                                    ? QStringLiteral("personal")
                                    : QStringLiteral("negocio");
        pocket_->addItem(QString::fromStdString(pocket.name) + QStringLiteral(" · ") + account,
                         QString::fromStdString(pocket.id));
    }
    pocket_->setCurrentIndex(std::max(0, pocket_->findData(previous)));
}

void CaptureWidget::reparse() {
    draft_ = core::parseCapture(input_->text().toStdString(), context_);

    filling_ = true;
    if (draft_.amountMinor) {
        amount_->setText(
            theme::formatMoney(core::Money::fromMinor(*draft_.amountMinor, snapshot_.currency)));
    } else {
        amount_->setText(QStringLiteral("—"));
    }

    if (!manualKind_) {
        const int index = draft_.salary ? kSueldo
                          : draft_.kind == core::MovementKind::Ingreso ? kIngreso
                                                                       : kGasto;
        if (kind_->currentIndex() != index) {
            kind_->setCurrentIndex(index);
        }
    }
    if (!manualCategory_) {
        category_->setCategory(QString::fromStdString(draft_.category));
        source_->setText(input_->text().trimmed().isEmpty() ? QString()
                                                            : sourceText(draft_.categorySource));
    }
    if (!manualPocket_ && !draft_.pocketId.empty()) {
        pocket_->setCurrentIndex(
            std::max(0, pocket_->findData(QString::fromStdString(draft_.pocketId))));
    }
    if (!manualDate_) {
        date_->setDate(toQDate(draft_.date));
    }
    if (!manualJob_) {
        job_->setCurrentIndex(std::max(0, job_->findData(QString::fromStdString(draft_.jobId))));
    }
    if (!manualAccount_) {
        const core::Pocket* pocket =
            snapshot_.pocket(pocket_->currentData().toString().toStdString());
        newCategoryAccount_ = pocket == nullptr ? core::Account::Negocio : core::accountOf(*pocket);
    }
    filling_ = false;

    const QColor amountColor = !draft_.amountMinor ? theme::kTextFaint
                               : kind_->currentIndex() == kIngreso ? theme::kPositive
                               : kind_->currentIndex() == kSueldo  ? theme::kAccent
                                                                   : theme::kNegative;
    theme::setLabelColor(amount_, amountColor);
    updateAccountButton();

    if (input_->text().trimmed().isEmpty()) {
        showHint(QString(), false);
    } else if (!draft_.amountMinor) {
        showHint(QStringLiteral("Falta el monto: un número al principio o al final."), false);
    } else {
        showHint(QString(), false);
    }
}

bool CaptureWidget::isNewCategory() const {
    const QString name = category_->category();
    return kind_->currentIndex() != kSueldo && !name.isEmpty() &&
           core::findCategory(snapshot_.categories, name.toStdString()) == nullptr;
}

core::Account CaptureWidget::currentCategoryAccount() const {
    if (const core::Category* category =
            core::findCategory(snapshot_.categories, category_->category().toStdString())) {
        return category->account;
    }
    return newCategoryAccount_;
}

void CaptureWidget::updateAccountButton() {
    const bool fresh = isNewCategory();
    account_->setVisible(fresh);
    account_->setText(newCategoryAccount_ == core::Account::Personal
                          ? QStringLiteral("nueva · personal")
                          : QStringLiteral("nueva · negocio"));
}

void CaptureWidget::updateJobField() {
    const bool income = kind_->currentIndex() == kIngreso;
    jobLabel_->setVisible(income);
    job_->setVisible(income);
    grid_->setColumnStretch(5, income ? 2 : 0);
}

void CaptureWidget::showHint(const QString& text, bool error) {
    if (text.isEmpty()) {
        hint_->setText(QStringLiteral("Enter guardar  ·  Shift+Enter guardar y otro  ·  "
                                      "Tab corregir un campo  ·  Esc cerrar"));
        theme::setLabelColor(hint_, theme::kTextFaint);
        return;
    }
    hint_->setText(text);
    theme::setLabelColor(hint_, error ? theme::kNegative : theme::kInversion);
}

void CaptureWidget::submit(bool keepOpen) {
    if (!draft_.amountMinor) {
        showHint(QStringLiteral("Falta el monto: un número al principio o al final."), true);
        input_->setFocus();
        return;
    }

    core::Movement movement = draft_.toMovement();
    movement.date = fromQDate(date_->date());
    movement.pocketId = pocket_->currentData().toString().toStdString();
    movement.jobId = job_->currentData().toString().toStdString();

    core::Category newCategory;
    if (kind_->currentIndex() == kSueldo) {
        movement.kind = core::MovementKind::Traspaso;
        movement.category.clear();
        movement.jobId.clear();
        movement.targetPocketId = snapshot_.personalPocket();
        if (movement.name == "Sin descripcion") movement.name = "Sueldo";
    } else {
        movement.kind = kind_->currentIndex() == kIngreso ? core::MovementKind::Ingreso
                                                          : core::MovementKind::Gasto;
        movement.targetPocketId.clear();
        if (movement.kind != core::MovementKind::Ingreso) movement.jobId.clear();
        movement.category = category_->category().toStdString();
        if (isNewCategory()) {
            newCategory.name = movement.category;
            newCategory.account = newCategoryAccount_;
            newCategory.kind = movement.kind;
        }
    }

    // Una reparacion que vino de Cotizaciones se cobra alla: anotar el cobro
    // aca crearia un segundo ingreso por lo mismo.
    if (movement.kind == core::MovementKind::Ingreso && !movement.jobId.empty()) {
        const core::Repair* repair = snapshot_.repair(movement.jobId);
        if (repair != nullptr && !repair->sourceRef.empty()) {
            showHint(QStringLiteral("%1 se cobra en DakeLabs Cotizaciones: márcalo pagado allá y "
                                    "aparece acá solo.")
                         .arg(QString::fromStdString(repair->orderNo)),
                     true);
            return;
        }
    }

    if (!movement.isWellFormed()) {
        showHint(movement.kind == core::MovementKind::Traspaso
                     ? QStringLiteral("Para anotar sueldo hace falta un bolsillo personal distinto "
                                      "del de origen.")
                     : QStringLiteral("Falta un bolsillo."),
                 true);
        return;
    }

    const qint64 elapsed = clock_.isValid() ? clock_.elapsed() : 0;
    emit submitted(movement, newCategory, elapsed, keepOpen);
    reset();
    if (keepOpen) {
        startClock();
    }
}

void CaptureWidget::reset() {
    input_->clear();
    manualKind_ = manualCategory_ = manualPocket_ = manualDate_ = manualJob_ = manualAccount_ =
        false;
    clock_.invalidate();
    reparse();
    input_->setFocus();
}

void CaptureWidget::startClock() {
    clock_.start();
}

void CaptureWidget::setInput(const QString& text) {
    input_->setText(text);
    reparse();
}

void CaptureWidget::focusInput() {
    input_->setFocus();
    input_->selectAll();
}

bool CaptureWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            // Si el completador de categorias esta abierto, Enter elige la
            // sugerencia y no guarda: guardar con la categoria a medio escribir
            // es justo el error que el completador existe para evitar.
            if (watched == category_->lineEdit() && category_->lineEdit()->completer() &&
                category_->lineEdit()->completer()->popup()->isVisible()) {
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
