# Recorte a lo fundamental — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dejar Finanzas con lo fundamental. Hoy pasa a tener un formulario de
anotar y una lista de pendientes. Todos los números se van a Informes (5
pestañas por pregunta). Se suma un bolsillo Emergencia. Se quitan el intérprete,
"Dura", Revisión y el banco. Se arreglan la celda vacía al editar y la lentitud.

**Architecture:** El núcleo (`dake::core`, solo STL) pierde el intérprete y el
lector de extractos, gana `PocketKind::Emergencia`, `reserveTotal`,
`parseAmount` y el pendiente `SinCuadrar`, y deja de repartir gastos en meses.
En la interfaz, `EntryForm` reemplaza a `CaptureWidget` (lo usan Hoy y la
ventana chica), `PendingList` reemplaza a `ReviewPage` dentro de Hoy, y
`ReportsPage` se rearma en 5 pestañas que reciben las tarjetas que salen de Hoy
y de Bolsillos. Las páginas ocultas se rellenan recién al mostrarse. El
teléfono recibe el mismo formulario y el tipo Emergencia.

**Tech Stack:** Qt 6.8.3 MSVC 2022, C++20, CMake presets, SQLite, QML (teléfono).

**Spec:** `docs/superpowers/specs/2026-09-27-recorte-a-lo-fundamental-design.md`

## Global Constraints

- Compilar con `.\compilar.ps1 debug -Probar` desde la raíz del repo. Tienen que pasar las 4 suites (nucleo, negocio, persistencia, interfaz) sin **ninguna advertencia** (/W4).
- `dake::core` es solo STL: sin Qt, sin SQL, sin API de Windows.
- Los montos se guardan en centavos (`int64`).
- No se borra ninguna columna ni tabla de la base ni de la nube: se deja de usar lo que sobra.
- Las pruebas de interfaz corren offscreen con QTest. **Nunca** se simulan teclas globales (`keybd_event`/`SendInput`).
- Todo el texto de la interfaz va en español. El separador decimal es la coma.
- La ventana principal es el único lugar que escribe en la base. Las páginas emiten señales.
- Cada commit termina con estas dos líneas:
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
  `Claude-Session: https://claude.ai/code/session_01JhaoWSW44Z6pTAXd225bUt`
- Si `dake_pruebas.exe` está abierto, el enlazado falla con LNK1168. En ese caso se compilan los demás objetivos y se le pide a David que la cierre desde la bandeja. Nunca se la mata sin avisar.

## Review Focus

1. **Enter con el completador de categorías abierto** tiene que elegir la sugerencia, no guardar a medias. Prueba en la Task 5.
2. **Montos escritos como se dicen:** "1.200" es mil doscientos, "25,5" es 25,50, y "0", "abc" y "12 pan" no son montos. Prueba en la Task 3.
3. **Traspaso con De = A, o con un solo bolsillo:** se muestra el error bajo el botón y no se guarda nada. Prueba en la Task 5.
4. **Bolsillo archivado:** no aparece en el formulario y nunca da "sin cuadrar". Pruebas en las Tasks 4 y 5.
5. **Gasto de una categoría personal pagado desde la caja del negocio:** se sigue guardando como sueldo más gasto personal (`splitCrossExpense`). Prueba en la Task 5.

## Desviación documentada respecto del spec

- El spec dice que el tipo se cambia con `Ctrl+1/2/3`, pero esas teclas ya cambian de sección en la ventana (`MainWindow::buildUi`). El tipo pasa a cambiarse con **Alt+G**, **Alt+I** y **Alt+T** (mnemónicos de los botones). En la Task 5 se corrige esa línea del spec.
- "Material por delante" también sale del Hoy del teléfono. Ese número deja de existir en el núcleo, así que su tarjeta no tiene de dónde leer.

---

### Task 1: Bolsillo Emergencia en el núcleo y en el tema

**Files:**
- Modify: `core/include/dake/core/model.hpp:44-56`, `core/src/model.cpp:28-55`
- Modify: `core/include/dake/core/report.hpp:51-56`, `core/src/report.cpp:118-125,185-195,400-405`
- Modify: `ui/theme.hpp`, `ui/theme.cpp` (papel nuevo, `pocketColor`)
- Modify: `ui/pocketspage.cpp`, `ui/todaypage.cpp`, `mobile/appbridge.cpp` (sumas de reserva)
- Test: `tests/selftest.cpp`

**Interfaces:**
- Produces:
  - `core::PocketKind::Emergencia` (el valor 4)
  - `core::allPocketKinds()` → `std::array<PocketKind, 5>`
  - `core::isReserve(Emergencia) == true`
  - `core::Money core::reserveTotal(const std::vector<PocketBalance>&, Currency)`
  - `theme::Papel::Emergencia`, `theme::kEmergencia`, `kPapeles = 19`

- [ ] **Step 1: Prueba que falla** — agregar al final de `tests/selftest.cpp`, antes de `main`, y llamarla desde `main`:

```cpp
void bolsilloDeEmergencia() {
    std::printf("\n-- bolsillo de emergencia --\n");
    check(pocketKindFromString("Emergencia") == PocketKind::Emergencia, "'Emergencia' se lee");
    check(toString(PocketKind::Emergencia) == "Emergencia", "y se escribe igual");
    check(isReserve(PocketKind::Emergencia), "es reserva, como el ahorro");
    check(allPocketKinds().size() == 5, "hay cinco tipos de bolsillo");

    std::vector<Pocket> pockets{pocket("caja", PocketKind::Operacion, 100'00),
                                pocket("ahorro", PocketKind::Ahorro, 200'00),
                                pocket("fondo", PocketKind::Emergencia, 50'00)};
    const auto balances = pocketBalances(pockets, {}, kUsd, Date::fromIso("2026-09-27"));
    checkMinor(reserveTotal(balances, kUsd).minor(), 250'00, "la reserva suma ahorro y emergencia");
}
```

  (`pocket(id, kind, opening)` es el ayudante que ya existe en `selftest.cpp`. Si su firma es otra, se arma el `Pocket` a mano con `id`, `name`, `kind` y `openingMinor`.)

