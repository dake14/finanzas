// ui/settingspage.cpp — lo que se configura una vez.
//
// Nada de esto hace falta para empezar: cada valor tiene su default, y la
// pantalla existe para corregir el que no sirva.

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QSignalBlocker>
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

    auto* captureCard = new Card(QStringLiteral("CAPTURA"), page);
    captureCard->setSubtitle(
        QStringLiteral("El atajo abre la ventana de anotar desde cualquier programa. Para que "
                       "funcione, la aplicación tiene que estar abierta: por eso arranca con "
                       "Windows y al cerrarla queda en la bandeja del sistema."));
    auto* hotkeyRow = new QWidget(captureCard);
    auto* hotkeyLayout = new QHBoxLayout(hotkeyRow);
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->setSpacing(10);
    auto* hotkeyLabel = new QLabel(QStringLiteral("Atajo"), hotkeyRow);
    hotkeyLabel->setFont(theme::bodyFont(10));
    theme::setLabelColor(hotkeyLabel, theme::kTextMuted);
    hotkey_ = new QKeySequenceEdit(hotkeyRow);
    hotkey_->setMaximumSequenceLength(1);
    hotkey_->setFixedWidth(200);
    connect(hotkey_, &QKeySequenceEdit::editingFinished, this, [this] {
        if (!hotkey_->keySequence().isEmpty()) {
            emit hotkeyChanged(hotkey_->keySequence());
        }
    });
    hotkeyStatus_ = new QLabel(hotkeyRow);
    hotkeyStatus_->setFont(theme::bodyFont(9));
    hotkeyLayout->addWidget(hotkeyLabel);
    hotkeyLayout->addWidget(hotkey_);
    hotkeyLayout->addWidget(hotkeyStatus_, 1);
    captureCard->addContent(hotkeyRow);

    autostart_ = new QCheckBox(QStringLiteral("Arrancar con Windows, en la bandeja del sistema"),
                               captureCard);
    autostart_->setFont(theme::bodyFont(10));
    connect(autostart_, &QCheckBox::toggled, this, &SettingsPage::autostartChanged);
    captureCard->addContent(autostart_);

    captureTiming_ = new QLabel(captureCard);
    captureTiming_->setFont(theme::bodyFont(10));
    captureTiming_->setWordWrap(true);
    captureCard->addContent(captureTiming_);
    layout->addWidget(captureCard);

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

    {
        const QSignalBlocker blockHotkey(hotkey_);
        hotkey_->setKeySequence(QKeySequence(snapshot.hotkey));
        const QSignalBlocker blockAutostart(autostart_);
        autostart_->setChecked(snapshot.autostart);
    }
    if (snapshot.hotkeyRegistered) {
        hotkeyStatus_->setText(QStringLiteral("activo"));
        theme::setLabelColor(hotkeyStatus_, theme::kPositive);
    } else {
        hotkeyStatus_->setText(QStringLiteral("no se pudo registrar: otro programa lo usa o la "
                                              "tecla no se admite; elige otra."));
        theme::setLabelColor(hotkeyStatus_, theme::kNegative);
    }
    // El criterio de aceptacion, medido con el uso de verdad.
    if (snapshot.captureMedianMs < 0) {
        captureTiming_->setText(QStringLiteral("Todavía no hay capturas medidas."));
        theme::setLabelColor(captureTiming_, theme::kTextMuted);
    } else {
        const double seconds = static_cast<double>(snapshot.captureMedianMs) / 1000.0;
        const bool ok = snapshot.captureMedianMs < 10000;
        captureTiming_->setText(
            QStringLiteral("Anotar te toma %1 s (mediana de las últimas 30). La meta es menos de 10.")
                .arg(QString::number(seconds, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(','))));
        theme::setLabelColor(captureTiming_, ok ? theme::kPositive : theme::kNegative);
    }

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
