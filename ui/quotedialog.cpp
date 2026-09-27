#include "quotedialog.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString reasonText(const core::QuotePlan& plan, const Snapshot& snapshot) {
    switch (plan.hold) {
        case core::QuoteHold::Duplicado:
            return QStringLiteral("Parece el mismo que %1: mismo cliente, mismo equipo, casi el "
                                  "mismo monto.")
                .arg(QString::fromStdString(plan.relatedNumber));
        case core::QuoteHold::YaAnotado: {
            for (const core::Movement& m : snapshot.movements) {
                if (m.id == plan.candidateMovementId) {
                    return QStringLiteral("Ya anotaste «%1» por %2 el %3: ¿es este cobro?")
                        .arg(QString::fromStdString(m.name),
                             theme::formatMoney(core::Money::fromMinor(m.amountMinor, snapshot.currency)),
                             QString::fromStdString(m.date.toIso()));
                }
            }
            return QStringLiteral("Hay un ingreso anotado a mano que parece este.");
        }
        case core::QuoteHold::EnCorreccion:
            return QStringLiteral("Volvió a borrador en Cotizaciones. Se actualiza cuando se vuelva "
                                  "a entregar; mientras, no se toca nada.");
        case core::QuoteHold::SinMonto:
            return QStringLiteral("El total es cero.");
        case core::QuoteHold::Ninguno:
            break;
    }
    return {};
}

} // namespace

QuoteReviewDialog::QuoteReviewDialog(const Snapshot& snapshot, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Documentos de Cotizaciones por revisar"));
    setMinimumWidth(900);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    auto* intro = new QLabel(
        QStringLiteral("Estos documentos no se importaron solos porque podrían contar un cobro dos "
                       "veces. La opción sugerida ya está elegida: Enter las aplica."),
        this);
    intro->setWordWrap(true);
    intro->setFont(theme::bodyFont(10));
    theme::setLabelColor(intro, theme::kTextMuted);
    layout->addWidget(intro);

    auto* table = makeTable({QStringLiteral("Documento"), QStringLiteral("Cliente · equipo"),
                             QStringLiteral("Total"), QStringLiteral("Por qué espera"),
                             QStringLiteral("Qué hacer")},
                            3);
    table->setWordWrap(true);
    fixColumn(table, 4, 290);
    layout->addWidget(table, 1);

    for (const core::QuotePlan& plan : snapshot.quotePlans) {
        if (plan.decision != core::QuoteDecision::Esperar) continue;
        const core::QuoteDoc* doc = snapshot.quoteDoc(plan.docId);
        const int row = table->rowCount();
        table->insertRow(row);
        setText(table, row, 0, QString::fromStdString(plan.number));
        setText(table, row, 1,
                doc != nullptr ? QString::fromStdString(doc->client + " · " + doc->device) : QString(),
                theme::kTextMuted);
        setNumber(table, row, 2,
                  doc != nullptr
                      ? theme::formatMoney(core::Money::fromMinor(doc->baseMinor, snapshot.currency))
                      : QString());
        setText(table, row, 3, reasonText(plan, snapshot), theme::kAviso);

        auto* choice = cellCombo(table);
        switch (plan.hold) {
            case core::QuoteHold::Duplicado:
                choice->addItem(QStringLiteral("Ignorarlo: es el mismo"), QStringLiteral("ignorar"));
                choice->addItem(QStringLiteral("Importarlo: es otro cobro"), QStringLiteral("nuevo"));
                break;
            case core::QuoteHold::YaAnotado:
                choice->addItem(QStringLiteral("Es ese: enlazarlos"),
                                QStringLiteral("enlace:") + QString::fromStdString(plan.candidateMovementId));
                choice->addItem(QStringLiteral("Es otro: importarlo aparte"), QStringLiteral("nuevo"));
                choice->addItem(QStringLiteral("Ignorar el documento"), QStringLiteral("ignorar"));
                break;
            case core::QuoteHold::SinMonto:
                choice->addItem(QStringLiteral("Ignorarlo"), QStringLiteral("ignorar"));
                choice->addItem(QStringLiteral("Esperar"), QString());
                break;
            case core::QuoteHold::EnCorreccion:
            case core::QuoteHold::Ninguno:
                choice->addItem(QStringLiteral("Esperar a que se entregue"), QString());
                break;
        }
        table->setCellWidget(row, 4, choice);
        rows_.push_back({plan.docId, choice});
    }
    table->resizeRowsToContents();

    if (rows_.empty()) {
        auto* none = new QLabel(QStringLiteral("No hay nada esperando."), this);
        none->setFont(theme::bodyFont(10));
        layout->addWidget(none);
    }

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), this);
    cancel->setObjectName(QStringLiteral("GhostButton"));
    auto* apply = new QPushButton(QStringLiteral("Aplicar"), this);
    apply->setObjectName(QStringLiteral("PrimaryButton"));
    apply->setDefault(true);
    for (QPushButton* b : {cancel, apply}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        b->setFont(theme::bodyFont(10, QFont::DemiBold));
        buttons->addWidget(b);
    }
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply, &QPushButton::clicked, this, &QDialog::accept);
    layout->addLayout(buttons);
    resize(980, 420);
}

core::QuoteDecisions QuoteReviewDialog::decisions() const {
    core::QuoteDecisions out;
    for (const Row& row : rows_) {
        const QString value = row.choice->currentData().toString();
        if (!value.isEmpty()) {
            out[row.docId] = value.toStdString();
        }
    }
    return out;
}

} // namespace dake::ui
