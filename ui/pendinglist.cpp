#include "pendinglist.hpp"

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

[[nodiscard]] QString kindName(core::InboxKind kind) {
    switch (kind) {
        case core::InboxKind::PorConfirmar: return QStringLiteral("PorConfirmar");
        case core::InboxKind::Cotizaciones: return QStringLiteral("Cotizaciones");
        case core::InboxKind::PorCobrar: return QStringLiteral("PorCobrar");
        case core::InboxKind::SinEntregar: return QStringLiteral("SinEntregar");
        case core::InboxKind::SinHoras: return QStringLiteral("SinHoras");
        case core::InboxKind::CostoRepuesto: return QStringLiteral("CostoRepuesto");
        case core::InboxKind::SinCuadrar: return QStringLiteral("SinCuadrar");
    }
    return {};
}

[[nodiscard]] QString money(const Snapshot& s, qint64 minor) {
    return theme::formatMoney(core::Money::fromMinor(minor, s.currency));
}

/// Enter en el campo de una fila es apretar su boton.
class EnterPresses : public QObject {
public:
    EnterPresses(QPushButton* button, QObject* parent) : QObject(parent), button_(button) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                button_->click();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPushButton* button_;
};

} // namespace

PendingList::PendingList(QWidget* parent) : QWidget(parent) {
    rows_ = new QVBoxLayout(this);
    rows_->setContentsMargins(0, 0, 0, 0);
    rows_->setSpacing(10);
}

std::string PendingList::snoozeKey(const core::InboxItem& item) {
    switch (item.kind) {
        case core::InboxKind::SinHoras: return item.refId + ":horas";
        case core::InboxKind::SinCuadrar: return item.refId + ":cuadrar";
        case core::InboxKind::Cotizaciones: return "cotizaciones";
        default: return item.refId;
    }
}

void PendingList::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    while (QLayoutItem* item = rows_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    if (snapshot_.inbox.empty()) {
        auto* empty = new QLabel(QStringLiteral("Nada pendiente."), this);
        empty->setObjectName(QStringLiteral("PendingEmpty"));
        empty->setFont(theme::bodyFont(10));
        theme::setLabelColor(empty, theme::kTextMuted);
        rows_->addWidget(empty);
        return;
    }
    for (const core::InboxItem& item : snapshot_.inbox) {
        if (QWidget* row = buildRow(item)) {
            rows_->addWidget(row);
        }
    }
}

