# El aspecto de Cotizaciones — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** La app de escritorio toma el sistema visual de DakeLabs Cotizaciones (barra azul-noche con logo, rojo de marca, Inter y Anton) con tema claro y oscuro elegible en Ajustes, que se cambia en el acto.

**Architecture:** Los colores dejan de ser `QColor` fijos y pasan a ser *papeles* (`theme::Tono`) que se resuelven contra la paleta del tema activo en el momento de usarse. Las etiquetas llevan el papel como propiedad dinámica `tono` y la hoja de estilo global (una sola, a nivel de `QApplication`) tiene una regla por papel. Cambiar de tema = regenerar esa hoja + `reload()` para rellenar tablas y texto enriquecido.

**Tech Stack:** C++20, Qt 6.8.3 Widgets (MSVC 2022), CMake presets, pruebas propias (`dake_uitest` offscreen con QTest).

**Spec:** `docs/superpowers/specs/2026-09-26-aspecto-cotizaciones-design.md`

## Global Constraints

- `.\compilar.ps1 debug -Probar` pasa entero al final de cada tarea. **Cero advertencias.**
- La app del teléfono (`mobile/`) no se toca.
- Los datos, el esquema y la sincronización no se tocan. El tema vive en la tabla local `settings`, clave `ui.tema`, valores `claro` / `oscuro`; cualquier otra cosa o ausencia = `claro`.
- Ningún widget escribe un literal hexadecimal fuera de `ui/theme.cpp`. Excepción única: la barra lateral, que es oscura en los dos temas (sus colores fijos también viven en `theme.cpp`).
- Anton nunca por debajo de 16 pt.
- El rojo `#EF233C` es solo de la marca: nunca un gasto ni un número negativo, nunca datos de una gráfica.
- Las pruebas de interfaz corren offscreen con `dake_uitest`. **Nunca** teclas globales (`keybd_event`, `SendInput`).
- Comentarios en español, sin tildes en el código (como el resto del repo). Commits en español, terminando con:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01JhaoWSW44Z6pTAXd225bUt
  ```

## Cómo compilar y probar (vale para todas las tareas)

La app abierta bloquea `build\debug\bin\dake_pruebas.exe` y el enlazado falla con `LNK1168`. Antes de compilar: `Get-Process dake_pruebas -ErrorAction SilentlyContinue` — si está abierta, pedirle a David que la cierre desde la bandeja (Salir). No matarla sin avisar.

```powershell
.\compilar.ps1 debug -Probar          # todo + ctest (4 pruebas)
```

Solo la prueba de interfaz, con su salida:

```bash
cd /c/Users/David/Documents/VisualStudio/finanzas-banco-pruebas
export PATH="/c/Qt/6.8.3/msvc2022_64/bin:$PATH"
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uitest.exe 2>&1 | grep -E "FALLA|Todo pasa|HAY FALLAS"
```

## Review Focus

1. **Cambiar de tema con la ventana mini de captura o un diálogo abierto** → se tienen que repintar con el tema nuevo. Solo funciona si ningún widget tiene su propio `setStyleSheet(theme::styleSheet())`. Tarea 2, paso 7, lo verifica con grep.
2. **`ui.tema` con basura (`"azul"`, vacío, mayúsculas raras)** → arranca en claro, sin fallar. Tarea 3, paso 1, lo prueba.
3. **Un color guardado en un miembro al construir** (la franja de `KpiCard`, las barras de `CompareChart`) → queda con el color del tema viejo. Tarea 2, paso 7: grep de miembros `QColor` en `ui/`, tiene que salir vacío.
4. **Anton en tamaño chico** → ilegible. Tarea 4, paso 6: grep de `figureFont(` con menos de 16 pt, vacío.
5. **Las fuentes empaquetadas no cargan** → la app tiene que abrir igual, con Segoe UI. Tarea 1: la prueba comprueba que en la compilación normal sí cargan (`fontsLoaded()`), y `pickFamily` sigue cayendo a Segoe si no.

---

### Task 1: Inter, Anton y el logo, empaquetados

**Files:**
- Create: `ui/recursos/fuentes/Inter-Regular.ttf`, `Inter-SemiBold.ttf`, `Inter-Bold.ttf`, `Inter-ExtraBold.ttf`, `Anton-Regular.ttf`, `Inter-OFL.txt`, `Anton-OFL.txt`, `README.md`
- Create: `ui/recursos/logo/DAke.png`
- Modify: `ui/CMakeLists.txt` (agregar recurso), `ui/theme.hpp`, `ui/theme.cpp:15-52` (familias)
- Test: `tests/uitest.cpp`

**Interfaces:**
- Produces: `bool theme::fontsLoaded()`; `QFont theme::figureFont(int pointSize)` (Anton); recursos `:/dake/fuentes/*.ttf` y `:/dake/logo/DAke.png`. `displayFont`, `bodyFont`, `numericFont` pasan a Inter (con Segoe UI detrás).

- [ ] **Step 1: Bajar las fuentes y copiar el logo**

```powershell
$S = "$env:TEMP\dake-fuentes"; New-Item -ItemType Directory -Force $S | Out-Null
$F = "ui\recursos\fuentes"; New-Item -ItemType Directory -Force $F, "ui\recursos\logo" | Out-Null
Invoke-WebRequest https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip -OutFile "$S\inter.zip"
Expand-Archive "$S\inter.zip" -DestinationPath "$S\inter" -Force
foreach ($w in "Regular","SemiBold","Bold","ExtraBold") { Copy-Item "$S\inter\extras\ttf\Inter-$w.ttf" $F }
Copy-Item "$S\inter\LICENSE.txt" "$F\Inter-OFL.txt"
Invoke-WebRequest https://github.com/google/fonts/raw/main/ofl/anton/Anton-Regular.ttf -OutFile "$F\Anton-Regular.ttf"
Invoke-WebRequest https://github.com/google/fonts/raw/main/ofl/anton/OFL.txt -OutFile "$F\Anton-OFL.txt"
Copy-Item "..\dakelabsfactura\src\renderer\src\recursos\DAke.png" "ui\recursos\logo\DAke.png"
Get-FileHash $F\*.ttf -Algorithm SHA256 | Format-Table Hash, Path -AutoSize
```

Si una descarga falla, parar y avisar: no reemplazar por otra fuente.

- [ ] **Step 2: Escribir `ui/recursos/fuentes/README.md`** con la procedencia y los SHA-256 del paso 1 (mismo estilo que `third_party/android_openssl/README.md`):

```markdown
# Fuentes empaquetadas

Van dentro del ejecutable como recurso de Qt (`:/dake/fuentes/`), para que la
app se vea igual en cualquier computadora. Las dos son SIL Open Font License.

| Archivo | De dónde | SHA-256 |
|---|---|---|
| Inter-Regular.ttf | rsms/inter v4.1, `extras/ttf/` | <hash del paso 1> |
| Inter-SemiBold.ttf | idem | <hash> |
| Inter-Bold.ttf | idem | <hash> |
| Inter-ExtraBold.ttf | idem | <hash> |
| Anton-Regular.ttf | google/fonts, `ofl/anton/` | <hash> |
```

(Los `<hash>` se reemplazan por los valores reales que imprimió el paso 1; no queda ninguno sin llenar.)

- [ ] **Step 3: Escribir la prueba que falla** — en `tests/uitest.cpp`, agregar `#include <QFontDatabase>` y `#include "theme.hpp"`, y justo después de la línea `std::printf("Banco de pruebas — la interfaz, sin mouse\n(%s)\n", ...)`:

```cpp
    std::printf("\n[fuentes y logo empaquetados]\n");
    check(QFile::exists(QStringLiteral(":/dake/fuentes/Anton-Regular.ttf")), "Anton va dentro del ejecutable");
    check(QFile::exists(QStringLiteral(":/dake/logo/DAke.png")), "y el logo tambien");
    check(ui::theme::fontsLoaded(), "Inter y Anton quedaron registradas");
    check(ui::theme::bodyFont(10).family() == QStringLiteral("Inter"), "el texto sale en Inter");
    check(ui::theme::figureFont(21).family() == QStringLiteral("Anton"), "las cifras grandes en Anton");
```

- [ ] **Step 4: Correr y ver que no compila** (`fontsLoaded` y `figureFont` no existen). Esperado: error C3861 / C2039.

- [ ] **Step 5: Agregar el recurso** — en `ui/CMakeLists.txt`, después de `target_compile_definitions(dake_ui ...)`:

```cmake
# Inter, Anton y el logo van dentro del ejecutable: la app tiene que verse igual
# en cualquier computadora, tenga o no esas fuentes instaladas.
qt_add_resources(dake_ui "dake_recursos"
    PREFIX "/dake"
    BASE recursos
    FILES
        recursos/fuentes/Inter-Regular.ttf
        recursos/fuentes/Inter-SemiBold.ttf
        recursos/fuentes/Inter-Bold.ttf
        recursos/fuentes/Inter-ExtraBold.ttf
        recursos/fuentes/Anton-Regular.ttf
        recursos/logo/DAke.png
)
```

- [ ] **Step 6: Registrar las fuentes** — en `ui/theme.hpp`, en la sección de tipografía:

```cpp
/// Anton, para cifras grandes y la marca. Nunca por debajo de 16 pt: en chico
/// se lee mal.
[[nodiscard]] QFont figureFont(int pointSize);

/// Inter y Anton quedaron registradas desde el recurso. Si es falso, la app
/// sigue con Segoe UI: se ve distinta, pero anda.
[[nodiscard]] bool fontsLoaded();
```

En `ui/theme.cpp`, dentro del namespace anónimo, antes de `pickFamily`:

```cpp
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
```

Reemplazar `uiFamily()` y `numericFamily()` por:

```cpp
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
```

Y fuera del namespace anónimo:

```cpp
bool fontsLoaded() {
    return registerFonts();
}

QFont figureFont(int pointSize) {
    QFont font(figureFamily());
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);   // Anton tiene un solo peso
    return font;
}
```

- [ ] **Step 7: Que se note si no cargan** — la spec pide que el diagnóstico lo diga. En `MainWindow::MainWindow` (`ui/mainwindow.cpp`), después de crear `repository_`:

```cpp
    if (!theme::fontsLoaded()) {
        // La app sigue con Segoe UI; se ve distinta pero anda. Que quede escrito.
        qWarning("Finanzas: no se pudieron registrar Inter/Anton desde el recurso; se usa Segoe UI.");
    }
```

(`#include <QtGlobal>` si hace falta.)

- [ ] **Step 8: Correr la prueba** (comandos de arriba). Esperado: las 5 líneas nuevas en `ok`, `Todo pasa.`

- [ ] **Step 9: `.\compilar.ps1 debug -Probar`** — 4/4, cero advertencias.

- [ ] **Step 10: Commit**

```bash
git add ui/recursos ui/CMakeLists.txt ui/theme.hpp ui/theme.cpp ui/mainwindow.cpp tests/uitest.cpp
git commit -m "Inter, Anton y el logo, dentro del ejecutable" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01JhaoWSW44Z6pTAXd225bUt"
```

Todos los commits de las tareas siguientes llevan ese mismo pie.

---

### Task 2: Los colores pasan a ser papeles, con paleta clara y oscura

**Files:**
- Modify: `ui/theme.hpp`, `ui/theme.cpp` (paleta, `setTheme`, hoja de estilo, `setLabelColor`)
- Modify (ajustes mecánicos que marque el compilador): `ui/cards.hpp`, `ui/cards.cpp`, `ui/charts.hpp`, `ui/charts.cpp`, `ui/capturewidget.cpp`, `ui/capturewindow.cpp`, `ui/repairspage.cpp`, `ui/reportspage.cpp`, `ui/todaypage.cpp`, `ui/movementspage.cpp`, `ui/pocketspage.cpp`, `ui/settingspage.cpp`, `ui/reviewpage.cpp`, `ui/dialogs.cpp`, `ui/quotedialog.cpp`, `ui/bankdialog.cpp`, `ui/repairdialogs.cpp`, `ui/mainwindow.cpp`, `ui/preview.cpp`, `ui/tables.hpp`
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: `figureFont`, `fontsLoaded` (Task 1).
- Produces (en `dake::ui::theme`):
  ```cpp
  enum class Papel { Fondo, Superficie, SuperficieAlzada, Barra, Borde, Texto, Tenue, Apagado,
                     Marca, MarcaSuave, Ingreso, Gasto, Aviso, Serie,
                     Operacion, Ahorro, Inversion, Personal };
  enum class Tema { Claro, Oscuro };
  struct Tono { Papel papel; operator QColor() const; QString name() const; const char* id() const; };
  // alias que ya usa el codigo, ahora Tono:
  kBackground kSurface kSurfaceRaised kSidebar kBorder kText kTextMuted kTextFaint
  kAccent kAccentSoft kPositive kNegative kOperacion kAhorro kInversion kPersonal
  // nuevos: kAviso kSerie
  QColor color(Tono tono);
  Tema currentTheme();
  void setTheme(Tema tema);            // cambia paleta y hoja de estilo de toda la app
  Tema temaFromString(const QString&); // "oscuro" -> Oscuro; cualquier otra cosa -> Claro
  QString toString(Tema);              // "claro" / "oscuro"
  void setLabelColor(QWidget* label, Tono tono);   // pone la propiedad "tono"
  Tono pocketColor(core::PocketKind);  Tono alertColor(core::AlertLevel);
  ```
  Ids de `Tono::id()`: `fondo superficie superficie-alzada barra borde texto tenue apagado marca marca-suave ingreso gasto aviso serie operacion ahorro inversion personal`.

- [ ] **Step 1: Escribir la prueba que falla** — en `tests/uitest.cpp`, agregar `#include <QLabel>` si falta, y después del bloque de fuentes de la Task 1:

```cpp
    std::printf("\n[tema: un papel cambia de color con el tema]\n");
    {
        ui::theme::setTheme(ui::theme::Tema::Claro);
        QLabel probe;
        ui::theme::setLabelColor(&probe, ui::theme::kNegative);
        probe.ensurePolished();
        check(probe.property("tono").toString() == QStringLiteral("gasto"), "el monto de un gasto lleva el papel 'gasto'");
        check(probe.palette().color(probe.foregroundRole()) == QColor(QStringLiteral("#E8590C")),
              "en claro, naranja #E8590C");
        ui::theme::setTheme(ui::theme::Tema::Oscuro);
        probe.ensurePolished();
        check(probe.palette().color(probe.foregroundRole()) == QColor(QStringLiteral("#FF922B")),
              "en oscuro, el mismo papel da #FF922B sin volver a tocar la etiqueta");
        check(QColor(ui::theme::kNegative) == QColor(QStringLiteral("#FF922B")), "y pintar a mano tambien lo ve");
        ui::theme::setTheme(ui::theme::Tema::Claro);
    }
```

- [ ] **Step 2: Correr y ver que no compila** (`Tema`, `setTheme` no existen). Esperado: error de compilación.

- [ ] **Step 3: Reescribir la parte de colores de `ui/theme.hpp`** — reemplazar desde `// --- Superficies` hasta `[[nodiscard]] QColor alertColor(core::AlertLevel level);` por:

```cpp
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
};
inline constexpr int kPapeles = 18;

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

[[nodiscard]] QColor color(Tono tono);
[[nodiscard]] Tema currentTheme();
/// Cambia la paleta y regenera la hoja de estilo de toda la aplicacion. Lo que
/// se pinta a mano necesita un update(); las tablas, que se vuelvan a llenar.
void setTheme(Tema tema);
[[nodiscard]] Tema temaFromString(const QString& text);
[[nodiscard]] QString toString(Tema tema);

[[nodiscard]] Tono pocketColor(core::PocketKind kind);

/// Color de un aviso segun su gravedad.
[[nodiscard]] Tono alertColor(core::AlertLevel level);
```

Y cambiar la firma al final del archivo: `void setLabelColor(QWidget* label, Tono tono);` (su comentario: "Le pone a la etiqueta el papel `tono`; la hoja de estilo global decide el color. Cambiar de tema la repinta sola."). Agregar `class QWidget;` arriba si no está.

- [ ] **Step 4: Paleta y cambio de tema en `ui/theme.cpp`** — agregar `#include <QApplication>`, `#include <QStyle>`, `#include <array>`. Dentro del namespace anónimo:

```cpp
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
    hex(0x4361EE), hex(0x7B2CBF), hex(0x0F8FA3), hex(0x8D99AE)};
const std::array<QColor, kPapeles> kOscuro{
    hex(0x23253A), hex(0x2B2D42), hex(0x33364F), hex(0x1A1C2B), hex(0x3A3D55),
    hex(0xEDF2F4), hex(0xA9B1C4), hex(0x737C96),
    hex(0xEF233C), hex(0x4A2332),
    hex(0x3ECF8E), hex(0xFF922B), hex(0xF6C453), hex(0x7B93FF),
    hex(0x7B93FF), hex(0xB57BFF), hex(0x3CC8DC), hex(0xA9B1C4)};

constexpr std::array<const char*, kPapeles> kIds{
    "fondo", "superficie", "superficie-alzada", "barra", "borde",
    "texto", "tenue", "apagado",
    "marca", "marca-suave",
    "ingreso", "gasto", "aviso", "serie",
    "operacion", "ahorro", "inversion", "personal"};

Tema gTema = Tema::Claro;
```

Fuera del namespace anónimo:

```cpp
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
```

`pocketColor` y `alertColor` cambian el tipo de retorno a `Tono`; en `alertColor`, `Warning` devuelve `kAviso` (no `kInversion`).

`setLabelColor` queda:

```cpp
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
```

- [ ] **Step 5: La hoja de estilo** — en `styleSheet()`:
  1. Al final, antes del `return`, agregar las reglas por papel:
     ```cpp
     // Una regla por papel: setLabelColor solo pone la propiedad y esto decide
     // el color. Fondo transparente, como hacia la version anterior, para que
     // la etiqueta no pinte un rectangulo encima de la tarjeta.
     QString tonos;
     for (int i = 0; i < kPapeles; ++i) {
         const Tono tono{static_cast<Papel>(i)};
         tonos += QStringLiteral("*[tono=\"%1\"] { color: %2; background: transparent; }\n")
                      .arg(QLatin1String(tono.id()), tono.name());
     }
     ```
     y devolver `base + inputs + table + transfer + marco + tonos` (con `marco` del punto 3).
  2. `#PrimaryButton:hover` y `:pressed` dejan los cian literales: `#D90429` y `#B8001F`. `#PrimaryButton` usa `color: #FFFFFF` (hoy usa `%5` = fondo, que en claro sería blanco sobre rojo por casualidad y en oscuro no).
  3. Nuevo bloque `marco` (la barra es oscura en los dos temas, por eso sus colores son fijos acá y en ningún otro lado):
     ```cpp
     const QString marco = QStringLiteral(R"css(
     QPushButton {
         background-color: %1;
         border: 1px solid %2;
         border-radius: 8px;
         color: %3;
         padding: 6px 14px;
     }
     QPushButton:hover { border: 1px solid %4; }
     QPushButton:disabled { color: %5; }

     #Sidebar { background-color: %6; border-right: none; }
     #Sidebar QLabel { color: #A9B1C4; }
     #Sidebar QPushButton { background: transparent; border: none; color: #A9B1C4; text-align: left; }
     #Sidebar QPushButton:hover { color: #FFFFFF; }
     #NavButton { border-radius: 0px; padding: 9px 14px; }
     #NavButton:checked {
         background-color: rgba(239, 35, 60, 46);
         border-left: 3px solid #EF233C;
         padding-left: 11px;
         color: #FFFFFF;
         font-weight: 600;
     }
     #CaptureFrame { background-color: %1; border: 1px solid #EF233C; border-radius: 14px; }
     )css")
                                .arg(kSurface.name(), kBorder.name(), kText.name(), kTextMuted.name(),
                                     kTextFaint.name(), kSidebar.name());
     ```
  4. En el bloque `base`, borrar las reglas viejas `#Sidebar`, `#SidebarBrand`, `#NavButton`, `#NavButton:hover`, `#NavButton:checked` y `#CardTitle` (las reemplaza `marco`; los títulos de tarjeta toman `Marca` en la Task 4). Ajustar la lista de `.arg(...)` si algún marcador queda sin uso: Qt avisa en la consola con "QString::arg: Argument missing".

- [ ] **Step 6: Que compile** — `.\compilar.ps1 debug`. El compilador marca cada lugar que usaba un `QColor`. Reglas para arreglarlos, en este orden:
  1. **Donde hace falta `QColor` y no hay conversión implícita** (`setForeground(...)`, `QPen(tono, 2)`, `QBrush(...)`, `row.color = ...` hacia un `QColor`): envolver con `theme::color(...)`.
  2. **Miembros y campos que guardan un color de tema** pasan a `theme::Tono`: `KpiCard::accent_` (y su parámetro `const QColor& accent` → `theme::Tono accent`, y `setNote(const QString&, theme::Tono)`), `CompareChart::barColor_` y `setData(..., theme::Tono barColor)`, los dos `QColor color;` de `ui/charts.hpp` (líneas ~146 y ~201) → `theme::Tono color{theme::Papel::Texto};`. Funciones que devuelven un color de tema (`statusColor` en `repairspage.cpp`, `kindColor` en `movementspage.cpp`) → devuelven `theme::Tono`. Variables locales que luego van a `setLabelColor` (`amountColor` en `capturewidget.cpp`) → `const theme::Tono`.
  3. `ui/tables.hpp`: `setText`/`setNumber` dejan `const QColor& color = theme::kText` tal cual (el `Tono` se convierte al llamar, que es cuando corresponde).
  4. Texto enriquecido que arma HTML con `.name()` (`repairspage.cpp:line()`, `repairdialogs.cpp:151`): sin cambios; se regenera en cada `setSnapshot`. `line()` recibe `theme::Tono` en vez de `const QColor&`.
  5. **Reasignar significados** (no los marca el compilador; hacerlo a mano):
     - `kInversion` queda **solo** para el bolsillo (`pocketColor`). Todo otro uso → `kAviso`: `capturewidget.cpp:375`, `dialogs.cpp:198`, `pocketspage.cpp:143`, `quotedialog.cpp:84`, `repairspage.cpp:54,355,487,537`, `reportspage.cpp:415,485,715`, `settingspage.cpp:385,418,434`, `todaypage.cpp:144,157,464`, `charts.cpp:619-623`. Antes de cambiar cada uno, leer la línea: si de verdad habla del bolsillo de inversión, se queda.
     - `kAccent` en datos de gráficas → `kSerie`: `charts.cpp:125,225,229,234,358,639,641` y `reportspage.cpp:717`. El resto de `kAccent` (sueldo, títulos, ícono de bandeja) queda: es la marca.
  6. **Borrar toda hoja de estilo por widget**: `setStyleSheet(theme::styleSheet())` en `bankdialog.cpp:45`, `capturewindow.cpp:19`, `dialogs.cpp:48`, `mainwindow.cpp:365`, `quotedialog.cpp:50`, `repairdialogs.cpp:46`; y en `capturewindow.cpp:27-29` el `frame->setStyleSheet(...)` (lo cubre `#CaptureFrame` del paso 5). En `preview.cpp:220`, `app.setStyleSheet(dake::ui::theme::styleSheet())` → `dake::ui::theme::setTheme(dake::ui::theme::Tema::Claro);`. En `MainWindow::MainWindow` (`mainwindow.cpp`), después de crear `repository_` (línea ~91), agregar `theme::setTheme(theme::Tema::Claro);` (la Task 3 lo cambia por el valor guardado).

- [ ] **Step 7: Verificar los focos de revisión 1 y 3**

```bash
cd /c/Users/David/Documents/VisualStudio/finanzas-banco-pruebas
grep -rn "setStyleSheet(theme::styleSheet())\|setStyleSheet(dake::ui::theme::styleSheet())" ui tests   # esperado: nada
grep -rnE "QColor [a-zA-Z]+_;|QColor color;" ui                                                       # esperado: nada
grep -rn "kInversion" ui | grep -v "theme\.\(hpp\|cpp\)"                                                # esperado: solo usos del bolsillo
grep -rnE "#[0-9A-Fa-f]{6}" ui --include=*.cpp | grep -v "^ui/theme.cpp"                                # esperado: nada
```

- [ ] **Step 8: Correr `dake_uitest`.** Esperado: las 4 líneas de `[tema: un papel cambia de color con el tema]` en `ok`, y todo lo demás como antes. Si la del color efectivo falla pero la de `tono` pasa, el problema es que la hoja no se re-aplicó: revisar que no quede ninguna hoja por widget (paso 7).

- [ ] **Step 9: Mirar una captura** para confirmar que nada quedó sin fondo o ilegible:

```bash
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe hoy "$TEMP/hoy-claro.png" 1280 1400
```

Abrirla con la herramienta de lectura de imágenes. Esperado: fondo claro, tarjetas blancas, montos de gasto en naranja.

- [ ] **Step 10: `.\compilar.ps1 debug -Probar`** — 4/4, cero advertencias.

- [ ] **Step 11: Commit** — `"Los colores son papeles: paleta clara y oscura"`.

---

### Task 3: Elegir el tema en Ajustes, y que se recuerde

**Files:**
- Modify: `ui/pages.hpp` (`SettingsPage`), `ui/settingspage.cpp`, `ui/mainwindow.cpp`, `ui/capturewidget.cpp` (nombre de objeto del monto)
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: `theme::setTheme`, `currentTheme`, `temaFromString`, `toString` (Task 2).
- Produces: señal `SettingsPage::themeChanged(dake::ui::theme::Tema tema)`; `QComboBox` con objectName `TemaSelector` (índice 0 = Claro, 1 = Oscuro); `QLabel` del monto de la captura con objectName `CaptureAmount`.

- [ ] **Step 1: Escribir la prueba que falla** — en `tests/uitest.cpp`, antes de `std::printf("\n%s\n", gFailures == 0 ...`:

```cpp
    std::printf("\n[tema oscuro desde Ajustes, y que se recuerde]\n");
    {
        check(ui::theme::temaFromString(QStringLiteral("azul")) == ui::theme::Tema::Claro, "un valor raro arranca en claro");
        check(ui::theme::temaFromString(QString()) == ui::theme::Tema::Claro, "sin valor, claro");
        check(ui::theme::temaFromString(QStringLiteral(" Oscuro ")) == ui::theme::Tema::Oscuro, "'Oscuro' con espacios, oscuro");

        auto* selector = window.findChild<QComboBox*>(QStringLiteral("TemaSelector"));
        auto* amount = window.findChild<QLabel*>(QStringLiteral("CaptureAmount"));
        auto* input = window.findChild<QLineEdit*>(QStringLiteral("CaptureInput"));
        check(selector != nullptr && amount != nullptr && input != nullptr, "el selector de tema esta en Ajustes");
        if (selector != nullptr && amount != nullptr && input != nullptr) {
            input->clear();
            input->setFocus();
            QTest::keyClicks(input, QStringLiteral("25 almuerzo"));
            settle();
            check(amount->property("tono").toString() == QStringLiteral("gasto"), "el monto del gasto lleva el papel 'gasto'");
            selector->setCurrentIndex(1);
            settle();
            check(ui::theme::currentTheme() == ui::theme::Tema::Oscuro, "elegir Oscuro cambia el tema");
            check(amount->palette().color(amount->foregroundRole()) == QColor(QStringLiteral("#FF922B")),
                  "y el monto ya esta en el naranja del oscuro, sin reabrir nada");
            check(repository.setting(QStringLiteral("ui.tema")).value_or(QString()) == QStringLiteral("oscuro"),
                  "queda guardado en ui.tema");
            input->clear();
        }
    }
    {
        ui::theme::setTheme(ui::theme::Tema::Claro);
        ui::MainWindow again(path);
        check(ui::theme::currentTheme() == ui::theme::Tema::Oscuro, "al reabrir, arranca en oscuro");
    }
    ui::theme::setTheme(ui::theme::Tema::Claro);
```

(Si `repository.setting` no se llama así, usar el getter que ya existe en `storage::Repository` junto a `setSetting`; `mainwindow.cpp:803` usa `repository_->setting(...)`.)

- [ ] **Step 2: Correr y ver que falla**: `el selector de tema esta en Ajustes` → FALLA.

- [ ] **Step 3: El monto con nombre** — en `ui/capturewidget.cpp`, después de crear `amount_`: `amount_->setObjectName(QStringLiteral("CaptureAmount"));`

- [ ] **Step 4: El selector en Ajustes** — en `ui/pages.hpp`, dentro de `SettingsPage`: `#include "theme.hpp"` si falta; señal `void themeChanged(dake::ui::theme::Tema tema);`; miembro `QComboBox* tema_ = nullptr;`. En `ui/settingspage.cpp`, en `buildUi()`, antes de `auto* captureCard = new Card(QStringLiteral("CAPTURA"), page);`:

```cpp
    auto* lookCard = new Card(QStringLiteral("APARIENCIA"), page);
    lookCard->setSubtitle(QStringLiteral("Claro u oscuro. Cambia al instante, tambien en la ventana de anotar."));
    tema_ = new QComboBox(lookCard);
    tema_->setObjectName(QStringLiteral("TemaSelector"));
    tema_->setFont(theme::bodyFont(10));
    tema_->addItems({QStringLiteral("Claro"), QStringLiteral("Oscuro")});
    tema_->setMaximumWidth(220);
    connect(tema_, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit themeChanged(index == 1 ? theme::Tema::Oscuro : theme::Tema::Claro);
    });
    lookCard->addContent(tema_);
    layout->addWidget(lookCard);
```

(Usar el mismo nombre de layout que usan las demás tarjetas de esa función.) En `setSnapshot`, junto al `QSignalBlocker blockAutostart(autostart_);`:

```cpp
        const QSignalBlocker blockTema(tema_);
        tema_->setCurrentIndex(theme::currentTheme() == theme::Tema::Oscuro ? 1 : 0);
```

- [ ] **Step 5: Aplicar y guardar** — en `ui/mainwindow.cpp`:
  1. Reemplazar el `theme::setTheme(theme::Tema::Claro);` que agregó la Task 2 por:
     ```cpp
     // El tema va antes que cualquier widget: asi nada se construye con la
     // hoja de otro tema.
     theme::setTheme(theme::temaFromString(
         repository_->setting(QStringLiteral("ui.tema")).value_or(QString())));
     ```
  2. Junto a los demás `connect(settings_, ...)`:
     ```cpp
     connect(settings_, &SettingsPage::themeChanged, this, [this](theme::Tema tema) {
         theme::setTheme(tema);
         repository_->setSetting(QStringLiteral("ui.tema"), theme::toString(tema));
         // Las tablas y el texto enriquecido guardan el color con que se
         // llenaron: se vuelven a llenar. Lo pintado a mano, solo se repinta.
         reload();
         for (QWidget* widget : QApplication::allWidgets()) {
             widget->update();
         }
     });
     ```

- [ ] **Step 6: Correr la prueba.** Esperado: todo el bloque `[tema oscuro desde Ajustes, y que se recuerde]` en `ok`, `Todo pasa.`

- [ ] **Step 7: `.\compilar.ps1 debug -Probar`** — 4/4, cero advertencias.

- [ ] **Step 8: Commit** — `"El tema se elige en Ajustes y se recuerda"`.

---

### Task 4: La marca — logo, Anton, rótulos y botón principal

**Files:**
- Modify: `ui/mainwindow.cpp:631-645` (`buildSidebar`), `ui/cards.cpp`, `ui/todaypage.cpp`, `ui/mainwindow.cpp:218-232` (`trayIcon`)
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: `figureFont` (Task 1), `:/dake/logo/DAke.png` (Task 1), `kAccent` = Marca (Task 2).
- Produces: `QLabel` objectName `SidebarLogo` (con pixmap) y `SidebarBrand` (texto `DAKE`+`LABS`).

- [ ] **Step 1: Escribir la prueba que falla** — en `tests/uitest.cpp`, después de `check(QTest::qWaitForWindowExposed(&window), "la ventana principal se abre");`:

```cpp
    {
        auto* logo = window.findChild<QLabel*>(QStringLiteral("SidebarLogo"));
        auto* brand = window.findChild<QLabel*>(QStringLiteral("SidebarBrand"));
        check(logo != nullptr && !logo->pixmap().isNull(), "el logo de DakeLabs esta en la barra lateral");
        check(brand != nullptr && brand->text().contains(QStringLiteral("LABS")) &&
                  brand->font().family() == QStringLiteral("Anton"),
              "con DAKELABS en Anton");
    }
```

- [ ] **Step 2: Correr y ver que falla.**

- [ ] **Step 3: La barra lateral** — en `buildSidebar`, reemplazar los dos `QLabel` `brand` ("Finanzas") y `subtitle` ("DakeLabs") por:

```cpp
    auto* brandRow = new QHBoxLayout();
    brandRow->setSpacing(8);
    auto* logo = new QLabel(parent);
    logo->setObjectName(QStringLiteral("SidebarLogo"));
    logo->setPixmap(QPixmap(QStringLiteral(":/dake/logo/DAke.png"))
                        .scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandRow->addWidget(logo);
    // La barra es azul-noche en los dos temas: DAKE siempre blanco, LABS en la
    // marca. Por eso el color va escrito aca y no por papel.
    auto* brand = new QLabel(QStringLiteral("<span style='color:#FFFFFF'>DAKE</span>"
                                            "<span style='color:#EF233C'>LABS</span>"),
                             parent);
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    brand->setFont(theme::figureFont(18));
    brandRow->addWidget(brand);
    brandRow->addStretch(1);
    layout->addLayout(brandRow);

    auto* subtitle = new QLabel(QStringLiteral("Finanzas"), parent);
    subtitle->setFont(theme::bodyFont(9));
    layout->addWidget(subtitle);
```

(Nota: esta es la excepción de la barra lateral de Global Constraints; el `grep` de hexadecimales de la Task 2 paso 7 va a mostrar estas dos líneas y es correcto. Agregar `#include <QPixmap>` si falta.)

- [ ] **Step 4: Rótulos de tarjeta en rojo de marca** — en `ui/cards.cpp`: el `titleLabel` de `Card` y `title_` de `KpiCard` pasan de `theme::kTextMuted` a `theme::kAccent`. `value_` de `KpiCard`: `theme::numericFont(21, QFont::Bold)` → `theme::figureFont(21)`.

- [ ] **Step 5: Cifras grandes de Hoy en Anton** — listar las candidatas:

```bash
grep -n "numericFont(\(1[6-9]\|[2-9][0-9]\)" ui/todaypage.cpp ui/reportspage.cpp ui/cards.cpp
```

Cada una (valores de 16 pt o más: caja, resultado, costo de estructura, titular de "con qué se pagó") → `theme::figureFont(<mismo tamaño>)`. Las de menos de 16 pt quedan en `numericFont`.

- [ ] **Step 6: Foco de revisión 4** — ninguna Anton chica:

```bash
grep -rnE "figureFont\(([0-9]|1[0-5])\)" ui    # esperado: nada
```

- [ ] **Step 7: Ícono de la bandeja** — en `trayIcon()`, la letra pasa de `theme::kBackground` a `Qt::white` (sobre el rojo de marca, en los dos temas).

- [ ] **Step 8: Correr la prueba y mirar las capturas**:

```bash
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uitest.exe 2>&1 | grep -E "FALLA|Todo pasa"
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe hoy "$TEMP/hoy-marca.png" 1280 1400
```

Nota: `dake_uipreview` muestra páginas sueltas, sin la barra lateral; la barra se verifica en la Task 6 con la ventana de verdad. En la captura: rótulos rojos, cifras de las tarjetas en Anton.

- [ ] **Step 9: `.\compilar.ps1 debug -Probar`** — 4/4, cero advertencias.

- [ ] **Step 10: Commit** — `"La marca: logo, Anton y el rojo de DakeLabs"`.

---

### Task 5: Pastillas de estado en Reparaciones

**Files:**
- Modify: `ui/theme.cpp` (reglas de pastilla), `ui/repairspage.cpp:42-58, 193-196, 416, 449-450`
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: paleta (Task 2).
- Produces: propiedad dinámica `pill` en `QLabel` con valores `proceso` / `cobrar` / `cobrada`; el `QLabel` de estado de la ficha con objectName `RepairStatus`.

- [ ] **Step 1: Escribir la prueba que falla** — en `tests/uitest.cpp`, en el bloque `[el cobro anotado en la captura cierra la reparacion]`, después de `check(asus != nullptr && asus->status == core::RepairStatus::Cobrada, ...)`:

```cpp
        if (asus != nullptr) {
            auto* repairsPage = window.findChild<ui::RepairsPage*>();
            repairsPage->selectRepair(asus->jobId);
            settle();
            auto* pill = repairsPage->findChild<QLabel*>(QStringLiteral("RepairStatus"));
            check(pill != nullptr && pill->text() == QStringLiteral("Cobrada") &&
                      pill->property("pill").toString() == QStringLiteral("cobrada"),
                  "la ficha muestra la pastilla 'Cobrada'");
        }
```

- [ ] **Step 2: Correr y ver que falla** (hoy el texto es `COBRADA` y no hay objectName).

- [ ] **Step 3: Reglas de pastilla** — en `styleSheet()` de `ui/theme.cpp`, un bloque más (sumarlo al `return`):

```cpp
    // Pastillas de estado, como las de Cotizaciones. Alfa en 0-255: 20 % = 51,
    // 15 % = 38.
    const auto alpha = [](Tono tono, int a) {
        const QColor c = color(tono);
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
    };
    const QString pills = QStringLiteral(R"css(
QLabel[pill] { border-radius: 9px; padding: 2px 10px; font-weight: 600; }
QLabel[pill="proceso"] { background-color: %1; color: %2; }
QLabel[pill="cobrar"]  { background-color: %3; color: %4; }
QLabel[pill="cobrada"] { background-color: %5; color: %6; }
)css")
                              .arg(kText.name(), kSurface.name(), alpha(kAviso, 51), kAviso.name(),
                                   alpha(kPositive, 38), kPositive.name());
```

- [ ] **Step 4: La pastilla en la página** — en `ui/repairspage.cpp`, reemplazar `statusColor` por:

```cpp
[[nodiscard]] QString pillId(core::RepairStatus status) {
    switch (status) {
        case core::RepairStatus::EnProceso: return QStringLiteral("proceso");
        case core::RepairStatus::Entregada: return QStringLiteral("cobrar");
        case core::RepairStatus::Cobrada: return QStringLiteral("cobrada");
    }
    return QStringLiteral("proceso");
}

/// Convierte la etiqueta en pastilla de estado; el color lo pone la hoja global.
void setPill(QLabel* label, core::RepairStatus status) {
    label->setText(statusLabel(status));
    label->setProperty("pill", pillId(status));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

/// Una pastilla para una celda de tabla: sin el contenedor se estiraria a
/// todo el ancho de la columna.
[[nodiscard]] QWidget* pillCell(core::RepairStatus status) {
    auto* cell = new QWidget();
    auto* row = new QHBoxLayout(cell);
    row->setContentsMargins(6, 3, 6, 3);
    auto* pill = new QLabel(cell);
    pill->setFont(theme::bodyFont(8, QFont::DemiBold));
    setPill(pill, status);
    row->addWidget(pill);
    row->addStretch(1);
    return cell;
}
```

- En la construcción de la ficha (línea ~193): `status_->setObjectName(QStringLiteral("RepairStatus"));`
- Línea ~449-450: `setPill(status_, repair->status);` en lugar de `setText(...toUpper())` + `setLabelColor`.
- Línea ~416: en lugar de `setText(list_, row, 4, statusLabel(...), statusColor(...))`: `setText(list_, row, 4, QString()); list_->setCellWidget(row, 4, pillCell(repair.status));`
- Agregar `#include <QStyle>` si falta.

- [ ] **Step 5: Correr la prueba** — `la ficha muestra la pastilla 'Cobrada'` en `ok`; el resto igual.

- [ ] **Step 6: Captura de Reparaciones en claro**:

```bash
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe reparaciones "$TEMP/rep.png" 1280 1000
```

Esperado: columna de estado con pastillas; ninguna estirada a lo ancho.

- [ ] **Step 7: `.\compilar.ps1 debug -Probar`** — 4/4, cero advertencias.

- [ ] **Step 8: Commit** — `"Pastillas de estado en Reparaciones"`.

---

### Task 6: Revisión a ojo en los dos temas

**Files:**
- Modify: `ui/preview.cpp` (argumento `--oscuro`)
- Test: capturas + app real

**Interfaces:**
- Consumes: todo lo anterior.

- [ ] **Step 1: El banco visual acepta `--oscuro`** — en `ui/preview.cpp`, en `main`, justo después de `QApplication app(argc, argv);`:

```cpp
    QStringList arguments = QCoreApplication::arguments();
    // "--oscuro" en cualquier lugar genera las capturas con el tema oscuro.
    const bool dark = arguments.removeAll(QStringLiteral("--oscuro")) > 0;
    dake::ui::theme::setTheme(dark ? dake::ui::theme::Tema::Oscuro : dake::ui::theme::Tema::Claro);
```

y borrar la línea `const QStringList arguments = QCoreApplication::arguments();` que venía después (ahora `arguments` ya existe, sin el flag). Agregar `--oscuro` al texto de uso.

- [ ] **Step 2: Todas las pantallas en los dos temas**

```bash
cd /c/Users/David/Documents/VisualStudio/finanzas-banco-pruebas
export PATH="/c/Qt/6.8.3/msvc2022_64/bin:$PATH"
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe todas "$TEMP/claro" 1280 1400
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe todas "$TEMP/oscuro" 1280 1400 --oscuro
QT_QPA_PLATFORM=offscreen ./build/debug/bin/dake_uipreview.exe "captura:25 almuerzo ayer" "$TEMP/captura-oscuro.png" 960 200 --oscuro
```

- [ ] **Step 3: Mirar las 15 capturas** con la herramienta de lectura de imágenes. Lista de control, por captura:
  - ningún texto ilegible (gris sobre gris, blanco sobre blanco);
  - ningún rectángulo de otro color detrás de una etiqueta;
  - gastos y negativos en naranja, nunca en rojo; rojo solo en rótulos, botón principal y sueldo;
  - las gráficas en azul/verde/naranja, sin rojo;
  - tablas: encabezado, filas y selección legibles; el calendario emergente no negro sobre negro.

  Cada defecto se arregla en `ui/theme.cpp` (o en el widget que lo causa, si guarda un color) y se vuelve a generar la captura.

- [ ] **Step 4: La app de verdad** — `.\compilar.ps1 debug -Probar` (4/4, cero advertencias), abrir `build\debug\bin\dake_pruebas.exe` y avisarle a David que la mire: barra lateral con el logo, Ajustes → Apariencia → Oscuro, y que vuelva a Claro. **No** probar el cambio con teclas del sistema.

- [ ] **Step 5: Commit** — `"Banco visual en claro y oscuro"`.
