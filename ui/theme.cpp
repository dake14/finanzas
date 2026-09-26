#include "theme.hpp"

#include <QFontDatabase>
#include <QStringList>
#include <QWidget>

#include <cmath>
#include <cstdlib>
#include <optional>

namespace dake::ui::theme {
namespace {

/// Primera fuente disponible de la lista. Segoe UI existe en Windows 10/11 y
/// las de Apple en macOS; las demas son red de seguridad para que la app no
/// caiga en una fuente con serifas si algun dia corre en otra maquina.
///
/// El ultimo recurso es la fuente que el sistema ya eligio y no un nombre
/// escrito a mano: devolver "Segoe UI" en una Mac nombra una familia que no
/// existe, y ahi Qt resuelve por su cuenta con un resultado peor que el que se
/// intentaba evitar.
[[nodiscard]] QString pickFamily(const QStringList& candidates) {
    const QStringList available = QFontDatabase::families();
    for (const QString& candidate : candidates) {
        if (available.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }
    return QFont().family();
}

/// En macOS la familia de la interfaz no se pide por su nombre comercial: "SF
/// Pro" no figura en el catalogo de fuentes. El nombre que si resuelve al San
/// Francisco del sistema es ".AppleSystemUIFont", y Helvetica Neue queda
/// detras por si eso cambia.
[[nodiscard]] QString uiFamily() {
    static const QString family =
        pickFamily({QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI"),
                    QStringLiteral(".AppleSystemUIFont"), QStringLiteral("Helvetica Neue"),
                    QStringLiteral("Inter"), QStringLiteral("Arial")});
    return family;
}

/// Las cifras se pintan con ancho tabular para que las columnas de numeros
/// queden alineadas en las tablas y no bailen al actualizarse.
[[nodiscard]] QString numericFamily() {
    static const QString family =
        pickFamily({QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI"),
                    QStringLiteral(".AppleSystemUIFont"), QStringLiteral("Helvetica Neue"),
                    QStringLiteral("Consolas"), QStringLiteral("Arial")});
    return family;
}

/// Separa la parte entera con puntos de miles: 1234567 -> "1.234.567"
[[nodiscard]] QString groupThousands(qint64 value) {
    QString digits = QString::number(value);
    QString grouped;
    int count = 0;
    for (int i = digits.size() - 1; i >= 0; --i) {
        grouped.prepend(digits.at(i));
        if (++count % 3 == 0 && i > 0) {
            grouped.prepend(QLatin1Char('.'));
        }
    }
    return grouped;
}

} // namespace

QColor pocketColor(core::PocketKind kind) {
    switch (kind) {
    case core::PocketKind::Operacion: return kOperacion;
    case core::PocketKind::Ahorro:    return kAhorro;
    case core::PocketKind::Inversion: return kInversion;
    case core::PocketKind::Personal:  return kPersonal;
    }
    return kOperacion;
}

QColor alertColor(core::AlertLevel level) {
    switch (level) {
    case core::AlertLevel::Danger:  return kNegative;
    case core::AlertLevel::Warning: return kInversion;
    case core::AlertLevel::Info:    return kTextMuted;
    }
    return kTextMuted;
}

QFont displayFont(int pointSize, QFont::Weight weight) {
    QFont font(uiFamily());
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

QFont bodyFont(int pointSize, QFont::Weight weight) {
    QFont font(uiFamily());
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

QFont numericFont(int pointSize, QFont::Weight weight) {
    QFont font(numericFamily());
    font.setPointSize(pointSize);
    font.setWeight(weight);
    // Cifras de ancho fijo: sin esto, un 1 ocupa menos que un 8 y las columnas
    // de numeros se desalinean al refrescar. La fuente elegida puede no traer
    // la caracteristica, de ahi el optional.
    if (const std::optional<QFont::Tag> tag = QFont::Tag::fromString(QByteArrayLiteral("tnum"))) {
        font.setFeature(*tag, 1);
    }
    return font;
}

QString formatMoney(const core::Money& amount) {
    const qint64 minor = static_cast<qint64>(amount.minor());
    const qint64 scale = static_cast<qint64>(amount.currency().scale());

    const bool negative = minor < 0;
    const qint64 absolute = negative ? -minor : minor;

    const qint64 major = absolute / scale;
    const qint64 fraction = absolute % scale;

    QString text = groupThousands(major);
    if (amount.currency().exponent() > 0) {
        text += QLatin1Char(',');
        text += QString::number(fraction).rightJustified(amount.currency().exponent(),
                                                        QLatin1Char('0'));
    }
    if (negative) {
        text.prepend(QLatin1Char('-'));
    }
    return text;
}

QString formatCompact(const core::Money& amount) {
    const qint64 minor = static_cast<qint64>(amount.minor());
    const qint64 scale = static_cast<qint64>(amount.currency().scale());

    const bool negative = minor < 0;
    const qint64 absolute = negative ? -minor : minor;
    const qint64 major = absolute / scale;

    QString text;
    if (major >= 1'000'000) {
        const double millions = static_cast<double>(major) / 1'000'000.0;
        text = QString::number(millions, 'f', millions < 10.0 ? 1 : 0)
                   .replace(QLatin1Char('.'), QLatin1Char(',')) +
               QStringLiteral(" M");
    } else if (major >= 1'000) {
        const double thousands = static_cast<double>(major) / 1'000.0;
        text = QString::number(thousands, 'f', thousands < 10.0 ? 1 : 0)
                   .replace(QLatin1Char('.'), QLatin1Char(',')) +
               QStringLiteral(" k");
    } else {
        text = QString::number(major);
    }

    if (negative) {
        text.prepend(QLatin1Char('-'));
    }
    return text;
}

QString formatBps(int basisPoints) {
    const double percent = static_cast<double>(basisPoints) / 100.0;
    return QString::number(percent, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(',')) +
           QStringLiteral("%");
}

QString monthName(core::Date date) {
    static const char* const kNames[] = {"enero",      "febrero", "marzo",     "abril",
                                         "mayo",       "junio",   "julio",     "agosto",
                                         "septiembre", "octubre", "noviembre", "diciembre"};
    const unsigned index = date.month >= 1 && date.month <= 12 ? date.month - 1 : 0;
    return QStringLiteral("%1 %2").arg(QString::fromLatin1(kNames[index])).arg(date.year);
}

QString changeText(const core::Money& current, const core::Money& previous) {
    const core::Money delta = current - previous;
    if (delta.isZero()) {
        return QStringLiteral("igual");
    }
    if (previous.isZero()) {
        return QStringLiteral("nuevo");
    }
    const bool up = !delta.isNegative();
    const core::Money magnitude = up ? delta : -delta;
    const qint64 percent = (magnitude.minor() * 100 + previous.minor() / 2) / previous.minor();
    return QStringLiteral("%1%2 %3 %4%")
        .arg(up ? QStringLiteral("+") : QStringLiteral("−"), formatMoney(magnitude),
             up ? QStringLiteral("▲") : QStringLiteral("▼"))
        .arg(percent);
}

void setLabelColor(QWidget* label, const QColor& color) {
    if (label == nullptr) {
        return;
    }
    // El fondo transparente es tan importante como el color: sin el, la
    // etiqueta hereda el fondo de la regla global QWidget y dibuja un
    // rectangulo mas oscuro encima de la tarjeta que la contiene.
    label->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(color.name()));
}

QString styleSheet() {
    // Se construye con las constantes de arriba para que no haya un segundo
    // lugar donde vivan los colores.
    const QString background = kBackground.name();
    const QString surface = kSurface.name();
    const QString raised = kSurfaceRaised.name();
    const QString sidebar = kSidebar.name();
    const QString border = kBorder.name();
    const QString text = kText.name();
    const QString muted = kTextMuted.name();
    const QString accent = kAccent.name();

    const QString base = QStringLiteral(R"css(
QWidget {
    background-color: %1;
    color: %6;
}

/* Las etiquetas no pintan fondo propio: dentro de una tarjeta dibujarian un
   rectangulo del color de la ventana encima del color de la tarjeta. */
QLabel {
    background: transparent;
}

QScrollArea, QScrollArea > QWidget > QWidget {
    background-color: %1;
    border: none;
}

#Sidebar {
    background-color: %4;
    border-right: 1px solid %5;
}

#SidebarBrand {
    color: %6;
}

#NavButton {
    background-color: transparent;
    border: none;
    border-radius: 8px;
    padding: 10px 14px;
    text-align: left;
    color: %7;
}

#NavButton:hover {
    background-color: %3;
    color: %6;
}

#NavButton:checked {
    background-color: %3;
    color: %8;
    font-weight: 600;
}