QWidget* PendingList::buildRow(const core::InboxItem& item) {
    const Snapshot& s = snapshot_;
    const core::Movement* m = s.movement(item.refId);
    const core::Repair* r = s.repair(item.refId);
    const core::Job* job = s.job(item.refId);

    auto repairTitle = [&](const core::Repair& rp) {
        QString title = QString::fromStdString(rp.orderNo + " · " + rp.device);
        if (job != nullptr && !job->client.empty()) {
            title += QStringLiteral(" · ") + QString::fromStdString(job->client);
        }
        return title;
    };

    QString title;
    QString detail;
    QString actionText;
    QString valueText;
    QString valueHint;
    bool wantsValue = false;

    switch (item.kind) {
        case core::InboxKind::PorConfirmar:
            if (m == nullptr) return nullptr;
            title = QStringLiteral("%1 · %2").arg(QString::fromStdString(m->name), money(s, m->amountMinor));
            detail = QStringLiteral("Gasto fijo de %1, anotado solo con el monto estimado. Si la factura "
                                    "vino distinta, escribe el monto real.")
                         .arg(QString::fromStdString(m->date.toIso()));
            wantsValue = true;
            valueText = moneyFieldText(m->amountMinor, s.currency);
            actionText = QStringLiteral("Confirmar");
            break;
        case core::InboxKind::Cotizaciones:
            title = QStringLiteral("%1 cotización(es) esperan que decidas").arg(item.days);
            detail = QStringLiteral("Podrían contar un cobro dos veces: una parece repetida, o ya hay un "
                                    "ingreso anotado a mano que parece el mismo.");
            actionText = QStringLiteral("Revisar");
            break;
        case core::InboxKind::PorCobrar:
            if (r == nullptr) return nullptr;
            title = repairTitle(*r);
            detail = !r->sourceRef.empty()
                         ? QStringLiteral("Entregada hace %1 días. Se cobra en DakeLabs Cotizaciones: "
                                          "márcala pagada allá.")
                               .arg(item.days)
                         : QStringLiteral("Entregada hace %1 días y sin cobrar.").arg(item.days);
            actionText = r->sourceRef.empty() ? QStringLiteral("Cobrar") : QString();
            break;
        case core::InboxKind::SinEntregar:
            if (r == nullptr) return nullptr;
            title = repairTitle(*r);
            detail = QStringLiteral("En proceso hace %1 días. ¿Se entregó y faltó anotarlo?").arg(item.days);
            actionText = QStringLiteral("Abrir");
            break;
        case core::InboxKind::SinHoras:
            if (r == nullptr) return nullptr;
            title = repairTitle(*r);
            detail = QStringLiteral("¿Cuántas horas le dedicaste de verdad? Sin eso no se sabe cuánto "
                                    "deja cada hora.");
            wantsValue = true;
            valueText = r->estMinutes > 0 ? hoursText(r->estMinutes) : QString();
            valueHint = QStringLiteral("horas, 2,5");
            actionText = QStringLiteral("Guardar horas");
            break;
        case core::InboxKind::CostoRepuesto: {
            const core::RepairPart* p = s.part(item.refId);
            if (p == nullptr) return nullptr;
            const core::Repair* owner = s.repair(p->jobId);
            title = QString::fromStdString(p->name) +
                    (owner != nullptr ? QStringLiteral(" · ") + QString::fromStdString(owner->orderNo)
                                      : QString());
            detail = QStringLiteral("¿Cuánto te costó? Lo que cobró el informe por la pieza no es lo "
                                    "que pagaste.");
            wantsValue = true;
            valueHint = QStringLiteral("costo");
            actionText = QStringLiteral("Guardar costo");
            break;
        }
        case core::InboxKind::SinCuadrar:
            title = QStringLiteral("%1: sin cuadrar hace %2 días").arg(s.pocketName(item.refId)).arg(item.days);
            detail = QStringLiteral("Cuenta lo que hay y escríbelo.");
            actionText = QStringLiteral("Cuadrar");
            break;
    }

    auto* row = new QWidget(this);
    row->setObjectName(QStringLiteral("Pending:") + kindName(item.kind));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* texts = new QVBoxLayout();
    texts->setSpacing(1);
    auto* titleLabel = new QLabel(title, row);
    titleLabel->setFont(theme::bodyFont(11, QFont::DemiBold));
    titleLabel->setWordWrap(true);
    theme::setLabelColor(titleLabel, theme::kText);
    auto* detailLabel = new QLabel(detail, row);
    detailLabel->setFont(theme::bodyFont(9));
    detailLabel->setWordWrap(true);
    theme::setLabelColor(detailLabel, theme::kTextMuted);
    texts->addWidget(titleLabel);
    texts->addWidget(detailLabel);
    layout->addLayout(texts, 1);

    QLineEdit* value = nullptr;
    if (wantsValue) {
        value = new QLineEdit(valueText, row);
        value->setObjectName(QStringLiteral("PendingValue"));
        value->setPlaceholderText(valueHint);
        value->setFixedWidth(110);
        value->setFont(theme::bodyFont(10));
        layout->addWidget(value);
    }

    const core::InboxItem it = item;
    if (!actionText.isEmpty()) {
        auto* action = new QPushButton(actionText, row);
        action->setObjectName(QStringLiteral("PendingAction"));
        action->setCursor(Qt::PointingHandCursor);
        action->setFont(theme::bodyFont(9, QFont::DemiBold));
        action->setFixedHeight(30);
        layout->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, it, value] {
            switch (it.kind) {
                case core::InboxKind::PorConfirmar:
                    if (const auto amount = parseMoneyText(value->text(), snapshot_.currency)) {
                        emit recurringConfirmed(it.refId, *amount);
                    }
                    break;
                case core::InboxKind::Cotizaciones: emit quotesReviewRequested(); break;
                case core::InboxKind::PorCobrar: emit chargeRequested(it.refId); break;
                case core::InboxKind::SinEntregar: emit repairOpened(it.refId); break;
                case core::InboxKind::SinHoras:
                    if (const auto minutes = parseHoursText(value->text())) {
                        emit realHoursSet(it.refId, *minutes);
                    }
                    break;
                case core::InboxKind::CostoRepuesto:
                    if (const auto cost = parseMoneyText(value->text(), snapshot_.currency)) {
                        emit partCostSet(it.refId, *cost);
                    }
                    break;
                case core::InboxKind::SinCuadrar: emit reconcileRequested(it.refId); break;
            }
        });
        if (value != nullptr) {
            value->installEventFilter(new EnterPresses(action, value));
        }
    }

    auto* later = new QPushButton(QStringLiteral("Después"), row);
    later->setObjectName(QStringLiteral("PendingLater"));
    later->setToolTip(QStringLiteral("Que no aparezca por una semana."));
    later->setCursor(Qt::PointingHandCursor);
    later->setFont(theme::bodyFont(9));
    later->setFixedHeight(30);
    layout->addWidget(later);
    connect(later, &QPushButton::clicked, this, [this, it] { emit snoozed(snoozeKey(it), 7); });
    return row;
}

} // namespace dake::ui
