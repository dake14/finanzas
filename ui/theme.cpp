#include "theme.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QStringList>
#include <QStyle>
#include <QWidget>
#include <array>

#include <cmath>
#include <cstdlib>
#include <optional>

namespace dake::ui::theme {
namespace {

/// Registra las fuentes empaquetadas una sola vez. Tiene que correr antes de
/// la primera consulta a QFontDatabase::families(), por eso la llaman las
/// funciones que eligen familia y no main().
bool registerFonts() {
    static const bool loaded = [] {
        bool ok = true;
        for (const char* file : {"Inter-Regular", "Inter-SemiBold", "Inter-Bold", "Inter-ExtraBold",
                                 "Anton-Regular"}) {
            ok = QFontDatabase::addApplicationFont(
                     QStringLiteral(":/dake/fuentes/%1.ttf").arg(QLatin1String(file))) >= 0 &&
                 ok;
        }
        return ok;
    }();
    return loaded;
}

[[nodiscard]] QColor hex(unsigned rgb) {
    return QColor::fromRgb(static_cast<QRgb>(rgb));
}

// Mismo orden que enum Papel. Claro y oscuro salen de Cotizaciones
// (editor.css) y del logo; los colores de datos, de la maqueta aprobada.
const std::array<QColor, kPapeles> kClaro{
    hex(0xF7F8FA), hex(0xFFFFFF), hex(0xF1F3F7), hex(0x2B2D42), hex(0xE4E7EE),
    hex(0x2B2D42), hex(0x6B7690), hex(0x8D99AE),
    hex(0xEF233C), hex(0xFDE8EB),
    hex(0x1F9D6B), hex(0xE8590C), hex(0xB7791F), hex(0x4361EE),
    hex(0x4361EE), hex(0x7B2CBF), hex(0x0F8FA3), hex(0x8D99AE),
    hex(0xD6336C)};
const std::array<QColor, kPapeles> kOscuro{
    hex(0x23253A), hex(0x2B2D42), hex(0x33364F), hex(0x1A1C2B), hex(0x3A3D55),
    hex(0xEDF2F4), hex(0xA9B1C4), hex(0x737C96),
    hex(0xEF233C), hex(0x4A2332),
    hex(0x3ECF8E), hex(0xFF922B), hex(0xF6C453), hex(0x7B93FF),
    hex(0x7B93FF), hex(0xB57BFF), hex(0x3CC8DC), hex(0xA9B1C4),
    hex(0xF06595)};

constexpr std::array<const char*, kPapeles> kIds{
    "fondo", "superficie", "superficie-alzada", "barra", "borde",
    "texto", "tenue", "apagado",
    "marca", "marca-suave",
    "ingreso", "gasto", "aviso", "serie",
    "operacion", "ahorro", "inversion", "personal",
    "emergencia"};

Tema gTema = Tema::Claro;

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

[[nodiscard]] QString uiFamily() {
    registerFonts();
    static const QString family =
        pickFamily({QStringLiteral("Inter"), QStringLiteral("Segoe UI Variable"),
                    QStringLiteral("Segoe UI"), QStringLiteral("Arial")});
    return family;
}

/// Inter trae cifras tabulares (tnum), asi que las columnas de numeros usan la
/// misma familia que el texto.
[[nodiscard]] QString numericFamily() {
    return uiFamily();
}

[[nodiscard]] QString figureFamily() {
    registerFonts();
    static const QString family = pickFamily({QStringLiteral("Anton"), uiFamily()});
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

Tono::operator QColor() const {
    return color(*this);
}

QString Tono::name() const {
    return color(*this).name();
}

const char* Tono::id() const {
    return kIds[static_cast<std::size_t>(papel)];
}

QColor color(Tono tono) {
    const auto& paleta = gTema == Tema::Oscuro ? kOscuro : kClaro;
    return paleta[static_cast<std::size_t>(tono.papel)];
}

Tema currentTheme() {
    return gTema;
}

void setTheme(Tema tema) {
    gTema = tema;
    // Una sola hoja, a nivel de aplicacion: la ventana mini y los dialogos la
    // heredan, y al reemplazarla Qt vuelve a pulir todos los widgets.
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        // Inter tambien para lo que no pide letra: con hoja de estilo, Qt no
        // siempre respeta un setFont (los encabezados de tabla lo pierden) y
        // cae en la letra por defecto del sistema.
        app->setFont(bodyFont(10));
        app->setStyleSheet(styleSheet());
    }
}

Tema temaFromString(const QString& text) {
    return text.trimmed().compare(QStringLiteral("oscuro"), Qt::CaseInsensitive) == 0 ? Tema::Oscuro
                                                                                        : Tema::Claro;
}

QString toString(Tema tema) {
    return tema == Tema::Oscuro ? QStringLiteral("oscuro") : QStringLiteral("claro");
}

bool fontsLoaded() {
    return registerFonts();
}

QFont figureFont(int pointSize) {
    QFont font(figureFamily());
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);  // Anton tiene un solo peso
    return font;
}

Tono pocketColor(core::PocketKind kind) {
    switch (kind) {
    case core::PocketKind::Operacion: return kOperacion;
    case core::PocketKind::Ahorro:    return kAhorro;
    case core::PocketKind::Inversion: return kInversion;
    case core::PocketKind::Personal:  return kPersonal;
    case core::PocketKind::Emergencia: return kEmergencia;
    }
    return kOperacion;
}

Tono alertColor(core::AlertLevel level) {
    switch (level) {
    case core::AlertLevel::Danger:  return kNegative;
    case core::AlertLevel::Warning: return kAviso;
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

void setLabelColor(QWidget* label, Tono tono) {
    if (label == nullptr) {
        return;
    }
    // El color lo pone la regla [tono="..."] de la hoja global. Sin volver a
    // pulir, un cambio de propiedad no se nota hasta el proximo cambio de hoja.
    label->setProperty("tono", QString::fromLatin1(tono.id()));
    label->style()->unpolish(label);
    label->style()->polish(label);
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
    color: %5;
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

#Card {
    background-color: %2;
    border: 1px solid %4;
    border-radius: 14px;
}

QToolTip {
    background-color: %3;
    color: %5;
    border: 1px solid %4;
    padding: 6px 8px;
    border-radius: 6px;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background: %4;
    border-radius: 5px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: %6;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
)css")
        .arg(background, surface, raised, border, text, muted);

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
   color semantico del movimiento (verde entra, naranja sale). */
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
    color: #FFFFFF;
    padding: 0px 16px;
}

#PrimaryButton:hover {
    background-color: #D90429;
}

#PrimaryButton:pressed {
    background-color: #B8001F;
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
    selection-background-color: %7;
    selection-color: %3;
}