- [ ] **Step 2:** `.\compilar.ps1 debug -Probar`. Esperado: falla la compilación porque no existe `PocketKind::Emergencia`.
- [ ] **Step 3: Implementar.**
  - `model.hpp`: agregar `Emergencia  ///< el fondo para imprevistos; no se toca salvo emergencia` al enum. La firma pasa a `std::array<PocketKind, 5> allPocketKinds() noexcept;`.
  - `model.cpp`:
    - `toString` suma `case PocketKind::Emergencia: return "Emergencia";`.
    - La tabla de `pocketKindFromString` pasa a tener 5 entradas.
    - `allPocketKinds` devuelve los 5.
    - `isReserve` queda `value == Ahorro || value == Inversion || value == Emergencia`.
  - `report.hpp`: agregar

    ```cpp
    /// Todo lo que es reserva (ahorro, inversion, emergencia): lo que se puede
    /// consumir si el negocio no alcanza.
    [[nodiscard]] Money reserveTotal(const std::vector<PocketBalance>& balances, Currency currency);
    ```

  - `report.cpp`: implementarla sumando `balance.balance` de los `isReserve(balance.kind)`. En las líneas 190-191 y en todo lugar que haga `totalFor(Ahorro) + totalFor(Inversion)` (`ui/pocketspage.cpp:164`, `mobile/appbridge.cpp:437`) se usa `reserveTotal`.
  - `theme.hpp`:
    - agregar `Emergencia` al final de `Papel`;
    - `kPapeles = 19`;
    - `inline constexpr Tono kEmergencia{Papel::Emergencia};`.
  - `theme.cpp`:
    - en `kClaro` el color es `#D6336C`, y en `kOscuro` es `#F06595`;
    - el id del papel en el mapa de ids es `"emergencia"`;
    - `pocketColor` suma `case core::PocketKind::Emergencia: return kEmergencia;`.
  - `ui/pocketspage.cpp` `kindLabel` y `mobile/appbridge.cpp` `kindLabel` suman `case core::PocketKind::Emergencia: return QStringLiteral("Emergencia");`.
- [ ] **Step 4:** `.\compilar.ps1 debug -Probar`. Esperado: todo pasa, 0 advertencias. Si hay un `switch` sin el caso nuevo, /W4 lo marca (C4061/C4062) y se completa.
- [ ] **Step 5: Commit** `Bolsillo Emergencia: tipo nuevo, reserva y color`.

### Task 2: "Dura" deja de repartir

**Files:**
- Modify: `core/src/report.cpp:197-215,280-300,458-462,510-513`; `core/include/dake/core/report.hpp:120-131`
- Modify: `ui/todaypage.cpp` (quitar `kpiPrepaid_`), `ui/pages.hpp`, `mobile/appbridge.cpp:311-312`, `mobile/qml/Hoy.qml:115-120`
- Test: `tests/selftest.cpp` (`repartoDelCosto`), `tests/negociotest.cpp:1267`

**Interfaces:**
- Produces: `costInMonth` cuenta el gasto completo en el mes de su fecha, sin mirar `spreadMonths`. `unusedPrepaid` deja de existir.

- [ ] **Step 1: Prueba.** Se reemplaza el cuerpo de `repartoDelCosto()` en `tests/selftest.cpp` por:

```cpp
void repartoDelCosto() {
    std::printf("\n-- un gasto cuenta entero en su mes --\n");
    // Un movimiento viejo con "dura 4 meses" ya no se reparte: cuenta completo
    // el mes en que se pago.
    const Movement pla = expense("pla", "2026-08-24", 46'00, "caja", 4);
    checkMinor(costInMonth(pla, kUsd, 2026, 8).minor(), 46'00, "los 46,00 caen enteros en agosto");
    checkMinor(costInMonth(pla, kUsd, 2026, 9).minor(), 0, "y septiembre no carga nada");
    const auto flow = cashFlow({pla}, kUsd, Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));
    checkMinor(flow.cost.minor(), 46'00, "el costo de agosto es lo pagado");
}
```

  - En `negociotest.cpp:1267` se deja `filamento.spreadMonths = 4;` (prueba que se ignora). Se corrigen los números esperados de `utilidadNetaDelMes` y de sus vecinos que cambien: junio pasa a cargar los 120,00 completos.
- [ ] **Step 2:** Compilar y probar. Esperado: FALLA "los 46,00 caen enteros en agosto".
- [ ] **Step 3: Implementar.**
  - En `costInMonth`, reemplazar el bloque del offset y del allocate por
    `return monthIndex(year, month) == monthIndex(movement.date) ? Money::fromMinor(movement.amountMinor, currency) : Money::zero(currency);`.
  - En `summarizeByMonth`, `span` vale siempre 1.
  - Borrar `unusedPrepaid` del header y del .cpp, junto con el aviso que lo usa (líneas 458-462).
  - Quitar la tarjeta "MATERIAL POR DELANTE" de `todaypage.cpp` y `pages.hpp`.
  - Quitar `summary_["prepaid"]` de `appbridge.cpp` y el `Cifra` de `Hoy.qml`.
  - `model.hpp:132`: el comentario de `spreadMonths` pasa a decir "Legado: ya no se usa; se guarda para no romper la sincronizacion".
- [ ] **Step 4:** Compilar y probar. Esperado: todo pasa.
- [ ] **Step 5: Commit** `Un gasto cuenta entero en su mes: fuera "Dura" y material por delante`.

### Task 3: Fuera el intérprete y el banco del núcleo

**Files:**
- Modify: `core/include/dake/core/capture.hpp`, `core/src/capture.cpp` (quedan solo los ayudantes)
- Delete: `core/include/dake/core/bankcsv.hpp`, `core/src/bankcsv.cpp`; se saca de `core/CMakeLists.txt`
- Modify: `ui/fields.hpp` (usa `parseAmount`), `ui/mainwindow.cpp:1442-1447` (`businessPocket`)
- Test: `tests/negociotest.cpp` (se borran `capturaDelTipoYLaFecha`, `capturaDeLaCategoria`, `capturaDeLaReparacion`, `aprenderCategorias` en su parte de `learnedCategory`/`normalizeDescription`, `csvDelBanco` y `cruzarConLoAnotado`; se reescribe `capturaDelMonto`)

**Interfaces:**
- Produces (en `capture.hpp`):

```cpp
/// Un monto escrito como se dice: "25", "25,50", "25.50", "$25", "1.200".
/// Vacio si no es un monto mayor que cero.
[[nodiscard]] std::optional<std::int64_t> parseAmount(std::string_view text, Currency currency);

/// El bolsillo que se propone: el ultimo usado de esa cuenta (o de cualquiera),
/// sin contar traspasos ni archivados; si no hay historia, el primero.
[[nodiscard]] Id suggestedPocket(const std::vector<Pocket>& pockets,
                                 const std::vector<Movement>& history,
                                 std::optional<Account> account);

[[nodiscard]] std::string foldText(std::string_view text);

[[nodiscard]] std::vector<std::string> categoriesByUse(const std::vector<Category>& categories,
                                                       const std::vector<Movement>& history,
                                                       MovementKind kind, Date today);
```

  Se borran: `RepairRef`, `CaptureContext`, `CategorySource`, `CaptureDraft`, `parseCapture`, `normalizeDescription` y `learnedCategory`.

- [ ] **Step 1: Prueba.** `capturaDelMonto` pasa a ser:

