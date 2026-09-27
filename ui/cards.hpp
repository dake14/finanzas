#pragma once
//
// ui/cards.hpp — contenedores visuales reutilizables.
//
// `Card` es la caja con borde redondeado que agrupa cualquier contenido del
// tablero. `KpiCard` es la variante para una sola cifra grande.
//
#include <QFrame>
#include <QString>

#include "theme.hpp"

class QLabel;
class QVBoxLayout;

namespace dake::ui {

/// Caja con titulo opcional y un area de contenido vertical.
class Card : public QFrame {
    Q_OBJECT

public:
    explicit Card(const QString& title, QWidget* parent = nullptr);

    /// Agrega un widget al cuerpo de la tarjeta, debajo del titulo.
    void addContent(QWidget* widget, int stretch = 0);

    void setSubtitle(const QString& text);

private:
    QVBoxLayout* body_ = nullptr;
    QLabel* subtitle_ = nullptr;
};

/// Tarjeta de una sola cifra: rotulo arriba, valor grande al centro y una nota
/// de contexto abajo. La franja de color a la izquierda identifica la metrica
/// de un vistazo sin tener que leer el rotulo.
class KpiCard : public QFrame {
    Q_OBJECT

public:
    KpiCard(const QString& title, theme::Tono accent, QWidget* parent = nullptr);

    void setValue(const QString& value);
    void setNote(const QString& note, theme::Tono color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// El papel de la franja, no su color: se resuelve en cada paintEvent.
    theme::Tono accent_;
    QLabel* title_ = nullptr;
    QLabel* value_ = nullptr;
    QLabel* note_ = nullptr;
};

} // namespace dake::ui
