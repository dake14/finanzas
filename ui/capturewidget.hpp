#pragma once
//
// ui/capturewidget.hpp — anotar escribiendo una linea.
//
// "25 almuerzo" y Enter. El interprete de dake::core deduce el resto y la
// vista previa lo muestra mientras se escribe, asi que antes de guardar se ve
// exactamente que va a quedar. Si algo salio mal, Tab lleva a ese campo y se
// corrige sin tocar el mouse; lo corregido a mano ya no lo pisa el interprete.
//
// Es el mismo widget en la ventana mini del atajo global y arriba de la
// pantalla Hoy: una sola forma de anotar, no dos.
//
#include <QElapsedTimer>
#include <QWidget>

#include "dake/core/capture.hpp"
#include "snapshot.hpp"

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;

namespace dake::ui {

class CategoryBox;

class CaptureWidget : public QWidget {
    Q_OBJECT

public:
    explicit CaptureWidget(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

    void focusInput();

    /// Escribe una linea como si se hubiera tipeado. Lo usa el banco visual.
    void setInput(const QString& text);

    /// Deja la linea vacia y olvida lo corregido a mano. El cronometro arranca
    /// de nuevo con la proxima tecla.
    void reset();

    /// Arranca el cronometro ahora. La ventana mini lo llama al abrirse: para
    /// quien usa el atajo, anotar empieza en ese momento.
    void startClock();

signals:
    /// `newCategory` trae nombre solo si la categoria no existia: con la cuenta
    /// que se eligio en la vista previa. `elapsedMs` es lo que tardo la
    /// captura; `keepOpen`, si fue Shift+Enter.
    void submitted(const dake::core::Movement& movement, const dake::core::Category& newCategory,
                   qint64 elapsedMs, bool keepOpen);
    void cancelled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void reparse();
    void fillPockets();
    /// Las categorias del tipo elegido, de la mas usada a la menos.
    void refreshSuggestions();
    void updateAccountButton();
    void submit(bool keepOpen);
    void showHint(const QString& text, bool error);

    [[nodiscard]] bool isNewCategory() const;
    [[nodiscard]] core::Account currentCategoryAccount() const;

    Snapshot snapshot_;
    core::CaptureContext context_;
    core::CaptureDraft draft_;

    QLineEdit* input_ = nullptr;
    QLabel* amount_ = nullptr;
    QComboBox* kind_ = nullptr;
    CategoryBox* category_ = nullptr;
    QLabel* source_ = nullptr;
    QPushButton* account_ = nullptr;
    QComboBox* pocket_ = nullptr;
    QDateEdit* date_ = nullptr;
    QComboBox* job_ = nullptr;
    QLabel* hint_ = nullptr;

    // Lo que se corrigio a mano. El interprete deja de tocar ese campo hasta
    // el proximo reset(): corregir y seguir escribiendo no puede deshacer la
    // correccion.
    bool manualKind_ = false;
    bool manualCategory_ = false;
    bool manualPocket_ = false;
    bool manualDate_ = false;
    bool manualJob_ = false;
    bool manualAccount_ = false;
    core::Account newCategoryAccount_ = core::Account::Negocio;

    /// Rellenar un campo desde el interprete no es una correccion a mano.
    bool filling_ = false;

    QElapsedTimer clock_;
};

} // namespace dake::ui