```cpp
void montoEscrito() {
    std::printf("\n[un monto escrito como se dice]\n");
    check(parseAmount("25", kUsd) == 25'00, "'25'");
    check(parseAmount("25,50", kUsd) == 25'50, "coma decimal");
    check(parseAmount("25.50", kUsd) == 25'50, "punto decimal");
    check(parseAmount("25,5", kUsd) == 25'50, "'25,5' son 25,50");
    check(parseAmount("$25", kUsd) == 25'00, "con signo");
    check(parseAmount("1.200", kUsd) == 1200'00, "1.200 son mil doscientos");
    check(!parseAmount("0", kUsd), "cero no es un monto");
    check(!parseAmount("abc", kUsd), "letras tampoco");
    check(!parseAmount("12 pan", kUsd), "ni un monto con texto");
    check(!parseAmount("", kUsd), "ni vacio");
}
```

  - `aprenderCategorias` conserva solo sus chequeos de `categoriesByUse`.
  - Los chequeos de `suggestedPocket` (líneas 377-379) pasan a la firma nueva: `suggestedPocket(c.pockets, c.history, Account::Personal)`.
  - El `contexto()` de las pruebas se convierte en un struct local con `pockets`, `categories`, `history` y `today`.
- [ ] **Step 2:** Compilar. Esperado: falla porque no existe `parseAmount`.
- [ ] **Step 3: Implementar.**
  - En `capture.cpp` quedan `fold`, `parseAmountToken` (renombrado a uso interno de `parseAmount`: el token tiene que ser el texto entero recortado, y `plus` se rechaza), `findPocket`, `lastPocket` (con la firma `(pockets, history, account)`), `foldText` y `categoriesByUse`. Se borra el resto.
  - `fields.hpp::parseMoneyText` queda `return core::parseAmount(trimmed.toStdString(), currency);`.
  - `mainwindow.cpp::businessPocket` queda `return core::suggestedPocket(snapshot_.pockets, snapshot_.movements, core::Account::Negocio);`.
  - Se borran `bankcsv.*` y su línea en `core/CMakeLists.txt`.
  - Se quita `#include "dake/core/bankcsv.hpp"` de `mainwindow.hpp`. Las funciones de banco de la ventana se borran en la Task 6. Mientras tanto, para que esta task compile, **en esta misma task** se borran:
    - `ui/bankdialog.*` y sus líneas en `ui/CMakeLists.txt`;
    - `MainWindow::loadBankProfiles`, `saveBankProfile`, `chooseBankFile` e `importBankFile` (declaración y definición);
    - el `QShortcut` Ctrl+I;
    - la conexión `bankImportRequested`, la señal `SettingsPage::bankImportRequested` y la tarjeta "EXTRACTOS DEL BANCO" de `settingspage.cpp`;
    - el bloque "[extracto del banco]" de `tests/uitest.cpp`.
  - `capturewidget.cpp` y `reviewpage.cpp` todavía usan el intérprete. **En esta task** se mantienen compilando así:
    - `CaptureWidget` todavía no se borra (se va en la Task 5). Para que compile sin `parseCapture`, su `reparse()` usa `core::parseAmount` sobre la línea entera y deja tipo, categoría y fecha como estén.
    - En `reviewpage.cpp`, `learnedCategory` se reemplaza por `std::string()`.

    Son parches de una task: las Tasks 5 y 6 borran ambos archivos.
- [ ] **Step 4:** Compilar y probar. Esperado: pasan todas. En uitest, los bloques que escriben "25 almuerzo ayer" y "60 cobro asus" en `CaptureInput` **van a fallar**, porque la línea ya no interpreta. En esta task esos bloques se marcan `#if 0` con el comentario `// se reescribe en la Task 5 con el formulario`.
- [ ] **Step 5: Commit** `Fuera el interprete y el lector de extractos del nucleo`.

### Task 4: La bandeja: tres tipos menos y "sin cuadrar"

**Files:**
- Modify: `core/include/dake/core/fixed.hpp:117-150`, `core/src/fixed.cpp:141-195`
- Test: `tests/negociotest.cpp` (`bandeja`)

**Interfaces:**
- Produces:

```cpp
enum class InboxKind { PorConfirmar, Cotizaciones, PorCobrar, SinEntregar, SinHoras, CostoRepuesto, SinCuadrar };

struct InboxInput {
    std::vector<Movement> movements;
    std::vector<MovementMeta> metas;
    std::vector<Repair> repairs;
    std::vector<RepairPart> parts;
    std::vector<Pocket> pockets;
    /// La ultima vez que se cuadro cada bolsillo.
    std::vector<std::pair<Id, Date>> lastReconciled;
    /// Desde cuando se cuenta un bolsillo que nunca se cuadro.
    Date reconcileSince;
    int quoteHolds = 0;
    std::vector<std::pair<Id, Date>> snoozed;
    Date today;
};
```

  Las claves para posponer son: `jobId + ":horas"` (sin horas), `pocketId + ":cuadrar"` (sin cuadrar) y `"cotizaciones"` (cotizaciones por decidir). Las demás usan su `refId`.

- [ ] **Step 1: Prueba.** Se reescribe `bandeja()`: se quitan los movimientos "sin", "sug" y "vida" y el bloque `legado`, y se agrega:

```cpp
    in.pockets = {pocketNamed("caja"), pocketNamed("viejo"), pocketNamed("fondo")};
    in.pockets[1].archived = true;
    in.reconcileSince = Date{2026, 8, 1};                 // 55 dias antes del 25/9
    in.lastReconciled = {{"fondo", Date{2026, 8, 26}}};   // 30 dias: todavia no
    // ...
    const std::vector<InboxKind> want{InboxKind::PorConfirmar, InboxKind::Cotizaciones,
                                      InboxKind::PorCobrar,    InboxKind::SinEntregar,
                                      InboxKind::SinHoras,     InboxKind::CostoRepuesto,
                                      InboxKind::SinCuadrar};
    check(kinds == want, "siete pendientes; sin categoria, sugerido y vida util ya no existen");
    check(items.back().refId == "caja" && items.back().days == 55,
          "la caja nunca se cuadro: cuenta desde el inicio, 55 dias");
    // el archivado no aparece; el fondo, a los 30 dias, tampoco
    in.lastReconciled = {{"fondo", Date{2026, 8, 25}}};  // 31 dias
    // ... fondo aparece
    in.snoozed = {{"caja:cuadrar", Date{2026, 9, 30}}, {"cotizaciones", Date{2026, 9, 30}}};
    // ... ni la caja ni las cotizaciones aparecen
```

  Además hay que:
  - escribir el ayudante `pocketNamed(id)`, que arma un `Pocket` de tipo Operacion con `id` y `name`;
  - dejar la prueba de "un movimiento sin categoría ya no es pendiente", con un gasto sin categoría que no produce ningún item;
  - completar los `check` elididos con `std::any_of` sobre `inbox(in)`.
- [ ] **Step 2:** Compilar. Esperado: falla porque no existe `InboxKind::SinCuadrar`.
- [ ] **Step 3: Implementar.**
  - En `fixed.cpp` se borra el bloque de `SinCategoria` y el loop de metas se reduce a `"confirmar" → PorConfirmar`.
  - `Cotizaciones` se agrega solo si `!isSnoozed(input, "cotizaciones")`.
  - Al final se agrega:

