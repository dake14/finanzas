#pragma once
//
// ui/theme.hpp — paleta y tipografia de DakeLabs.
//
// Todos los colores del sistema salen de aqui. Ningun widget escribe un
// literal hexadecimal: si manana cambia el logo, se cambia este archivo y no
// veinte archivos de pintura.
//
// La paleta deriva del icono de DakeLabs: fondo casi negro (#0a0a0a), cuerpo
// blanco (#fafafa) y la barra cian (#38bdf8) como color de marca.
//
#include <QColor>
#include <QFont>
#include <QString>

#include "dake/core/model.hpp"
#include "dake/core/report.hpp"
#include "dake/core/money.hpp"

namespace dake::ui::theme {

// --- Superficies ---------------------------------------------------------
inline const QColor kBackground{0x0a, 0x0a, 0x0a};   // fondo de la ventana
inline const QColor kSurface{0x14, 0x14, 0x14};      // tarjetas
inline const QColor kSurfaceRaised{0x1c, 0x1c, 0x1c};// hover / elementos activos
inline const QColor kSidebar{0x0d, 0x0d, 0x0d};
inline const QColor kBorder{0x26, 0x26, 0x26};

// --- Texto ---------------------------------------------------------------
inline const QColor kText{0xfa, 0xfa, 0xfa};         // cuerpo del logo
inline const QColor kTextMuted{0x8a, 0x8a, 0x8a};
inline const QColor kTextFaint{0x5a, 0x5a, 0x5a};

// --- Marca y semantica ---------------------------------------------------
inline const QColor kAccent{0x38, 0xbd, 0xf8};       // barra cian del logo
inline const QColor kAccentSoft{0x0e, 0x2a, 0x38};
inline const QColor kPositive{0x34, 0xd3, 0x99};     // ingresos
inline const QColor kNegative{0xfb, 0x71, 0x85};     // gastos

// --- Bolsillos -----------------------------------------------------------
// La caja de operacion toma el cian de la marca: es la que se mira todos los
// dias. Las reservas van en colores calidos porque tocarlas tiene que
// destacar, no pasar desapercibido.
inline const QColor kOperacion{0x38, 0xbd, 0xf8};
inline const QColor kAhorro{0xa7, 0x8b, 0xfa};
inline const QColor kInversion{0xfb, 0xbf, 0x24};
inline const QColor kPersonal{0x9c, 0xa3, 0xaf};

[[nodiscard]] QColor pocketColor(core::PocketKind kind);

/// Color de un aviso segun su gravedad.
[[nodiscard]] QColor alertColor(core::AlertLevel level);

// --- Tipografia ----------------------------------------------------------
[[nodiscard]] QFont displayFont(int pointSize, QFont::Weight weight = QFont::DemiBold);
[[nodiscard]] QFont bodyFont(int pointSize, QFont::Weight weight = QFont::Normal);
[[nodiscard]] QFont numericFont(int pointSize, QFont::Weight weight = QFont::DemiBold);

// --- Formato de cifras ---------------------------------------------------
/// Formato colombiano: punto para miles, coma para decimales.
/// 1234567.89 COP -> "1.234.567,89"
[[nodiscard]] QString formatMoney(const core::Money& amount);

/// Version compacta para ejes y etiquetas apretadas: "1,2 M", "850 k".
[[nodiscard]] QString formatCompact(const core::Money& amount);

/// Puntos basicos a porcentaje legible: 4000 -> "40,0%"
[[nodiscard]] QString formatBps(int basisPoints);

/// Pinta el texto de una etiqueta del color indicado.
///
/// Va por hoja de estilo y no por QPalette a proposito: cuando hay un
/// stylesheet activo, Qt le da prioridad sobre la paleta, asi que un
/// setColor(QPalette::WindowText, ...) queda pisado por la regla global y la
/// etiqueta sale siempre del color por defecto.
/// "septiembre 2026". Los reportes nombran el mes entero: "sep 2026" se lee
/// bien en un eje, no en una frase.
[[nodiscard]] QString monthName(core::Date date);

/// "+12,00 ▲ 30%", "−5,00 ▼ 10%", "nuevo" o "igual". Para gastos: subir es
/// malo, y el color lo decide quien lo muestra.
[[nodiscard]] QString changeText(const core::Money& current, const core::Money& previous);

void setLabelColor(QWidget* label, const QColor& color);

/// Hoja de estilo global de la aplicacion.
[[nodiscard]] QString styleSheet();

} // namespace dake::ui::theme
