#pragma once
//
// ui/dialogs.hpp — los formularios que no entran en la barra de carga rapida.
//
// Todos devuelven una entidad del dominio y NO escriben en la base: quien
// guarda es la ventana principal, por un solo camino. Con eso, la validacion
// de la forma del movimiento sigue siendo el unico portal de escritura.
//
#include <QDialog>
#include <optional>

#include "dake/core/model.hpp"
#include "snapshot.hpp"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QDateEdit;
class QLabel;
class QLineEdit;

namespace dake::ui {

class CategoryBox;

/// Alta de un bolsillo. El saldo inicial es obligatorio de hecho aunque se
/// pueda dejar en cero: sin el, el saldo de la aplicacion nunca va a coincidir
/// con lo que hay en la mano, y un saldo que no coincide no se mira.
class PocketDialog : public QDialog {
    Q_OBJECT

public:
    explicit PocketDialog(core::Currency currency, QWidget* parent = nullptr);
    [[nodiscard]] std::optional<core::Pocket> result() const;

private:
    core::Currency currency_;
    QLineEdit* name_ = nullptr;
    QComboBox* kind_ = nullptr;
    QLineEdit* opening_ = nullptr;
    QLabel* hint_ = nullptr;
    QLabel* emergencyNote_ = nullptr;  ///< "EmergencyNote": primero el APK nuevo
};

/// Cuadrar un bolsillo contra la realidad.
///
/// La diferencia NO se aplica moviendo el saldo: se anota como un movimiento
/// con su fecha y su nombre. Un ajuste invisible tapa el problema que lo
/// causo, y el mes siguiente hay que ajustar otra vez.
class ReconcileDialog : public QDialog {
    Q_OBJECT

public:
    ReconcileDialog(const core::Pocket& pocket,
                    const core::Money& computed,
                    core::Date today,
                    QWidget* parent = nullptr);

    /// nullopt si no hay diferencia o si el dato no es valido.
    [[nodiscard]] std::optional<core::Movement> result() const;

private:
    core::Pocket pocket_;
    core::Money computed_;
    core::Date today_;
    QLineEdit* real_ = nullptr;
    QLabel* difference_ = nullptr;
};

/// Editar o borrar un movimiento ya cargado.
class MovementEditor : public QDialog {
    Q_OBJECT

public:
    MovementEditor(const core::Movement& original, const Snapshot& snapshot,
                   QWidget* parent = nullptr);

    [[nodiscard]] bool wasDeleted() const noexcept { return deleted_; }
    [[nodiscard]] core::Movement result() const { return edited_; }

private slots:
    void save();
    void remove();

private:
    void applyKind();

    core::Movement base_;
    core::Movement edited_;
    Snapshot snapshot_;
    bool deleted_ = false;

    QLineEdit* name_ = nullptr;
    QLineEdit* amount_ = nullptr;
    QDateEdit* date_ = nullptr;
    QComboBox* pocket_ = nullptr;
    QComboBox* target_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    CategoryBox* category_ = nullptr;
    QComboBox* job_ = nullptr;
    QFormLayout* form_ = nullptr;
    QCheckBox* settled_ = nullptr;
    /// Cuando se cobro o se pago. Solo se habilita con `settled_` marcado, y
    /// arranca en la fecha del movimiento y no en la de hoy: el caso comun es
    /// anotar algo que ya estaba cobrado, y ofrecer hoy invitaria a dejar una
    /// fecha equivocada de un clic.
    QDateEdit* settledDate_ = nullptr;
    class QLabel* settledDateLabel_ = nullptr;
    QLabel* error_ = nullptr;
};

} // namespace dake::ui