QTableWidget::item {
    padding: 7px 10px;
    border: none;
}

QTableWidget::item:selected {
    background-color: %7;
    color: %3;
}

QTableWidget::item:hover {
    background-color: %6;
}

QHeaderView::section {
    background-color: %1;
    color: %4;
    font-weight: 600;
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
                             .arg(surface, border, text, muted, accent, raised, kAccentSoft.name());

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

    // La barra lateral es azul-noche en los dos temas: por eso sus colores son
    // fijos, y viven aca y en ningun otro lado.
    const QString marco = QStringLiteral(R"css(
QPushButton {
    background-color: %1;
    border: 1px solid %2;
    border-radius: 8px;
    color: %3;
    padding: 6px 14px;
}

QPushButton:hover {
    border: 1px solid %4;
}

QPushButton:disabled {
    color: %5;
}

#Sidebar {
    background-color: %6;
    border-right: none;
}

#Sidebar QLabel {
    color: #A9B1C4;
}

#Sidebar QPushButton {
    background: transparent;
    border: none;
    color: #A9B1C4;
    text-align: left;
}

#Sidebar QPushButton:hover {
    color: #FFFFFF;
}

#NavButton {
    border-radius: 0px;
    padding: 9px 14px;
}

#NavButton:checked {
    background-color: rgba(239, 35, 60, 46);
    border-left: 3px solid #EF233C;
    padding-left: 11px;
    color: #FFFFFF;
    font-weight: 600;
}

/* El cuerpo de una tarjeta (un QWidget suelto que agrupa etiquetas) no pinta
   fondo: si no, la regla QWidget le pone el color de la ventana y queda un
   rectangulo de otro tono dentro de la tarjeta. */
QWidget[cardBody="true"] {
    background: transparent;
}

#PillCell {
    background: transparent;
}

#CaptureFrame {
    background-color: %1;
    border: 1px solid #EF233C;
    border-radius: 14px;
}
)css")
                              .arg(kSurface.name(), kBorder.name(), kText.name(), kTextMuted.name(),
                                   kTextFaint.name(), kSidebar.name());

    // Pastillas de estado, como las de Cotizaciones. Alfa en 0-255: 20 % = 51,
    // 15 % = 38.
    const auto alpha = [](Tono tono, int a) {
        const QColor c = color(tono);
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };
    const QString pills = QStringLiteral(R"css(
QLabel[pill] {
    border-radius: 8px;
    padding: 1px 8px;
}

QLabel[pill="proceso"] {
    background-color: %1;
    color: %2;
}

QLabel[pill="cobrar"] {
    background-color: %3;
    color: %4;
}

QLabel[pill="cobrada"] {
    background-color: %5;
    color: %6;
}
)css")
                              .arg(kText.name(), kSurface.name(), alpha(kAviso, 51), kAviso.name(),
                                   alpha(kPositive, 38), kPositive.name());

    // Una regla por papel: setLabelColor solo pone la propiedad y esto decide
    // el color. Fondo transparente, como hacia la version anterior, para que
    // la etiqueta no pinte un rectangulo encima de la tarjeta.
    QString tonos;
    for (int i = 0; i < kPapeles; ++i) {
        const Tono tono{static_cast<Papel>(i)};
        tonos += QStringLiteral("*[tono=\"%1\"] { color: %2; background: transparent; }\n")
                     .arg(QLatin1String(tono.id()), tono.name());
    }

    return base + inputs + table + transfer + marco + pills + tonos;
}

} // namespace dake::ui::theme
