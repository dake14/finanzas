#pragma once
//
// ui/categorybox.hpp — campo de categoria con memoria.
//
// Un QComboBox editable con dos particularidades:
//
//  * El desplegable se llena con las categorias que el usuario ya uso, en el
//    orden en que se las pasan (quien las provee ya decidio el criterio; este
//    widget no reordena nada).
//  * Al escribir, sugiere por SUBCADENA y no por prefijo: tipear "mer" tiene
//    que ofrecer "Supermercado". Un completador de prefijo obligaria a recordar
//    como empieza cada categoria, que es justo lo que este campo evita.
//
// Sigue aceptando texto libre: si lo escrito no esta en la lista, se devuelve
// tal cual y quien guarda decide que hacer con ello.
//
#include <QComboBox>
#include <QStringList>

class QCompleter;
class QStringListModel;

namespace dake::ui {

class CategoryBox : public QComboBox {
    Q_OBJECT

public:
    explicit CategoryBox(QWidget* parent = nullptr);

    /// Reemplaza las sugerencias conservando lo que el usuario tenga escrito.
    /// Se llama cada vez que cambian los datos o el tipo de movimiento, y no
    /// puede borrar una categoria a medio tipear.
    void setSuggestions(const QStringList& suggestions);

    /// Texto actual sin espacios sobrantes.
    [[nodiscard]] QString category() const;

    /// Fija el texto sin exigir que exista en la lista.
    void setCategory(const QString& text);

private:
    /// Modelo propio del completador, separado del modelo del combo. Cambiar
    /// los items del combo reinicia su completador interno; tener uno propio
    /// evita que el filtrado por subcadena se pierda en cada recarga.
    QStringListModel* completionModel_ = nullptr;
    QCompleter* completer_ = nullptr;
};

} // namespace dake::ui