#Card {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 14px;
}

#CardTitle {
    color: %7;
}

QToolTip {
    background-color: %3;
    color: %6;
    border: 1px solid %5;
    padding: 6px 8px;
    border-radius: 6px;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background: %5;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: %7;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
)css")
        .arg(background, surface, raised, sidebar, border, text, muted, accent);

    // Los controles de entrada van en su propio bloque: QString::arg admite
    // hasta nueve sustituciones por llamada y el bloque de arriba ya usa ocho.
    const QString inputs = QStringLiteral(R"css(
QLineEdit, QComboBox {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    padding: 7px 10px;
    color: %3;
    selection-background-color: %4;
    selection-color: %5;
}

QLineEdit:focus, QComboBox:focus {
    border: 1px solid %4;
}

QLineEdit::placeholder {
    color: %6;
}

QComboBox::drop-down {
    border: none;
    width: 18px;
}

QComboBox QAbstractItemView {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    padding: 4px;
    color: %3;
    selection-background-color: %4;
    selection-color: %5;
    outline: none;
}

QCheckBox {
    color: %6;
    spacing: 7px;
}

QCheckBox:disabled {
    color: %7;
}

QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid %2;
    border-radius: 4px;
    background-color: %1;
}

QCheckBox::indicator:checked {
    background-color: %4;
    border: 1px solid %4;
}

