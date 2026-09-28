#pragma once
//
// ui/entryform.hpp — anotar: tipo, monto, categoria, bolsillo y fecha.
//
// Sin nada de por medio. No hay interprete que adivine a partir de una frase:
// se elige el tipo, se escribe el monto, se elige la categoria (o se escribe
// una nueva) y Enter. El bolsillo arranca en el ultimo que se uso para ese
// tipo y la fecha en hoy, y los dos se pueden cambiar.
//
// Es el mismo widget arriba de Hoy y en la ventana chica del atajo global.
// No guarda nada: emite `submitted` y la ventana principal guarda, que es el
// unico camino de escritura de la aplicacion.
//
#include <QDate>
#include <QWidget>

#include "snapshot.hpp"

class QComboBox;
class QGridLayout;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;

namespace dake::ui {

class CategoryBox;

class EntryForm : public QWidget {
    Q_OBJECT

public:
    enum class Kind { Gasto, Ingreso, Traspaso };

    explicit EntryForm(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

    /// Foco en el monto, con lo escrito seleccionado.
    void focusAmount();

    void setKind(Kind kind);
    [[nodiscard]] Kind kind() const noexcept { return kind_; }

signals:
    /// Listo para guardar. `newCategory` trae nombre solo si la categoria no
    /// existia todavia. `keepOpen`: fue Shift+Enter.
    void submitted(const dake::core::Movement& movement, const dake::core::Category& newCategory,
                   bool keepOpen);
    void cancelled();

public slots:
    /// Lo llama la ventana despues de guardar: vacia el monto, deja tipo,
    /// categoria y bolsillo como estaban, y dice que quedo anotado.
    void confirmSaved(const QString& summary);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void applyKind();
    void fillPockets();
    void refreshCategories();
    /// El bolsillo por defecto del tipo elegido: el ultimo usado para ese
    /// tipo, o el que propone el nucleo.
    void selectDefaultPockets();
    void submit(bool keepOpen);
    void showError(const QString& text);

    Snapshot snapshot_;
    Kind kind_ = Kind::Gasto;

    QPushButton* gasto_ = nullptr;
    QPushButton* ingreso_ = nullptr;
    QPushButton* traspaso_ = nullptr;
    QLineEdit* amount_ = nullptr;
    QLabel* categoryLabel_ = nullptr;
    CategoryBox* category_ = nullptr;
    QLabel* pocketLabel_ = nullptr;
    QComboBox* pocket_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    QComboBox* target_ = nullptr;
    QDateEdit* date_ = nullptr;
    QPushButton* submit_ = nullptr;
    QLabel* error_ = nullptr;
    QLabel* done_ = nullptr;
    QGridLayout* grid_ = nullptr;
    /// El "hoy" que se mostro la ultima vez: si la fecha sigue ahi, sigue a hoy.
    QDate shownToday_;
};

} // namespace dake::ui
