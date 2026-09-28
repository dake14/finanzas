// ui/movementspage.cpp — el historial completo.
//
// Dos columnas que la lista actual no tiene y que son las que permiten
// auditar: DE DONDE salio cada peso, y CUANTO DURA lo que se compro. Sin la
// primera, "Compra PLA Blanco 4Kg · 46,00" no dice si esa plata era del
// negocio o del ahorro, que es justo la pregunta que quedo sin respuesta.

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <algorithm>

#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString kindLabel(core::MovementKind kind) {
    switch (kind) {
        case core::MovementKind::Ingreso: return QStringLiteral("Ingreso");
        case core::MovementKind::Gasto: return QStringLiteral("Gasto");
        case core::MovementKind::Traspaso: return QStringLiteral("Traspaso");
    }
    return {};
}

[[nodiscard]] theme::Tono kindColor(core::MovementKind kind) {
    switch (kind) {
        case core::MovementKind::Ingreso: return theme::kPositive;
        case core::MovementKind::Gasto: return theme::kNegative;
        case core::MovementKind::Traspaso: return theme::kAhorro;
    }
    return theme::kText;
}

} // namespace

MovementsPage::MovementsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void MovementsPage::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto* headerRow = new QHBoxLayout();
    auto* heading = new QLabel(QStringLiteral("Movimientos"), this);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    headerRow->addWidget(heading);
    headerRow->addStretch(1);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText(QStringLiteral("Buscar por nombre, categoria o bolsillo"));
    search_->setFont(theme::bodyFont(9));
    search_->setFixedHeight(34);
    search_->setMinimumWidth(300);
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, [this] { refill(); });
    headerRow->addWidget(search_);
    layout->addLayout(headerRow);

    summary_ = new QLabel(this);
    summary_->setFont(theme::bodyFont(10));
    theme::setLabelColor(summary_, theme::kTextMuted);
    layout->addWidget(summary_);

    table_ = makeTable({QStringLiteral("Fecha"), QStringLiteral("Que fue"),
                        QStringLiteral("Tipo"), QStringLiteral("Bolsillo"),
                        QStringLiteral("Categoria"), QStringLiteral("Monto")},
                       1);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = table_->item(row, 0);
        if (item != nullptr) {
            emit movementActivated(item->data(Qt::UserRole).toString().toStdString());
        }
    });
    layout->addWidget(table_, 1);
}

void MovementsPage::focusSearch() {
    search_->setFocus(Qt::ShortcutFocusReason);
    search_->selectAll();
}

void MovementsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;
    refill();
}

void MovementsPage::refill() {
    const QString needle = search_->text().trimmed().toLower();

    std::vector<core::Movement> visible;
    for (const core::Movement& movement : snapshot_.movements) {
        if (movement.deleted) {
            continue;
        }
        if (!needle.isEmpty()) {
            const QString haystack =
                (QString::fromStdString(movement.name) + QLatin1Char(' ') +
                 QString::fromStdString(movement.category) + QLatin1Char(' ') +
                 snapshot_.pocketName(movement.pocketId) + QLatin1Char(' ') +
                 snapshot_.jobName(movement.jobId))
                    .toLower();
            if (!haystack.contains(needle)) {
                continue;
            }
        }
        visible.push_back(movement);
    }

    // Del mas nuevo al mas viejo, con desempate estable por id: sin el, las
    // filas del mismo dia saltan de lugar en cada refresco.
    std::sort(visible.begin(), visible.end(),
              [](const core::Movement& a, const core::Movement& b) {
                  if (a.date != b.date) {
                      return b.date < a.date;
                  }
                  return a.id > b.id;
              });

    table_->setRowCount(static_cast<int>(visible.size()));
    for (int row = 0; row < static_cast<int>(visible.size()); ++row) {
        const core::Movement& movement = visible[static_cast<std::size_t>(row)];

        setText(table_, row, 0, QString::fromStdString(movement.date.toIso()),
                theme::kTextMuted);
        table_->item(row, 0)->setData(Qt::UserRole, QString::fromStdString(movement.id));

        QString name = QString::fromStdString(movement.name);
        if (!movement.settled) {
            name += QStringLiteral("  · sin cobrar");
        }
        setText(table_, row, 1, name);

        setText(table_, row, 2, kindLabel(movement.kind), kindColor(movement.kind));

        // De donde salio y a donde fue, en una sola celda: en un traspaso, la
        // mitad de la informacion es el destino.
        QString pocket = snapshot_.pocketName(movement.pocketId);
        if (movement.kind == core::MovementKind::Traspaso) {
            pocket += QStringLiteral(" → ") + snapshot_.pocketName(movement.targetPocketId);
        }
        setText(table_, row, 3, pocket, theme::kTextMuted);

        setText(table_, row, 4, QString::fromStdString(movement.category), theme::kTextMuted);

        const core::Money amount =
            core::Money::fromMinor(movement.amountMinor, snapshot_.currency);
        const QString sign = movement.kind == core::MovementKind::Ingreso
                                 ? QStringLiteral("+")
                                 : (movement.kind == core::MovementKind::Gasto
                                        ? QStringLiteral("−")
                                        : QString());
        setNumber(table_, row, 5, sign + theme::formatMoney(amount), kindColor(movement.kind));
    }

    summary_->setText(QStringLiteral("%1 movimientos. Doble clic para editar o borrar.")
                          .arg(visible.size()));
}

} // namespace dake::ui
