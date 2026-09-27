#include "dialogs.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

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

QDialogButtonBox* buttons(QDialog* dialog, const QString& okText) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box->button(QDialogButtonBox::Ok)->setText(okText);
    box->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("PrimaryButton"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    QObject::connect(box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return box;
}

void styleDialog(QDialog* dialog, const QString& title, int width) {
    dialog->setWindowTitle(title);
    dialog->setMinimumWidth(width);
    dialog->setModal(true);
}

[[nodiscard]] QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::bodyFont(9));
    label->setWordWrap(true);
    theme::setLabelColor(label, theme::kTextMuted);
    return label;
}

void fillPockets(QComboBox* box, const Snapshot& snapshot, const core::Id& selected) {
    box->clear();
    for (const core::Pocket& pocket : snapshot.pockets) {
        if (pocket.deleted) {
            continue;
        }
        box->addItem(QString::fromStdString(pocket.name), QString::fromStdString(pocket.id));
    }
    const int index = box->findData(QString::fromStdString(selected));
    if (index >= 0) {
        box->setCurrentIndex(index);
    }
}

void fillJobs(QComboBox* box, const Snapshot& snapshot, const core::Id& selected) {
    box->clear();
    box->addItem(QStringLiteral("— sin trabajo —"), QString());
    for (const core::Job& job : snapshot.jobs) {
        if (job.deleted) {
            continue;
        }
        box->addItem(QString::fromStdString(job.name), QString::fromStdString(job.id));
    }
    const int index = box->findData(QString::fromStdString(selected));
    box->setCurrentIndex(index >= 0 ? index : 0);
}

