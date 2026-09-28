// ui/reviewpage.cpp — la revision de la semana, de a un pendiente por vez.
//
// Cada pendiente trae la sugerencia ya puesta: Enter la acepta. Escribir la
// corrige. Ctrl+→ lo salta por ahora, Ctrl+P lo pospone una semana y
// Ctrl+Supr borra el movimiento. La meta es que la semana entera se ordene en
// menos de cinco minutos, y el cronometro de Ajustes dice si se cumple.

#include <QAbstractItemView>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include "cards.hpp"
#include "categorybox.hpp"
#include "fields.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString kindLabel(core::InboxKind kind) {
    switch (kind) {
        case core::InboxKind::SinCategoria: return QStringLiteral("SIN CATEGORÍA");
        case core::InboxKind::PorConfirmar: return QStringLiteral("GASTO FIJO POR CONFIRMAR");
        case core::InboxKind::Sugerido: return QStringLiteral("IMPORTADO: ¿ESTÁ BIEN LA CATEGORÍA?");
        case core::InboxKind::VidaUtil: return QStringLiteral("HERRAMIENTA NUEVA");
        case core::InboxKind::Cotizaciones: return QStringLiteral("DAKELABS COTIZACIONES");
        case core::InboxKind::PorCobrar: return QStringLiteral("ENTREGADA Y SIN COBRAR");
        case core::InboxKind::SinEntregar: return QStringLiteral("EN PROCESO HACE MUCHO");
        case core::InboxKind::SinHoras: return QStringLiteral("FALTAN LAS HORAS REALES");
        case core::InboxKind::CostoRepuesto: return QStringLiteral("REPUESTO SIN COSTO");
    }
    return {};
}

[[nodiscard]] QString money(const Snapshot& s, std::int64_t minor) {
    return theme::formatMoney(core::Money::fromMinor(minor, s.currency));
}

} // namespace

ReviewPage::ReviewPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void ReviewPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto* heading = new QLabel(QStringLiteral("Revisión"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    layout->addWidget(heading);

    summary_ = new QLabel(this);
    summary_->setFont(theme::bodyFont(10));
    theme::setLabelColor(summary_, theme::kTextMuted);
    layout->addWidget(summary_);

    card_ = new Card(QString(), this);
    auto* body = new QWidget(card_);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);

    kind_ = new QLabel(body);
    kind_->setFont(theme::bodyFont(8, QFont::DemiBold));
    theme::setLabelColor(kind_, theme::kAccent);
    title_ = new QLabel(body);
    title_->setFont(theme::displayFont(15, QFont::DemiBold));
    title_->setWordWrap(true);
    theme::setLabelColor(title_, theme::kText);
    detail_ = new QLabel(body);
    detail_->setFont(theme::bodyFont(10));
    detail_->setWordWrap(true);
    theme::setLabelColor(detail_, theme::kTextMuted);
    bodyLayout->addWidget(kind_);
    bodyLayout->addWidget(title_);
    bodyLayout->addWidget(detail_);

    auto* inputRow = new QHBoxLayout();
    category_ = new CategoryBox(body);
    category_->setMinimumWidth(260);
    category_->setFont(theme::bodyFont(11));
    value_ = new QLineEdit(body);
    value_->setFixedWidth(160);
    value_->setFont(theme::bodyFont(11));
    accept_ = new QPushButton(body);
    accept_->setObjectName(QStringLiteral("PrimaryButton"));
    accept_->setCursor(Qt::PointingHandCursor);
    accept_->setFont(theme::bodyFont(10, QFont::DemiBold));
    accept_->setFixedHeight(34);
    connect(accept_, &QPushButton::clicked, this, &ReviewPage::acceptCurrent);
    inputRow->addWidget(category_);
    inputRow->addWidget(value_);
    inputRow->addWidget(accept_);
    inputRow->addStretch(1);
    bodyLayout->addLayout(inputRow);

    auto* keys = new QLabel(QStringLiteral("Enter aceptar  ·  Ctrl+→ saltar  ·  Ctrl+P en una semana  ·  "
                                           "Ctrl+Supr borrar el movimiento"),
                            body);
    keys->setFont(theme::bodyFont(8));
    theme::setLabelColor(keys, theme::kTextFaint);
    bodyLayout->addWidget(keys);
    card_->addContent(body);
    layout->addWidget(card_);

    upcoming_ = new QLabel(this);
    upcoming_->setFont(theme::bodyFont(9));
    upcoming_->setWordWrap(true);
    theme::setLabelColor(upcoming_, theme::kTextFaint);
    layout->addWidget(upcoming_);
    layout->addStretch(1);

    for (QWidget* w : {static_cast<QWidget*>(value_), static_cast<QWidget*>(category_->lineEdit()),
                       static_cast<QWidget*>(accept_)}) {
        w->installEventFilter(this);
    }

    auto shortcut = [this](const QKeySequence& keys, auto slot) {
        auto* s = new QShortcut(keys, this);
        s->setContext(Qt::WindowShortcut);
        connect(s, &QShortcut::activated, this, [this, slot] {
            if (isVisible()) (this->*slot)();
        });
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), &ReviewPage::skipCurrent);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_P), &ReviewPage::snoozeCurrent);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete), &ReviewPage::deleteCurrent);
}

void ReviewPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    showCurrent();
    // Despues de resolver uno, el foco va al campo del siguiente: la revision
    // entera se hace con Enter.
    if (isVisible()) {
        focusInput();
    }
}

void ReviewPage::startReview() {
    skipped_.clear();
    clock_.start();
    showCurrent();
    focusInput();
}

const core::InboxItem* ReviewPage::current() const {
    for (const core::InboxItem& item : snapshot_.inbox) {
        if (!skipped_.contains(keyOf(item))) return &item;
    }
    return nullptr;
}

std::string ReviewPage::keyOf(const core::InboxItem& item) {
    return std::to_string(static_cast<int>(item.kind)) + ":" + item.refId;
}

void ReviewPage::focusInput() {
    if (value_->isVisible()) {
        value_->setFocus();
        value_->selectAll();
    } else if (category_->isVisible()) {
        category_->setFocus();
        category_->lineEdit()->selectAll();
    } else {
        accept_->setFocus();
    }
}

void ReviewPage::showCurrent() {
    const int remaining = static_cast<int>(snapshot_.inbox.size());
    const core::InboxItem* item = current();
    if (item == nullptr) {
        card_->hide();
        summary_->setText(remaining == 0
                              ? QStringLiteral("Nada pendiente: todo lo anotado está completo.")
                              : QStringLiteral("Saltaste %1. Vuelven la próxima vez que abras Revisión.")
                                    .arg(remaining));
        upcoming_->clear();
        // Revision completa: se anota cuanto tardo.
        if (clock_.isValid() && remaining == 0) {
            emit reviewFinished(clock_.elapsed());
            clock_.invalidate();
        }
        return;
    }
    card_->show();
    // A diez segundos cada uno, que es lo que deberia tardar con la
    // sugerencia bien puesta.
    const int minutes = std::max(1, (remaining * 10 + 59) / 60);
    summary_->setText(QStringLiteral("%1 %2 · %3")
                          .arg(remaining)
                          .arg(remaining == 1 ? QStringLiteral("pendiente") : QStringLiteral("pendientes"),
                               minutes == 1 ? QStringLiteral("menos de un minuto")
                                            : QStringLiteral("unos %1 minutos").arg(minutes)));

    category_->hide();
    value_->hide();
    value_->clear();
    kind_->setText(kindLabel(item->kind));
    const Snapshot& s = snapshot_;
    const core::Movement* m = s.movement(item->refId);
    const core::Repair* r = s.repair(item->refId);
    const core::Job* job = s.job(item->refId);
    auto movementTitle = [&](const core::Movement& mv) {
        return QStringLiteral("%1 · %2").arg(QString::fromStdString(mv.name), money(s, mv.amountMinor));
    };
    auto movementDetail = [&](const core::Movement& mv) {
        return QStringLiteral("%1 · %2").arg(QString::fromStdString(mv.date.toIso()), s.pocketName(mv.pocketId));
    };
    auto repairTitle = [&](const core::Repair& rp) {
        QString title = QString::fromStdString(rp.orderNo + " · " + rp.device);
        if (job != nullptr && !job->client.empty()) title += QStringLiteral(" · ") + QString::fromStdString(job->client);
        return title;
    };

    switch (item->kind) {
        case core::InboxKind::SinCategoria:
        case core::InboxKind::Sugerido: {
            if (m == nullptr) break;
            title_->setText(movementTitle(*m));
            detail_->setText(movementDetail(*m));
            QStringList names;
            for (const std::string& name : core::categoriesByUse(s.categories, s.movements, m->kind, s.today)) {
                names << QString::fromStdString(name);
            }
            category_->setSuggestions(names);
            const std::string suggestion = item->kind == core::InboxKind::Sugerido
                                               ? m->category
                                               : std::string();
            category_->setCategory(QString::fromStdString(suggestion));
            category_->show();
            accept_->setText(QStringLiteral("Guardar categoría"));
            break;
        }
        case core::InboxKind::PorConfirmar:
            if (m == nullptr) break;
            title_->setText(movementTitle(*m));
            detail_->setText(QStringLiteral("Se anotó solo con el monto estimado. Si la factura vino "
                                            "distinta, escribe el monto real: será el estimado del mes "
                                            "que viene."));
            value_->setText(moneyFieldText(m->amountMinor, s.currency));
            value_->show();
            accept_->setText(QStringLiteral("Confirmar"));
            break;
        case core::InboxKind::VidaUtil:
            if (m == nullptr) break;
            title_->setText(movementTitle(*m));
            detail_->setText(QStringLiteral("¿Cuántos meses va a durar? Su costo se reparte en esos meses "
                                            "en vez de caer entero en el mes de la compra."));
            value_->setText(QStringLiteral("24"));
            value_->show();
            accept_->setText(QStringLiteral("Meses de vida útil"));
            break;
        case core::InboxKind::Cotizaciones:
            title_->setText(QStringLiteral("%1 documento(s) esperan que decidas").arg(item->days));
            detail_->setText(QStringLiteral("Podrían contar un cobro dos veces: uno parece repetido, o ya "
                                            "hay un ingreso anotado a mano que parece el mismo."));
            accept_->setText(QStringLiteral("Revisar"));
            break;
        case core::InboxKind::PorCobrar:
            if (r == nullptr) break;
            title_->setText(repairTitle(*r));
            if (!r->sourceRef.empty()) {
                detail_->setText(QStringLiteral("Entregada hace %1 días. Se cobra en DakeLabs Cotizaciones: "
                                                "márcala pagada allá.").arg(item->days));
                accept_->setText(QStringLiteral("Recordarme en una semana"));
            } else {
                detail_->setText(QStringLiteral("Entregada hace %1 días y sin cobrar.").arg(item->days));
                accept_->setText(QStringLiteral("Ya me pagaron: cobrada hoy"));
            }
            break;
        case core::InboxKind::SinEntregar:
            if (r == nullptr) break;
            title_->setText(repairTitle(*r));
            detail_->setText(QStringLiteral("En proceso hace %1 días. ¿Se entregó y faltó anotarlo?").arg(item->days));
            accept_->setText(QStringLiteral("Abrir la reparación"));
            break;
        case core::InboxKind::SinHoras:
            if (r == nullptr) break;
            title_->setText(repairTitle(*r));
            detail_->setText(QStringLiteral("¿Cuántas horas le dedicaste de verdad? Sin eso no se sabe "
                                            "cuánto deja cada hora."));
            value_->setText(r->estMinutes > 0 ? hoursText(r->estMinutes) : QString());
            value_->setPlaceholderText(QStringLiteral("horas, 2,5"));
            value_->show();
            accept_->setText(QStringLiteral("Guardar horas"));
            break;
        case core::InboxKind::CostoRepuesto: {
            const core::RepairPart* p = s.part(item->refId);
            if (p == nullptr) break;
            const core::Repair* owner = s.repair(p->jobId);
            title_->setText(QString::fromStdString(p->name) +
                            (owner != nullptr ? QStringLiteral(" · ") + QString::fromStdString(owner->orderNo)
                                              : QString()));
            detail_->setText(QStringLiteral("¿Cuánto te costó? Lo que cobró el informe por la pieza no es "
                                            "lo que pagaste."));
            value_->setPlaceholderText(QStringLiteral("costo"));
            value_->show();
            accept_->setText(QStringLiteral("Guardar costo"));
            break;
        }
    }

    QStringList next;
    bool first = true;
    for (const core::InboxItem& other : snapshot_.inbox) {
        if (skipped_.contains(keyOf(other))) continue;
        if (first) { first = false; continue; }
        if (next.size() == 6) { next << QStringLiteral("…"); break; }
        next << kindLabel(other.kind).toLower();
    }
    upcoming_->setText(next.isEmpty() ? QString() : QStringLiteral("Después: ") + next.join(QStringLiteral(" · ")));
}

