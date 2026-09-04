#pragma once
//
// ui/quickentry.hpp — la barra de carga rapida.
//
// Es la pantalla que mas se usa y la que decide si la aplicacion se sigue
// usando en marzo. Cuatro cosas que la version actual no tiene, y que son
// exactamente las cuatro que obligan a corregir a mano despues:
//
//   1. FECHA. Hoy la carga rapida estampa la fecha de hoy y punto. La plata se
//      gasta en la calle y se anota en la computadora dos dias despues; sin un
//      campo de fecha hay que agregar el movimiento y despues abrirlo para
//      corregirlo. Aca hay dos botones —Hoy, Ayer— y un calendario.
//   2. BOLSILLO. De donde sale o a donde entra. Sin esto ningun saldo se puede
//      cuadrar contra la realidad.
//   3. TRABAJO. A que reparacion o encargo pertenece. Es lo unico que despues
//      permite saber cuanto dejo cada uno.
//   4. CUANTO DURA. Un rollo de filamento no se consume el dia que se compra.
//
// Nada de eso es obligatorio: con nombre y monto y Enter alcanza, y los
// desplegables conservan lo ultimo elegido, que es como se cargan cinco
// movimientos seguidos sin tocar el mouse.
//
#include <QWidget>

#include "dake/core/model.hpp"
#include "snapshot.hpp"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace dake::ui {

class CategoryBox;

class QuickEntry : public QWidget {
    Q_OBJECT

public:
    explicit QuickEntry(QWidget* parent = nullptr);

    /// Recarga bolsillos, trabajos y categorias conservando lo que el usuario
    /// tenga elegido: refrescar la pantalla no puede costarle la seleccion.
    void setSnapshot(const Snapshot& snapshot);

    void focusName();

signals:
    /// El movimiento viene sin id ni deviceId: eso lo pone quien guarda.
    void submitted(const dake::core::Movement& draft);

private slots:
    void submit();

private:
    void buildUi();
    void applyKind();
    void showError(const QString& message);
    void clearError();
    [[nodiscard]] core::MovementKind currentKind() const;

    Snapshot snapshot_;

    QGridLayout* grid_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* amountEdit_ = nullptr;
    QPushButton* expenseButton_ = nullptr;
    QPushButton* incomeButton_ = nullptr;
    QPushButton* transferButton_ = nullptr;
    QPushButton* addButton_ = nullptr;

    QPushButton* todayButton_ = nullptr;
    QPushButton* yesterdayButton_ = nullptr;
    QDateEdit* dateEdit_ = nullptr;

    QLabel* pocketLabel_ = nullptr;
    QComboBox* pocketBox_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    QComboBox* targetBox_ = nullptr;

    CategoryBox* categoryBox_ = nullptr;
    QComboBox* jobBox_ = nullptr;
    QComboBox* spreadBox_ = nullptr;
    QCheckBox* unsettledCheck_ = nullptr;

    QLabel* errorLabel_ = nullptr;

    /// La fecha se pone en "hoy" la PRIMERA vez que llegan datos y despues no
    /// se toca. Si se reescribiera en cada refresco, back-datar un movimiento
    /// duraria hasta que se guardara el anterior.
    bool dateInitialized_ = false;
};

} // namespace dake::ui
