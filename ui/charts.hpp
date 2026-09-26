#pragma once
//
// ui/charts.hpp — las tres formas de grafica que usa la aplicacion.
//
// Se pintan a mano con QPainter y no con Qt Charts, por lo mismo que dice
// ui/CMakeLists.txt: son barras y una linea, y una dependencia mas seria mas
// codigo, no menos. Qt Charts ademas arrastra su propio motor de escenas para
// dibujar cuatro rectangulos.
//
// Tres formas y no cuatro, porque las cuatro graficas que pidio el usuario se
// reducen a tres:
//
//   resultado por mes        -> BarChart con una serie
//   ingresos contra costos   -> BarChart con dos series
//   caja acumulada           -> LineChart
//   gastos por categoria     -> RankChart
//
// Ninguna sabe de finanzas: reciben numeros ya calculados por dake::core y los
// dibujan. Esa frontera es la que permite probar las cuentas sin abrir una
// ventana.
//
#include <QString>
#include <QWidget>
#include <vector>

namespace dake::ui {

/// Un punto de cualquiera de las graficas.
struct ChartPoint {
    QString label;       ///< "ago 2026", "filamento"
    double primary = 0;  ///< la serie principal
    double secondary = 0;///< la segunda, si la grafica usa dos
    QString primaryText; ///< el valor ya formateado, para el tooltip y la etiqueta
};

/// Barras verticales, una o dos series.
///
/// Con `signed_` en true, el cero queda en el medio y las barras negativas
/// bajan: es lo que hace falta para "resultado por mes", donde un mes en
/// perdida tiene que VERSE como perdida y no como una barra corta.
class BarChart : public QWidget {
    Q_OBJECT

public:
    explicit BarChart(QWidget* parent = nullptr);

    void setData(std::vector<ChartPoint> points);
    /// Nombres de las series, para la leyenda. Con `second` vacio no se dibuja
    /// la segunda serie ni su leyenda.
    void setSeries(const QString& first, const QString& second = {});
    /// true = eje con cero en el medio y barras que pueden bajar.
    void setSigned(bool value);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<ChartPoint> points_;
    QString firstName_;
    QString secondName_;
    bool signed_ = false;
};

/// Una linea con puntos, para una serie que se lee como evolucion.
class LineChart : public QWidget {
    Q_OBJECT

public:
    explicit LineChart(QWidget* parent = nullptr);

    void setData(std::vector<ChartPoint> points);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<ChartPoint> points_;
};

/// Barras horizontales ordenadas de mayor a menor, con la etiqueta al lado.
///
/// Horizontal y no vertical porque las etiquetas son palabras —"filamento",
/// "electricidad"— y en vertical habria que rotarlas o cortarlas.
class RankChart : public QWidget {
    Q_OBJECT

public:
    explicit RankChart(QWidget* parent = nullptr);

    void setData(std::vector<ChartPoint> points);
    /// Cuantas filas como mucho. El resto se suma en una fila "otros": una
    /// lista de treinta categorias no se lee, y las que importan son las de
    /// arriba.
    void setLimit(int rows);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<ChartPoint> points_;
    int limit_ = 6;
};

/// Una fila por categoria: la barra es el mes elegido y la raya vertical, el
/// mes anterior. Al lado, la diferencia en numeros. Contesta "en que gaste mas
/// que el mes pasado" sin leer una tabla.
struct CompareRow {
    QString label;
    double current = 0;
    double previous = 0;
    QString currentText;
    QString changeText;  ///< "+12,00 ▲ 30%", ya formateado
    bool rose = false;   ///< subio: se pinta en rojo, porque es gasto
};

class CompareChart : public QWidget {
    Q_OBJECT

public:
    explicit CompareChart(QWidget* parent = nullptr);

    void setData(std::vector<CompareRow> rows, const QColor& barColor);
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<CompareRow> rows_;
    QColor barColor_;
};

} // namespace dake::ui