```cpp
    for (const Pocket& p : input.pockets) {
        if (p.archived || isSnoozed(input, p.id + ":cuadrar")) continue;
        Date last = input.reconcileSince;
        for (const auto& [id, date] : input.lastReconciled) {
            if (id == p.id) last = date;
        }
        const auto days = static_cast<int>(today - last.toEpochDays());
        if (days > 30) add(InboxKind::SinCuadrar, p.id, days);
    }
```

  - Se actualiza el comentario del orden.
  - En `mainwindow.cpp` y `reviewpage.cpp` se borran los `case` que ya no existen. `ReviewPage` se va en la Task 6, así que solo hace falta que compile.
- [ ] **Step 4:** Compilar y probar. Esperado: todo pasa.
- [ ] **Step 5: Commit** `Bandeja: fuera sin categoria, sugerido y vida util; nuevo bolsillo sin cuadrar`.

### Task 5: `EntryForm`, el formulario de anotar

**Files:**
- Create: `ui/entryform.hpp`, `ui/entryform.cpp`
- Delete: `ui/capturewidget.hpp`, `ui/capturewidget.cpp`
- Modify: `ui/capturewindow.hpp/.cpp`, `ui/todaypage.cpp`, `ui/pages.hpp`, `ui/mainwindow.cpp` (conexiones, `addMovement`, Ctrl+N, bandeja), `ui/snapshot.hpp` (sin `openRepairRefs`, sin `captureMedianMs`), `ui/settingspage.cpp` (sin `captureTiming_`), `ui/preview.cpp`, `ui/CMakeLists.txt`
- Modify: `docs/superpowers/specs/2026-09-27-recorte-a-lo-fundamental-design.md` (Alt+G/I/T)
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: `core::categoriesByUse`, `core::suggestedPocket` (Task 3), `parseMoneyText` (`fields.hpp`).
- Produces:

```cpp
class EntryForm : public QWidget {
    Q_OBJECT
public:
    enum class Kind { Gasto, Ingreso, Traspaso };
    explicit EntryForm(QWidget* parent = nullptr);
    void setSnapshot(const Snapshot& snapshot);
    void focusAmount();          ///< foco en el monto, texto seleccionado
    void setKind(Kind kind);
    [[nodiscard]] Kind kind() const noexcept;
signals:
    /// Listo para guardar. `newCategory` trae nombre solo si la categoria no
    /// existia. `keepOpen`: fue Shift+Enter.
    void submitted(const dake::core::Movement& movement,
                   const dake::core::Category& newCategory, bool keepOpen);
    void cancelled();
    /// Lo pide la ventana para recordar el ultimo bolsillo de cada tipo.
    void pocketsUsed(dake::ui::EntryForm::Kind kind, const QString& from, const QString& to);
public slots:
    /// La ventana lo llama despues de guardar: limpia el monto, deja lo demas,
    /// y muestra "✓ Anotado: ...".
    void confirmSaved(const QString& summary);
};
```

  Nombres de objeto para las pruebas: `EntryAmount` (QLineEdit), `EntryCategory` (CategoryBox), `EntryPocket` y `EntryTarget` (QComboBox), `EntryDate` (QDateEdit), `EntryError` (QLabel), `EntryDone` (QLabel), `EntryGasto`, `EntryIngreso` y `EntryTraspaso` (QPushButton checkable, con los textos "&Gasto", "&Ingreso" y "&Traspaso"), y `EntrySubmit` (QPushButton "Anotar").

  Snapshot gana `QString lastPocket[3]` y `QString lastTarget`, que se leen de los ajustes `anotar.ultimo.gasto`, `anotar.ultimo.ingreso`, `anotar.ultimo.traspaso.de` y `anotar.ultimo.traspaso.a`.

**Comportamiento (del spec):**
- **Orden de tabulación:** tipo → monto → categoría → bolsillo (o De) → A → fecha → Anotar.
- **Categoría:** la llena `categoriesByUse` para el tipo, sin `kUncategorized` ni `kAdjustment`. Al final va el ítem "+ Nueva…"; elegirlo vacía el campo y le da el foco.
- **Bolsillos:** solo los no archivados, como "Nombre · negocio" o "Nombre · personal". El rótulo es "Sale de" en Gasto y "Entra a" en Ingreso. En Traspaso se ven "De" y "A", y la categoría se esconde.
- **Por defecto:** si hay `lastPocket` válido para el tipo se usa ese; si no, `suggestedPocket`. Así se cubre el Review Focus 4.
- **Fecha:** `QDateEdit` con `setMaximumDate(hoy)`. Arranca en hoy y no se reinicia al guardar.
- **Validación:** va en este orden y muestra el texto exacto en `EntryError`:
  1. "El monto tiene que ser mayor que cero"
  2. (Gasto e Ingreso) "Falta la categoría"
  3. (Traspaso) "De y A tienen que ser distintos"
  4. "La fecha no puede ser futura"
  5. "Falta un bolsillo"
- **El movimiento que se emite:**
  - `name` vacío, `settled = true`, `spreadMonths = 1`, `jobId` vacío;
  - en Traspaso, `category` vacía y `targetPocketId` = A;
  - `newCategory` se arma con el tipo del movimiento y la cuenta de `snapshot.categoryAccount(nombre, pocketId)`, con clase General.
- **Enter:** Enter y Shift+Enter guardan desde cualquier campo. La excepción es el completador de categoría abierto, donde Enter elige (Review Focus 1). Esc emite `cancelled`.
- **Al guardar:** la ventana llama `confirmSaved("Gasto 25,00 · Comida · Caja del negocio")`. El formulario vacía el monto, conserva tipo, categoría y bolsillo, y devuelve el foco al monto.

**MainWindow:**
- `addMovement(movement, newCategory)` se queda como está, con dos cambios:
  - la etiqueta de deshacer usa `main.name.empty() ? main.category : main.name`;
  - se borra el alta de meta `"vida"`. La herramienta se sigue creando con 24 meses, pero sin pendiente.
- Guarda el último bolsillo en los ajustes (`anotar.ultimo.*`).
- Una sola función conecta los dos formularios:

```cpp
auto wire = [this](EntryForm* form, bool mini) {
    connect(form, &EntryForm::submitted, this,
            [this, form, mini](const core::Movement& m, const core::Category& c, bool keepOpen) {
                if (!addMovement(m, c)) return;   // addMovement pasa a devolver bool
                rememberPockets(m);
                form->confirmSaved(savedSummary(m));
                if (mini && !keepOpen) captureWindow_->hide();
            });
};
```

  (`addMovement` pasa de `void` a `bool`: `false` si hubo error.)
- **Ctrl+N:** `showPage(0); today_->entry()->focusAmount();`.
- `TodayPage::capture()` pasa a llamarse `TodayPage::entry()`.
- Se borran de los ajustes el cronómetro `"captura"`, `captureMedianMs` y la etiqueta `captureTiming_`.

