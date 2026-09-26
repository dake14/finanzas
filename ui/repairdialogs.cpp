#include "repairdialogs.hpp"

#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "fields.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QDate toQDate(const core::Date& date) {
    return QDate(date.year, static_cast<int>(date.month), static_cast<int>(date.day));
}

[[nodiscard]] QLabel* errorLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setFont(theme::bodyFont(9));
    label->setWordWrap(true);
    theme::setLabelColor(label, theme::kNegative);
    label->hide();
    return label;
}

[[nodiscard]] QLabel* hint(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::bodyFont(9));
    label->setWordWrap(true);
    theme::setLabelColor(label, theme::kTextMuted);
    return label;
}

void styleDialog(QDialog* dialog, const QString& title, int width) {
    dialog->setWindowTitle(title);
    dialog->setMinimumWidth(width);
    dialog->setModal(true);
    dialog->setStyleSheet(theme::styleSheet());
}

[[nodiscard]] QPushButton* button(const QString& text, bool primary, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    b->setObjectName(primary ? QStringLiteral("PrimaryButton") : QStringLiteral("GhostButton"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(theme::bodyFont(10, QFont::DemiBold));
    b->setFixedHeight(34);
    return b;
}

} // namespace

// ------------------------------------------------------------------ Alta

NewRepairDialog::NewRepairDialog(const Snapshot& snapshot, QWidget* parent) : QDialog(parent) {
    styleDialog(this, QStringLiteral("Nueva reparación"), 480);
    clock_.start();

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(hint(QStringLiteral("La plantilla trae el precio, las horas, los "
                                          "consumibles y los repuestos típicos. Después se "
                                          "ajusta lo que cambió."),
                           this));

    auto* form = new QFormLayout();
    form->setSpacing(8);

    // Una linea y no un desplegable: la lista de sugerencias de un desplegable
    // editable se queda con el Enter, y el dialogo no avanza. Aca Enter elige
    // la plantilla marcada y pasa al cliente; las flechas recorren la lista.
    templates_ = snapshot.templates;
    template_ = new QLineEdit(this);
    template_->setPlaceholderText(QStringLiteral("gpu, laptop, placa…"));
    template_->installEventFilter(this);
    connect(template_, &QLineEdit::textEdited, this, &NewRepairDialog::matchTemplate);
    form->addRow(QStringLiteral("Plantilla"), template_);
    templateList_ = new QLabel(this);
    templateList_->setTextFormat(Qt::RichText);
    templateList_->setWordWrap(true);
    templateList_->setFont(theme::bodyFont(9));
    form->addRow(QString(), templateList_);
    showTemplates();

    client_ = new QLineEdit(this);
    auto* clients = new QCompleter(snapshot.clients(), client_);
    clients->setCaseSensitivity(Qt::CaseInsensitive);
    clients->setFilterMode(Qt::MatchContains);
    client_->setCompleter(clients);
    client_->setPlaceholderText(QStringLiteral("Nombre del cliente"));
    form->addRow(QStringLiteral("Cliente"), client_);

    device_ = new QLineEdit(this);
    device_->setPlaceholderText(QStringLiteral("RTX 3080, Asus X556U…"));
    form->addRow(QStringLiteral("Equipo"), device_);
    layout->addLayout(form);

    error_ = errorLabel(this);
    layout->addWidget(error_);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    auto* cancel = button(QStringLiteral("Cancelar"), false, this);
    auto* create = button(QStringLiteral("Crear"), true, this);
    create->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(create, &QPushButton::clicked, this, &NewRepairDialog::tryAccept);
    row->addWidget(cancel);
    row->addWidget(create);
    layout->addLayout(row);

    // Enter en cada campo avanza al siguiente; en el ultimo, crea. El de la
    // plantilla lo maneja eventFilter.
    connect(client_, &QLineEdit::returnPressed, device_, qOverload<>(&QWidget::setFocus));
    connect(device_, &QLineEdit::returnPressed, this, &NewRepairDialog::tryAccept);
    template_->setFocus();
}

core::Id NewRepairDialog::templateId() const {
    return chosen_ >= 0 ? templates_[static_cast<std::size_t>(chosen_)].id : core::Id();
}

void NewRepairDialog::matchTemplate() {
    const QString needle = template_->text().trimmed();
    chosen_ = -1;
    if (!needle.isEmpty()) {
        for (int i = 0; i < static_cast<int>(templates_.size()); ++i) {
            if (QString::fromStdString(templates_[static_cast<std::size_t>(i)].name)
                    .contains(needle, Qt::CaseInsensitive)) {
                chosen_ = i;
                break;
            }
        }
    }
    showTemplates();
}

void NewRepairDialog::showTemplates() {
    QStringList names;
    for (int i = 0; i < static_cast<int>(templates_.size()); ++i) {
        const QString name =
            QString::fromStdString(templates_[static_cast<std::size_t>(i)].name).toHtmlEscaped();
        names << (i == chosen_ ? QStringLiteral("<span style='color:%1;font-weight:600'>▸ %2</span>")
                                     .arg(theme::kAccent.name(), name)
                               : QStringLiteral("<span style='color:%1'>%2</span>")
                                     .arg(theme::kTextMuted.name(), name));
    }
    templateList_->setText(names.join(QStringLiteral(" &nbsp;·&nbsp; ")));
}

bool NewRepairDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == template_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const int count = static_cast<int>(templates_.size());
        if ((key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) && count > 0) {
            const int step = key->key() == Qt::Key_Down ? 1 : -1;
            chosen_ = chosen_ < 0 ? (step > 0 ? 0 : count - 1) : (chosen_ + step + count) % count;
            template_->setText(QString::fromStdString(templates_[static_cast<std::size_t>(chosen_)].name));
            showTemplates();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (chosen_ < 0) {
                error_->setText(QStringLiteral("Ninguna plantilla coincide con eso."));
                error_->show();
            } else {
                template_->setText(QString::fromStdString(templates_[static_cast<std::size_t>(chosen_)].name));
                error_->hide();
                client_->setFocus();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

QString NewRepairDialog::client() const {
    return client_->text().trimmed();
}

QString NewRepairDialog::device() const {
    return device_->text().trimmed();
}

void NewRepairDialog::tryAccept() {
    if (templateId().empty()) {
        error_->setText(QStringLiteral("Elige una plantilla."));
        error_->show();
        template_->setFocus();
        return;
    }
    if (device().isEmpty()) {
        error_->setText(QStringLiteral("Falta el equipo: es lo que después se busca."));
        error_->show();
        device_->setFocus();
        return;
    }
    elapsed_ = clock_.elapsed();
    accept();
}

// ---------------------------------------------------------------- Entrega

DeliverDialog::DeliverDialog(const core::Repair& repair, const core::Money& price,
                             core::Date today, QWidget* parent)
    : QDialog(parent), currency_(price.currency()) {
    styleDialog(this, QStringLiteral("Entregar %1").arg(QString::fromStdString(repair.orderNo)),
                440);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(hint(QStringLiteral("Las horas reales son las que de verdad le dedicaste: "
                                          "sin ellas no se sabe cuánto deja cada hora."),
                           this));

    auto* form = new QFormLayout();
    form->setSpacing(8);
    hours_ = new QLineEdit(hoursText(repair.realMinutes.value_or(repair.estMinutes)), this);
    hours_->setPlaceholderText(QStringLiteral("2,5"));
    form->addRow(QStringLiteral("Horas reales"), hours_);
    price_ = new QLineEdit(moneyFieldText(price.minor(), currency_), this);
    form->addRow(QStringLiteral("Precio final"), price_);
    date_ = new QDateEdit(toQDate(today), this);
    date_->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    date_->setCalendarPopup(true);
    form->addRow(QStringLiteral("Fecha"), date_);
    layout->addLayout(form);

    error_ = errorLabel(this);
    layout->addWidget(error_);

    auto* row = new QHBoxLayout();
    auto* cancel = button(QStringLiteral("Cancelar"), false, this);
    auto* deliverOnly = button(QStringLiteral("Entregar, por cobrar"), false, this);
    auto* deliverCharge = button(QStringLiteral("Entregar y cobrar"), true, this);
    deliverCharge->setDefault(true);
    row->addWidget(cancel);
    row->addStretch(1);
    row->addWidget(deliverOnly);
    row->addWidget(deliverCharge);
    layout->addLayout(row);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(deliverOnly, &QPushButton::clicked, this, [this] { finish(false); });
    connect(deliverCharge, &QPushButton::clicked, this, [this] { finish(true); });
    hours_->setFocus();
    hours_->selectAll();
}

int DeliverDialog::realMinutes() const {
    return parseHoursText(hours_->text()).value_or(0);
}

std::int64_t DeliverDialog::priceMinor() const {
    return parseMoneyText(price_->text(), currency_).value_or(0);
}

core::Date DeliverDialog::date() const {
    const QDate d = date_->date();
    return core::Date::fromYmd(d.year(), static_cast<unsigned>(d.month()),
                               static_cast<unsigned>(d.day()));
}

void DeliverDialog::finish(bool charged) {
    if (!parseHoursText(hours_->text())) {
        error_->setText(QStringLiteral("Las horas van en horas: 2,5 son dos horas y media."));
        error_->show();
        hours_->setFocus();
        return;
    }
    if (priceMinor() <= 0) {
        error_->setText(QStringLiteral("Falta el precio. Si fue sin cargo, anótala desde la "
                                       "ficha con precio 0."));
        error_->show();
        price_->setFocus();
        return;
    }
    charged_ = charged;
    accept();
}

} // namespace dake::ui