/* Selector de tipo: apagado se lee como opcion disponible, encendido toma el
   color semantico del movimiento (verde entra, rojo sale). */
#KindExpense, #KindIncome {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    color: %6;
}

#KindExpense:hover, #KindIncome:hover {
    color: %3;
}

#KindExpense:checked {
    background-color: %8;
    border: 1px solid %8;
    color: %5;
}

#KindIncome:checked {
    background-color: %9;
    border: 1px solid %9;
    color: %5;
}

#PrimaryButton {
    background-color: %4;
    border: none;
    border-radius: 8px;
    color: %5;
    padding: 0px 16px;
}

#PrimaryButton:hover {
    background-color: #7dd3fc;
}

#PrimaryButton:pressed {
    background-color: #0ea5e9;
}

QProgressBar {
    background-color: %1;
    border: none;
    border-radius: 3px;
}

QProgressBar::chunk {
    background-color: %4;
    border-radius: 3px;
}
)css")
                              .arg(raised, border, text, accent, background, muted,
                                   kTextFaint.name(), kNegative.name(), kPositive.name());

    // La tabla va en su propio bloque y no dentro de `inputs` por un limite
    // concreto: QString::arg() solo sustituye hasta %9, y ese bloque ya usa los
    // nueve. Partirlo es mas barato que renumerar todo.
    const QString table = QStringLiteral(R"css(
QTableWidget {
    background-color: %1;
    alternate-background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    gridline-color: %2;
    selection-background-color: %5;
    selection-color: %3;
}

QTableWidget::item {
    padding: 7px 10px;
    border: none;
}

QTableWidget::item:selected {
    background-color: %5;
    color: %3;
}

QTableWidget::item:hover {
    background-color: %6;
}

QHeaderView::section {
    background-color: %1;
    color: %4;
    padding: 8px 10px;
    border: none;
    border-bottom: 1px solid %2;
}

QHeaderView::section:hover {
    color: %3;
}

QTableCornerButton::section {
    background-color: %1;
    border: none;
}

QDateEdit {
    background-color: %6;
    color: %3;
    border: 1px solid %2;
    border-radius: 6px;
    padding: 6px 8px;
}

QDateEdit:focus {
    border: 1px solid %5;
}

/* El calendario emergente hereda el fondo de la ventana y sin esto queda un
   bloque negro sobre negro donde no se distingue ningun dia. */
QCalendarWidget QWidget {
    background-color: %6;
    color: %3;
}

QCalendarWidget QAbstractItemView {
    background-color: %6;
    color: %3;
    selection-background-color: %5;
    selection-color: %1;
}
)css")
                             .arg(surface, border, text, muted, accent, raised);

    // El tercer boton del selector de tipo. Va aparte porque el bloque
    // principal ya agoto los nueve marcadores que sustituye QString::arg().
    const QString transfer = QStringLiteral(R"css(
#KindTransfer {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    color: %3;
}

#KindTransfer:hover {
    color: %4;
}

#KindTransfer:checked {
    background-color: %5;
    border: 1px solid %5;
    color: %6;
}

/* Un boton dentro de una celda de tabla no hereda la regla general de
   QPushButton, y sin esto Qt lo pinta con el estilo nativo: un rectangulo
   claro que en un tema oscuro parece un error de dibujo. */
#GhostButton {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 6px;
    color: %4;
    padding: 2px 10px;
}

#GhostButton:hover {
    border: 1px solid %5;
    color: %5;
}
)css")
                                .arg(raised, border, muted, text, kAhorro.name(), background);

    return base + inputs + table + transfer;
}

} // namespace dake::ui::theme