- [ ] **Step 1: Prueba.** En `tests/uitest.cpp` se reemplaza el bloque "[anotar desde Hoy]" y el `#if 0` de la Task 3 por "[anotar con el formulario]":

```cpp
    std::printf("\n[anotar con el formulario]\n");
    {
        auto* amount = window.findChild<QLineEdit*>(QStringLiteral("EntryAmount"));
        auto* category = window.findChild<ui::CategoryBox*>(QStringLiteral("EntryCategory"));
        auto* date = window.findChild<QDateEdit*>(QStringLiteral("EntryDate"));
        auto* error = window.findChild<QLabel*>(QStringLiteral("EntryError"));
        check(amount && category && date && error, "el formulario esta en Hoy");
        auto type = [&](QWidget* w, const QString& text) { w->setFocus(); QTest::keyClicks(w, text); };
        auto findSaved = [&](auto pred) {
            for (const core::Movement& m : repository.loadMovements()) if (pred(m)) return true;
            return false;
        };

        // 1. gasto con categoria nueva
        type(amount, QStringLiteral("25"));
        category->lineEdit()->clear();
        type(category->lineEdit(), QStringLiteral("Almuerzo taller"));
        QTest::keyClick(category->lineEdit(), Qt::Key_Return);
        settle();
        check(findSaved([](const core::Movement& m) {
                  return m.kind == core::MovementKind::Gasto && m.amountMinor == 25'00 &&
                         m.category == "Almuerzo taller" && m.name.empty() && m.settled; }),
              "25 + categoria nueva + Enter guarda un gasto de 25,00, cobrado, sin nombre");
        check(amount->text().isEmpty() && amount->hasFocus(), "el monto queda vacio y con el foco");
        check(category->category() == QStringLiteral("Almuerzo taller"), "la categoria se conserva");
        check(category->findText(QStringLiteral("Almuerzo taller")) >= 0, "y ya esta en la lista");

        // 2. ingreso con la fecha cambiada (Alt+I)
        QTest::keyClick(amount, Qt::Key_I, Qt::AltModifier);
        type(amount, QStringLiteral("40,5"));
        type(category->lineEdit(), QStringLiteral("Venta"));
        date->setDate(date->date().addDays(-3));
        QTest::keyClick(amount, Qt::Key_Return);
        settle();
        check(findSaved([&](const core::Movement& m) {
                  return m.kind == core::MovementKind::Ingreso && m.amountMinor == 40'50 &&
                         m.date.toIso() == date->date().toString(Qt::ISODate).toStdString(); }),
              "Alt+I, 40,5 y la fecha de hace tres dias: un ingreso de 40,50 con esa fecha");

        // 3. fecha futura: no deja
        date->setMaximumDate(QDate(9999, 1, 1));  // la prueba fuerza el limite
        date->setDate(QDate::currentDate().addDays(2));
        const auto before = repository.loadMovements().size();
        type(amount, QStringLiteral("5"));
        QTest::keyClick(amount, Qt::Key_Return);
        settle();
        check(repository.loadMovements().size() == before &&
                  error->text() == QStringLiteral("La fecha no puede ser futura"),
              "una fecha futura no se guarda y se dice por que");
        date->setDate(QDate::currentDate());

        // 4. traspaso con De = A
        QTest::keyClick(amount, Qt::Key_T, Qt::AltModifier);
        auto* from = window.findChild<QComboBox*>(QStringLiteral("EntryPocket"));
        auto* to = window.findChild<QComboBox*>(QStringLiteral("EntryTarget"));
        to->setCurrentIndex(from->currentIndex());
        amount->clear();
        type(amount, QStringLiteral("10"));
        QTest::keyClick(amount, Qt::Key_Return);
        settle();
        check(error->text() == QStringLiteral("De y A tienen que ser distintos"),
              "un traspaso al mismo bolsillo no se guarda");
    }
```

  Se agregan dos pruebas más en el mismo bloque:
  - **Sueldo:** De = caja del negocio, A = el bolsillo personal. Se guarda un `Traspaso` y `snapshot.salary`/Informes lo ven. El chequeo es sobre la base: existe un Traspaso de 300 del negocio a lo personal.
  - **Gasto cruzado (Review Focus 5):** una categoría personal (`saveCategory({"Comida", Personal, General, Gasto})` antes de abrir) pagada desde la caja genera dos movimientos, un traspaso y un gasto en el bolsillo personal.

  Además:
  - Cambian los bloques que usan `CaptureInput`. El de "el cobro anotado cierra la reparación" se borra: el cobro se hace con el botón Cobrar. En el de Cotizaciones, "'50 cobro 3070' no crea un segundo ingreso" se borra, porque el formulario ya no enlaza reparaciones.
  - El de tema oscuro pasa a usar `EntryAmount` y a mirar el `tono` del campo de monto: en Gasto, el color del texto del monto es el papel "gasto".
  - Se agrega: `ui::CaptureWindow` contiene un `EntryForm` (`findChild<ui::EntryForm*>() != nullptr`).
- [ ] **Step 2:** Compilar y probar. Esperado: falla la compilación porque no existe `entryform.hpp`.
- [ ] **Step 3: Implementar** `EntryForm`, siguiendo el contrato de arriba.
  - La fila de tipo lleva tres `QPushButton` checkable en un `QButtonGroup` exclusivo.
  - Los campos van en un `QGridLayout` de rótulo arriba y campo abajo, como en la captura vieja: TIPO · MONTO · CATEGORÍA / DE · BOLSILLO / A · FECHA · [Anotar].
  - El monto va en `theme::figureFont(16)` y su color de papel cambia con el tipo: `kNegative` para Gasto, `kPositive` para Ingreso y `kAccent` para Traspaso.
  - Después:
    - se borran `capturewidget.*`;
    - `CaptureWindow` pasa a tener `EntryForm* entry()`, y `popup()` llama `focusAmount()`;
    - `TodayPage` crea el `EntryForm` dentro de la tarjeta "ANOTAR", cuyo subtítulo es `"Monto, categoría, Enter. Alt+G gasto · Alt+I ingreso · Alt+T traspaso."` más el atajo global.
  - En el spec se cambia `Ctrl+1`, `Ctrl+2`, `Ctrl+3` por `Alt+G`, `Alt+I`, `Alt+T`.
  - En `preview.cpp`, la pantalla "captura" usa `window->entry()->setSnapshot(snapshot)` y ya no usa `setInput`.
- [ ] **Step 4:** Compilar y probar. Esperado: todo pasa.
- [ ] **Step 5: Commit** `Anotar es un formulario: tipo, monto, categoria, bolsillo y fecha`.

### Task 6: Hoy = anotar + pendientes; fuera Revisión

