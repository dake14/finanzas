#pragma once
//
// ui/bankdialog.hpp — importar un extracto del banco.
//
// Arriba, a que bolsillo va y con que perfil se lee (que columna es la fecha,
// cual la descripcion, cual el monto). Abajo, lo que va a pasar con cada fila:
// nueva (con la categoria sugerida, que se puede cambiar), ya anotada a mano
// (se enlaza) o ya importada (no hace nada). Enter importa.
//
#include <QDialog>

#include "dake/core/bankcsv.hpp"
#include "snapshot.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace dake::ui {

class BankImportDialog : public QDialog {
    Q_OBJECT

public:
    BankImportDialog(const Snapshot& snapshot, const QString& fileName, const QString& text,
                     std::vector<core::BankProfile> profiles, QWidget* parent = nullptr);

    [[nodiscard]] core::BankProfile profile() const;
    [[nodiscard]] core::Id pocketId() const;
    /// Las filas como quedaron, con la categoria elegida en la tabla.
    [[nodiscard]] std::vector<core::BankMatch> matches() const;

private:
    void applyProfile(const core::BankProfile& profile);
    void refresh();

    Snapshot snapshot_;
    std::vector<std::vector<std::string>> csv_;
    std::vector<core::BankProfile> profiles_;
    std::vector<core::BankMatch> matches_;
    bool filling_ = false;

    QComboBox* pocket_ = nullptr;
    QComboBox* profileBox_ = nullptr;
    QLineEdit* profileName_ = nullptr;
    QComboBox* dateColumn_ = nullptr;
    QComboBox* descriptionColumn_ = nullptr;
    QComboBox* amountColumn_ = nullptr;
    QComboBox* debitColumn_ = nullptr;
    QComboBox* creditColumn_ = nullptr;
    QComboBox* dateFormat_ = nullptr;
    QCheckBox* negativeIsExpense_ = nullptr;
    QSpinBox* headerRows_ = nullptr;
    QLabel* summary_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace dake::ui