void fillSpread(QComboBox* box, int months) {
    box->addItem(QStringLiteral("se gasta en el mes"), 1);
    for (int option : {2, 3, 4, 6, 12}) {
        box->addItem(QStringLiteral("dura %1 meses").arg(option), option);
    }
    const int index = box->findData(months);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

// ----------------------------------------------------------------- Bolsillo

PocketDialog::PocketDialog(core::Currency currency, QWidget* parent)
    : QDialog(parent), currency_(currency) {
    styleDialog(this, QStringLiteral("Nuevo bolsillo"), 440);

    name_ = new QLineEdit(this);
    name_->setPlaceholderText(QStringLiteral("Caja del negocio"));

    kind_ = new QComboBox(this);
    kind_->addItem(QStringLiteral("Operacion — el dia a dia del negocio"),
                   static_cast<int>(core::PocketKind::Operacion));
    kind_->addItem(QStringLiteral("Ahorro — reserva"),
                   static_cast<int>(core::PocketKind::Ahorro));
    kind_->addItem(QStringLiteral("Inversion — capital trabajando"),
                   static_cast<int>(core::PocketKind::Inversion));
    kind_->addItem(QStringLiteral("Personal — lo que ya te pagaste"),
                   static_cast<int>(core::PocketKind::Personal));

    opening_ = new QLineEdit(QStringLiteral("0"), this);
    opening_->setAlignment(Qt::AlignRight);
    opening_->setFont(theme::numericFont(10));

    hint_ = hintLabel(
        QStringLiteral("El saldo inicial es lo que hay HOY en ese bolsillo, antes de cargar "
                       "ningun movimiento. Dejarlo en cero cuando no lo esta hace que ningun "
                       "saldo cuadre nunca, y un saldo que no cuadra se deja de mirar."),
        this);

    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Nombre"), name_);
    form->addRow(QStringLiteral("Para que"), kind_);
    form->addRow(QStringLiteral("Cuanto hay hoy"), opening_);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(hint_);
    root->addWidget(buttons(this, QStringLiteral("Crear")));
}

std::optional<core::Pocket> PocketDialog::result() const {
    auto* self = const_cast<PocketDialog*>(this);
    if (name_->text().trimmed().isEmpty()) {
        QMessageBox::warning(self, QStringLiteral("Falta el nombre"),
                             QStringLiteral("El bolsillo necesita un nombre."));
        return std::nullopt;
    }

    core::Pocket pocket;
    pocket.name = name_->text().trimmed().toStdString();
    pocket.kind = static_cast<core::PocketKind>(kind_->currentData().toInt());
    try {
        pocket.openingMinor =
            core::Money::parse(opening_->text().trimmed().toStdString(), currency_).minor();
    } catch (const std::exception&) {
        QMessageBox::warning(self, QStringLiteral("Saldo invalido"),
                             QStringLiteral("Escribi el saldo como 1.250,00"));
        return std::nullopt;
    }
    return pocket;
}


// ------------------------------------------------------------------ Cuadrar

ReconcileDialog::ReconcileDialog(const core::Pocket& pocket, const core::Money& computed,
                                 core::Date today, QWidget* parent)
    : QDialog(parent), pocket_(pocket), computed_(computed), today_(today) {
    styleDialog(this, QStringLiteral("Cuadrar ") + QString::fromStdString(pocket.name), 460);

    auto* saysLabel = new QLabel(
        QStringLiteral("La aplicacion dice que en \"%1\" hay %2.")
            .arg(QString::fromStdString(pocket.name), theme::formatMoney(computed)),
        this);
    saysLabel->setFont(theme::bodyFont(11, QFont::DemiBold));
    saysLabel->setWordWrap(true);
    theme::setLabelColor(saysLabel, theme::kText);

    real_ = new QLineEdit(this);
    real_->setAlignment(Qt::AlignRight);
    real_->setFont(theme::numericFont(11));
    real_->setPlaceholderText(theme::formatMoney(computed));

    difference_ = hintLabel(QString(), this);

    connect(real_, &QLineEdit::textChanged, this, [this] {
        try {
            const core::Money real =
                core::Money::parse(real_->text().trimmed().toStdString(), computed_.currency());
            const core::Money delta = real - computed_;
            if (delta.isZero()) {
                difference_->setText(QStringLiteral("Cuadra exacto."));
                theme::setLabelColor(difference_, theme::kPositive);
            } else {
                difference_->setText(
                    QStringLiteral("Diferencia de %1. Se va a anotar como un movimiento del "
                                   "%2 llamado \"%3\".")
                        .arg(theme::formatMoney(delta),
                             QString::fromStdString(today_.toIso()),
                             QString::fromUtf8(core::kAdjustment.data(),
                                               static_cast<int>(core::kAdjustment.size()))));
                theme::setLabelColor(difference_, theme::kAviso);
            }
        } catch (const std::exception&) {
            difference_->setText(QStringLiteral("Escribi el saldo como 1.250,00"));
            theme::setLabelColor(difference_, theme::kTextMuted);
        }
    });

    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Cuanto hay de verdad"), real_);

    auto* root = new QVBoxLayout(this);
    root->addWidget(saysLabel);
    root->addLayout(form);
    root->addWidget(difference_);
    root->addWidget(hintLabel(
        QStringLiteral("La diferencia no se aplica moviendo el saldo por debajo: queda como "
                       "un movimiento con fecha y nombre. Un ajuste invisible tapa lo que lo "
                       "causo, y al mes siguiente hay que ajustar otra vez."),
        this));
    root->addWidget(buttons(this, QStringLiteral("Anotar el ajuste")));
}

std::optional<core::Movement> ReconcileDialog::result() const {
    core::Money real;
    try {
        real = core::Money::parse(real_->text().trimmed().toStdString(), computed_.currency());
    } catch (const std::exception&) {
        return std::nullopt;
    }

    const core::Money delta = real - computed_;
    if (delta.isZero()) {
        return std::nullopt;
    }

    core::Movement movement;
    movement.date = today_;
    movement.name = "Ajuste de " + pocket_.name;
    movement.kind = delta.isNegative() ? core::MovementKind::Gasto : core::MovementKind::Ingreso;
    movement.amountMinor = delta.isNegative() ? -delta.minor() : delta.minor();
    movement.pocketId = pocket_.id;
    movement.category = std::string(core::kAdjustment);
    movement.settled = true;
    return movement;
}

// -------------------------------------------------------- Editar movimiento

MovementEditor::MovementEditor(const core::Movement& original, const Snapshot& snapshot,
                               QWidget* parent)
    : QDialog(parent), base_(original), edited_(original), snapshot_(snapshot) {
    styleDialog(this, QStringLiteral("Editar movimiento"), 460);

    name_ = new QLineEdit(QString::fromStdString(original.name), this);

    amount_ = new QLineEdit(this);
    amount_->setAlignment(Qt::AlignRight);
    amount_->setFont(theme::numericFont(10));
    amount_->setText(theme::formatMoney(
        core::Money::fromMinor(original.amountMinor, snapshot.currency)));

    date_ = new QDateEdit(toQDate(original.date), this);
    date_->setCalendarPopup(true);
    date_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    pocket_ = new QComboBox(this);
    fillPockets(pocket_, snapshot, original.pocketId);

    target_ = new QComboBox(this);
    fillPockets(target_, snapshot, original.targetPocketId);

    category_ = new CategoryBox(this);
    category_->setSuggestions(snapshot.categoriesFor(original.kind));
    category_->setCategory(QString::fromStdString(original.category));

    job_ = new QComboBox(this);
    fillJobs(job_, snapshot, original.jobId);

    spread_ = new QComboBox(this);
    fillSpread(spread_, original.spreadMonths);

    settled_ = new QCheckBox(original.kind == core::MovementKind::Gasto
                                 ? QStringLiteral("ya lo pague")
                                 : QStringLiteral("ya me lo pagaron"),
                             this);
    settled_->setChecked(original.settled);

    settledDateLabel_ = new QLabel(original.kind == core::MovementKind::Gasto
                                       ? QStringLiteral("Cuando lo pague")
                                       : QStringLiteral("Cuando me pagaron"),
                                   this);

    const core::Date initialSettledDate =
        original.settledDate.has_value() ? *original.settledDate : original.date;
    settledDate_ = new QDateEdit(toQDate(initialSettledDate), this);
    settledDate_->setCalendarPopup(true);
    settledDate_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    const auto updateSettledDateEnabled = [this](bool checked) {
        settledDate_->setEnabled(checked);
        settledDateLabel_->setEnabled(checked);
    };
    connect(settled_, &QCheckBox::toggled, this, updateSettledDateEnabled);
    updateSettledDateEnabled(settled_->isChecked());

    auto* form = new QFormLayout();
    form_ = form;
    form->addRow(QStringLiteral("Que fue"), name_);
    form->addRow(QStringLiteral("Monto"), amount_);
    form->addRow(QStringLiteral("Fecha"), date_);
    form->addRow(original.kind == core::MovementKind::Ingreso ? QStringLiteral("Entra a")
                                                              : QStringLiteral("Sale de"),
                 pocket_);
    targetLabel_ = new QLabel(QStringLiteral("Va a"), this);
    form->addRow(targetLabel_, target_);
    form->addRow(QStringLiteral("Categoria"), category_);
    form->addRow(QStringLiteral("Trabajo"), job_);
    form->addRow(QStringLiteral("Cuanto dura"), spread_);
    form->addRow(QString(), settled_);
    form->addRow(settledDateLabel_, settledDate_);

    error_ = new QLabel(this);
    error_->setFont(theme::bodyFont(9));
    error_->setWordWrap(true);
    theme::setLabelColor(error_, theme::kNegative);
    error_->setVisible(false);

    auto* deleteButton = new QPushButton(QStringLiteral("Eliminar"), this);
    deleteButton->setObjectName(QStringLiteral("KindExpense"));
    deleteButton->setCursor(Qt::PointingHandCursor);
    connect(deleteButton, &QPushButton::clicked, this, &MovementEditor::remove);

    auto* cancelButton = new QPushButton(QStringLiteral("Cancelar"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveButton = new QPushButton(QStringLiteral("Guardar"), this);
    saveButton->setObjectName(QStringLiteral("PrimaryButton"));
    saveButton->setCursor(Qt::PointingHandCursor);
    connect(saveButton, &QPushButton::clicked, this, &MovementEditor::save);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(deleteButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(saveButton);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(error_);
    root->addLayout(buttonRow);

    applyKind();
    name_->setFocus();
    name_->selectAll();
}

void MovementEditor::applyKind() {
    const bool transfer = base_.kind == core::MovementKind::Traspaso;
    const bool expense = base_.kind == core::MovementKind::Gasto;

    targetLabel_->setVisible(transfer);
    target_->setVisible(transfer);
    category_->setVisible(!transfer);
    // Solo un cobro es de un trabajo: un gasto no es de ninguno.
    form_->setRowVisible(job_, base_.kind == core::MovementKind::Ingreso);
    spread_->setVisible(expense);
    settled_->setVisible(!transfer);
    settledDateLabel_->setVisible(!transfer);
    settledDate_->setVisible(!transfer);
}

void MovementEditor::save() {
    const QString name = name_->text().trimmed();
    if (name.isEmpty()) {
        error_->setText(QStringLiteral("Falta decir que fue."));
        error_->setVisible(true);
        return;
    }

    core::Money amount;
    try {
        amount = core::Money::parse(amount_->text().trimmed().toStdString(), snapshot_.currency);
    } catch (const std::exception&) {
        error_->setText(QStringLiteral("Monto invalido. Ejemplos: 46  ·  46,00  ·  1.250,50"));
        error_->setVisible(true);
        return;
    }
    if (amount.isNegative() || amount.isZero()) {
        error_->setText(QStringLiteral("El monto va en positivo; el signo lo pone el tipo."));
        error_->setVisible(true);
        return;
    }

    // Se parte del original para conservar id, hlc y deviceId: solo se pisan
    // los campos que este dialogo deja tocar.
    edited_ = base_;
    edited_.name = name.toStdString();
    edited_.amountMinor = amount.minor();
    edited_.date = fromQDate(date_->date());
    edited_.pocketId = pocket_->currentData().toString().toStdString();

    if (base_.kind == core::MovementKind::Traspaso) {
        edited_.targetPocketId = target_->currentData().toString().toStdString();
        if (edited_.targetPocketId == edited_.pocketId) {
            error_->setText(QStringLiteral("Un traspaso necesita dos bolsillos distintos."));
            error_->setVisible(true);
            return;
        }
    } else {
        const QString category = category_->category().trimmed();
        edited_.category =
            category.isEmpty() ? std::string(core::kUncategorized) : category.toStdString();
        // Un gasto conserva el suyo: el de un repuesto lo puso su ficha.
        if (base_.kind == core::MovementKind::Ingreso) {
            edited_.jobId = job_->currentData().toString().toStdString();
        }
        edited_.settled = settled_->isChecked();
        if (edited_.settled) {
            edited_.settledDate = fromQDate(settledDate_->date());
        } else {
            edited_.settledDate = std::nullopt;
        }
        if (base_.kind == core::MovementKind::Gasto) {
            edited_.spreadMonths = spread_->currentData().toInt();
        }
    }

    accept();
}

void MovementEditor::remove() {
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Eliminar movimiento"),
        QStringLiteral("¿Eliminar \"%1\"?").arg(name_->text().trimmed()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    deleted_ = true;
    accept();
}

} // namespace dake::ui
