// ui/settingspage.cpp — lo que se configura una vez.
//
// Nada de esto hace falta para empezar: cada valor tiene su default, y la
// pantalla existe para corregir el que no sirva.

#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "cards.hpp"
#include "pages.hpp"
#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

[[nodiscard]] QString classLabel(core::CategoryClass cls) {
    switch (cls) {
        case core::CategoryClass::General: return QStringLiteral("General");
        case core::CategoryClass::Fija: return QStringLiteral("Gasto fijo");
        case core::CategoryClass::Variable: return QStringLiteral("Consumible");
        case core::CategoryClass::Activo: return QStringLiteral("Herramienta");
    }
    return {};
}

constexpr core::CategoryClass kClasses[] = {core::CategoryClass::General,
                                            core::CategoryClass::Fija,
                                            core::CategoryClass::Variable,
                                            core::CategoryClass::Activo};

} // namespace

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void SettingsPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);
    auto* page = new QWidget(scroll);
    scroll->setWidget(page);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto* heading = new QLabel(QStringLiteral("Ajustes"), page);
    heading->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading, theme::kText);
    layout->addWidget(heading);

    auto* categoriesCard = new Card(QStringLiteral("CATEGORÍAS"), page);
    categoriesCard->setSubtitle(
        QStringLiteral("Se crean solas al escribirlas. La cuenta dice si un gasto es del negocio "
                       "o tuyo; la clase, cómo entra al costo de las reparaciones: los gastos "
                       "fijos se reparten por hora trabajada y las herramientas, por su vida "
                       "útil."));
    categories_ = makeTable({QStringLiteral("Categoría"), QStringLiteral("Tipo"),
                             QStringLiteral("Cuenta"), QStringLiteral("Clase")},
                            0);
    categories_->setMinimumHeight(260);
    fixColumn(categories_, 2, 130);
    fixColumn(categories_, 3, 150);
    categoriesCard->addContent(categories_);
    layout->addWidget(categoriesCard);
    layout->addStretch(1);
}

void SettingsPage::setSnapshot(const Snapshot& snapshot) {
    snapshot_ = snapshot;

    categories_->setRowCount(static_cast<int>(snapshot.categories.size()));
    for (int row = 0; row < static_cast<int>(snapshot.categories.size()); ++row) {
        const core::Category category = snapshot.categories[static_cast<std::size_t>(row)];
        setText(categories_, row, 0, QString::fromStdString(category.name));
        setText(categories_, row, 1,
                category.kind == core::MovementKind::Ingreso ? QStringLiteral("Ingreso")
                                                             : QStringLiteral("Gasto"),
                theme::kTextMuted);

        auto* account = cellCombo(categories_);
        account->addItems({QStringLiteral("Negocio"), QStringLiteral("Personal")});
        account->setCurrentIndex(category.account == core::Account::Personal ? 1 : 0);
        connect(account, &QComboBox::currentIndexChanged, this, [this, category](int index) {
            core::Category changed = category;
            changed.account = index == 1 ? core::Account::Personal : core::Account::Negocio;
            emit categoryChanged(changed);
        });
        categories_->setCellWidget(row, 2, account);

        auto* cls = cellCombo(categories_);
        for (const core::CategoryClass value : kClasses) {
            cls->addItem(classLabel(value));
        }
        cls->setCurrentIndex(static_cast<int>(category.cls));
        cls->setEnabled(category.kind == core::MovementKind::Gasto);
        connect(cls, &QComboBox::currentIndexChanged, this, [this, category](int index) {
            core::Category changed = category;
            changed.cls = kClasses[index];
            emit categoryChanged(changed);
        });
        categories_->setCellWidget(row, 3, cls);
    }
}

} // namespace dake::ui