void ReviewPage::acceptCurrent() {
    const core::InboxItem* item = current();
    if (item == nullptr) return;
    const core::InboxItem it = *item;
    switch (it.kind) {
        case core::InboxKind::SinCategoria:
        case core::InboxKind::Sugerido:
            if (!category_->category().isEmpty()) emit categorySet(it.refId, category_->category());
            break;
        case core::InboxKind::PorConfirmar:
            if (const auto amount = parseMoneyText(value_->text(), snapshot_.currency)) {
                emit recurringConfirmed(it.refId, *amount);
            }
            break;
        case core::InboxKind::VidaUtil: {
            bool ok = false;
            const int months = value_->text().trimmed().toInt(&ok);
            if (ok && months > 0) emit toolLifeSet(it.refId, months);
            break;
        }
        case core::InboxKind::Cotizaciones:
            emit quotesReviewRequested();
            break;
        case core::InboxKind::PorCobrar: {
            const core::Repair* r = snapshot_.repair(it.refId);
            if (r != nullptr && r->sourceRef.empty()) {
                emit chargeRequested(it.refId);
            } else {
                emit snoozed(it.refId, 7);
            }
            break;
        }
        case core::InboxKind::SinEntregar:
            emit repairOpened(it.refId);
            break;
        case core::InboxKind::SinHoras:
            if (const auto minutes = parseHoursText(value_->text())) emit realHoursSet(it.refId, *minutes);
            break;
        case core::InboxKind::CostoRepuesto:
            if (const auto cost = parseMoneyText(value_->text(), snapshot_.currency)) emit partCostSet(it.refId, *cost);
            break;
    }
}

void ReviewPage::skipCurrent() {
    if (const core::InboxItem* item = current()) {
        skipped_.insert(keyOf(*item));
        showCurrent();
        focusInput();
    }
}

void ReviewPage::snoozeCurrent() {
    if (const core::InboxItem* item = current()) {
        // Las horas faltantes se posponen aparte del cobro de la misma
        // reparacion: son dos preguntas distintas.
        const std::string id = item->kind == core::InboxKind::SinHoras ? item->refId + ":horas" : item->refId;
        emit snoozed(id, 7);
    }
}

void ReviewPage::deleteCurrent() {
    if (const core::InboxItem* item = current()) {
        if (snapshot_.movement(item->refId) != nullptr) emit movementDeleted(item->refId);
    }
}

bool ReviewPage::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (watched == category_->lineEdit() && category_->lineEdit()->completer() &&
                category_->lineEdit()->completer()->popup()->isVisible()) {
                return false;
            }
            acceptCurrent();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace dake::ui