**Files:**
- Create: `ui/pendinglist.hpp`, `ui/pendinglist.cpp`
- Delete: `ui/reviewpage.cpp`, y la clase `ReviewPage` de `ui/pages.hpp`
- Modify:
  - `ui/todaypage.cpp`: solo "Hoy · fecha", Anotar y Pendientes; todo lo demás se muda en la Task 7, así que aquí se **borra de Hoy**;
  - `ui/mainwindow.cpp` y `mainwindow.hpp`: 6 secciones, conexiones de pendientes, `reconcile` guarda la fecha, fuera `checkReminder`, `setMovementCategory`, `setToolLife`, `deleteMovementById` y `review_`;
  - `ui/settingspage.cpp` y `pages.hpp`: fuera la tarjeta "REVISIÓN DE LA SEMANA", la señal `reminderChanged`, `reminderDay_`, `reminderHour_` y `timings_`;
  - `ui/snapshot.hpp`: fuera `reminderWeekday`, `reminderHour` y `reviewMedianMs`;
  - `ui/preview.cpp`, `ui/CMakeLists.txt`.
- Test: `tests/uitest.cpp`

**Interfaces:**
- Consumes: `core::InboxKind` y `InboxInput` (Task 4).
- Produces:

```cpp
/// Los pendientes, todos a la vista, uno por fila. Cada fila trae la accion
/// que la resuelve y "Despues" (una semana).
class PendingList : public QWidget {
    Q_OBJECT
public:
    explicit PendingList(QWidget* parent = nullptr);
    void setSnapshot(const Snapshot& snapshot);
signals:
    void recurringConfirmed(const dake::core::Id& movementId, qint64 amountMinor);
    void quotesReviewRequested();
    void chargeRequested(const dake::core::Id& jobId);
    void repairOpened(const dake::core::Id& jobId);
    void realHoursSet(const dake::core::Id& jobId, int minutes);
    void partCostSet(const dake::core::Id& partId, qint64 costMinor);
    void reconcileRequested(const dake::core::Id& pocketId);
    void snoozed(const std::string& key, int days);
};
```

  Cada fila es un `QWidget` con `objectName` `"Pending:" + kind` (por ejemplo `"Pending:SinCuadrar"`) que contiene:
  - título en `bodyFont(11, DemiBold)` y detalle en `kTextMuted`;
  - un `QLineEdit` (objectName `PendingValue`) cuando el pendiente pide un valor (PorConfirmar, SinHoras, CostoRepuesto);
  - el botón de acción (`PendingAction`) y el botón "Después" (`PendingLater`).

  Enter en el campo equivale a la acción. Si no hay pendientes, la lista muestra "Nada pendiente.". Los textos de título y detalle se copian de `reviewpage.cpp::showCurrent` para los tipos que se quedan. SinCuadrar dice `"%1: sin cuadrar hace %2 días"` y detalle `"Cuenta lo que hay y escríbelo."`, y su acción es "Cuadrar".

  Claves de posponer: SinHoras usa `refId + ":horas"`, SinCuadrar `refId + ":cuadrar"`, Cotizaciones `"cotizaciones"`, y el resto `refId`.

**MainWindow:**
- `reload()` arma el `InboxInput` con:
  - `pockets`;
  - `lastReconciled`, leído de `bolsillo.<id>.cuadrado` para cada bolsillo;
  - `reconcileSince`, leído de `recorte.inicio`. Si no existe, se escribe con hoy la primera vez.
- `reconcile(pocketId)`: si el diálogo se acepta, se escribe `bolsillo.<id>.cuadrado = hoy` **aunque no haya diferencia**, y después se sigue como ahora.
- Barra lateral: `{"Hoy","Reparaciones","Movimientos","Bolsillos","Informes","Ajustes"}`. `Ctrl+1` a `Ctrl+6` en ese orden. El tooltip de la bandeja dice "N pendientes".
- Se borran `hourlyTimer_ → checkReminder`. El timer horario se queda para `generateRecurring`.

- [ ] **Step 1: Prueba.** Se reemplaza el bloque "[revision de la semana, con el teclado]" de uitest por "[pendientes en Hoy]".
  - Usa la misma base sembrada con la luz a 3 meses. Antes de abrir se hace `repo.setSetting("recorte.inicio", hace 40 días)` para un bolsillo.
  - Chequeos:
    - la barra tiene 6 botones y ninguno dice "Revisión";
    - `findChildren<QWidget*>` con prefijo `"Pending:PorConfirmar"` da 3 filas **a la vez** (lista, no una por vez);
    - en la primera fila se escribe `45` en `PendingValue` y Enter: la luz de ese mes queda en 45,00 y el recurrente en 45,00;
    - quedan 2 filas de PorConfirmar y las demás siguen ahí;
    - en la fila `Pending:SinCuadrar`, se deja programado con `onNextDialog` que el diálogo de cuadrar reciba el saldo que muestra y Enter; al apretar `PendingAction`, la fila desaparece y queda `bolsillo.<id>.cuadrado`;
    - `PendingLater` en una fila la saca y escribe `bandeja.pospuestos`.
  - Se borran los chequeos de "sin categoría", "vida útil" y "revision cronometrada".
- [ ] **Step 2:** Compilar y probar. Esperado: falla porque no existe `pendinglist.hpp`.
- [ ] **Step 3: Implementar** `PendingList` y los cambios de `TodayPage`, `MainWindow` y `SettingsPage`. Se borran `reviewpage.cpp` y todas sus conexiones.
- [ ] **Step 4:** Compilar y probar. Esperado: todo pasa.
- [ ] **Step 5: Commit** `Hoy: anotar y pendientes en lista; fuera la seccion Revision`.

### Task 7: Informes por pregunta

**Files:**
- Create: `ui/reportsummary.cpp`, con las secciones que llegan de Hoy y de Bolsillos: implementa `ReportsPage::buildSummarySection`, `buildMonthExtras`, `buildJobsKpis`, `buildOverheadSection`, `buildSalaryKpi` y sus `refill*`, más `FundingBar`, que se muda de `todaypage.cpp`
- Modify: `ui/reportspage.cpp` (5 pestañas, `build*Tab` pasan a `add*Section(QVBoxLayout*)`), `ui/pages.hpp`, `ui/pocketspage.cpp` (fuera Mes a mes), `ui/movementspage.cpp` (fuera Dura y Trabajo), `ui/dialogs.cpp/.hpp` (fuera "Cuánto dura"; `PocketDialog` con Emergencia y aviso), `ui/CMakeLists.txt`
- Test: `tests/uitest.cpp`

**Pestañas y orden de tarjetas** (spec, tabla de Informes). `tabs_` nombra cada pestaña con objectName `Informe:Resumen`, `Informe:ElMes`, `Informe:Gastos`, `Informe:Trabajos` e `Informe:Sueldo`:

