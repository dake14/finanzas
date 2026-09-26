#pragma once
//
// ui/tables.hpp — la tabla del proyecto, en un solo lugar.
//
// Tres pantallas muestran listas y las tres necesitan lo mismo: sin edicion
// directa, seleccion por fila, cabecera fija y las cifras a la derecha en
// tipografia de ancho fijo. Alineadas por la coma se comparan de un vistazo,
// que es exactamente para lo que se miran.
//
#include <QColor>
#include <QComboBox>
#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "theme.hpp"

namespace dake::ui {

/// `stretchColumn` es la columna que se come el espacio sobrante; el resto se
/// ajusta a su contenido. Repartir el ancho en partes iguales, que es el
/// comportamiento por defecto, deja "Lote de impresiones por enc..." al lado de
/// una columna de fechas con la mitad vacia.
[[nodiscard]] inline QTableWidget* makeTable(const QStringList& headers,
                                             int stretchColumn = 0) {
    auto* table = new QTableWidget(0, static_cast<int>(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
    table->horizontalHeader()->setFont(theme::bodyFont(9, QFont::DemiBold));
    table->setFont(theme::bodyFont(9));
    table->verticalHeader()->setDefaultSectionSize(30);
    return table;
}

inline void setText(QTableWidget* table, int row, int column, const QString& text,
                    const QColor& color = theme::kText) {
    auto* item = new QTableWidgetItem(text);
    item->setForeground(color);
    table->setItem(row, column, item);
}

inline void setNumber(QTableWidget* table, int row, int column, const QString& text,
                      const QColor& color = theme::kText) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    item->setFont(theme::numericFont(9));
    item->setForeground(color);
    table->setItem(row, column, item);
}

/// Ancho fijo para una columna que lleva un boton o un desplegable.
/// ResizeToContents mide el texto de la celda, no el widget que tiene
/// encima, y el widget queda cortado ("uadr" en vez de "Cuadrar").
inline void fixColumn(QTableWidget* table, int column, int width) {
    table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
    table->setColumnWidth(column, width);
}

/// Un desplegable que entra en una fila de tabla. El estilo general le pone
/// 7 px de relleno, que en una fila de 30 px se come el texto.
[[nodiscard]] inline QComboBox* cellCombo(QTableWidget* table) {
    auto* combo = new QComboBox(table);
    combo->setStyleSheet(QStringLiteral("QComboBox { padding: 0px 8px; border-radius: 6px; min-height: 22px; }"));
    combo->setFont(theme::bodyFont(9));
    return combo;
}

} // namespace dake::ui
