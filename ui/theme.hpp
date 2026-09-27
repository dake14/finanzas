#pragma once
//
// ui/theme.hpp — paleta y tipografia de DakeLabs.
//
// Todos los colores del sistema salen de aqui. Ningun widget escribe un
// literal hexadecimal: si manana cambia el logo, se cambia este archivo y no
// veinte archivos de pintura.
//
// El sistema visual es el de DakeLabs Cotizaciones: barra azul-noche, rojo de
// marca #EF233C, Inter y Anton, en claro y en oscuro.
//
#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

#include "dake/core/model.hpp"
#include "dake/core/report.hpp"
#include "dake/core/money.hpp"

namespace dake::ui::theme {

// --- Papeles -------------------------------------------------------------
//
// Un color no se escribe: se pide por lo que significa. El tema activo dice
// que color tiene cada papel, y al cambiar de tema todo lo que se pide despues
// sale del tema nuevo. Ver docs/superpowers/specs/2026-09-26-aspecto-cotizaciones-design.md.
enum class Papel {
    Fondo, Superficie, SuperficieAlzada, Barra, Borde,
    Texto, Tenue, Apagado,
    Marca, MarcaSuave,
    Ingreso, Gasto, Aviso, Serie,
    Operacion, Ahorro, Inversion, Personal,
    Emergencia,
};
inline constexpr int kPapeles = 19;

enum class Tema { Claro, Oscuro };

/// Un papel. Se convierte solo a QColor, resuelto contra el tema activo en el
/// momento de usarlo; por eso nunca se guarda el QColor, se guarda el Tono.
struct Tono {
    Papel papel;
    operator QColor() const;  // NOLINT(google-explicit-constructor): convertirse es su trabajo
    [[nodiscard]] QString name() const;
    /// El nombre del papel en la hoja de estilo: "gasto", "texto", "marca-suave".
    [[nodiscard]] const char* id() const;
};

// Los nombres de siempre, ahora papeles.
inline constexpr Tono kBackground{Papel::Fondo};
inline constexpr Tono kSurface{Papel::Superficie};
inline constexpr Tono kSurfaceRaised{Papel::SuperficieAlzada};
inline constexpr Tono kSidebar{Papel::Barra};
inline constexpr Tono kBorder{Papel::Borde};
inline constexpr Tono kText{Papel::Texto};
inline constexpr Tono kTextMuted{Papel::Tenue};
inline constexpr Tono kTextFaint{Papel::Apagado};
inline constexpr Tono kAccent{Papel::Marca};          // el rojo de DakeLabs
inline constexpr Tono kAccentSoft{Papel::MarcaSuave};
inline constexpr Tono kPositive{Papel::Ingreso};
inline constexpr Tono kNegative{Papel::Gasto};        // naranja: el rojo es de la marca
inline constexpr Tono kAviso{Papel::Aviso};           // advertencia que no es error
inline constexpr Tono kSerie{Papel::Serie};           // grafica sin significado propio
inline constexpr Tono kOperacion{Papel::Operacion};
inline constexpr Tono kAhorro{Papel::Ahorro};
inline constexpr Tono kInversion{Papel::Inversion};   // SOLO el bolsillo de inversion
inline constexpr Tono kPersonal{Papel::Personal};
inline constexpr Tono kEmergencia{Papel::Emergencia};  // el fondo para imprevistos

[[nodiscard]] QColor color(Tono tono);
[[nodiscard]] Tema currentTheme();
/// Cambia la paleta y regenera la hoja de estilo de toda la aplicacion. Lo que
/// se pinta a mano necesita un update(); las tablas, que se vuelvan a llenar.
void setTheme(Tema tema);
/// "oscuro" (sin importar mayusculas ni espacios) -> Oscuro; cualquier otra
/// cosa, incluso vacio, -> Claro.
[[nodiscard]] Tema temaFromString(const QString& text);
/// "claro" / "oscuro", lo que se guarda en settings bajo ui.tema.
[[nodiscard]] QString toString(Tema tema);

[[nodiscard]] Tono pocketColor(core::PocketKind kind);

/// Color de un aviso segun su gravedad.
[[nodiscard]] Tono alertColor(core::AlertLevel level);

// --- Tipografia ----------------------------------------------------------
[[nodiscard]] QFont displayFont(int pointSize, QFont::Weight weight = QFont::DemiBold);
[[nodiscard]] QFont bodyFont(int pointSize, QFont::Weight weight = QFont::Normal);
[[nodiscard]] QFont numericFont(int pointSize, QFont::Weight weight = QFont::DemiBold);
/// Anton, para cifras grandes y la marca. Nunca por debajo de 16 pt: en chico
/// se lee mal.
[[nodiscard]] QFont figureFont(int pointSize);
/// Inter y Anton quedaron registradas desde el recurso. Si es falso, la app
/// sigue con Segoe UI: se ve distinta, pero anda.
[[nodiscard]] bool fontsLoaded();

// --- Formato de cifras ---------------------------------------------------
/// Formato colombiano: punto para miles, coma para decimales.
/// 1234567.89 COP -> "1.234.567,89"
[[nodiscard]] QString formatMoney(const core::Money& amount);

/// Version compacta para ejes y etiquetas apretadas: "1,2 M", "850 k".
[[nodiscard]] QString formatCompact(const core::Money& amount);

/// Puntos basicos a porcentaje legible: 4000 -> "40,0%"
[[nodiscard]] QString formatBps(int basisPoints);

/// "septiembre 2026". Los reportes nombran el mes entero: "sep 2026" se lee
/// bien en un eje, no en una frase.
[[nodiscard]] QString monthName(core::Date date);

/// "+12,00 ▲ 30%", "−5,00 ▼ 10%", "nuevo" o "igual". Para gastos: subir es
/// malo, y el color lo decide quien lo muestra.
[[nodiscard]] QString changeText(const core::Money& current, const core::Money& previous);

/// Le pone a la etiqueta el papel `tono` (propiedad dinamica "tono"); el color
/// lo decide la regla [tono="..."] de la hoja global, asi que cambiar de tema
/// la repinta sola.
void setLabelColor(QWidget* label, Tono tono);

/// Hoja de estilo global de la aplicacion.
[[nodiscard]] QString styleSheet();

} // namespace dake::ui::theme