| Pestaña | Contenido, en orden |
|---|---|
| **Resumen** | fila de KPI [Caja del negocio, Reservas (Ahorro, Inversión y Emergencia, con `reserveTotal`), Meses de reserva] → "CON QUÉ SE PAGÓ ESTE MES" (sin la nota vieja) → "LO QUE HAY QUE MIRAR" |
| **El mes** | fila de KPI [¿Cuánto gasto? · este mes] → Resultado por mes → Ingresos contra costos → Caja acumulada → sección de Flujo de caja (titular, barras, línea, tabla) → "MES A MES" (se muda de `pocketspage.cpp`, mismo código) → "CAJA Y RESULTADO NO SON EL MISMO NÚMERO". El detalle de esta última pierde la frase de "la aplicación actual calcula solo el primero…" y conserva la primera oración |
| **Gastos** | Gastos por categoría (gráfica `chartCategories_`) → la sección de Gastos por categoría con su selector de mes → "COSTO DE ESTRUCTURA" |
| **Trabajos** | fila de KPI [¿Subir precios?, Para no perder, Ticket promedio] → fila de KPI [Hecho y sin cobrar, Días en cobrar] → Margen por trabajo → Por reparación → Por tipo |
| **Sueldo** | fila de KPI [¿Cuánto me puedo pagar?] → la sección Sueldo actual |

- El encabezado de la página pasa a decir "Informes".
- `ReportsPage::showTab(int)` sigue existiendo.

**Otras secciones:**
- `movementspage.cpp`: las columnas pasan a ser Fecha · Qué fue · Tipo · Bolsillo · Categoría · Monto. "Qué fue" muestra `name` o, si está vacío, la categoría (en Traspaso, "Traspaso").
- `dialogs.cpp` `MovementEditor`: se borran `spread_`, `fillSpread` y la fila "Cuanto dura"; `edited_.spreadMonths` no se toca.
- `PocketDialog` gana el ítem "Emergencia" (dato 4). Bajo el tipo va una etiqueta `EmergencyNote`, visible solo con Emergencia elegido, que dice: "Antes de crear el primero, instala el APK nuevo en el teléfono: el viejo no conoce este tipo y dejaría de sincronizar."

- [ ] **Step 1: Prueba.** Se agrega a uitest "[informes por pregunta]":

```cpp
    {
        QTest::keyClick(&window, Qt::Key_5, Qt::ControlModifier);
        settle();
        auto* tabs = window.findChild<ui::ReportsPage*>()->findChild<QTabWidget*>();
        QStringList names;
        for (int i = 0; i < tabs->count(); ++i) names << tabs->tabText(i);
        check(names == QStringList({"Resumen", "El mes", "Gastos", "Trabajos", "Sueldo"}),
              "Ctrl+5 abre Informes con cinco pestañas, una por pregunta");
        auto* today = window.findChild<ui::TodayPage*>();
        check(today->findChildren<ui::KpiCard*>().isEmpty(), "Hoy ya no tiene tarjetas de numeros");
        auto* pockets = window.findChild<ui::PocketsPage*>();
        bool monthsInPockets = false;
        for (auto* card : pockets->findChildren<ui::Card*>())
            monthsInPockets = monthsInPockets || card->title() == QStringLiteral("MES A MES");
        check(!monthsInPockets, "Mes a mes ya no esta en Bolsillos");
        auto* table = window.findChild<ui::MovementsPage*>()->findChild<QTableWidget*>();
        QStringList headers;
        for (int c = 0; c < table->columnCount(); ++c) headers << table->horizontalHeaderItem(c)->text();
        check(!headers.contains("Dura") && !headers.contains("Trabajo"), "Movimientos sin Dura ni Trabajo");
    }
```

  Si `Card` no expone `title()`, en esta task se agrega `[[nodiscard]] QString title() const;` a `cards.hpp`.

  Prueba del diálogo de bolsillo: se abre `PocketDialog` a mano en la prueba, se elige "Emergencia" y se comprueba que `EmergencyNote` está visible; con "Ahorro", no.
- [ ] **Step 2:** Compilar y probar. Esperado: FALLA "cinco pestañas".
- [ ] **Step 3: Implementar** según la tabla. El código de cada tarjeta se **mueve** (cortar y pegar), no se reescribe:
  - las KPI, funding, dos números, estructura, avisos y las tres preguntas de `todaypage.cpp` pasan a `reportsummary.cpp`;
  - Mes a mes pasa de `pocketspage.cpp` a `reportsummary.cpp`;
  - los miembros pasan de `TodayPage`/`PocketsPage` a `ReportsPage` en `pages.hpp`.
- [ ] **Step 4:** Compilar y probar. Esperado: todo pasa.
- [ ] **Step 5: Commit** `Informes por pregunta: resumen, el mes, gastos, trabajos y sueldo`.

### Task 8: La celda vacía y la lentitud

**Files:**
- Modify: `ui/theme.cpp` (hoja, bloque de tabla), `ui/mainwindow.cpp/.hpp` (relleno perezoso, medición)
- Test: `tests/uitest.cpp`

- [ ] **Step 1: Prueba de la celda.** Se agrega a uitest "[editar una celda se ve]":

```cpp
    {
        QTest::keyClick(&window, Qt::Key_6, Qt::ControlModifier);   // Ajustes
        settle();
        QTableWidget* templates = nullptr;
        for (auto* t : window.findChild<ui::SettingsPage*>()->findChildren<QTableWidget*>())
            if (t->rowCount() > 0 && t->editTriggers() != QAbstractItemView::NoEditTriggers) { templates = t; break; }
        check(templates != nullptr, "hay una tabla editable en Ajustes");
        if (templates != nullptr) {
            templates->editItem(templates->item(0, 0));
            settle();
            auto* editor = templates->findChild<QLineEdit*>();
            check(editor != nullptr, "doble clic abre el campo dentro de la celda");
            if (editor != nullptr) {
                QStyleOptionFrame opt;
                opt.initFrom(editor);
                opt.rect = editor->rect();
                opt.lineWidth = 1;
                const QRect text = editor->style()->subElementRect(QStyle::SE_LineEditContents, &opt, editor);
                check(text.height() >= editor->fontMetrics().height(),
                      "y el texto que se escribe entra y se ve");
                QTest::keyClick(editor, Qt::Key_Escape);
            }
        }
    }
```

- [ ] **Step 2:** Compilar y probar. Esperado: FALLA "y el texto que se escribe entra y se ve".
- [ ] **Step 3: Implementar.** En el bloque de tabla de `theme::styleSheet()`, justo después de `QTableWidget::item:hover`, agregar:

```css
QTableView QLineEdit, QTableView QComboBox, QTableView QAbstractSpinBox {
    padding: 0px 4px;
    margin: 0px;
    border-radius: 0px;
    min-height: 0px;
}
```

