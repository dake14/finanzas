#include "bankdialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "tables.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

/// "A · Fecha" para cada columna, con la cabecera del archivo si tiene.
void fillColumns(QComboBox* box, const std::vector<std::vector<std::string>>& csv, bool allowNone) {
    box->clear();
    if (allowNone) box->addItem(QStringLiteral("—"), -1);
    const std::size_t columns = csv.empty() ? 0 : csv.front().size();
    for (std::size_t i = 0; i < columns; ++i) {
        const QString letter = QString(QChar('A' + static_cast<int>(i % 26)));
        box->addItem(letter + QStringLiteral(" · ") + QString::fromStdString(csv.front()[i]).left(24),
                     static_cast<int>(i));
    }
}

void select(QComboBox* box, int column) {
    const int index = box->findData(column);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

BankImportDialog::BankImportDialog(const Snapshot& snapshot, const QString& fileName, const QString& text,
                                   std::vector<core::BankProfile> profiles, QWidget* parent)
    : QDialog(parent), snapshot_(snapshot), profiles_(std::move(profiles)) {
    setWindowTitle(QStringLiteral("Importar ") + fileName);
    setModal(true);
    resize(1060, 680);

    const std::string raw = text.toStdString();
    csv_ = core::parseCsv(raw, core::detectSeparator(raw));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* form = new QFormLayout();
    pocket_ = new QComboBox(this);
    for (const core::Pocket& p : snapshot.pockets) {
        if (!p.archived) pocket_->addItem(QString::fromStdString(p.name), QString::fromStdString(p.id));
    }
    form->addRow(QStringLiteral("Bolsillo de esta cuenta"), pocket_);

    auto* profileRow = new QHBoxLayout();
    profileBox_ = new QComboBox(this);
    for (const core::BankProfile& p : profiles_) profileBox_->addItem(QString::fromStdString(p.name));
    profileBox_->addItem(QStringLiteral("Nuevo perfil…"));
    profileName_ = new QLineEdit(this);
    profileName_->setPlaceholderText(QStringLiteral("Nombre del banco, para la próxima vez"));
    profileRow->addWidget(profileBox_);
    profileRow->addWidget(profileName_, 1);
    form->addRow(QStringLiteral("Perfil"), profileRow);

    auto* columns = new QHBoxLayout();
    dateColumn_ = new QComboBox(this);
    descriptionColumn_ = new QComboBox(this);
    amountColumn_ = new QComboBox(this);
    debitColumn_ = new QComboBox(this);
    creditColumn_ = new QComboBox(this);
    fillColumns(dateColumn_, csv_, false);
    fillColumns(descriptionColumn_, csv_, false);
    fillColumns(amountColumn_, csv_, true);
    fillColumns(debitColumn_, csv_, true);
    fillColumns(creditColumn_, csv_, true);
    for (auto [label, box] : {std::pair{QStringLiteral("fecha"), dateColumn_},
                              std::pair{QStringLiteral("descripción"), descriptionColumn_},
                              std::pair{QStringLiteral("monto"), amountColumn_},
                              std::pair{QStringLiteral("o cargo"), debitColumn_},
                              std::pair{QStringLiteral("y abono"), creditColumn_}}) {
        auto* l = new QLabel(label, this);
        theme::setLabelColor(l, theme::kTextMuted);
        columns->addWidget(l);
        columns->addWidget(box);
        connect(box, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    }
    form->addRow(QStringLiteral("Columnas"), columns);

    auto* options = new QHBoxLayout();
    dateFormat_ = new QComboBox(this);
    dateFormat_->addItems({QStringLiteral("día/mes/año"), QStringLiteral("año-mes-día"), QStringLiteral("mes/día/año")});
    negativeIsExpense_ = new QCheckBox(QStringLiteral("negativo es gasto"), this);
    headerRows_ = new QSpinBox(this);
    headerRows_->setRange(0, 20);
    auto* headerLabel = new QLabel(QStringLiteral("filas de cabecera"), this);
    theme::setLabelColor(headerLabel, theme::kTextMuted);
    options->addWidget(dateFormat_);
    options->addWidget(negativeIsExpense_);
    options->addWidget(headerLabel);
    options->addWidget(headerRows_);
    options->addStretch(1);
    form->addRow(QStringLiteral("Formato"), options);
    connect(dateFormat_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(negativeIsExpense_, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(headerRows_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    layout->addLayout(form);

    summary_ = new QLabel(this);
    summary_->setFont(theme::bodyFont(11, QFont::DemiBold));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);

    table_ = makeTable({QStringLiteral("Fecha"), QStringLiteral("Descripción"), QStringLiteral("Monto"),
                        QStringLiteral("Qué pasa"), QStringLiteral("Categoría")},
                       1);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    layout->addWidget(table_, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), this);
    cancel->setObjectName(QStringLiteral("GhostButton"));
    auto* accept = new QPushButton(QStringLiteral("Importar"), this);
    accept->setObjectName(QStringLiteral("PrimaryButton"));
    accept->setDefault(true);
    for (QPushButton* b : {cancel, accept}) {
        b->setFixedHeight(34);
        b->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(b);
    }
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(accept, &QPushButton::clicked, this, &QDialog::accept);
    layout->addLayout(buttons);

    connect(profileBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < static_cast<int>(profiles_.size())) {
            applyProfile(profiles_[static_cast<std::size_t>(index)]);
        }
    });
    applyProfile(profiles_.empty() ? core::BankProfile{} : profiles_.front());
}

void BankImportDialog::applyProfile(const core::BankProfile& p) {
    filling_ = true;
    profileName_->setText(QString::fromStdString(p.name));
    select(dateColumn_, p.dateColumn);
    select(descriptionColumn_, p.descriptionColumn);
    select(amountColumn_, p.amountColumn);
    select(debitColumn_, p.debitColumn);
    select(creditColumn_, p.creditColumn);
    dateFormat_->setCurrentIndex(static_cast<int>(p.dateFormat));
    negativeIsExpense_->setChecked(p.negativeIsExpense);
    headerRows_->setValue(p.headerRows);
    filling_ = false;
    refresh();
}

core::BankProfile BankImportDialog::profile() const {
    core::BankProfile p;
    p.name = profileName_->text().trimmed().toStdString();
    p.dateColumn = dateColumn_->currentData().toInt();
    p.descriptionColumn = descriptionColumn_->currentData().toInt();
    p.amountColumn = amountColumn_->currentData().toInt();
    p.debitColumn = debitColumn_->currentData().toInt();
    p.creditColumn = creditColumn_->currentData().toInt();
    p.dateFormat = static_cast<core::DateFormat>(dateFormat_->currentIndex());
    p.negativeIsExpense = negativeIsExpense_->isChecked();
    p.headerRows = headerRows_->value();
    return p;
}

core::Id BankImportDialog::pocketId() const {
    return pocket_->currentData().toString().toStdString();
}

void BankImportDialog::refresh() {
    if (filling_) return;
    const core::BankRead read = core::readBankRows(csv_, profile(), snapshot_.currency);
    matches_ = core::matchBankRows(read.rows, snapshot_.movements, snapshot_.metas);

    int fresh = 0;
    int linked = 0;
    int done = 0;
    table_->setRowCount(static_cast<int>(matches_.size()));
    for (int row = 0; row < static_cast<int>(matches_.size()); ++row) {
        const core::BankMatch& m = matches_[static_cast<std::size_t>(row)];
        setText(table_, row, 0, QString::fromStdString(m.row.date.toIso()), theme::kTextMuted);
        setText(table_, row, 1, QString::fromStdString(m.row.description));
        const core::Money amount = core::Money::fromMinor(m.row.amountMinor, snapshot_.currency);
        setNumber(table_, row, 2,
                  (m.row.kind == core::MovementKind::Gasto ? QStringLiteral("−") : QStringLiteral("+")) +
                      theme::formatMoney(amount),
                  m.row.kind == core::MovementKind::Gasto ? theme::kNegative : theme::kPositive);
        QString what;
        QColor color = theme::kText;
        switch (m.status) {
            case core::BankStatus::Nuevo: what = QStringLiteral("nuevo"); ++fresh; break;
            case core::BankStatus::YaAnotado: {
                const core::Movement* existing = snapshot_.movement(m.matchedMovementId);
                what = QStringLiteral("ya anotado: ") +
                       (existing != nullptr ? QString::fromStdString(existing->name) : QString());
                color = theme::kPositive;
                ++linked;
                break;
            }
            case core::BankStatus::YaImportado: what = QStringLiteral("ya importado"); color = theme::kTextFaint; ++done; break;
        }
        setText(table_, row, 3, what, color);
        for (int column = 0; column < 4; ++column) {
            table_->item(row, column)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        auto* category = new QTableWidgetItem(QString::fromStdString(m.suggestedCategory));
        if (m.status != core::BankStatus::Nuevo) category->setFlags(Qt::ItemIsEnabled);
        table_->setItem(row, 4, category);
    }
    summary_->setText(QStringLiteral("%1 nuevos · %2 ya anotados a mano (se enlazan, no se duplican) · "
                                     "%3 ya importados%4")
                          .arg(fresh)
                          .arg(linked)
                          .arg(done)
                          .arg(read.errors.empty() ? QString()
                                                   : QStringLiteral(" · %1 filas no se entendieron").arg(read.errors.size())));
}

std::vector<core::BankMatch> BankImportDialog::matches() const {
    std::vector<core::BankMatch> out = matches_;
    for (int row = 0; row < static_cast<int>(out.size()); ++row) {
        if (const QTableWidgetItem* item = table_->item(row, 4)) {
            out[static_cast<std::size_t>(row)].suggestedCategory = item->text().trimmed().toStdString();
        }
    }
    return out;
}

} // namespace dake::ui