- [ ] **Step 4:** Compilar y probar. Esperado: pasa.
- [ ] **Step 5: Medir la lentitud.**
  - En `MainWindow::reload()` se envuelve con `QElapsedTimer` cada tramo: lectura de la base, cálculos del núcleo, `setSnapshot` de cada página.
  - Si la variable de entorno `DAKE_MEDIR` existe, se imprime con `qInfo("Finanzas: recarga %lld ms (base %lld, nucleo %lld, paginas: hoy %lld, ...)")`.
  - Se corre la versión release con la base real, sin escribir en ella: se copia `finanzas-v2.db` a la carpeta temporal y se abre con `DAKE_TEST_DB_PATH`. Se anotan los números en el commit.
- [ ] **Step 6: Relleno perezoso.**
  - `MainWindow` gana `std::array<bool, 6> stale_{}`.
  - `reload()` hace los cálculos, marca todas las páginas como viejas y llama `refreshPage(stack_->currentIndex())`.
  - `showPage(i)` llama `refreshPage(i)`.
  - `refreshPage(i)` hace `setSnapshot` solo si `stale_[i]` y después baja la marca.
  - La ventana chica y la bandeja se siguen actualizando siempre, porque son baratas.
  - Si la medición muestra que la base o el núcleo pesan más que las páginas, se agrega lo mínimo que la medición señale (por ejemplo, no releer `loadTemplates` si no cambió) y se documenta en el commit.
  - Criterio: con la base real, en release, elegir una reparación y agregar un repuesto se ve en menos de 150 ms.
- [ ] **Step 7: Prueba del relleno perezoso.** En uitest, después de anotar un gasto estando en Hoy, `ReportsPage` todavía no se rellenó (el titular del Resumen conserva el texto anterior). Al apretar `Ctrl+5` ya muestra el valor nuevo. Se compara el texto de la KPI "CAJA DEL NEGOCIO" antes y después de mostrar la página.
- [ ] **Step 8:** Compilar y probar. Commit `La celda se ve al escribir y solo se rellena la pagina visible`, con los números medidos en el cuerpo.

### Task 9: El teléfono

**Files:**
- Modify: `mobile/qml/Anotar.qml`, `mobile/qml/Historial.qml:80-84`, `mobile/qml/Bolsillos.qml:382-384`, `mobile/qml/Estilo.qml:27-65`, `mobile/appbridge.cpp` (`saveMovement`, `addPocket`)

- [ ] **Step 1: `appbridge.cpp::saveMovement`**
  - El nombre deja de ser obligatorio.
  - En Gasto e Ingreso, la categoría pasa a ser obligatoria: si está vacía devuelve `"Falta la categoría."`.
  - Se ignoran `jobId`, `spreadMonths` y `settled`: se guarda `settled = true` y `spreadMonths = 1`.
  - Si la fecha es posterior a hoy devuelve `"La fecha no puede ser futura."`.
  - `addPocket` hace `std::clamp(kind, 0, 4)`.
- [ ] **Step 2: `Anotar.qml`**
  - Se borran las propiedades `jobId`, `jobNombre`, `duracion` y `pagado`, el `TextField nombre`, las filas "Trabajo", "Cuanto dura" y "Ya lo pague", y los selectores `trabajoSelector` y `duracionSelector`.
  - `guardar()` manda solo `amount`, `kind`, `date`, `pocketId`, `targetPocketId` y `category`.
  - El botón Guardar se habilita con `monto !== "" && (esTraspaso || categoria !== "")`.
  - El `onAccepted` del monto llama `guardar()`.
  - `limpiar()` vacía solo el monto.
  - El orden de las filas queda: Categoría (o "Va a" en traspaso) → "Sale de"/"Entra a" → Fecha.
- [ ] **Step 3:** `Historial.qml` deja de mostrar `spreadMonths`. `Estilo.qml` gana `readonly property color emergencia: "#f06595"`, y `colorBolsillo` suma `if (kind === 4) return emergencia`. El comentario pasa a decir "4 emergencia". `Bolsillos.qml` pasa a `model: ["Operacion", "Ahorro", "Inversion", "Personal", "Emergencia"]`.
- [ ] **Step 4:** Compilar (`dake_movil` compila en la build de escritorio).
  - Correr `dake_movil.exe` con `QT_QPA_PLATFORM=offscreen` durante 5 s y revisar que su salida no traiga `qrc:` ni errores de QML.
  - Esperado: sin errores de QML.
- [ ] **Step 5: Commit** `Telefono: el mismo formulario y el bolsillo Emergencia`.

### Task 10: Vista previa, capturas y cierre

**Files:**
- Modify: `ui/preview.cpp` (pantallas: hoy, reparaciones, movimientos, bolsillos, informes (una por pestaña: `informes-resumen`, `informes-mes`, …), ajustes, captura)
- Modify: `PENDIENTE.md` (quitar lo que se fue y agregar "instalar APK antes del primer bolsillo Emergencia")

- [ ] **Step 1:** `preview.cpp`:
  - saca "revision";
  - "reportes" pasa a llamarse "informes" y saca una imagen por pestaña con `showTab(i)`;
  - "captura" usa el `EntryForm`.
- [ ] **Step 2:** `.\compilar.ps1 debug -Probar` y `.\compilar.ps1 release`. Esperado: 4 suites verdes y 0 advertencias en las dos.
- [ ] **Step 3:** Sacar capturas con `dake_uipreview todas`, en claro y en `--oscuro`. Mirarlas todas:
  - Hoy no tiene números;
  - Informes muestra sus 5 pestañas con contenido;
  - el formulario se ve entero a 1280 px.
  Se corrige lo que se vea mal.
- [ ] **Step 4:** Commit `Vista previa y pendientes al dia con el recorte`.
- [ ] **Step 5:** Revisión final de la rama por un revisor nuevo (executing-plans).

## Self-review

- **Cobertura del spec:**

  | Sección del spec | Task |
  |---|---|
  | Barra lateral | 6 |
  | Hoy / Anotar | 5 |
  | Pendientes | 4, 6 |
  | Informes | 7 |
  | Otras secciones | 7 |
  | Celda vacía y lentitud | 8 |
  | Emergencia | 1, 7 (aviso), 9 |
  | Teléfono | 9 |
  | Eliminaciones: intérprete, banco | 3 |
  | Eliminaciones: Dura | 2 |
  | Eliminaciones: Revisión | 6 |
  | Eliminaciones: contador de capturas | 5 |
  | Eliminaciones: tipos de bandeja | 4 |
  | Pruebas | en cada task |

- **Tipos:**
  - `EntryForm::Kind` se usa igual en las Tasks 5 y 6.
  - `InboxKind::SinCuadrar` y las claves `":cuadrar"`/`"cotizaciones"` son las mismas en las Tasks 4 y 6.
  - `reserveTotal` sale de la Task 1 y se usa en las Tasks 7 y 9.
- **Review Focus:**

  | Punto | Dónde se prueba |
  |---|---|
  | 1 | Task 5, con el completador en la prueba del gasto |
  | 2 | Task 3 |
  | 3 | Task 5 |
  | 4 | Task 4 (archivado en la bandeja) y Task 5 (el formulario filtra `archived`) |
  | 5 | Task 5 |
