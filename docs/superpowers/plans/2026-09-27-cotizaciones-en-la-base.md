# Cotizaciones en la base de datos — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cotizaciones sube lo esencial de cada documento a `v2_quotes`, aplica lo que el teléfono cambia, tiene su app de Android, y Finanzas lee las cotizaciones desde la base en vez de la carpeta.

**Architecture:** La lógica de fila, reloj, servidor y fusión vive en `src/nube/` de Cotizaciones, en TypeScript puro sin Node, para que la usen el proceso principal de Electron y la app de Android. El proceso principal suma `src/main/nube/` (estado persistido, sesión cifrada, cableado IPC y relojes de sincronización). La app de Android es un segundo punto de entrada de Vite (`src/movil/`) empaquetado con Capacitor 6. Finanzas agrega `quotes` a las tablas que baja `SyncEngine` (solo en el escritorio), guarda cada fila tal como llega y arma los `QuoteDoc` desde ahí.

**Tech Stack:**
- Cotizaciones: Electron 33, React 18, TypeScript 5, vitest 2 (jsdom), `fetch` nativo; Capacitor 6 (`@capacitor/core`, `@capacitor/android`, `@capacitor/preferences`, `@capacitor/cli`).
- Finanzas: Qt 6.8.3, C++20, SQLite, PostgREST (Supabase).

**Spec:** `docs/superpowers/specs/2026-09-27-cotizaciones-en-la-base-design.md`

**Repos:**
- F = `C:\Users\David\Documents\VisualStudio\finanzas-banco-pruebas`, rama `escritorio-cuentas-reparaciones`.
- C = `C:\Users\David\Documents\VisualStudio\dakelabsfactura`. Hoy está en `master`: la tarea C1 crea la rama `nube` antes de tocar nada.

## Global Constraints

- Finanzas nunca escribe en la carpeta de Cotizaciones ni en `v2_quotes`: solo baja.
- A la base va solo lo esencial. **No suben:** el resumen, las notas, la forma de pago, la garantía, las cláusulas, los datos del cliente (identificación, teléfono, correo, dirección), el historial salvo la fecha de pago, las fotos, el PDF y la firma.
- Montos en centavos enteros. En la base: cadena vacía en lugar de `null` donde no hay dato; `deleted` como lápida; `updated_at` lo pone el trigger y el cliente nunca lo manda.
- HLC con el formato de Finanzas: `"%013d-%05d-<deviceId>"`. Se comparan como texto.
- Nada se marca subido sin la confirmación 2xx del servidor.
- El teléfono no emite, no numera y no corrige. Solo:
  - acepta o rechaza (rechazar pide motivo) una cotización enviada;
  - cobra un informe entregado, con fecha;
  - crea, edita y borra sus propios borradores mientras la PC no los tomó.
- `supabase.json` de Finanzas (`%APPDATA%\DakeLabs\Finanzas DakeLabs\supabase.json`, claves `url` y `anon_key`, **con BOM**) no se copia a ningún repo ni se sube a git.
- Pruebas de Finanzas:
  - la interfaz se prueba con `dake_uitest` offscreen; nunca teclas globales;
  - `.\compilar.ps1 debug -Probar` sin advertencias, con las 4 suites en verde.
- Pruebas de Cotizaciones: `npm test` (vitest) en verde y `npm run build` sin errores de tipos.
- Nunca se usan los datos reales de David en pruebas:
  - ni la base `finanzas-v2.db`;
  - ni la carpeta `Documentos\DakeLabs Cotizaciones`;
  - ni su cuenta de Supabase.
- Commits en español, terminados con:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01JhaoWSW44Z6pTAXd225bUt
  ```

## Review Focus

1. **El mismo documento cambia de estado en la PC y en el teléfono entre dos sincronizaciones.**
   - Esperado: gana un lado de forma determinista, el documento de la PC queda con un historial coherente y nada se cae.
   - Prueba en C3 ("choque de estado: si el cambio del teléfono ya no se puede aplicar, la PC sube el suyo").
2. **`supabase.json` con BOM, ausente o sin clave.**
   - Esperado: un mensaje claro en la sección Nube, no una excepción.
   - Prueba en C2 (`leerConfigSupabase`).
3. **Sin señal a mitad de la subida.**
   - Esperado: los pendientes quedan; la corrida siguiente los sube.
   - Pruebas en C3 (PC) y C5 (teléfono).
4. **La PC tomó un borrador del teléfono y el teléfono lo sigue editando.**
   - Esperado: el teléfono deja de ofrecer Editar, y un cambio que ya estaba en su cola pierde contra el reloj de la PC.
   - Pruebas en C5 (`fusionar`) y C6 (Detalle).
5. **Una fila con campos faltantes o `null` (`lineas` nula, sin `fecha_pago`).**
   - Esperado: Finanzas y el teléfono la leen con valores vacíos, sin lanzar.
   - Pruebas en F1 (`quoteFromRow`) y C1 (`normalizarFila`).

---

## Mapa de archivos

**Finanzas (F):**
- Crear:
  - `supabase_v4_cotizaciones.sql`: la tabla `v2_quotes`, su índice, su trigger y RLS.
  - `storage/include/dake/storage/quoterows.hpp` y `storage/src/quoterows.cpp`: fila → `QuoteDoc`.
- Borrar: `storage/include/dake/storage/quotefolder.hpp` y `storage/src/quotefolder.cpp`.
- Modificar:
  - `storage/src/database.cpp`: esquema v4 con la tabla `quotes`.
  - `storage/include/dake/storage/repository.hpp` y `storage/src/repository.cpp`: `applyRemoteQuote`, `loadQuoteRows`, `wipe`.
  - `storage/CMakeLists.txt`.
  - `sync/include/dake/sync/sync_engine.hpp` y `sync/src/sync_engine.cpp`: las tablas pasan a ser un parámetro.
  - `mobile/appbridge.cpp`.
  - `ui/mainwindow.hpp` y `ui/mainwindow.cpp`: `importQuotes` lee de la base; sale el vigilante de la carpeta.
  - `ui/pages.hpp` y `ui/settingspage.cpp`: la tarjeta sin el campo de la carpeta.
  - `ui/snapshot.hpp` y `ui/preview.cpp`.
  - `tests/storagetest.cpp`, `tests/uitest.cpp` y `tests/synccheck.cpp`.
  - `PENDIENTE.md`.

**Cotizaciones (C):**
- `src/nube/`, puro:
  - `fila.ts`: tipos, columnas, fila parcial, normalizar.
  - `esencial.ts`: documento ↔ fila y totales de una fila.
  - `reloj.ts`: el HLC.
  - `servidor.ts`: la sesión y PostgREST por `fetch`.
  - `fusion.ts`: fila local + fila remota.
  - Una prueba por archivo.
- `src/main/nube/`:
  - `cola.ts`: el estado persistido de la PC.
  - `sincronizar.ts`: la corrida de la PC.
  - `sesion.ts`: `supabase.json` y el token cifrado.
  - `ipcNube.ts`: canales, relojes y marcas de cambio.
  - Pruebas de `cola` y `sincronizar`.
- Modificar:
  - `src/main/ipcDatos.ts`: marcar cambios y exportar `carpetaBase`, `configActual` y `hoyISO`.
  - `src/main/index.ts`.
  - `src/preload/tipos.ts` y `src/preload/index.ts`: `nube`.
  - `src/renderer/src/datos/Datos.tsx` y su prueba: la sección Nube.
  - `src/renderer/src/editor/Editor.tsx`.
  - `src/renderer/src/editor/editor.css`.
  - `src/renderer/src/dominio/tipos.ts` y `dominio/indice.ts`: `delTelefono`.
  - `src/renderer/src/listado/Listado.tsx` y su prueba: la marca "del teléfono".
- `src/movil/`:
  - `index.html`, `main.tsx`, `App.tsx`, `movil.css`;
  - `plataforma.ts`: Preferences y la configuración que se fija al construir;
  - `almacen.ts`: el estado del teléfono;
  - `acciones.ts`: aceptar, rechazar, cobrar y borradores;
  - `sincronizarTelefono.ts`;
  - `pantallas/`: `Entrar.tsx`, `Lista.tsx`, `Detalle.tsx`, `Nueva.tsx`;
  - pruebas de `almacen`, `acciones`, `sincronizarTelefono` y de las cuatro pantallas.
- Configuración y construcción:
  - `vite.movil.config.ts`, `capacitor.config.json`;
  - `construir-android.ps1`;
  - `android/`, generado por Capacitor;
  - `package.json`, `.gitignore`.

---

## Parte 2a — Finanzas

### Tarea F1: la tabla local `quotes` y el lector de filas

**Files:**
- Create:
  - `supabase_v4_cotizaciones.sql`
  - `storage/include/dake/storage/quoterows.hpp`
  - `storage/src/quoterows.cpp`
- Modify:
  - `storage/src/database.cpp`: `kTargetVersion` y `kUpgrades`, y la tabla al final de `kSchemaV1`.
  - `storage/include/dake/storage/repository.hpp` y `storage/src/repository.cpp`.
  - `storage/CMakeLists.txt`.
  - `tests/storagetest.cpp`.

**Interfaces:**
- Produces:
  - `bool Repository::applyRemoteQuote(const QString& id, const QString& updatedAt, bool deleted, const QString& json)`
  - `std::vector<QString> Repository::loadQuoteRows()`
  - `core::QuoteDoc storage::quoteFromRow(const QJsonObject& row)`
  - `storage::QuoteRowsRead storage::readQuoteRows(Repository&)`, con los campos `docs` y `errors`.

- [ ] **Step 1: SQL de Supabase.** Crear `supabase_v4_cotizaciones.sql`:

```sql
-- ============================================================
-- supabase_v4_cotizaciones.sql — lo esencial de DakeLabs Cotizaciones.
--
-- CORRELO VOS en el panel: SQL Editor > New query > pegar > Run.
-- Es seguro correrlo más de una vez y no toca ninguna tabla que ya exista.
--
-- Una fila por documento. Solo lo que hace falta para verlo en el teléfono y
-- para que Finanzas lo importe; lo pesado (descripciones, cláusulas, notas,
-- datos del cliente, fotos) se queda en la PC.
--
-- Dos grupos de columnas, cada uno con su reloj:
--   contenido (hlc): lo escribe solo el dueño del documento;
--   estado (estado_hlc): lo escriben la PC y el teléfono.
-- Todas tienen valor por defecto: el teléfono sube solo el grupo que cambió.
-- ============================================================

create table if not exists public.v2_quotes (
    id              text primary key,
    user_id         uuid not null references auth.users(id) on delete cascade,

    numero          text    not null default '',
    tipo            text    not null default 'cotizacion' check (tipo in ('cotizacion', 'informe')),
    forma           text    not null default 'servicio'   check (forma in ('servicio', 'proyecto')),
    cliente         text    not null default '',
    equipo          text    not null default '',
    lineas          jsonb   not null default '[]'::jsonb,
    descuento_tipo  text    not null default '' check (descuento_tipo in ('', 'porcentaje', 'monto')),
    descuento_valor bigint  not null default 0,
    abono_minor     bigint  not null default 0,
    fecha_emision   text    not null default '',
    fecha_ingreso   text    not null default '',
    origen_id       text    not null default '',
    hecho_en        text    not null default 'pc' check (hecho_en in ('pc', 'telefono')),
    hlc             text    not null default '',

    estado          text    not null default 'borrador',
    fecha_entrega   text    not null default '',
    fecha_pago      text    not null default '',
    motivo_rechazo  text    not null default '',
    estado_hlc      text    not null default '',

    device_id       text    not null default '',
    deleted         boolean not null default false,
    updated_at      timestamptz not null default now()
);

create index if not exists v2_quotes_user_updated_idx
    on public.v2_quotes (user_id, updated_at);

drop trigger if exists v2_quotes_touch on public.v2_quotes;
create trigger v2_quotes_touch before insert or update on public.v2_quotes
    for each row execute function public.v2_touch_updated_at();

alter table public.v2_quotes enable row level security;

drop policy if exists "lectura propia" on public.v2_quotes;
drop policy if exists "alta propia" on public.v2_quotes;
drop policy if exists "edicion propia" on public.v2_quotes;

create policy "lectura propia" on public.v2_quotes
    for select using (auth.uid() = user_id);
create policy "alta propia" on public.v2_quotes
    for insert with check (auth.uid() = user_id);
create policy "edicion propia" on public.v2_quotes
    for update using (auth.uid() = user_id) with check (auth.uid() = user_id);

-- Sin política de DELETE: igual que las demás, un borrado es una lápida.
```

- [ ] **Step 2: Prueba que falla (storagetest).** Reemplazar el bloque "La carpeta de DakeLabs Cotizaciones" de `tests/storagetest.cpp` (y su `#include "dake/storage/quotefolder.hpp"` por `#include "dake/storage/quoterows.hpp"` más `<QJsonDocument>` y `<QJsonObject>` si faltan). Además, cambiar la versión esperada de la migración: `checkMinor(version.value(0).toInt(), 4, "la base vieja sube a la version 4")`.

```cpp
    // --- Lo esencial de DakeLabs Cotizaciones, desde la base -----------------
    //
    // Filas sinteticas con la misma forma que las de v2_quotes. Ni la cuenta
    // ni los documentos de David se tocan aca.
    {
        storage::Database db(path + QStringLiteral(".cot"));
        storage::Repository repository(db);
        auto put = [&repository](const char* json, const char* updatedAt) {
            const QJsonObject row = QJsonDocument::fromJson(QByteArray(json)).object();
            return repository.applyRemoteQuote(row.value(QStringLiteral("id")).toString(),
                                               QString::fromLatin1(updatedAt),
                                               row.value(QStringLiteral("deleted")).toBool(),
                                               QString::fromUtf8(json));
        };
        const char* i4 = R"({"id":"i4","numero":"INF-2026-004","tipo":"informe","forma":"servicio",
          "cliente":"Josue Rodríguez","equipo":"asus x556U",
          "lineas":[{"seccion":"Mano de obra","concepto":"Diagnostico y Reparación","cantidad":1,"valorUnitario":3000},
                    {"seccion":"Repuestos y materiales","concepto":"Insumos","cantidad":1,"valorUnitario":500}],
          "descuento_tipo":"porcentaje","descuento_valor":7144,"abono_minor":400,
          "fecha_emision":"2026-09-19","fecha_ingreso":"2026-07-17","origen_id":"c1","hecho_en":"pc",
          "estado":"pagado","fecha_entrega":"2026-09-19","fecha_pago":"2026-09-22","motivo_rechazo":"",
          "deleted":false})";
        const char* c1 = R"({"id":"c1","numero":"COT-2026-001","tipo":"cotizacion","forma":"servicio",
          "cliente":"Josue Rodríguez","equipo":"asus x556U",
          "lineas":[{"seccion":"Mano de obra","concepto":"Reparacion","cantidad":2,"valorUnitario":500}],
          "descuento_tipo":"","descuento_valor":0,"abono_minor":0,"fecha_emision":"2026-09-19",
          "fecha_ingreso":"2026-07-17","origen_id":"","hecho_en":"pc","estado":"aceptada",
          "fecha_entrega":"","fecha_pago":"","motivo_rechazo":"","deleted":false})";
        check(put(i4, "2026-09-27T10:00:00+00:00"), "una fila nueva entra");
        check(put(c1, "2026-09-27T10:00:01+00:00"), "y otra");
        check(!put(i4, "2026-09-27T09:00:00+00:00"), "una version mas vieja no pisa a la nueva");
        check(!put(i4, "2026-09-27T10:00:00+00:00"), "la misma version otra vez no cambia nada");
        // Una fila rara: sin lineas, con nulos. Se lee con vacios, sin lanzar.
        check(put(R"({"id":"raro","numero":null,"tipo":"informe","lineas":null,"estado":"entregado","deleted":false})",
                  "2026-09-27T10:00:02+00:00"),
              "una fila con nulos tambien entra");
        check(put(R"({"id":"borrado","tipo":"informe","estado":"entregado","deleted":true})",
                  "2026-09-27T10:00:03+00:00"),
              "una lapida entra");
        check(put(R"({"sin":"id"})", "2026-09-27T10:00:04+00:00") == false,
              "una fila sin id no entra");

        const storage::QuoteRowsRead read = storage::readQuoteRows(repository);
        checkMinor(static_cast<int>(read.docs.size()), 3, "tres vivas: la lapida no se lee");
        check(read.errors.isEmpty(), "sin errores");

        const core::QuoteDoc* inf = nullptr;
        const core::QuoteDoc* cot = nullptr;
        const core::QuoteDoc* raro = nullptr;
        for (const auto& d : read.docs) {
            if (d.id == "i4") inf = &d;
            if (d.id == "c1") cot = &d;
            if (d.id == "raro") raro = &d;
        }
        check(inf != nullptr && cot != nullptr && raro != nullptr, "cada una con su id");
        if (inf != nullptr) {
            check(inf->kind == core::QuoteKind::Informe && inf->status == "pagado" &&
                      inf->number == "INF-2026-004",
                  "tipo, estado y numero");
            check(inf->client == "Josue Rodríguez" && inf->device == "asus x556U",
                  "cliente y equipo, con tildes");
            checkMinor(inf->baseMinor, 10'00, "35 con 71,44% de descuento: 10");
            checkMinor(inf->depositMinor, 4'00, "el abono");
            checkMinor(inf->balanceMinor(), 6'00, "el saldo");
            check(inf->issued == (core::Date{2026, 9, 19}), "emision");
            check(inf->received == (core::Date{2026, 7, 17}), "ingreso del equipo");
            check(inf->delivered == (core::Date{2026, 9, 19}), "fecha de entrega");
            check(inf->paid == (core::Date{2026, 9, 22}), "la fecha de pago");
            check(inf->originId == "c1", "su cotizacion de origen");
            check(inf->lines.size() == 2 && inf->lines[1].section == "Repuestos y materiales" &&
                      inf->lines[1].item == "Insumos" && inf->lines[1].totalMinor == 5'00,
                  "las lineas, con su seccion");
        }
        if (cot != nullptr) {
            check(cot->kind == core::QuoteKind::Cotizacion && cot->status == "aceptada",
                  "la cotizacion aceptada");
            checkMinor(cot->baseMinor, 10'00, "2 x 5, sin descuento");
            check(cot->originId.empty() && !cot->delivered && !cot->paid, "sin origen ni fechas de mas");
        }
        if (raro != nullptr) {
            check(raro->number == "raro" && raro->lines.empty() && raro->baseMinor == 0 &&
                      raro->client.empty() && !raro->paid,
                  "la fila con nulos: numero = id, sin lineas ni montos");
        }
        repository.wipe();
        check(repository.loadQuoteRows().empty(), "wipe vacia tambien las cotizaciones");
    }
```

- [ ] **Step 3: Correr y verlo fallar.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera un error de compilación: no existen `applyRemoteQuote`, `quoterows.hpp` ni `readQuoteRows`.

- [ ] **Step 4: Esquema v4.** Cambios en `storage/src/database.cpp`:
  - `constexpr int kTargetVersion = 4;`
  - Al final de `kUpgrades`, este paso. Igual que el 2 → 3, la tabla ya la crea el esquema; el paso existe para que el bucle corra:
    ```cpp
    // 3 -> 4: lo esencial de Cotizaciones, que baja de v2_quotes.
    "CREATE TABLE IF NOT EXISTS quotes (id TEXT PRIMARY KEY, updated_at TEXT NOT NULL DEFAULT '', "
    "deleted INTEGER NOT NULL DEFAULT 0, data TEXT NOT NULL)",
    ```
  - Al final de `kSchemaV1`, antes de `)SQL"`:
    ```sql
    -- Lo esencial de cada documento de DakeLabs Cotizaciones, tal como bajo de
    -- v2_quotes. Finanzas solo lo lee: nunca escribe ni encola nada aca. Se
    -- guarda el JSON entero de la fila: si Cotizaciones suma una columna,
    -- no hay que migrar esta tabla.
    CREATE TABLE IF NOT EXISTS quotes (
        id         TEXT PRIMARY KEY,
        updated_at TEXT NOT NULL DEFAULT '',
        deleted    INTEGER NOT NULL DEFAULT 0,
        data       TEXT NOT NULL
    );
    ```

- [ ] **Step 5: Repositorio.**

En `repository.hpp`, después de `applyRemote(const core::Movement&)`:

```cpp
    /// Una fila de v2_quotes tal como bajo del servidor. Finanzas nunca escribe
    /// cotizaciones, asi que no hay reloj propio que comparar: manda la fila que
    /// el servidor marco mas tarde (`updated_at`). Devuelve true si entro o
    /// cambio algo; false si era mas vieja, igual, o no tenia id.
    bool applyRemoteQuote(const QString& id, const QString& updatedAt, bool deleted,
                          const QString& json);

    /// El JSON de cada cotizacion viva, por id.
    [[nodiscard]] std::vector<QString> loadQuoteRows();
```

En `repository.cpp`, junto a los otros `applyRemote`:

```cpp
bool Repository::applyRemoteQuote(const QString& id, const QString& updatedAt, bool deleted,
                                  const QString& json) {
    if (id.isEmpty()) {
        return false;
    }
    QSqlQuery check(db_.handle());
    check.prepare(QStringLiteral("SELECT updated_at, data FROM quotes WHERE id = ?"));
    check.addBindValue(id);
    run(check);
    if (check.next()) {
        const QString localUpdated = check.value(0).toString();
        if (updatedAt < localUpdated ||
            (updatedAt == localUpdated && check.value(1).toString() == json)) {
            return false;
        }
    }
    QSqlQuery write(db_.handle());
    write.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO quotes (id, updated_at, deleted, data) VALUES (?, ?, ?, ?)"));
    write.addBindValue(id);
    write.addBindValue(updatedAt);
    write.addBindValue(deleted ? 1 : 0);
    write.addBindValue(json);
    run(write);
    return true;
}

std::vector<QString> Repository::loadQuoteRows() {
    QSqlQuery query(db_.handle());
    query.prepare(QStringLiteral("SELECT data FROM quotes WHERE deleted = 0 ORDER BY id"));
    run(query);
    std::vector<QString> out;
    while (query.next()) {
        out.push_back(query.value(0).toString());
    }
    return out;
}
```

En `wipe()`, la lista de tablas pasa a ser `{quotes, movements, jobs, pockets}`.

- [ ] **Step 6: El lector.** Crear `storage/include/dake/storage/quoterows.hpp`:

```cpp
#pragma once
//
// dake/storage/quoterows.hpp — los documentos de DakeLabs Cotizaciones, desde
// lo que bajo de v2_quotes.
//
// Solo lectura: Finanzas nunca escribe cotizaciones. Una fila que no se puede
// leer se anota como error y se saltea: un documento roto no puede frenar a
// los demas.
//
#include <QJsonObject>
#include <QStringList>

#include <vector>

#include "dake/core/quotes.hpp"
#include "dake/storage/repository.hpp"

namespace dake::storage {

struct QuoteRowsRead {
    std::vector<core::QuoteDoc> docs;
    QStringList errors;  ///< "<id>: <por que>"
};

/// Un documento desde su fila. Lanza std::runtime_error si no tiene id. Lo que
/// falte o venga nulo se lee vacio.
[[nodiscard]] core::QuoteDoc quoteFromRow(const QJsonObject& row);

[[nodiscard]] QuoteRowsRead readQuoteRows(Repository& repository);

} // namespace dake::storage
```

Crear `storage/src/quoterows.cpp`:

```cpp
#include "dake/storage/quoterows.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include <stdexcept>

namespace dake::storage {
namespace {

[[nodiscard]] std::string text(const QJsonValue& value) {
    return value.isString() ? value.toString().toStdString() : std::string();
}

/// Una fecha "aaaa-mm-dd", o nada si no hay o no se entiende.
[[nodiscard]] std::optional<core::Date> date(const QJsonValue& value) {
    const std::string iso = text(value);
    if (iso.size() < 10) {
        return std::nullopt;
    }
    try {
        return core::Date::fromIso(iso.substr(0, 10));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

core::QuoteDoc quoteFromRow(const QJsonObject& o) {
    core::QuoteDoc doc;
    doc.id = text(o.value(QStringLiteral("id")));
    if (doc.id.empty()) {
        throw std::runtime_error("no tiene id");
    }
    doc.number = text(o.value(QStringLiteral("numero")));
    if (doc.number.empty()) {
        doc.number = doc.id;  // un borrador todavia no tiene numero
    }
    doc.kind = text(o.value(QStringLiteral("tipo"))) == "cotizacion" ? core::QuoteKind::Cotizacion
                                                                      : core::QuoteKind::Informe;
    doc.status = text(o.value(QStringLiteral("estado")));
    doc.client = text(o.value(QStringLiteral("cliente")));
    doc.device = text(o.value(QStringLiteral("equipo")));
    doc.originId = text(o.value(QStringLiteral("origen_id")));
    doc.issued = date(o.value(QStringLiteral("fecha_emision")));
    doc.received = date(o.value(QStringLiteral("fecha_ingreso")));
    doc.delivered = date(o.value(QStringLiteral("fecha_entrega")));
    doc.paid = date(o.value(QStringLiteral("fecha_pago")));

    // Los totales, con las mismas reglas que calcularTotales de Cotizaciones.
    std::int64_t subtotal = 0;
    for (const QJsonValue& lineValue : o.value(QStringLiteral("lineas")).toArray()) {
        const QJsonObject line = lineValue.toObject();
        core::QuoteLine out;
        out.section = text(line.value(QStringLiteral("seccion")));
        out.item = text(line.value(QStringLiteral("concepto")));
        out.totalMinor = core::quoteLineTotal(
            line.value(QStringLiteral("cantidad")).toDouble(),
            static_cast<std::int64_t>(line.value(QStringLiteral("valorUnitario")).toDouble()));
        subtotal += out.totalMinor;
        doc.lines.push_back(out);
    }
    const std::string discountKind = text(o.value(QStringLiteral("descuento_tipo")));
    const auto discountValue =
        static_cast<std::int64_t>(o.value(QStringLiteral("descuento_valor")).toDouble());
    doc.baseMinor = core::quoteBase(subtotal,
                                    discountKind == "porcentaje" ? core::DiscountKind::Porcentaje
                                    : discountKind == "monto"    ? core::DiscountKind::Monto
                                                                 : core::DiscountKind::Ninguno,
                                    discountValue);
    doc.depositMinor = static_cast<std::int64_t>(o.value(QStringLiteral("abono_minor")).toDouble());
    return doc;
}

QuoteRowsRead readQuoteRows(Repository& repository) {
    QuoteRowsRead read;
    for (const QString& json : repository.loadQuoteRows()) {
        const QJsonObject row = QJsonDocument::fromJson(json.toUtf8()).object();
        try {
            read.docs.push_back(quoteFromRow(row));
        } catch (const std::exception& error) {
            read.errors << row.value(QStringLiteral("id")).toString() + QStringLiteral(": ") +
                               QString::fromUtf8(error.what());
        }
    }
    return read;
}

} // namespace dake::storage
```

En `storage/CMakeLists.txt`, agregar `src/quoterows.cpp` junto a `src/quotefolder.cpp`. La carpeta se borra en F3, cuando nadie la usa.

- [ ] **Step 7: Correr las pruebas.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera que `persistencia` pase y que las otras suites sigan en verde.

- [ ] **Step 8: Commit.** Agregar `supabase_v4_cotizaciones.sql`, `storage` y `tests/storagetest.cpp`, con el mensaje `Finanzas guarda lo esencial de Cotizaciones que baja de v2_quotes`.

### Tarea F2: `SyncEngine` con lista de tablas; el escritorio baja `quotes`

**Files:**
- Modify:
  - `sync/include/dake/sync/sync_engine.hpp` y `sync/src/sync_engine.cpp`.
  - `ui/mainwindow.cpp:115`.
  - `mobile/appbridge.cpp:110`.
  - `tests/synccheck.cpp`: la comprobación de tablas incluye `v2_quotes`.
  - `tests/uitest.cpp`.

**Interfaces:**
- Consumes: `Repository::applyRemoteQuote` (F1).
- Produces:
  - `QStringList sync::desktopTables()`, que da `{"pockets","jobs","movements","quotes"}`.
  - `QStringList sync::phoneTables()`, que da `{"pockets","jobs","movements"}`.
  - `SyncEngine(SupabaseClient&, storage::Repository&, QStringList tables, QObject* parent = nullptr)`.
  - `const QStringList& SyncEngine::tables() const noexcept`.

- [ ] **Step 1: Prueba que falla.** En `tests/uitest.cpp`, cerca del inicio de `main` (después de preparar la app), agregar un bloque `[sincronizacion]`:

```cpp
    std::printf("\n[sincronizacion]\n");
    {
        check(sync::desktopTables() ==
                  QStringList({QStringLiteral("pockets"), QStringLiteral("jobs"),
                               QStringLiteral("movements"), QStringLiteral("quotes")}),
              "el escritorio baja las cotizaciones, al final");
        check(!sync::phoneTables().contains(QStringLiteral("quotes")),
              "el telefono de Finanzas no las baja");
        storage::Database db(temp.path() + QStringLiteral("/sync.db"));
        storage::Repository repo(db);
        sync::SupabaseClient client(sync::SupabaseConfig{}, nullptr);
        sync::SyncEngine engine(client, repo, sync::desktopTables());
        check(engine.tables() == sync::desktopTables(), "el motor usa las tablas que se le dan");
        QString error;
        QObject::connect(&engine, &sync::SyncEngine::finished,
                         [&error](int, int, const QString& e) { error = e; });
        engine.sync();
        check(!error.isEmpty(), "sin sesion, la corrida termina con error y no se cuelga");
    }
```

Antes de escribirlo, confirmar en `sync/include/dake/sync/supabase_client.hpp` cómo se construye un `SupabaseClient` sin credenciales y ajustar esa línea a la firma real. Si hacen falta, agregar los `#include` de `dake/sync/sync_engine.hpp` y `dake/sync/supabase_client.hpp` a `uitest.cpp`.

- [ ] **Step 2: Verlo fallar.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera un error de compilación: no existen `desktopTables` ni el constructor de tres argumentos.

- [ ] **Step 3: Implementación.**

En `sync_engine.hpp`:
- Reemplazar `kTables` por:
  ```cpp
  /// Las tablas del escritorio, en orden de dependencia: bolsillos, trabajos y
  /// movimientos, y al final lo esencial de Cotizaciones, que solo baja (nunca
  /// hay cotizaciones en la outbox). El nombre es el de la tabla local y el de
  /// la remota sin el prefijo `v2_`.
  [[nodiscard]] QStringList desktopTables();
  /// Las del telefono de Finanzas: no importa cotizaciones, asi que no las baja.
  [[nodiscard]] QStringList phoneTables();
  ```
- Ajustar el constructor:
  ```cpp
  SyncEngine(SupabaseClient& client, storage::Repository& repository, QStringList tables,
             QObject* parent = nullptr);
  [[nodiscard]] const QStringList& tables() const noexcept;
  ```
- Los miembros quedan así:
  - `QStringList tables_;`
  - `std::vector<QString> cursors_;`, con el comentario de "un cursor POR TABLA" conservado.
  - Se borra `std::array<QString, 3>` y se incluye `<QStringList>`.
- En los comentarios de cabecera, "tres tablas" pasa a "las tablas que se le dan, en orden", y hay que decir que `quotes` solo baja.

En `sync_engine.cpp`:
- Definir `desktopTables()` y `phoneTables()`.
- El constructor guarda `tables_` y hace `cursors_.resize(tables_.size())`.
- `tables()` devuelve `tables_`.
- `sync()` recorre `tables_` en lugar de `kTables`.
- `pullNextPage` usa `tables_.size()` y `tables_.at(...)`.
- El despacho por tabla compara contra `"pockets"`, `"jobs"` y `"movements"` con `QLatin1String`, y suma esta rama:

```cpp
                } else if (localTable == QLatin1String("quotes")) {
                    // Lo esencial de Cotizaciones. Se guarda la fila entera tal
                    // como vino; quien la lee arma el documento.
                    applied = repository_.applyRemoteQuote(
                        row.value(QStringLiteral("id")).toString(), updatedAt,
                        row.value(QStringLiteral("deleted")).toBool(),
                        QString::fromUtf8(QJsonDocument(row).toJson(QJsonDocument::Compact)));
                }
```

Llamadas:
- `ui/mainwindow.cpp:115`: `std::make_unique<sync::SyncEngine>(*supabase_, *repository_, sync::desktopTables(), this)`.
- `mobile/appbridge.cpp:110`: lo mismo con `sync::phoneTables()`.

En `tests/synccheck.cpp`, el paso 4 recorre también `v2_quotes`. Si esa tabla da 404, imprimir: `"v2_quotes no existe: correr supabase_v4_cotizaciones.sql"`.

- [ ] **Step 4: Correr las pruebas.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera: 4/4 en verde, con los checks de `[sincronizacion]` pasando.

- [ ] **Step 5: Commit.** El mensaje es `El motor de sincronizacion recibe sus tablas; el escritorio baja las cotizaciones`.

### Tarea F3: Finanzas importa desde la base; sale la carpeta

**Files:**
- Modify:
  - `ui/mainwindow.hpp` y `ui/mainwindow.cpp`: `importQuotes`, el constructor, el `finished` del motor, y se borran `quoteFolder()` y `watchQuoteFolder()`.
  - `ui/pages.hpp` y `ui/settingspage.cpp`: la tarjeta.
  - `ui/snapshot.hpp`: salen `quoteFolder` y `quoteFolderFound`; queda `quoteErrors`.
  - `ui/preview.cpp`: los comandos `cotizaciones` y `plan`.
  - `tests/uitest.cpp`.
- Delete: `storage/include/dake/storage/quotefolder.hpp` y `storage/src/quotefolder.cpp`, y su línea en `storage/CMakeLists.txt`.

**Interfaces:**
- Consumes: `readQuoteRows`, `applyRemoteQuote` y `desktopTables` (F1 y F2).
- Produces: `public slots: void MainWindow::importQuotes(bool notify)`. La llama la sincronización al terminar, y las pruebas también.

- [ ] **Step 1: Prueba que falla.** En `tests/uitest.cpp`:
  - Reemplazar `writeDoc` e `informe` por los helpers de abajo.
  - Reescribir el bloque `[DakeLabs Cotizaciones]` con filas en lugar de archivos.
  - Borrar las dos líneas `repo.setSetting(QStringLiteral("cot.carpeta"), …)`, en las líneas 250 y 819 aprox.

Helpers:

```cpp
/// Una fila de v2_quotes con lo esencial, como la sube Cotizaciones.
[[nodiscard]] QByteArray quoteRow(const char* id, const char* number, const char* kind,
                                  const char* status, const char* client, const char* device,
                                  int unit, const char* origin, const char* paidOn) {
    QByteArray json = R"({"id":"ID","numero":"NUM","tipo":"KIND","forma":"servicio","cliente":"CLI",
      "equipo":"DEV","lineas":[{"seccion":"Mano de obra","concepto":"Reparacion","cantidad":1,"valorUnitario":UNIT}],
      "descuento_tipo":"","descuento_valor":0,"abono_minor":0,"fecha_emision":"2026-09-19",
      "fecha_ingreso":"2026-09-10","origen_id":"ORIG","hecho_en":"pc","estado":"EST",
      "fecha_entrega":"DELIV","fecha_pago":"PAID","motivo_rechazo":"","deleted":false})";
    const bool informe = QByteArray(kind) == "informe";
    json.replace("ID", id).replace("NUM", number).replace("KIND", kind).replace("EST", status)
        .replace("CLI", client).replace("DEV", device).replace("UNIT", QByteArray::number(unit))
        .replace("ORIG", origin == nullptr ? QByteArray() : QByteArray(origin))
        .replace("DELIV", informe ? QByteArray("2026-09-19") : QByteArray())
        .replace("PAID", paidOn == nullptr ? QByteArray() : QByteArray(paidOn));
    return json;
}

/// Lo que haria la sincronizacion al bajarla.
void putQuote(storage::Repository& repo, const QByteArray& json, const char* updatedAt) {
    const QJsonObject row = QJsonDocument::fromJson(json).object();
    repo.applyRemoteQuote(row.value(QStringLiteral("id")).toString(), QString::fromLatin1(updatedAt),
                          false, QString::fromUtf8(json));
}
```

Bloque nuevo:

```cpp
    // --- DakeLabs Cotizaciones -----------------------------------------------
    //
    // Otra base y otra ventana, con filas como las que baja la sincronizacion:
    // una cotizacion aceptada, su informe entregado, y el Macbook dos veces.
    std::printf("\n[DakeLabs Cotizaciones]\n");
    {
        const QString quotesPath = temp.path() + QStringLiteral("/cotizaciones.db");
        {
            storage::Database setup(quotesPath);
            storage::Repository repo(setup);
            putQuote(repo, quoteRow("c1", "COT-2026-001", "cotizacion", "aceptada", "Josue Rodríguez",
                                    "asus x556U", 0, nullptr, nullptr), "2026-09-27T10:00:00+00:00");
            putQuote(repo, quoteRow("i4", "INF-2026-004", "informe", "entregado", "Josue Rodríguez",
                                    "asus x556U", 1000, "c1", nullptr), "2026-09-27T10:00:01+00:00");
            putQuote(repo, quoteRow("i1", "INF-2026-001", "informe", "pagado", "Sr. Galván",
                                    "Macbook M1", 6983, nullptr, "2026-09-22"), "2026-09-27T10:00:02+00:00");
            putQuote(repo, quoteRow("i2", "INF-2026-002", "informe", "pagado", "Sr. Galván",
                                    "Macbook M1", 6982, nullptr, "2026-09-22"), "2026-09-27T10:00:03+00:00");
        }
        qputenv("DAKE_TEST_DB_PATH", quotesPath.toLocal8Bit());

        ui::MainWindow quotesWindow(quotesPath);
        quotesWindow.show();
        (void)QTest::qWaitForWindowExposed(&quotesWindow);
        quotesWindow.activateWindow();
        settle();

        storage::Database qdb(quotesPath);
        storage::Repository qrepo(qdb);
        auto movements = qrepo.loadMovements();
        const core::Movement* saldo = findMovement(movements, "cot-i4-saldo");
        check(saldo != nullptr && saldo->amountMinor == 10'00 && !saldo->settled,
              "al arrancar: el informe entregado es un ingreso por cobrar de 10,00");
        check(saldo != nullptr && saldo->jobId == "cot-c1",
              "sobre la reparacion que abrio la cotizacion");
        const auto quoteRepairs = qrepo.loadRepairs();
        const core::Repair* asus = findRepair(quoteRepairs, "asus x556U");
        check(asus != nullptr && asus->status == core::RepairStatus::Entregada &&
                  asus->orderNo == "INF-2026-004",
              "la reparacion queda entregada, con el numero del informe");
        const core::Movement* mac = findMovement(movements, "cot-i1-saldo");
        check(mac != nullptr && mac->settled && mac->settledDate == (core::Date{2026, 9, 22}),
              "el informe pagado entra cobrado, con su fecha de pago");
        check(findMovement(movements, "cot-i2-saldo") == nullptr,
              "el repetido no entra: espera que decidas");

        // Releer no duplica nada.
        const std::size_t before = qrepo.loadMovements().size();
        quotesWindow.importQuotes(false);
        settle();
        check(qrepo.loadMovements().size() == before, "leer otra vez no agrega nada");

        // El telefono marca el informe pagado: baja la fila nueva y, al terminar
        // la sincronizacion, Finanzas importa.
        putQuote(qrepo, quoteRow("i4", "INF-2026-004", "informe", "pagado", "Josue Rodríguez",
                                 "asus x556U", 1000, "c1", "2026-09-22"), "2026-09-27T11:00:00+00:00");
        quotesWindow.importQuotes(true);
        settle();
        const auto afterPaid = qrepo.loadMovements();
        const core::Movement* paid = findMovement(afterPaid, "cot-i4-saldo");
        check(paid != nullptr && paid->settled && paid->settledDate == (core::Date{2026, 9, 22}),
              "marcado pagado en el telefono, el ingreso queda cobrado con esa fecha");
        check(findRepair(qrepo.loadRepairs(), "asus x556U") != nullptr &&
                  findRepair(qrepo.loadRepairs(), "asus x556U")->status == core::RepairStatus::Cobrada,
              "y la reparacion, cobrada");

        auto* settings = quotesWindow.findChild<ui::SettingsPage*>();
        check(settings->findChild<QLineEdit*>(QStringLiteral("QuoteFolder")) == nullptr,
              "Ajustes ya no pide carpeta");

        // La revision: Enter aplica lo sugerido (ignorar el repetido).
        onNextDialog([](QWidget* dialog) { QTest::keyClick(dialog, Qt::Key_Return); });
        emit settings->quoteReviewRequested();
        reactivate(&quotesWindow);
        const QString decisions = qrepo.setting(QStringLiteral("cot.decisiones")).value_or(QString());
        check(decisions.contains(QStringLiteral("\"i2\":\"ignorar\"")),
              "revisar y Enter: el repetido queda ignorado");
        check(findMovement(qrepo.loadMovements(), "cot-i2-saldo") == nullptr, "y no entra");
    }
```

Nota: el `QuoteFolder` de la prueba es un objectName que hoy no existe. Esa comprobación vale como centinela y pasa en los dos casos; la que falla de verdad al principio es `importQuotes` público. Hay que ver en la salida que la compilación falla por `importQuotes`.

- [ ] **Step 2: Verlo fallar.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera un error de compilación: `importQuotes` es privada.

- [ ] **Step 3: Implementación.**

`mainwindow.hpp`:
- Mover `void importQuotes(bool notify);` a `public slots:`, con este comentario:
  ```cpp
  /// Arma el plan de lo que bajo de Cotizaciones y lo aplica. La llama la
  /// sincronizacion al terminar; tambien las pruebas.
  ```
- Borrar `quoteFolder()`, `watchQuoteFolder()`, `quoteWatcher_` y `quoteDebounce_`, y el forward o `#include` de `QFileSystemWatcher` si queda sin uso.

`mainwindow.cpp`:
- `#include "dake/storage/quoterows.hpp"` en lugar de `quotefolder.hpp`.
- Borrar el bloque que crea `quoteWatcher_` y `quoteDebounce_` (líneas 187-193) y la conexión `quoteFolderChanged` (516-520).
- El `finished` del motor queda así:
  ```cpp
      connect(syncEngine_.get(), &sync::SyncEngine::finished, this, [this](int uploaded, int downloaded, const QString& error) {
          if (error.isEmpty()) {
              updateCloudUi(QStringLiteral("%1 ↑, %2 ↓").arg(uploaded).arg(downloaded));
              reload();
              // Lo que bajo de Cotizaciones entra en la misma corrida.
              if (downloaded > 0) importQuotes(true);
          } else {
              updateCloudUi(error);
          }
      });
  ```
- El comienzo de `importQuotes` queda así:
  ```cpp
  void MainWindow::importQuotes(bool notify) {
      const storage::QuoteRowsRead read = storage::readQuoteRows(*repository_);
      snapshot_.quoteErrors = read.errors;
      snapshot_.quoteDocs = read.docs;
      // ... el resto sin cambios, desde `core::QuoteContext context;`
  ```
- Buscar con `grep -n "quoteReadRequested"` la conexión de "Leer ahora" y borrarla.

`pages.hpp`: borrar la señal `quoteFolderChanged`, la señal `quoteReadRequested` y el miembro `quoteFolder_`.

`settingspage.cpp`:
- El subtítulo de la tarjeta:
  ```cpp
      quotesCard->setSubtitle(
          QStringLiteral("Finanzas lee lo esencial de cada documento desde la nube y nunca lo cambia. "
                         "Un informe entregado crea el ingreso por cobrar; al marcarlo pagado —en la "
                         "PC o en el teléfono—, queda cobrado con esa fecha. Una cotización aceptada "
                         "abre la reparación."));
  ```
- Sale el `QLineEdit` y "Leer ahora". La fila queda con un stretch y `quoteReview_`.
- En `setSnapshot`:
  - borrar el bloque `quoteFolder_`;
  - reemplazar la rama `!snapshot.quoteFolderFound` por:
  ```cpp
      if (snapshot.quoteDocs.empty()) {
          quoteStatus_->setText(QStringLiteral("Todavía no bajó ningún documento. En DakeLabs "
                                               "Cotizaciones: Datos y respaldo → Nube → Conectar, "
                                               "con esta misma cuenta, y después Sincronizar."));
          theme::setLabelColor(quoteStatus_, theme::kTextMuted);
      } else {
          // (lo de hoy, con el texto cambiado a "%1 documentos desde la nube · %2 al día en Finanzas")
      }
  ```

`snapshot.hpp`: borrar `quoteFolder` y `quoteFolderFound`.

`preview.cpp`:
- `cotizaciones <base>`: abrir `Database`/`Repository` sobre esa base y usar `readQuoteRows` en lugar de la carpeta.
- `plan <base>`: lo mismo, sin el tercer argumento de la carpeta.
- Actualizar el texto de uso y cambiar el include a `quoterows.hpp`.
- Borrar la línea 310 que fija `cot.carpeta`.

Borrar los dos archivos `quotefolder.*` y su línea de CMake.

Verificación: `grep -rn "quotefolder\|quoteFolder\|cot.carpeta\|readQuoteFolder" ui storage tests mobile sync` no devuelve nada.

- [ ] **Step 4: Correr las pruebas.** Ejecutar `.\compilar.ps1 debug -Probar`. Se espera: 4/4 en verde, sin advertencias.

- [ ] **Step 5: `PENDIENTE.md`.** Agregar arriba una sección "Cotizaciones en la base" con los tres pasos de David que da el spec:
  1. correr `supabase_v4_cotizaciones.sql` en el panel de Supabase;
  2. en Cotizaciones de PC: Datos y respaldo → Nube → Conectar;
  3. instalar `CotizacionesDakeLabs.apk`.

  Aclarar que hasta que Cotizaciones suba, Finanzas no muestra documentos: ya no lee la carpeta.

- [ ] **Step 6: Commit.** El mensaje es `Finanzas lee las cotizaciones desde la base y deja de leer la carpeta`.

---

## Parte 2a — Cotizaciones de PC

Todas las tareas C corren en `C:\Users\David\Documents\VisualStudio\dakelabsfactura`. Las pruebas se corren con `npm test`; una sola con `npx vitest run <ruta>`.

### Tarea C1: la fila, lo esencial y el reloj

**Files:**
- Create:
  - `src/nube/fila.ts` y `src/nube/fila.test.ts`
  - `src/nube/esencial.ts` y `src/nube/esencial.test.ts`
  - `src/nube/reloj.ts` y `src/nube/reloj.test.ts`
- Modify:
  - `src/renderer/src/dominio/tipos.ts`: `EntradaIndice.delTelefono?`.
  - `src/renderer/src/dominio/indice.ts`: `CREADO_EN_TELEFONO` y `delTelefono`, con su prueba en `indice.test.ts`.
  - `src/renderer/src/dominio/estados.test.ts`: una prueba nueva.

**Interfaces:**
- Produces:
  - Tipos: `FilaCotizacion`, `ContenidoFila`, `EstadoFila`, `LineaFila`, `Grupos`.
  - De `fila.ts`:
    - `COLUMNAS_CONTENIDO` y `COLUMNAS_ESTADO`;
    - `filaParcial(fila, grupos, userId): Record<string, unknown>`;
    - `contenidoDeFila(f): ContenidoFila`, `estadoDeFila(f): EstadoFila`;
    - `mismoContenido(a, b)`, `mismoEstado(a, b)`;
    - `normalizarFila(x: unknown): FilaCotizacion | null`;
    - `filaVacia(id: string): FilaCotizacion`.
  - De `esencial.ts`:
    - `contenidoDe(doc, hechoEn)`, `estadoDe(doc)`;
    - `pasoHacia(doc, remoto: EstadoFila, hoy): { accion: Accion; datos: DatosAccion } | null`;
    - `borradorDeFila(fila, enBlanco: Documento): Documento`;
    - `totalesDeFila(fila): { subtotal: number; base: number; saldo: number }`.
  - De `reloj.ts`: `class Reloj(deviceId, ultimo?)` con `ahora(ms)`, `recibir(hlc)` y `ultimo()`, y `codificar(wall, contador, deviceId)`.
  - De `dominio/indice.ts`: `CREADO_EN_TELEFONO = 'Creado en el teléfono'`.

- [ ] **Step 0: Rama.** Ejecutar `git -C C:\Users\David\Documents\VisualStudio\dakelabsfactura switch -c nube`.

- [ ] **Step 1: Pruebas que fallan.**

`src/nube/reloj.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { Reloj, codificar } from './reloj'

describe('reloj', () => {
  it('codifica como Finanzas: 13 dígitos, guion, 5 dígitos, guion, aparato', () => {
    expect(codificar(1754611200000, 42, 'abc')).toBe('1754611200000-00042-abc')
  })
  it('dos sellos seguidos con el mismo milisegundo nunca son iguales, y crecen', () => {
    const r = new Reloj('d')
    const a = r.ahora(1000)
    const b = r.ahora(1000)
    expect(b > a).toBe(true)
  })
  it('un reloj de pared atrasado no retrocede', () => {
    const r = new Reloj('d')
    const a = r.ahora(5000)
    expect(r.ahora(1000) > a).toBe(true)
  })
  it('lo que se sella después de recibir uno remoto queda por encima', () => {
    const r = new Reloj('d')
    r.recibir(codificar(9000, 7, 'otro-aparato-con-guiones'))
    expect(r.ahora(1000) > codificar(9000, 7, 'otro-aparato-con-guiones')).toBe(true)
  })
  it('arranca desde el último guardado', () => {
    const r = new Reloj('d', codificar(9000, 3, 'd'))
    expect(r.ahora(1000)).toBe(codificar(9000, 4, 'd'))
  })
})
```

`src/nube/esencial.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { contenidoDe, estadoDe, pasoHacia, borradorDeFila, totalesDeFila } from './esencial'
import { filaVacia, type FilaCotizacion } from './fila'
import { documentoEnBlanco } from '../renderer/src/dominio/nuevo'
import { configDeEjemplo } from '../renderer/src/dominio/ejemplo'
import { aplicarAccion } from '../renderer/src/dominio/estados'
import type { Documento } from '../renderer/src/dominio/tipos'

function informe(): Documento {
  const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'informe', forma: 'servicio', hoy: '2026-09-19', nuevoId: () => 'i4' })
  return {
    ...d,
    numero: 'INF-2026-004',
    estado: 'entregado',
    fechaEntrega: '2026-09-19',
    clienteCongelado: { ...d.clienteCongelado, nombre: 'Josue Rodríguez', telefono: '6000-0000', identificacion: '8-1-1' },
    equipo: { ...d.equipo!, descripcion: 'asus x556U', fechaIngreso: '2026-07-17', fallaReportada: 'no enciende' },
    resumen: 'Se cambió el conector de carga y se limpió la placa.',
    notas: 'Garantía de 30 días',
    categorias: [
      { id: 'a', nombre: 'Mano de obra', lineas: [{ id: 'l1', concepto: 'Diagnóstico y reparación', cantidad: 1, valorUnitario: 3000 }] },
      { id: 'b', nombre: 'Repuestos y materiales', lineas: [{ id: 'l2', concepto: 'Conector', cantidad: 2, valorUnitario: 250 }] }
    ],
    descuento: { tipo: 'porcentaje', valor: 1000 },
    abono: 400,
    origenId: 'c1'
  }
}

describe('lo esencial de un documento', () => {
  it('da la fila esperada', () => {
    expect(contenidoDe(informe(), 'pc')).toEqual({
      numero: 'INF-2026-004', tipo: 'informe', forma: 'servicio', cliente: 'Josue Rodríguez', equipo: 'asus x556U',
      lineas: [
        { seccion: 'Mano de obra', concepto: 'Diagnóstico y reparación', cantidad: 1, valorUnitario: 3000 },
        { seccion: 'Repuestos y materiales', concepto: 'Conector', cantidad: 2, valorUnitario: 250 }
      ],
      descuento_tipo: 'porcentaje', descuento_valor: 1000, abono_minor: 400,
      fecha_emision: '2026-09-19', fecha_ingreso: '2026-07-17', origen_id: 'c1', hecho_en: 'pc'
    })
    expect(estadoDe(informe())).toEqual({ estado: 'entregado', fecha_entrega: '2026-09-19', fecha_pago: '', motivo_rechazo: '' })
  })

  it('no sube resumen, notas, cláusulas, falla ni datos del cliente', () => {
    const texto = JSON.stringify({ ...contenidoDe(informe(), 'pc'), ...estadoDe(informe()) })
    for (const nada of ['conector de carga', 'Garantía de 30', '6000-0000', '8-1-1', 'no enciende']) {
      expect(texto).not.toContain(nada)
    }
  })

  it('una fila típica pesa alrededor de 1 KB', () => {
    const fila: FilaCotizacion = { ...filaVacia('0199a1b2-c3d4-7e5f-8a9b-0c1d2e3f4a5b'), ...contenidoDe(informe(), 'pc'), ...estadoDe(informe()) }
    expect(JSON.stringify(fila).length).toBeLessThan(1200)
  })

  it('un borrador no lleva fecha de emisión ni de entrega', () => {
    const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'informe', forma: 'servicio', hoy: '2026-09-19' })
    expect(contenidoDe(d, 'pc').fecha_emision).toBe('')
    expect(estadoDe(d).fecha_entrega).toBe('')
  })

  it('la fecha de pago sale del evento Pagado', () => {
    const pagado = aplicarAccion(informe(), 'cobrar', { hoy: '2026-09-22' })
    expect(estadoDe(pagado).fecha_pago).toBe('2026-09-22')
  })

  it('en un proyecto, el equipo es el título', () => {
    const p = documentoEnBlanco(configDeEjemplo, [], { tipo: 'cotizacion', forma: 'proyecto', hoy: '2026-09-19' })
    expect(contenidoDe({ ...p, proyecto: { ...p.proyecto!, titulo: 'Red de la oficina' } }, 'pc').equipo).toBe('Red de la oficina')
  })
})

describe('lo que llega del teléfono', () => {
  it('cobrar con la fecha del teléfono deja el evento Pagado con esa fecha', () => {
    const paso = pasoHacia(informe(), { estado: 'pagado', fecha_entrega: '2026-09-19', fecha_pago: '2026-09-21', motivo_rechazo: '' }, '2026-09-27')
    expect(paso).toEqual({ accion: 'cobrar', datos: { hoy: '2026-09-21' } })
    const doc = aplicarAccion(informe(), paso!.accion, paso!.datos)
    expect(doc.historial.at(-1)).toEqual({ fecha: '2026-09-21', tipo: 'estado', detalle: 'Pagado' })
  })
  it('aceptar y rechazar una cotización enviada', () => {
    const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'cotizacion', forma: 'servicio', hoy: '2026-09-19' })
    const enviada = { ...d, estado: 'enviada' as const, numero: 'COT-2026-001' }
    const vacio = { fecha_entrega: '', fecha_pago: '', motivo_rechazo: '' }
    expect(pasoHacia(enviada, { ...vacio, estado: 'aceptada' }, '2026-09-27')?.accion).toBe('aceptar')
    expect(pasoHacia(enviada, { ...vacio, estado: 'rechazada', motivo_rechazo: 'caro' }, '2026-09-27'))
      .toEqual({ accion: 'rechazar', datos: { hoy: '2026-09-27', motivo: 'caro' } })
    expect(pasoHacia(enviada, { ...vacio, estado: 'rechazada' }, '2026-09-27')).toBeNull()
  })
  it('nada que el teléfono no pueda hacer', () => {
    expect(pasoHacia(informe(), { estado: 'borrador', fecha_entrega: '', fecha_pago: '', motivo_rechazo: '' }, '2026-09-27')).toBeNull()
  })

  it('un borrador del teléfono vuelve con cliente, equipo y líneas', () => {
    const fila: FilaCotizacion = {
      ...filaVacia('t1'), tipo: 'cotizacion', hecho_en: 'telefono', cliente: 'Ana', equipo: 'iPhone 11',
      fecha_ingreso: '2026-09-26',
      lineas: [
        { seccion: 'Mano de obra', concepto: 'Cambio de pantalla', cantidad: 1, valorUnitario: 2500 },
        { seccion: 'Repuestos y materiales', concepto: 'Pantalla', cantidad: 1, valorUnitario: 4000 }
      ]
    }
    const blanco = documentoEnBlanco(configDeEjemplo, [], { tipo: 'cotizacion', forma: 'servicio', hoy: '2026-09-27' })
    const doc = borradorDeFila(fila, blanco)
    expect(doc.id).toBe('t1')
    expect(doc.estado).toBe('borrador')
    expect(doc.clienteCongelado.nombre).toBe('Ana')
    expect(doc.equipo!.descripcion).toBe('iPhone 11')
    expect(doc.equipo!.fechaIngreso).toBe('2026-09-26')
    expect(doc.categorias.map((c) => c.nombre)).toEqual(['Mano de obra', 'Repuestos y materiales'])
    expect(doc.historial[0].detalle).toBe('Creado en el teléfono')
    const vuelta = contenidoDe(doc, 'telefono')
    expect(vuelta.lineas).toEqual(fila.lineas)
    expect(vuelta.cliente).toBe('Ana')
  })

  it('totales de una fila con las reglas de la PC', () => {
    const f: FilaCotizacion = { ...filaVacia('x'), lineas: [{ seccion: 'M', concepto: 'a', cantidad: 1.5, valorUnitario: 333 }], descuento_tipo: 'monto', descuento_valor: 100, abono_minor: 50 }
    expect(totalesDeFila(f)).toEqual({ subtotal: 500, base: 400, saldo: 350 })
  })
})
```

`src/nube/fila.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { filaParcial, filaVacia, normalizarFila, mismoContenido, mismoEstado } from './fila'

describe('fila', () => {
  it('la subida de estado lleva solo id, dueño, aparato y el estado con su reloj', () => {
    const f = { ...filaVacia('a'), estado: 'pagado' as const, fecha_pago: '2026-09-22', estado_hlc: 'E', hlc: 'C', device_id: 'tel' }
    expect(filaParcial(f, { contenido: false, estado: true }, 'u')).toEqual({
      id: 'a', user_id: 'u', device_id: 'tel', estado: 'pagado', fecha_entrega: '', fecha_pago: '2026-09-22', motivo_rechazo: '', estado_hlc: 'E'
    })
  })
  it('la de contenido lleva la lápida y su reloj, sin el estado', () => {
    const p = filaParcial({ ...filaVacia('a'), hlc: 'C', deleted: true }, { contenido: true, estado: false }, 'u')
    expect(p.deleted).toBe(true)
    expect(p.hlc).toBe('C')
    expect('estado' in p).toBe(false)
  })
  it('normalizar una fila con nulos la deja con vacíos', () => {
    const f = normalizarFila({ id: 'r', numero: null, lineas: null, estado: 'entregado', deleted: null, updated_at: 'T' })!
    expect(f.numero).toBe('')
    expect(f.lineas).toEqual([])
    expect(f.deleted).toBe(false)
    expect(f.updated_at).toBe('T')
    expect(normalizarFila({ sin: 'id' })).toBeNull()
  })
  it('compara contenido y estado por separado', () => {
    const a = filaVacia('a')
    expect(mismoContenido(a, { ...a, hlc: 'otro' })).toBe(true)
    expect(mismoContenido(a, { ...a, cliente: 'Otro' })).toBe(false)
    expect(mismoEstado(a, { ...a, cliente: 'Otro' })).toBe(true)
    expect(mismoEstado(a, { ...a, estado: 'enviada' })).toBe(false)
  })
})
```

Agregar a `indice.test.ts`:

```ts
it('marca del teléfono un borrador creado allí', () => {
  const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'cotizacion', forma: 'servicio', hoy: '2026-09-27' })
  const tel = { ...d, historial: [{ fecha: '2026-09-27', tipo: 'creado' as const, detalle: CREADO_EN_TELEFONO }] }
  expect(entradaDeDocumento(tel).delTelefono).toBe(true)
  expect(entradaDeDocumento(d).delTelefono).toBeFalsy()
  expect(entradaDeDocumento({ ...tel, estado: 'enviada', numero: 'COT-1' }).delTelefono).toBeFalsy()
})
```

Si `indice.test.ts` todavía no importa `documentoEnBlanco`, `configDeEjemplo` o `CREADO_EN_TELEFONO`, agregar esos imports.

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/nube src/renderer/src/dominio/indice.test.ts`. Se espera FAIL: no existen los módulos ni `CREADO_EN_TELEFONO`.

- [ ] **Step 3: Implementación.**

`src/nube/reloj.ts`:

```ts
/**
 * CONTRATO — Reloj lógico híbrido, con el formato de Finanzas.
 *
 * `"%013d-%05d-<aparato>"`: pared en milisegundos, contador y aparato. Se
 * compara como texto, así que el orden es el mismo en SQLite, en Postgres y
 * aquí. `ahora` nunca devuelve dos veces lo mismo ni retrocede aunque el reloj
 * de pared se atrase; `recibir` deja lo que se selle después por encima de lo
 * recibido.
 *
 * Dependencias permitidas: ninguna.
 */
export function codificar(wall: number, contador: number, deviceId: string): string {
  return `${String(wall).padStart(13, '0')}-${String(contador).padStart(5, '0')}-${deviceId}`
}

function partes(hlc: string): [number, number] | null {
  const m = /^(\d{13})-(\d{5})-/.exec(hlc)
  return m ? [Number(m[1]), Number(m[2])] : null
}

export class Reloj {
  private wall = 0
  private contador = 0

  constructor(private readonly deviceId: string, ultimo = '') {
    const p = partes(ultimo)
    if (p) [this.wall, this.contador] = p
  }

  ahora(ms: number): string {
    if (ms > this.wall) {
      this.wall = ms
      this.contador = 0
    } else {
      this.contador += 1
    }
    return codificar(this.wall, this.contador, this.deviceId)
  }

  recibir(hlc: string): void {
    const p = partes(hlc)
    if (!p) return
    const [w, c] = p
    if (w > this.wall || (w === this.wall && c > this.contador)) {
      this.wall = w
      this.contador = c
    }
  }

  ultimo(): string {
    return codificar(this.wall, this.contador, this.deviceId)
  }
}
```

`src/nube/fila.ts`:

```ts
/**
 * CONTRATO — La fila de un documento en la nube (`v2_quotes`).
 *
 * Solo lo esencial: lo que hace falta para verlo en el teléfono y para que
 * Finanzas lo importe. Dos grupos de columnas, cada uno con su reloj:
 *   contenido (`hlc`): lo escribe solo el dueño del documento;
 *   estado (`estado_hlc`): lo escriben la PC y el teléfono.
 * Una subida manda solo los grupos que cambiaron: así un cambio de estado del
 * teléfono nunca reescribe las líneas que la PC corrigió.
 *
 * Dependencias permitidas: ../renderer/src/dominio/tipos.
 */
import type { Estado, FormaTrabajo, TipoDocumento } from '../renderer/src/dominio/tipos'

export interface LineaFila {
  seccion: string
  concepto: string
  cantidad: number
  valorUnitario: number
}

export interface ContenidoFila {
  numero: string
  tipo: TipoDocumento
  forma: FormaTrabajo
  cliente: string
  equipo: string
  lineas: LineaFila[]
  descuento_tipo: '' | 'porcentaje' | 'monto'
  descuento_valor: number
  abono_minor: number
  fecha_emision: string
  fecha_ingreso: string
  origen_id: string
  hecho_en: 'pc' | 'telefono'
}

export interface EstadoFila {
  estado: Estado
  fecha_entrega: string
  fecha_pago: string
  motivo_rechazo: string
}

export interface FilaCotizacion extends ContenidoFila, EstadoFila {
  id: string
  hlc: string
  estado_hlc: string
  device_id: string
  deleted: boolean
  /** Lo pone el servidor. Nunca se manda. */
  updated_at?: string
}

export interface Grupos {
  contenido: boolean
  estado: boolean
}

export const COLUMNAS_CONTENIDO = [
  'numero', 'tipo', 'forma', 'cliente', 'equipo', 'lineas', 'descuento_tipo', 'descuento_valor',
  'abono_minor', 'fecha_emision', 'fecha_ingreso', 'origen_id', 'hecho_en'
] as const satisfies ReadonlyArray<keyof ContenidoFila>

export const COLUMNAS_ESTADO = ['estado', 'fecha_entrega', 'fecha_pago', 'motivo_rechazo'] as const satisfies ReadonlyArray<keyof EstadoFila>

export function filaVacia(id: string): FilaCotizacion {
  return {
    id, numero: '', tipo: 'cotizacion', forma: 'servicio', cliente: '', equipo: '', lineas: [],
    descuento_tipo: '', descuento_valor: 0, abono_minor: 0, fecha_emision: '', fecha_ingreso: '',
    origen_id: '', hecho_en: 'pc', hlc: '', estado: 'borrador', fecha_entrega: '', fecha_pago: '',
    motivo_rechazo: '', estado_hlc: '', device_id: '', deleted: false
  }
}

export function contenidoDeFila(f: ContenidoFila): ContenidoFila {
  const out = {} as Record<string, unknown>
  for (const c of COLUMNAS_CONTENIDO) out[c] = f[c]
  return out as unknown as ContenidoFila
}

export function estadoDeFila(f: EstadoFila): EstadoFila {
  return { estado: f.estado, fecha_entrega: f.fecha_entrega, fecha_pago: f.fecha_pago, motivo_rechazo: f.motivo_rechazo }
}

export function mismoContenido(a: ContenidoFila, b: ContenidoFila): boolean {
  return JSON.stringify(contenidoDeFila(a)) === JSON.stringify(contenidoDeFila(b))
}

export function mismoEstado(a: EstadoFila, b: EstadoFila): boolean {
  return JSON.stringify(estadoDeFila(a)) === JSON.stringify(estadoDeFila(b))
}

export function filaParcial(fila: FilaCotizacion, grupos: Grupos, userId: string): Record<string, unknown> {
  const out: Record<string, unknown> = { id: fila.id, user_id: userId, device_id: fila.device_id }
  if (grupos.contenido) {
    for (const c of COLUMNAS_CONTENIDO) out[c] = fila[c]
    out.hlc = fila.hlc
    out.deleted = fila.deleted
  }
  if (grupos.estado) {
    for (const c of COLUMNAS_ESTADO) out[c] = fila[c]
    out.estado_hlc = fila.estado_hlc
  }
  return out
}

const texto = (v: unknown): string => (typeof v === 'string' ? v : '')
const numero = (v: unknown): number => (typeof v === 'number' && Number.isFinite(v) ? v : 0)

/** Una fila tal como llega del servidor, con vacíos donde falte algo. null si no tiene id. */
export function normalizarFila(x: unknown): FilaCotizacion | null {
  if (!x || typeof x !== 'object') return null
  const o = x as Record<string, unknown>
  const id = texto(o.id)
  if (!id) return null
  const base = filaVacia(id)
  const lineas = Array.isArray(o.lineas)
    ? o.lineas.map((l) => {
        const r = (l ?? {}) as Record<string, unknown>
        return { seccion: texto(r.seccion), concepto: texto(r.concepto), cantidad: numero(r.cantidad), valorUnitario: numero(r.valorUnitario) }
      })
    : []
  const dt = texto(o.descuento_tipo)
  return {
    ...base,
    numero: texto(o.numero),
    tipo: o.tipo === 'informe' ? 'informe' : 'cotizacion',
    forma: o.forma === 'proyecto' ? 'proyecto' : 'servicio',
    cliente: texto(o.cliente),
    equipo: texto(o.equipo),
    lineas,
    descuento_tipo: dt === 'porcentaje' || dt === 'monto' ? dt : '',
    descuento_valor: numero(o.descuento_valor),
    abono_minor: numero(o.abono_minor),
    fecha_emision: texto(o.fecha_emision),
    fecha_ingreso: texto(o.fecha_ingreso),
    origen_id: texto(o.origen_id),
    hecho_en: o.hecho_en === 'telefono' ? 'telefono' : 'pc',
    hlc: texto(o.hlc),
    estado: (texto(o.estado) || 'borrador') as Estado,
    fecha_entrega: texto(o.fecha_entrega),
    fecha_pago: texto(o.fecha_pago),
    motivo_rechazo: texto(o.motivo_rechazo),
    estado_hlc: texto(o.estado_hlc),
    device_id: texto(o.device_id),
    deleted: o.deleted === true,
    ...(typeof o.updated_at === 'string' ? { updated_at: o.updated_at } : {})
  }
}
```

`src/nube/esencial.ts`:

```ts
/**
 * CONTRATO — De un Documento a su fila esencial, y de vuelta para un borrador
 * del teléfono.
 *
 * - `contenidoDe`: número ('' si es borrador), tipo, forma, nombre del
 *   cliente, equipo (en proyecto, el título), líneas con su sección, descuento,
 *   abono, emisión ('' sin número), ingreso del equipo, origen y quién lo hizo.
 * - `estadoDe`: estado, entrega (solo entregado o pagado), fecha del último
 *   evento "Pagado" (solo si está pagado) y motivo (solo si rechazada).
 * - `pasoHacia`: la transición que lleva el documento al estado que trae la
 *   fila, solo si es de las que hace el teléfono. Cobrar usa la fecha de pago
 *   de la fila; aceptar y rechazar, `hoy`.
 * - `borradorDeFila`: sobre un documento en blanco pone id, cliente, equipo,
 *   ingreso, líneas (una categoría por sección, en el orden en que aparecen),
 *   descuento y abono, y marca el historial con CREADO_EN_TELEFONO.
 *
 * Nunca sube resumen, notas, forma de pago, garantía, cláusulas, datos del
 * cliente, fotos ni firma.
 *
 * Dependencias permitidas: ./fila, ../renderer/src/dominio/*.
 */
import type { ContenidoFila, EstadoFila, FilaCotizacion } from './fila'
import type { Documento } from '../renderer/src/dominio/tipos'
import type { Accion, DatosAccion } from '../renderer/src/dominio/estados'
import { totalLinea } from '../renderer/src/dominio/totales'
import { redondearCentavos } from '../renderer/src/dominio/dinero'
import { CREADO_EN_TELEFONO } from '../renderer/src/dominio/indice'

export function contenidoDe(doc: Documento, hechoEn: 'pc' | 'telefono'): ContenidoFila {
  return {
    numero: doc.numero ?? '',
    tipo: doc.tipo,
    forma: doc.forma,
    cliente: doc.clienteCongelado.nombre,
    equipo: doc.forma === 'proyecto' ? (doc.proyecto?.titulo ?? '') : (doc.equipo?.descripcion ?? ''),
    lineas: doc.categorias.flatMap((c) =>
      c.lineas.map((l) => ({ seccion: c.nombre, concepto: l.concepto, cantidad: l.cantidad, valorUnitario: l.valorUnitario }))
    ),
    descuento_tipo: doc.descuento?.tipo ?? '',
    descuento_valor: doc.descuento?.valor ?? 0,
    abono_minor: doc.abono,
    fecha_emision: doc.numero ? doc.fechaEmision : '',
    fecha_ingreso: doc.equipo?.fechaIngreso ?? '',
    origen_id: doc.origenId ?? '',
    hecho_en: hechoEn
  }
}

export function estadoDe(doc: Documento): EstadoFila {
  let fechaPago = ''
  if (doc.estado === 'pagado') {
    for (const e of doc.historial) if (e.detalle.startsWith('Pagado')) fechaPago = e.fecha
  }
  const entregado = doc.estado === 'entregado' || doc.estado === 'pagado'
  return {
    estado: doc.estado,
    fecha_entrega: entregado ? (doc.fechaEntrega ?? '') : '',
    fecha_pago: fechaPago,
    motivo_rechazo: doc.estado === 'rechazada' ? (doc.motivoRechazo ?? '') : ''
  }
}

export function pasoHacia(doc: Documento, remoto: EstadoFila, hoy: string): { accion: Accion; datos: DatosAccion } | null {
  if (doc.tipo === 'cotizacion' && doc.estado === 'enviada') {
    if (remoto.estado === 'aceptada') return { accion: 'aceptar', datos: { hoy } }
    if (remoto.estado === 'rechazada' && remoto.motivo_rechazo.trim() !== '') {
      return { accion: 'rechazar', datos: { hoy, motivo: remoto.motivo_rechazo } }
    }
  }
  if (doc.tipo === 'informe' && doc.estado === 'entregado' && remoto.estado === 'pagado') {
    return { accion: 'cobrar', datos: { hoy: remoto.fecha_pago || hoy } }
  }
  return null
}

export function borradorDeFila(fila: FilaCotizacion, enBlanco: Documento): Documento {
  const secciones: string[] = []
  for (const l of fila.lineas) if (!secciones.includes(l.seccion)) secciones.push(l.seccion)
  const doc: Documento = {
    ...enBlanco,
    id: fila.id,
    clienteCongelado: { ...enBlanco.clienteCongelado, nombre: fila.cliente },
    categorias: secciones.map((nombre, i) => ({
      id: `${fila.id}-c${i}`,
      nombre,
      lineas: fila.lineas
        .filter((l) => l.seccion === nombre)
        .map((l, j) => ({ id: `${fila.id}-c${i}-l${j}`, concepto: l.concepto, cantidad: l.cantidad, valorUnitario: l.valorUnitario }))
    })),
    descuento: fila.descuento_tipo ? { tipo: fila.descuento_tipo, valor: fila.descuento_valor } : null,
    abono: fila.abono_minor,
    historial: [{ fecha: enBlanco.fechaEmision, tipo: 'creado', detalle: CREADO_EN_TELEFONO }]
  }
  if (doc.equipo) doc.equipo = { ...doc.equipo, descripcion: fila.equipo, fechaIngreso: fila.fecha_ingreso || doc.equipo.fechaIngreso }
  if (doc.proyecto) doc.proyecto = { ...doc.proyecto, titulo: fila.equipo }
  return doc
}

export function totalesDeFila(fila: FilaCotizacion): { subtotal: number; base: number; saldo: number } {
  let subtotal = 0
  for (const l of fila.lineas) subtotal += totalLinea({ id: '', concepto: l.concepto, cantidad: l.cantidad, valorUnitario: l.valorUnitario })
  const descuento =
    fila.descuento_tipo === 'monto' ? fila.descuento_valor
    : fila.descuento_tipo === 'porcentaje' ? redondearCentavos((subtotal * fila.descuento_valor) / 10000)
    : 0
  const base = subtotal - descuento
  return { subtotal, base, saldo: base - fila.abono_minor }
}
```

`dominio/tipos.ts`: agregar a `EntradaIndice`:

```ts
  /** Borrador que llegó del teléfono y la PC todavía no tomó. */
  delTelefono?: boolean
```

`dominio/indice.ts`:

```ts
/** Detalle del evento de creación de un borrador que llegó del teléfono. */
export const CREADO_EN_TELEFONO = 'Creado en el teléfono'
```

En `entradaDeDocumento`, agregar `delTelefono: documento.estado === 'borrador' && documento.historial[0]?.detalle === CREADO_EN_TELEFONO`. Para no cambiar las igualdades de las pruebas existentes, el campo va solo cuando es `true`:

```ts
...(documento.estado === 'borrador' && documento.historial[0]?.detalle === CREADO_EN_TELEFONO ? { delTelefono: true } : {})
```

- [ ] **Step 4: Correr las pruebas.** Ejecutar `npm test`. Se espera que todo pase, incluidas las pruebas existentes.

- [ ] **Step 5: Commit.** Ejecutar `git add src/nube src/renderer/src/dominio` y commitear con el mensaje `nube: la fila esencial, el reloj y lo que llega del telefono`.

### Tarea C2: servidor y sesión por `fetch`

**Files:**
- Create: `src/nube/servidor.ts` y `src/nube/servidor.test.ts`.

**Interfaces:**
- Consumes: `normalizarFila` y `FilaCotizacion` (C1).
- Produces:
  - `interface ConfigSupabase { url: string; clave: string }`
  - `interface Sesion { accessToken: string; refreshToken: string; userId: string; correo: string }`
  - `class ErrorNube extends Error { status: number }`
  - `leerConfigSupabase(texto: string): ConfigSupabase`, que lanza `Error` con un mensaje en español.
  - `entrar(cfg, correo, clave, f?): Promise<Sesion>`
  - `renovar(cfg, refreshToken, f?): Promise<Sesion>`
  - `interface Servidor { bajar(cursor: string): Promise<FilaCotizacion[]>; subir(filas: Record<string, unknown>[]): Promise<void> }`
  - `crearServidor(cfg, sesion: () => Sesion, alRenovar: (s: Sesion) => void, f?): Servidor`
  - `const PAGINA = 200`

- [ ] **Step 1: Pruebas que fallan.** Crear `src/nube/servidor.test.ts`:

```ts
import { describe, it, expect, vi } from 'vitest'
import { leerConfigSupabase, entrar, crearServidor, ErrorNube, type Sesion } from './servidor'

const cfg = { url: 'https://x.supabase.co', clave: 'pub' }
const sesion: Sesion = { accessToken: 'A', refreshToken: 'R', userId: 'u1', correo: 'd@x' }
const respuesta = (status: number, cuerpo: unknown) =>
  new Response(cuerpo === undefined ? null : JSON.stringify(cuerpo), { status, headers: { 'Content-Type': 'application/json' } })

describe('supabase.json', () => {
  it('lee url y anon_key aunque venga con BOM', () => {
    expect(leerConfigSupabase('\uFEFF{"url": "https://x.supabase.co/", "anon_key": "pub"}')).toEqual(cfg)
  })
  it('explica qué falta', () => {
    expect(() => leerConfigSupabase('{"url": ""}')).toThrow(/url.*anon_key|anon_key/)
    expect(() => leerConfigSupabase('no es json')).toThrow(/supabase.json/)
  })
})

describe('sesión', () => {
  it('entra con correo y contraseña', async () => {
    const f = vi.fn().mockResolvedValue(respuesta(200, { access_token: 'A', refresh_token: 'R', user: { id: 'u1', email: 'd@x' } }))
    expect(await entrar(cfg, 'd@x', 'clave', f)).toEqual(sesion)
    const [url, init] = f.mock.calls[0]
    expect(url).toBe('https://x.supabase.co/auth/v1/token?grant_type=password')
    expect(init.headers.apikey).toBe('pub')
    expect(JSON.parse(init.body)).toEqual({ email: 'd@x', password: 'clave' })
  })
  it('una contraseña mala da el mensaje del servidor', async () => {
    const f = vi.fn().mockResolvedValue(respuesta(400, { error_description: 'Invalid login credentials' }))
    await expect(entrar(cfg, 'd@x', 'mal', f)).rejects.toThrow('Invalid login credentials')
  })
})

describe('servidor', () => {
  it('baja desde el cursor, en orden, de a 200', async () => {
    const f = vi.fn().mockResolvedValue(respuesta(200, [{ id: 'a', lineas: null, updated_at: 'T1' }]))
    const filas = await crearServidor(cfg, () => sesion, () => {}, f).bajar('2026-09-27T10:00:00+00:00')
    expect(filas[0].id).toBe('a')
    expect(filas[0].lineas).toEqual([])
    const url = String(f.mock.calls[0][0])
    expect(url).toContain('/rest/v1/v2_quotes?select=*&user_id=eq.u1&order=updated_at.asc&limit=200')
    expect(url).toContain('&updated_at=gt.2026-09-27T10%3A00%3A00%2B00%3A00')
    expect(f.mock.calls[0][1].headers.Authorization).toBe('Bearer A')
  })
  it('sube con upsert, un pedido por forma de fila', async () => {
    const f = vi.fn().mockResolvedValue(respuesta(201, undefined))
    await crearServidor(cfg, () => sesion, () => {}, f).subir([
      { id: 'a', estado: 'pagado' }, { id: 'b', estado: 'aceptada' }, { id: 'c', cliente: 'Ana' }
    ])
    expect(f).toHaveBeenCalledTimes(2)
    const [url, init] = f.mock.calls[0]
    expect(url).toBe('https://x.supabase.co/rest/v1/v2_quotes?on_conflict=id')
    expect(init.method).toBe('POST')
    expect(init.headers.Prefer).toBe('resolution=merge-duplicates,return=minimal')
    expect(JSON.parse(init.body)).toHaveLength(2)
  })
  it('un token vencido se renueva una vez y se reintenta', async () => {
    const renovada = { ...sesion, accessToken: 'B', refreshToken: 'R2' }
    let actual = sesion
    const f = vi.fn()
      .mockResolvedValueOnce(respuesta(401, { message: 'JWT expired' }))
      .mockResolvedValueOnce(respuesta(200, { access_token: 'B', refresh_token: 'R2', user: { id: 'u1', email: 'd@x' } }))
      .mockResolvedValueOnce(respuesta(200, []))
    const alRenovar = vi.fn((s: Sesion) => { actual = s })
    await crearServidor(cfg, () => actual, alRenovar, f).bajar('')
    expect(alRenovar).toHaveBeenCalledWith(renovada)
    expect(f.mock.calls[2][1].headers.Authorization).toBe('Bearer B')
  })
  it('sin red lanza ErrorNube con status 0', async () => {
    const f = vi.fn().mockRejectedValue(new TypeError('Failed to fetch'))
    const e = await crearServidor(cfg, () => sesion, () => {}, f).bajar('').catch((x) => x)
    expect(e).toBeInstanceOf(ErrorNube)
    expect(e.status).toBe(0)
  })
  it('un error del servidor lanza con su mensaje', async () => {
    const f = vi.fn().mockResolvedValue(respuesta(404, { message: 'relation "v2_quotes" does not exist' }))
    await expect(crearServidor(cfg, () => sesion, () => {}, f).subir([{ id: 'a' }])).rejects.toThrow(/v2_quotes/)
  })
})
```

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/nube/servidor.test.ts`. Se espera FAIL: no existe el módulo.

- [ ] **Step 3: Implementación.** Crear `src/nube/servidor.ts`:

```ts
/**
 * CONTRATO — Hablar con Supabase por fetch: la sesión y la tabla v2_quotes.
 *
 * Sin bibliotecas: los mismos tres pedidos que ya hace Finanzas (token con
 * contraseña, token con refresh, y PostgREST). `fetch` se inyecta para poder
 * probarlo; por defecto es el global, que existe en Electron y en el WebView
 * del teléfono.
 *
 * - Un 401 renueva la sesión UNA vez con el refresh token y reintenta; si la
 *   renovación falla, lanza.
 * - Sin red, lanza ErrorNube con status 0. Con un error del servidor, lanza
 *   ErrorNube con el status y el mensaje que trae el cuerpo.
 * - `subir` agrupa por forma de fila (las mismas claves): PostgREST exige que
 *   todos los objetos de un pedido tengan las mismas columnas.
 *
 * Dependencias permitidas: ./fila.
 */
import { normalizarFila, type FilaCotizacion } from './fila'

export interface ConfigSupabase { url: string; clave: string }
export interface Sesion { accessToken: string; refreshToken: string; userId: string; correo: string }
export interface Servidor {
  bajar(cursor: string): Promise<FilaCotizacion[]>
  subir(filas: Record<string, unknown>[]): Promise<void>
}
type Fetch = (url: string, init?: RequestInit) => Promise<Response>

export const PAGINA = 200

export class ErrorNube extends Error {
  constructor(mensaje: string, readonly status: number) {
    super(mensaje)
    this.name = 'ErrorNube'
  }
}

export function leerConfigSupabase(texto: string): ConfigSupabase {
  let o: Record<string, unknown>
  try {
    o = JSON.parse(texto.replace(/^\uFEFF/, ''))
  } catch {
    throw new Error('supabase.json no se puede leer.')
  }
  const url = typeof o.url === 'string' ? o.url.trim().replace(/\/+$/, '') : ''
  const clave = typeof o.anon_key === 'string' ? o.anon_key.trim() : ''
  if (!url || !clave) throw new Error('A supabase.json le falta url o anon_key.')
  return { url, clave }
}

async function mensajeDe(r: Response): Promise<string> {
  try {
    const o = (await r.json()) as Record<string, unknown>
    for (const k of ['error_description', 'msg', 'message', 'error', 'hint']) {
      if (typeof o[k] === 'string' && o[k]) return o[k] as string
    }
  } catch {
    // Sin cuerpo JSON: queda el código.
  }
  return `HTTP ${r.status}`
}

async function pedir(f: Fetch, url: string, init: RequestInit): Promise<Response> {
  try {
    return await f(url, init)
  } catch {
    throw new ErrorNube('Sin conexión con el servidor.', 0)
  }
}

async function token(cfg: ConfigSupabase, tipo: string, cuerpo: object, f: Fetch): Promise<Sesion> {
  const r = await pedir(f, `${cfg.url}/auth/v1/token?grant_type=${tipo}`, {
    method: 'POST',
    headers: { apikey: cfg.clave, 'Content-Type': 'application/json' },
    body: JSON.stringify(cuerpo)
  })
  if (!r.ok) throw new ErrorNube(await mensajeDe(r), r.status)
  const o = (await r.json()) as { access_token: string; refresh_token: string; user: { id: string; email: string } }
  return { accessToken: o.access_token, refreshToken: o.refresh_token, userId: o.user.id, correo: o.user.email }
}

export function entrar(cfg: ConfigSupabase, correo: string, clave: string, f: Fetch = fetch): Promise<Sesion> {
  return token(cfg, 'password', { email: correo, password: clave }, f)
}

export function renovar(cfg: ConfigSupabase, refreshToken: string, f: Fetch = fetch): Promise<Sesion> {
  return token(cfg, 'refresh_token', { refresh_token: refreshToken }, f)
}

export function crearServidor(
  cfg: ConfigSupabase,
  sesion: () => Sesion,
  alRenovar: (s: Sesion) => void,
  f: Fetch = fetch
): Servidor {
  async function conSesion(url: string, init: RequestInit): Promise<Response> {
    const armar = (s: Sesion): RequestInit => ({
      ...init,
      headers: { ...(init.headers as Record<string, string>), apikey: cfg.clave, Authorization: `Bearer ${s.accessToken}` }
    })
    let r = await pedir(f, url, armar(sesion()))
    if (r.status === 401) {
      const nueva = await renovar(cfg, sesion().refreshToken, f)
      alRenovar(nueva)
      r = await pedir(f, url, armar(nueva))
    }
    if (!r.ok) throw new ErrorNube(await mensajeDe(r), r.status)
    return r
  }

  return {
    async bajar(cursor) {
      let url = `${cfg.url}/rest/v1/v2_quotes?select=*&user_id=eq.${sesion().userId}&order=updated_at.asc&limit=${PAGINA}`
      if (cursor) url += `&updated_at=gt.${encodeURIComponent(cursor)}`
      const r = await conSesion(url, { method: 'GET' })
      const filas = (await r.json()) as unknown[]
      return filas.map(normalizarFila).filter((x): x is FilaCotizacion => x !== null)
    },
    async subir(filas) {
      const grupos = new Map<string, Record<string, unknown>[]>()
      for (const fila of filas) {
        const clave = Object.keys(fila).sort().join(',')
        grupos.set(clave, [...(grupos.get(clave) ?? []), fila])
      }
      for (const grupo of grupos.values()) {
        await conSesion(`${cfg.url}/rest/v1/v2_quotes?on_conflict=id`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', Prefer: 'resolution=merge-duplicates,return=minimal' },
          body: JSON.stringify(grupo)
        })
      }
    }
  }
}
```

- [ ] **Step 4: Correr las pruebas.** Ejecutar `npm test`. Se espera que todo pase.
- [ ] **Step 5: Commit.** El mensaje es `nube: sesion y servidor por fetch`.

### Tarea C3: la corrida de la PC y su cola

**Files:**
- Create:
  - `src/main/nube/cola.ts` y `src/main/nube/cola.test.ts`
  - `src/main/nube/sincronizar.ts` y `src/main/nube/sincronizar.test.ts`

**Interfaces:**
- Consumes: C1 y C2.
- Produces, de `cola.ts`:
  - `interface EstadoNubePc { deviceId: string; userId: string; cursor: string; ultimoHlc: string; filas: Record<string, FilaCotizacion>; pendientes: string[]; inicial: boolean }`
  - `estadoVacio(deviceId: string, userId: string): EstadoNubePc`
  - `leerEstado(ruta: string, nuevoId?: () => string): Promise<EstadoNubePc>`
  - `guardarEstado(ruta: string, e: EstadoNubePc): Promise<void>`
  - `encolar(e: EstadoNubePc, id: string): void`, que muta.
- Produces, de `sincronizar.ts`:
  - `interface DocumentosPc { ids(): Promise<string[]>; leer(id): Promise<Documento | null>; escribir(doc): Promise<void>; borrar(id): Promise<void>; enBlanco(tipo, forma, id): Promise<Documento> }`
  - `interface Corrida { subidos: number; bajados: number; cambiados: string[] }`
  - `sincronizarPc(o: { servidor: Servidor; userId: string; estado: EstadoNubePc; docs: DocumentosPc; hoy: string; ahoraMs: () => number; guardar: (e: EstadoNubePc) => Promise<void> }): Promise<Corrida>`. Muta `o.estado` y lo guarda después de bajar y después de subir.

- [ ] **Step 1: Pruebas que fallan.**

`src/main/nube/cola.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { mkdtemp, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { leerEstado, guardarEstado, encolar, estadoVacio } from './cola'

describe('cola de la PC', () => {
  it('sin archivo arranca vacía, con un aparato nuevo', async () => {
    const dir = await mkdtemp(join(tmpdir(), 'nube-'))
    const e = await leerEstado(join(dir, 'nube.json'), () => 'dev-1')
    expect(e).toEqual(estadoVacio('dev-1', ''))
  })
  it('guarda y vuelve igual', async () => {
    const dir = await mkdtemp(join(tmpdir(), 'nube-'))
    const e = estadoVacio('dev-1', 'u1')
    encolar(e, 'a')
    encolar(e, 'a')
    encolar(e, 'b')
    expect(e.pendientes).toEqual(['a', 'b'])
    await guardarEstado(join(dir, 'nube.json'), e)
    expect(await leerEstado(join(dir, 'nube.json'), () => 'otro')).toEqual(e)
  })
  it('un archivo roto no tumba: arranca vacía', async () => {
    const dir = await mkdtemp(join(tmpdir(), 'nube-'))
    await writeFile(join(dir, 'nube.json'), '{ roto', 'utf-8')
    expect((await leerEstado(join(dir, 'nube.json'), () => 'dev-2')).deviceId).toBe('dev-2')
  })
})
```

`src/main/nube/sincronizar.test.ts`. Usa un servidor falso con la misma semántica que PostgREST: el upsert mezcla solo las columnas que llegan y `updated_at` crece solo.

```ts
import { describe, it, expect } from 'vitest'
import { sincronizarPc, type DocumentosPc } from './sincronizar'
import { estadoVacio, type EstadoNubePc } from './cola'
import { filaVacia, type FilaCotizacion } from '../../nube/fila'
import { ErrorNube, type Servidor } from '../../nube/servidor'
import { documentoEnBlanco } from '../../renderer/src/dominio/nuevo'
import { configDeEjemplo } from '../../renderer/src/dominio/ejemplo'
import { aplicarAccion } from '../../renderer/src/dominio/estados'
import type { Documento } from '../../renderer/src/dominio/tipos'

class ServidorFalso implements Servidor {
  filas = new Map<string, FilaCotizacion>()
  reloj = 0
  caido = false
  subidas: Record<string, unknown>[][] = []
  async bajar(cursor: string) {
    if (this.caido) throw new ErrorNube('Sin conexión con el servidor.', 0)
    return [...this.filas.values()].filter((f) => (f.updated_at ?? '') > cursor).sort((a, b) => (a.updated_at! < b.updated_at! ? -1 : 1)).slice(0, 200)
  }
  async subir(filas: Record<string, unknown>[]) {
    if (this.caido) throw new ErrorNube('Sin conexión con el servidor.', 0)
    this.subidas.push(filas)
    for (const p of filas) this.poner(p)
  }
  /** Lo que haría el teléfono: un upsert de las columnas que manda. */
  poner(p: Record<string, unknown>) {
    const { user_id: _u, ...resto } = p
    const previa = this.filas.get(p.id as string) ?? filaVacia(p.id as string)
    this.reloj += 1
    this.filas.set(p.id as string, { ...previa, ...resto, updated_at: `T${String(this.reloj).padStart(6, '0')}` } as FilaCotizacion)
  }
}

class DocsFalsos implements DocumentosPc {
  docs = new Map<string, Documento>()
  async ids() { return [...this.docs.keys()] }
  async leer(id: string) { return this.docs.get(id) ?? null }
  async escribir(d: Documento) { this.docs.set(d.id, d) }
  async borrar(id: string) { this.docs.delete(id) }
  async enBlanco(tipo: Documento['tipo'], forma: Documento['forma'], id: string) {
    return documentoEnBlanco(configDeEjemplo, [], { tipo, forma, hoy: '2026-09-27', nuevoId: () => id })
  }
}

function informeEntregado(id: string): Documento {
  const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'informe', forma: 'servicio', hoy: '2026-09-19', nuevoId: () => id })
  return { ...d, estado: 'entregado', numero: `INF-${id}`, fechaEntrega: '2026-09-19', clienteCongelado: { ...d.clienteCongelado, nombre: 'Ana' },
    categorias: [{ id: 'c', nombre: 'Mano de obra', lineas: [{ id: 'l', concepto: 'Reparación', cantidad: 1, valorUnitario: 3000 }] }] }
}

function preparar() {
  const servidor = new ServidorFalso()
  const docs = new DocsFalsos()
  const estado: EstadoNubePc = estadoVacio('pc-1', 'u1')
  let ms = 1_000_000
  const correr = () => sincronizarPc({ servidor, userId: 'u1', estado, docs, hoy: '2026-09-27', ahoraMs: () => (ms += 1), guardar: async () => {} })
  return { servidor, docs, estado, correr }
}

describe('sincronizar la PC', () => {
  it('la primera conexión sube todos los documentos', async () => {
    const { servidor, docs, correr } = preparar()
    for (let i = 1; i <= 7; i++) await docs.escribir(informeEntregado(`d${i}`))
    const r = await correr()
    expect(r.subidos).toBe(7)
    expect(servidor.filas.size).toBe(7)
    expect(servidor.filas.get('d1')!.cliente).toBe('Ana')
    expect(servidor.subidas[0].every((f) => f.user_id === 'u1')).toBe(true)
    expect((await correr()).subidos).toBe(0)
  })

  it('el teléfono cobra: la PC aplica Pagado con esa fecha y no lo vuelve a subir', async () => {
    const { servidor, docs, correr } = preparar()
    await docs.escribir(informeEntregado('i4'))
    await correr()
    servidor.poner({ id: 'i4', user_id: 'u1', device_id: 'tel', estado: 'pagado', fecha_entrega: '2026-09-19', fecha_pago: '2026-09-21', motivo_rechazo: '', estado_hlc: '9999999999999-00000-tel' })
    const r = await correr()
    expect(r.cambiados).toEqual(['i4'])
    const doc = (await docs.leer('i4'))!
    expect(doc.estado).toBe('pagado')
    expect(doc.historial.at(-1)).toEqual({ fecha: '2026-09-21', tipo: 'estado', detalle: 'Pagado' })
    expect(r.subidos).toBe(0)
  })

  it('un cambio de estado del teléfono no pisa las líneas que la PC corrigió', async () => {
    const { servidor, docs, estado, correr } = preparar()
    const d = documentoEnBlanco(configDeEjemplo, [], { tipo: 'cotizacion', forma: 'servicio', hoy: '2026-09-19', nuevoId: () => 'c1' })
    await docs.escribir({ ...d, estado: 'enviada', numero: 'COT-1' })
    await correr()
    // La PC corrige una línea (la marca la pone ipcNube al guardar) y, en el medio, el teléfono acepta.
    const doc = (await docs.leer('c1'))!
    await docs.escribir({ ...doc, categorias: [{ id: 'x', nombre: 'Mano de obra', lineas: [{ id: 'y', concepto: 'Nueva', cantidad: 1, valorUnitario: 900 }] }] })
    estado.pendientes.push('c1')
    servidor.poner({ id: 'c1', user_id: 'u1', device_id: 'tel', estado: 'aceptada', fecha_entrega: '', fecha_pago: '', motivo_rechazo: '', estado_hlc: '9999999999999-00000-tel' })
    await correr()
    const fila = servidor.filas.get('c1')!
    expect(fila.estado).toBe('aceptada')
    expect(fila.lineas).toEqual([{ seccion: 'Mano de obra', concepto: 'Nueva', cantidad: 1, valorUnitario: 900 }])
    expect((await docs.leer('c1'))!.estado).toBe('aceptada')
  })

  it('choque de estado: si el cambio del teléfono ya no se puede aplicar, la PC sube el suyo', async () => {
    const { servidor, docs, estado, correr } = preparar()
    await docs.escribir(informeEntregado('i9'))
    await correr()
    // La PC corrige (vuelve a borrador) y el teléfono cobra, a la vez.
    await docs.escribir(aplicarAccion((await docs.leer('i9'))!, 'corregir', { hoy: '2026-09-27' }))
    estado.pendientes.push('i9')
    servidor.poner({ id: 'i9', user_id: 'u1', device_id: 'tel', estado: 'pagado', fecha_entrega: '2026-09-19', fecha_pago: '2026-09-27', motivo_rechazo: '', estado_hlc: '9999999999999-00000-tel' })
    await correr()
    expect((await docs.leer('i9'))!.estado).toBe('borrador')
    expect(servidor.filas.get('i9')!.estado).toBe('borrador')
  })

  it('un borrador del teléfono llega como borrador local, y su lápida lo borra', async () => {
    const { servidor, docs, correr } = preparar()
    servidor.poner({ ...filaVacia('t1'), user_id: 'u1', hecho_en: 'telefono', cliente: 'Ana', equipo: 'iPhone', hlc: '9999999999999-00000-tel', estado_hlc: '9999999999999-00000-tel', lineas: [{ seccion: 'Mano de obra', concepto: 'Pantalla', cantidad: 1, valorUnitario: 2500 }] })
    const r = await correr()
    expect(r.cambiados).toEqual(['t1'])
    const d = (await docs.leer('t1'))!
    expect(d.estado).toBe('borrador')
    expect(d.clienteCongelado.nombre).toBe('Ana')
    expect(r.subidos).toBe(0) // llegar no es tomarlo
    servidor.poner({ id: 't1', user_id: 'u1', deleted: true, hlc: '9999999999999-00001-tel' })
    await correr()
    expect(await docs.leer('t1')).toBeNull()
  })

  it('al guardarlo en la PC, la PC lo toma: sube con hecho_en = pc', async () => {
    const { servidor, docs, estado, correr } = preparar()
    servidor.poner({ ...filaVacia('t2'), user_id: 'u1', hecho_en: 'telefono', cliente: 'Luis', hlc: '9999999999999-00000-tel' })
    await correr()
    estado.pendientes.push('t2')
    await correr()
    expect(servidor.filas.get('t2')!.hecho_en).toBe('pc')
  })

  it('borrar en la PC sube la lápida', async () => {
    const { servidor, docs, estado, correr } = preparar()
    await docs.escribir(informeEntregado('b1'))
    await correr()
    await docs.borrar('b1')
    estado.pendientes.push('b1')
    await correr()
    expect(servidor.filas.get('b1')!.deleted).toBe(true)
  })

  it('sin señal: lo pendiente queda en la cola y sale en la corrida siguiente', async () => {
    const { servidor, docs, estado, correr } = preparar()
    await docs.escribir(informeEntregado('s1'))
    servidor.caido = true
    await expect(correr()).rejects.toThrow('Sin conexión')
    expect(estado.pendientes).toEqual(['s1'])
    servidor.caido = false
    await correr()
    expect(estado.pendientes).toEqual([])
    expect(servidor.filas.has('s1')).toBe(true)
  })
})
```

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/main/nube`. Se espera FAIL: no existen los módulos.

- [ ] **Step 3: Implementación.**

`src/main/nube/cola.ts`:

```ts
/**
 * CONTRATO — Lo que la PC recuerda de la nube, en un JSON con escritura atómica.
 *
 * - `filas`: la última fila que se sabe que tiene el servidor, por documento.
 *   Es la sombra contra la que se decide qué cambió.
 * - `pendientes`: documentos con cambios locales sin subir. Solo salen de la
 *   lista cuando el servidor confirma.
 * - `inicial`: false hasta que la primera corrida encola todo lo que había.
 * - Un archivo ausente o roto da un estado vacío con un aparato nuevo: nunca
 *   tumba el arranque.
 *
 * Dependencias permitidas: node:fs/promises, ../atomico, ../../nube/fila.
 */
import { readFile } from 'node:fs/promises'
import { escribirAtomico } from '../atomico'
import type { FilaCotizacion } from '../../nube/fila'

export interface EstadoNubePc {
  deviceId: string
  userId: string
  cursor: string
  ultimoHlc: string
  filas: Record<string, FilaCotizacion>
  pendientes: string[]
  inicial: boolean
}

export function estadoVacio(deviceId: string, userId: string): EstadoNubePc {
  return { deviceId, userId, cursor: '', ultimoHlc: '', filas: {}, pendientes: [], inicial: false }
}

export async function leerEstado(ruta: string, nuevoId: () => string = () => crypto.randomUUID()): Promise<EstadoNubePc> {
  try {
    const o = JSON.parse(await readFile(ruta, 'utf-8')) as EstadoNubePc
    if (typeof o.deviceId === 'string' && o.deviceId && o.filas && Array.isArray(o.pendientes)) return o
  } catch {
    // Ausente o roto: se empieza de cero.
  }
  return estadoVacio(nuevoId(), '')
}

export async function guardarEstado(ruta: string, e: EstadoNubePc): Promise<void> {
  await escribirAtomico(ruta, JSON.stringify(e))
}

export function encolar(e: EstadoNubePc, id: string): void {
  if (!e.pendientes.includes(id)) e.pendientes.push(id)
}
```

`src/main/nube/sincronizar.ts`:

```ts
/**
 * CONTRATO — Una corrida de la PC: bajar, aplicar, subir.
 *
 * BAJAR, desde el cursor y de a páginas, por cada fila `r`, con `previa` = la
 * sombra que se tenía:
 *  - Hay documento local:
 *    · Es un borrador del teléfono que la PC no tomó (la sombra dice
 *      hecho_en = telefono, no está pendiente y sigue en borrador) y el
 *      contenido remoto es más nuevo: se rehace desde la fila, o se borra si
 *      trae lápida.
 *    · Si no, y el estado remoto es más nuevo y distinto: se aplica con la
 *      misma `aplicarAccion` si es una transición del teléfono (`pasoHacia`),
 *      con la fecha que trae la fila. Si no se puede aplicar, gana la PC: el
 *      paso SUBIR verá su estado distinto de la sombra y lo subirá.
 *  - No hay documento local y no había sombra: si es un borrador vivo del
 *    teléfono, se crea. Cualquier otra fila sin documento se ignora (un
 *    documento borrado en la PC cuya lápida todavía no subió no revive).
 *  - La sombra pasa a ser `r` y el cursor avanza a su updated_at.
 * SUBIR, cada pendiente:
 *  - Sin documento: si la sombra existe y no es lápida, sube la lápida.
 *  - Con documento: sube el grupo de contenido si cambió respecto de la
 *    sombra (o no hay sombra) y el de estado si cambió; cada uno con reloj
 *    nuevo. hecho_en = pc: guardar en la PC es tomarlo.
 *  - Solo con la confirmación del servidor se actualizan las sombras y se
 *    vacían los pendientes procesados. Si falla, lanza y no toca nada.
 * La primera corrida (`inicial` false) encola todos los documentos.
 *
 * Dependencias permitidas: ./cola, ../../nube/*, ../../renderer/src/dominio/*.
 */
import { encolar, type EstadoNubePc } from './cola'
import { contenidoDe, estadoDe, pasoHacia, borradorDeFila } from '../../nube/esencial'
import { filaParcial, filaVacia, mismoContenido, mismoEstado, type FilaCotizacion, type Grupos } from '../../nube/fila'
import { Reloj } from '../../nube/reloj'
import { PAGINA, type Servidor } from '../../nube/servidor'
import { aplicarAccion } from '../../renderer/src/dominio/estados'
import type { Documento, FormaTrabajo, TipoDocumento } from '../../renderer/src/dominio/tipos'

export interface DocumentosPc {
  ids(): Promise<string[]>
  leer(id: string): Promise<Documento | null>
  /** Escribe sin encolar: lo que baja no vuelve a subir. */
  escribir(doc: Documento): Promise<void>
  borrar(id: string): Promise<void>
  enBlanco(tipo: TipoDocumento, forma: FormaTrabajo, id: string): Promise<Documento>
}

export interface Corrida { subidos: number; bajados: number; cambiados: string[] }

export async function sincronizarPc(o: {
  servidor: Servidor
  userId: string
  estado: EstadoNubePc
  docs: DocumentosPc
  hoy: string
  ahoraMs: () => number
  guardar: (e: EstadoNubePc) => Promise<void>
}): Promise<Corrida> {
  const { servidor, userId, estado: e, docs, hoy } = o
  const reloj = new Reloj(e.deviceId, e.ultimoHlc)
  const cambiados: string[] = []

  if (!e.inicial) {
    for (const id of await docs.ids()) encolar(e, id)
    e.inicial = true
    await o.guardar(e)
  }

  let bajados = 0
  for (;;) {
    const pagina = await servidor.bajar(e.cursor)
    for (const r of pagina) {
      reloj.recibir(r.hlc)
      reloj.recibir(r.estado_hlc)
      const previa = e.filas[r.id]
      const local = await docs.leer(r.id)
      if (local) {
        const sinTomar = previa?.hecho_en === 'telefono' && !e.pendientes.includes(r.id) && local.estado === 'borrador'
        if (sinTomar && r.hecho_en === 'telefono' && r.hlc > (previa?.hlc ?? '')) {
          if (r.deleted) await docs.borrar(r.id)
          else await docs.escribir(borradorDeFila(r, await docs.enBlanco(r.tipo, r.forma, r.id)))
          cambiados.push(r.id)
        } else if (r.estado_hlc > (previa?.estado_hlc ?? '') && r.estado !== local.estado) {
          const paso = pasoHacia(local, r, hoy)
          if (paso) {
            await docs.escribir(aplicarAccion(local, paso.accion, paso.datos))
            cambiados.push(r.id)
          }
        }
      } else if (!previa && r.hecho_en === 'telefono' && !r.deleted && r.estado === 'borrador') {
        await docs.escribir(borradorDeFila(r, await docs.enBlanco(r.tipo, r.forma, r.id)))
        cambiados.push(r.id)
      }
      e.filas[r.id] = r
      if (r.updated_at && r.updated_at > e.cursor) e.cursor = r.updated_at
      bajados += 1
    }
    await o.guardar(e)
    if (pagina.length < PAGINA) break
  }

  const lote: Array<{ fila: FilaCotizacion; grupos: Grupos }> = []
  const procesados = [...e.pendientes]
  for (const id of procesados) {
    const previa = e.filas[id]
    const doc = await docs.leer(id)
    if (!doc) {
      if (previa && !previa.deleted) {
        lote.push({ fila: { ...previa, deleted: true, hlc: reloj.ahora(o.ahoraMs()), device_id: e.deviceId }, grupos: { contenido: true, estado: false } })
      }
      continue
    }
    const contenido = contenidoDe(doc, 'pc')
    const estado = estadoDe(doc)
    const grupos: Grupos = {
      contenido: !previa || previa.deleted || !mismoContenido(previa, contenido),
      estado: !previa || !mismoEstado(previa, estado)
    }
    if (!grupos.contenido && !grupos.estado) continue
    const base = previa ?? filaVacia(id)
    lote.push({
      fila: {
        ...base, ...contenido, ...estado, id, deleted: false, device_id: e.deviceId,
        hlc: grupos.contenido ? reloj.ahora(o.ahoraMs()) : base.hlc,
        estado_hlc: grupos.estado ? reloj.ahora(o.ahoraMs()) : base.estado_hlc
      },
      grupos
    })
  }
  if (lote.length > 0) await servidor.subir(lote.map((x) => filaParcial(x.fila, x.grupos, userId)))
  for (const x of lote) e.filas[x.fila.id] = x.fila
  e.pendientes = e.pendientes.filter((id) => !procesados.includes(id))
  e.ultimoHlc = reloj.ultimo()
  await o.guardar(e)
  return { subidos: lote.length, bajados, cambiados }
}
```

- [ ] **Step 4: Correr las pruebas.** Ejecutar `npm test`. Se espera que todo pase.
- [ ] **Step 5: Commit.** El mensaje es `nube: la corrida de la PC y su cola`.

### Tarea C4: cableado en la PC — sesión, IPC, relojes y la sección Nube

**Files:**
- Create: `src/main/nube/sesion.ts` y `src/main/nube/ipcNube.ts`.
- Modify:
  - `src/main/ipcDatos.ts`: exportar `carpetaBase`, `configActual` y `hoyISO`, y llamar a `marcarCambio` o `marcarTodos`.
  - `src/main/index.ts`: `registrarNube()`.
  - `src/preload/tipos.ts` y `src/preload/index.ts`.
  - `src/renderer/src/datos/Datos.tsx` y `Datos.test.tsx`.
  - `src/renderer/src/editor/Editor.tsx` y `editor.css`.
  - `src/renderer/src/listado/Listado.tsx` y `Listado.test.tsx`.

**Interfaces:**
- Consumes: C1, C2 y C3.
- Produces:
  - `interface EstadoNubeVista { configurada: boolean; conectado: boolean; correo: string; pendientes: number; ultima: string; sincronizando: boolean; error: string }`, en `preload/tipos.ts`.
  - `PuenteDake.nube`, con estas funciones:
    - `estado(): Promise<EstadoNubeVista>`
    - `conectar(correo: string, clave: string): Promise<EstadoNubeVista>`
    - `desconectar(): Promise<EstadoNubeVista>`
    - `sincronizar(): Promise<EstadoNubeVista>`
    - `alCambiar(cb: (e: { ids: string[]; vista: EstadoNubeVista }) => void): () => void`
  - Desde `ipcNube.ts`: `registrarNube(): void`, `marcarCambio(id: string): void` y `marcarTodos(): void`.

- [ ] **Step 1: Pruebas que fallan (renderer).** Agregar a `Datos.test.tsx` un `dibujarNube(nube)` que pase las props nuevas, y actualizar el `dibujar` existente con `nube={null}`, `onConectar`, `onDesconectar` y `onSincronizar`:

```tsx
const vista = (o: Partial<EstadoNubeVista> = {}): EstadoNubeVista => ({
  configurada: true, conectado: false, correo: '', pendientes: 0, ultima: '', sincronizando: false, error: '', ...o
})

describe('Datos — nube', () => {
  it('sin conectar pide correo y contraseña, y conectar los manda', () => {
    const { container, onConectar } = dibujar(undefined, vista())
    fireEvent.change(container.querySelector('.datos__correo')!, { target: { value: 'd@x' } })
    fireEvent.change(container.querySelector('.datos__clave')!, { target: { value: 'secreta' } })
    fireEvent.click(screen.getByText('Conectar'))
    expect(onConectar).toHaveBeenCalledWith('d@x', 'secreta')
  })
  it('conectada dice con qué cuenta y cuánto falta subir', () => {
    const { container } = dibujar(undefined, vista({ conectado: true, correo: 'd@x', pendientes: 3, ultima: '2026-09-27T15:04:00' }))
    expect(container.querySelector('.datos__cuenta-estado')!.textContent).toContain('d@x')
    expect(container.querySelector('.datos__cuenta-estado')!.textContent).toContain('3 cambios por subir')
  })
  it('sincronizar ahora y desconectar avisan', () => {
    const { onSincronizar, onDesconectar } = dibujar(undefined, vista({ conectado: true, correo: 'd@x' }))
    fireEvent.click(screen.getByText('Sincronizar ahora'))
    fireEvent.click(screen.getByText('Desconectar'))
    expect(onSincronizar).toHaveBeenCalled()
    expect(onDesconectar).toHaveBeenCalled()
  })
  it('sin supabase.json lo explica y no ofrece conectar', () => {
    const { container } = dibujar(undefined, vista({ configurada: false }))
    expect(container.querySelector('.datos__cuenta-error')!.textContent).toContain('supabase.json')
    expect(screen.queryByText('Conectar')).toBeNull()
  })
  it('muestra el error de la última corrida', () => {
    const { container } = dibujar(undefined, vista({ conectado: true, error: 'Sin conexión con el servidor.' }))
    expect(container.querySelector('.datos__cuenta-error')!.textContent).toBe('Sin conexión con el servidor.')
  })
})
```

`dibujar` pasa a ser `dibujar(carpeta = {…}, nube: EstadoNubeVista | null = null)` y devuelve también `onConectar`, `onDesconectar` y `onSincronizar`.

En `Listado.test.tsx`, agregar: `una entrada con delTelefono muestra la marca "del teléfono"`. Hay que buscar en `.listado__telefono` el texto `del teléfono`, usando el mismo helper de entradas que usa ese archivo.

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/renderer/src/datos src/renderer/src/listado`. Se espera FAIL.

- [ ] **Step 3: Implementación del renderer.**

`preload/tipos.ts`:
- Agregar `EstadoNubeVista`, con el contrato de cada campo:
  - `ultima`: ISO local de la última corrida buena, o '';
  - `error`: el de la última corrida, o '';
  - `pendientes`: cuántos documentos faltan subir.
- Agregar `nube` a `PuenteDake`, con las firmas de Interfaces.

`preload/index.ts`:

```ts
  nube: {
    estado: () => invocar('nube:estado'),
    conectar: (correo, clave) => invocar('nube:conectar', correo, clave),
    desconectar: () => invocar('nube:desconectar'),
    sincronizar: () => invocar('nube:sincronizar'),
    alCambiar: (cb) => {
      const oyente = (_e: unknown, datos: { ids: string[]; vista: EstadoNubeVista }) => cb(datos)
      ipcRenderer.on('nube:cambio', oyente)
      return () => ipcRenderer.removeListener('nube:cambio', oyente)
    }
  }
```

`Datos.tsx`:
- Props nuevas: `nube: EstadoNubeVista | null`, `onConectar(correo, clave)`, `onDesconectar()` y `onSincronizar()`.
- Actualizar el contrato del encabezado.
- La sección nueva va primero, antes de "Carpeta de datos":

```tsx
      <section className="datos__cuenta">
        <h3>Nube</h3>
        <p className="editor__pista">
          Sube lo esencial de cada documento —cliente, equipo, líneas, montos y estado— a la misma
          cuenta de Finanzas, para verlo y marcarlo desde el teléfono. Las descripciones, notas,
          cláusulas, fotos y datos del cliente se quedan en esta PC.
        </p>
        {!nube && <p className="datos__cuenta-estado">Cargando…</p>}
        {nube && !nube.configurada && (
          <p className="datos__cuenta-error">
            No se encuentra la configuración de Supabase de Finanzas (supabase.json). Abre Finanzas y
            conéctalo a la nube primero.
          </p>
        )}
        {nube?.configurada && nube.conectado && (
          <>
            <p className="datos__cuenta-estado">
              Conectado como {nube.correo} ·{' '}
              {nube.pendientes > 0 ? `${nube.pendientes} cambios por subir` : 'todo subido'}
              {nube.ultima && ` · última sincronización ${formatearFechaHora(nube.ultima)}`}
            </p>
            <button className="datos__sincronizar" onClick={onSincronizar} disabled={nube.sincronizando}>
              {nube.sincronizando ? 'Sincronizando…' : 'Sincronizar ahora'}
            </button>
            <button className="datos__desconectar" onClick={onDesconectar}>Desconectar</button>
          </>
        )}
        {nube?.configurada && !nube.conectado && (
          <form className="datos__entrar" onSubmit={(ev) => { ev.preventDefault(); onConectar(correo, clave) }}>
            <input className="datos__correo" type="email" placeholder="Correo" value={correo} onChange={(ev) => setCorreo(ev.target.value)} />
            <input className="datos__clave" type="password" placeholder="Contraseña" value={clave} onChange={(ev) => setClave(ev.target.value)} />
            <button type="submit">Conectar</button>
          </form>
        )}
        {nube?.error && <p className="datos__cuenta-error">{nube.error}</p>}
      </section>
```

Notas sobre `Datos.tsx`:
- `correo` y `clave` son `useState` locales.
- `formatearFechaHora(iso)` es local en el componente y da `dd/mm hh:mm` con `slice`.
- Se agrega `useState` a las dependencias del contrato.

`editor.css`: agregar reglas para `.datos__cuenta-error` (color de aviso, igual que `.editor__aviso`), `.datos__entrar` (flex, gap 8px) y `.listado__telefono` (pastilla chica en el color de marca).

`Editor.tsx`:
- Estado `const [nube, setNube] = useState<EstadoNubeVista | null>(null)`.
- En el efecto de arranque:
  ```ts
  void window.dake.nube.estado().then(setNube)
  const quitar = window.dake.nube.alCambiar(({ ids, vista }) => {
    setNube(vista)
    if (ids.length === 0) return
    void window.dake.documentos.listar().then(setEntradas)
    // Si lo que cambió está abierto, se recarga: cobrar sobre una copia vieja
    // pisaría la fecha que puso el teléfono.
    setDocumento((abierto) => {
      if (abierto && ids.includes(abierto.id)) {
        void window.dake.documentos.abrir(abierto.id).then((d) => d && setDocumento(d))
      }
      return abierto
    })
  })
  return quitar
  ```
  El efecto de arranque devuelve `quitar`.
- Pasar a `<Datos>` estas props:
  - `nube={nube}`
  - `onConectar={(c, k) => void window.dake.nube.conectar(c, k).then(setNube)}`
  - `onDesconectar={() => void window.dake.nube.desconectar().then(setNube)}`
  - `onSincronizar`, que sincroniza y, si trae error, lo muestra también en `setMensaje`:
    ```ts
    onSincronizar={() => void window.dake.nube.sincronizar().then((v) => { setNube(v); if (v.error) setMensaje(v.error) })}
    ```

`Listado.tsx`: al lado del nombre del cliente de cada fila, `{entrada.delTelefono && <span className="listado__telefono">del teléfono</span>}`.

- [ ] **Step 4: Implementación del proceso principal.**

`src/main/nube/sesion.ts`:

```ts
/**
 * CONTRATO — Dónde está la configuración de Supabase y cómo se guarda la sesión.
 *
 * - La URL y la clave pública salen del supabase.json de Finanzas, en
 *   %APPDATA%\DakeLabs\Finanzas DakeLabs. No se copian a ningún lado.
 * - La contraseña no se guarda nunca. Se guarda el refresh token, el id de
 *   usuario y el correo, cifrados con safeStorage, en userData/nube-sesion.bin.
 *   Si el cifrado no está disponible, no se guarda: hay que conectar en cada
 *   arranque.
 *
 * Dependencias permitidas: electron, node:fs/promises, node:path, ../../nube/servidor.
 */
import { app, safeStorage } from 'electron'
import { readFile, rm, writeFile } from 'node:fs/promises'
import { join } from 'node:path'
import { leerConfigSupabase, type ConfigSupabase, type Sesion } from '../../nube/servidor'

export function rutaSupabaseJson(): string {
  return join(app.getPath('appData'), 'DakeLabs', 'Finanzas DakeLabs', 'supabase.json')
}

/** null si no está o no se puede leer. */
export async function leerConfig(): Promise<ConfigSupabase | null> {
  try {
    return leerConfigSupabase(await readFile(rutaSupabaseJson(), 'utf-8'))
  } catch {
    return null
  }
}

const rutaSesion = (): string => join(app.getPath('userData'), 'nube-sesion.bin')

export async function leerSesion(): Promise<Sesion | null> {
  try {
    if (!safeStorage.isEncryptionAvailable()) return null
    const o = JSON.parse(safeStorage.decryptString(await readFile(rutaSesion()))) as Omit<Sesion, 'accessToken'>
    return { ...o, accessToken: '' }
  } catch {
    return null
  }
}

export async function guardarSesion(s: Sesion): Promise<void> {
  if (!safeStorage.isEncryptionAvailable()) return
  const { accessToken: _a, ...resto } = s
  await writeFile(rutaSesion(), safeStorage.encryptString(JSON.stringify(resto)))
}

export async function borrarSesion(): Promise<void> {
  await rm(rutaSesion(), { force: true })
}
```

Con `accessToken: ''`, la primera petición responde 401 y `crearServidor` renueva solo.

`src/main/nube/ipcNube.ts`:

```ts
/**
 * CONTRATO — La nube en el proceso principal: canales, relojes y marcas.
 *
 * - Canales: nube:estado, nube:conectar(correo, clave), nube:desconectar y
 *   nube:sincronizar. Todos devuelven EstadoNubeVista. conectar y sincronizar
 *   no lanzan: el error va en la vista.
 * - Cuándo corre: 3 s después de arrancar si hay sesión; 5 s después de cada
 *   cambio local (se agrupan); cada 5 minutos; y a pedido. Nunca dos corridas
 *   a la vez. Una marca que llega durante una corrida se guarda y dispara otra
 *   al terminar.
 * - Después de cada corrida manda 'nube:cambio' con los documentos que cambió y
 *   la vista, a todas las ventanas.
 * - marcarCambio(id) y marcarTodos() los llama ipcDatos después de cada
 *   escritura que hace el usuario. Lo que escribe la corrida no pasa por ahí.
 * - Si cambia la cuenta (otro user_id), el estado se reinicia con el mismo
 *   aparato: los documentos se vuelven a subir a la cuenta nueva.
 *
 * Dependencias permitidas: electron, node:path, ./cola, ./sincronizar, ./sesion,
 * ../almacen, ../repositorio, ../biblioteca, ../ipcDatos, ../rutas,
 * ../../nube/*, ../../renderer/src/dominio/*, ../../preload/tipos.
 */
import { app, BrowserWindow, ipcMain } from 'electron'
import { join } from 'node:path'
import { encolar, estadoVacio, guardarEstado, leerEstado, type EstadoNubePc } from './cola'
import { sincronizarPc, type DocumentosPc } from './sincronizar'
import { borrarSesion, guardarSesion, leerConfig, leerSesion } from './sesion'
import { crearServidor, entrar, type ConfigSupabase, type Sesion } from '../../nube/servidor'
import { guardarDocumento, leerDocumento, borrarDocumento } from '../almacen'
import { leerIndice, actualizarEntrada, quitarEntrada } from '../repositorio'
import { leerBiblioteca } from '../biblioteca'
import { carpetaBase, configActual, hoyISO } from '../ipcDatos'
import { nombreArchivoDocumento } from '../rutas'
import { entradaDeDocumento } from '../../renderer/src/dominio/indice'
import { documentoEnBlanco } from '../../renderer/src/dominio/nuevo'
import type { EstadoNubeVista } from '../../preload/tipos'

const CAMBIO_MS = 5_000
const PERIODO_MS = 5 * 60_000
const ARRANQUE_MS = 3_000

let cfg: ConfigSupabase | null = null
let sesion: Sesion | null = null
let estado: EstadoNubePc | null = null
let corriendo = false
let otraVez = false
let durante: string[] = []
let ultima = ''
let error = ''
let espera: ReturnType<typeof setTimeout> | null = null

const rutaEstado = (): string => join(app.getPath('userData'), 'nube.json')

function vista(): EstadoNubeVista {
  return {
    configurada: cfg !== null,
    conectado: sesion !== null,
    correo: sesion?.correo ?? '',
    pendientes: (estado?.pendientes.length ?? 0) + durante.length,
    ultima,
    sincronizando: corriendo,
    error
  }
}

function avisar(ids: string[]): void {
  for (const w of BrowserWindow.getAllWindows()) w.webContents.send('nube:cambio', { ids, vista: vista() })
}

const docs: DocumentosPc = {
  async ids() {
    return (await leerIndice(await carpetaBase())).map((e) => e.id)
  },
  async leer(id) {
    const carpeta = await carpetaBase()
    const entrada = (await leerIndice(carpeta)).find((e) => e.id === id)
    if (!entrada) return null
    const r = await leerDocumento(carpeta, nombreArchivoDocumento({ numero: entrada.numero, id }))
    return r.ok ? r.documento : null
  },
  async escribir(doc) {
    const carpeta = await carpetaBase()
    await guardarDocumento(carpeta, doc)
    await actualizarEntrada(carpeta, entradaDeDocumento(doc))
  },
  async borrar(id) {
    const carpeta = await carpetaBase()
    const entrada = (await leerIndice(carpeta)).find((e) => e.id === id)
    if (entrada) await borrarDocumento(carpeta, nombreArchivoDocumento({ numero: entrada.numero, id }))
    await quitarEntrada(carpeta, id)
  },
  async enBlanco(tipo, forma, id) {
    const carpeta = await carpetaBase()
    return documentoEnBlanco(await configActual(), await leerBiblioteca(carpeta), { tipo, forma, hoy: hoyISO(), nuevoId: () => id })
  }
}

async function correr(): Promise<void> {
  if (!cfg || !sesion || !estado) return
  if (corriendo) {
    otraVez = true
    return
  }
  corriendo = true
  avisar([])
  let cambiados: string[] = []
  try {
    const servidor = crearServidor(cfg, () => sesion!, (s) => {
      sesion = s
      void guardarSesion(s)
    })
    const r = await sincronizarPc({
      servidor, userId: sesion.userId, estado, docs, hoy: hoyISO(), ahoraMs: () => Date.now(),
      guardar: (e) => guardarEstado(rutaEstado(), e)
    })
    cambiados = r.cambiados
    ultima = new Date().toISOString()
    error = ''
  } catch (e) {
    error = e instanceof Error ? e.message : String(e)
  } finally {
    corriendo = false
    for (const id of durante) encolar(estado, id)
    durante = []
    await guardarEstado(rutaEstado(), estado).catch(() => {})
    avisar(cambiados)
  }
  if (otraVez) {
    otraVez = false
    void correr()
  }
}

function programar(ms: number): void {
  if (espera) clearTimeout(espera)
  espera = setTimeout(() => {
    espera = null
    void correr()
  }, ms)
}

export function marcarCambio(id: string): void {
  if (!estado) return
  if (corriendo) {
    if (!durante.includes(id)) durante.push(id)
  } else {
    encolar(estado, id)
    void guardarEstado(rutaEstado(), estado).catch(() => {})
  }
  programar(CAMBIO_MS)
}

export function marcarTodos(): void {
  void docs.ids().then((ids) => ids.forEach(marcarCambio))
}

async function cargar(): Promise<void> {
  cfg = await leerConfig()
  estado = await leerEstado(rutaEstado())
  sesion = await leerSesion()
  if (sesion && estado.userId !== sesion.userId) estado = estadoVacio(estado.deviceId, sesion.userId)
}

export function registrarNube(): void {
  const listo = cargar().then(() => {
    if (sesion) programar(ARRANQUE_MS)
  })
  setInterval(() => void correr(), PERIODO_MS)

  ipcMain.handle('nube:estado', async () => {
    await listo
    return vista()
  })

  ipcMain.handle('nube:conectar', async (_e, correo: string, clave: string) => {
    await listo
    cfg = await leerConfig()
    if (!cfg) return vista()
    try {
      sesion = await entrar(cfg, correo.trim(), clave)
      await guardarSesion(sesion)
      if (!estado || estado.userId !== sesion.userId) {
        estado = estadoVacio(estado?.deviceId ?? crypto.randomUUID(), sesion.userId)
      }
      error = ''
      await correr()
    } catch (e) {
      error = e instanceof Error ? e.message : String(e)
    }
    return vista()
  })

  ipcMain.handle('nube:desconectar', async () => {
    await listo
    sesion = null
    error = ''
    await borrarSesion()
    return vista()
  })

  ipcMain.handle('nube:sincronizar', async () => {
    await listo
    await correr()
    return vista()
  })
}
```

`ipcDatos.ts`:
- Exportar `base` como `carpetaBase` y actualizar sus usos internos.
- Exportar también `configActual` y `hoyISO`.
- Importar `marcarCambio` y `marcarTodos` de `./nube/ipcNube`. La importación circular no es un problema, porque solo se usan funciones dentro de handlers.
- Llamar después de cada escritura:
  - `documentos:guardar`, `documentos:accion` y `datos:restaurarCopia`: `marcarCambio(documento.id)`, con el id que corresponda en cada caso.
  - `documentos:emitir`: `marcarCambio(resultado.documento.id)`, solo si `ok`.
  - `documentos:convertir`: `marcarCambio(informe.id)`.
  - `documentos:duplicar`: `marcarCambio(copia.id)`.
  - `documentos:borrar`: `marcarCambio(id)`.
  - `datos:importarRespaldo`: `marcarTodos()`, cuando sale bien.
- Actualizar las dependencias del contrato.

`index.ts`: `registrarNube()` después de `registrarDatos()`.

- [ ] **Step 5: Correr las pruebas y construir.** Ejecutar `npm test` y después `npm run build`. Se espera que todo pase y que la construcción no dé errores de tipos.

- [ ] **Step 6: Humo sin red.** Ejecutar `npm run dev` con `ELECTRON_ENABLE_LOGGING=1` y mirar la consola.
  - Esperado: la app abre y Datos y respaldo muestra la sección Nube sin conectar, con el formulario, porque `supabase.json` existe.
  - No conectar: la cuenta real es de David.
  - Cerrar la ventana de desarrollo.
  - Si hay que verificar el aspecto, sacar una captura con la herramienta de captura de Electron o describir el resultado. No usar teclas globales.

- [ ] **Step 7: Commit.** El mensaje es `Cotizaciones sube lo esencial a la nube y aplica lo que llega del telefono`.

---

## Parte 2b — Cotizaciones en Android

### Tarea C5: el estado del teléfono, sus acciones y su corrida

**Files:**
- Create:
  - `src/nube/fusion.ts` y `src/nube/fusion.test.ts`
  - `src/movil/almacen.ts` y `src/movil/almacen.test.ts`
  - `src/movil/acciones.ts` y `src/movil/acciones.test.ts`
  - `src/movil/sincronizarTelefono.ts` y `src/movil/sincronizarTelefono.test.ts`

**Interfaces:**
- Consumes: C1 y C2.
- Produces:
  - `interface Pendiente { contenido: boolean; estado: boolean }`
  - `fusionar(local: FilaCotizacion | undefined, pendiente: Pendiente | undefined, remota: FilaCotizacion): { fila: FilaCotizacion; pendiente: Pendiente | undefined }`
  - `interface Kv { get(k: string): Promise<string | null>; set(k: string, v: string): Promise<void>; remove(k: string): Promise<void> }`
  - `interface EstadoTelefono { deviceId: string; userId: string; cursor: string; ultimoHlc: string; filas: Record<string, FilaCotizacion>; pendientes: Record<string, Pendiente> }`
  - De `almacen.ts`:
    - `leerTelefono(kv, nuevoId?): Promise<EstadoTelefono>` y `guardarTelefono(kv, e): Promise<void>`, con la clave `cotizaciones.estado`;
    - `leerSesionTelefono(kv): Promise<Sesion | null>`, `guardarSesionTelefono(kv, s)` y `borrarSesionTelefono(kv)`, con la clave `cotizaciones.sesion`.
  - De `acciones.ts`. Todas son puras: devuelven un estado nuevo, o lanzan un `Error` con un mensaje en español si la acción no corresponde.
    - `aceptar(e, id, sello)`
    - `rechazar(e, id, motivo, sello)`
    - `cobrar(e, id, fecha, sello)`
    - `guardarBorrador(e, datos: { id: string; cliente: string; equipo: string; lineas: LineaFila[]; hoy: string }, sello)`
    - `borrarBorrador(e, id, sello)`
    - `puedeEditar(f: FilaCotizacion): boolean`
    - `filasVisibles(e): FilaCotizacion[]`: sin lápidas; primero los borradores, después por número descendente.
  - `sincronizarTelefono(o: { servidor: Servidor; obtener: () => EstadoTelefono; poner: (e: EstadoTelefono) => Promise<void> }): Promise<{ subidos: number; bajados: number }>`

- [ ] **Step 1: Pruebas que fallan.**

`src/nube/fusion.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { fusionar } from './fusion'
import { filaVacia } from './fila'

const local = { ...filaVacia('a'), hecho_en: 'telefono' as const, cliente: 'Local', hlc: '0000000000005-00000-t', estado: 'pagado' as const, estado_hlc: '0000000000005-00000-t' }

describe('fusionar', () => {
  it('sin cambios locales, gana la remota', () => {
    const remota = { ...filaVacia('a'), cliente: 'Remota', updated_at: 'T' }
    expect(fusionar(local, undefined, remota)).toEqual({ fila: remota, pendiente: undefined })
  })
  it('un estado local más nuevo sin subir se queda, y sigue pendiente', () => {
    const remota = { ...filaVacia('a'), cliente: 'PC', hlc: '0000000000009-00000-pc', estado: 'entregado' as const, estado_hlc: '0000000000001-00000-pc' }
    const r = fusionar(local, { contenido: false, estado: true }, remota)
    expect(r.fila.cliente).toBe('PC')
    expect(r.fila.estado).toBe('pagado')
    expect(r.pendiente).toEqual({ contenido: false, estado: true })
  })
  it('si la PC tomó el borrador después, el contenido local pendiente pierde', () => {
    const remota = { ...filaVacia('a'), hecho_en: 'pc' as const, cliente: 'PC', hlc: '0000000000009-00000-pc' }
    const r = fusionar(local, { contenido: true, estado: false }, remota)
    expect(r.fila.cliente).toBe('PC')
    expect(r.fila.hecho_en).toBe('pc')
    expect(r.pendiente).toBeUndefined()
  })
})
```

`src/movil/acciones.test.ts`:

```ts
import { describe, it, expect } from 'vitest'
import { aceptar, rechazar, cobrar, guardarBorrador, borrarBorrador, puedeEditar, filasVisibles } from './acciones'
import type { EstadoTelefono } from './almacen'
import { filaVacia } from '../nube/fila'

function estado(): EstadoTelefono {
  return {
    deviceId: 'tel', userId: 'u', cursor: '', ultimoHlc: '', pendientes: {},
    filas: {
      c1: { ...filaVacia('c1'), numero: 'COT-2026-001', estado: 'enviada' },
      i4: { ...filaVacia('i4'), numero: 'INF-2026-004', tipo: 'informe', estado: 'entregado' },
      pc: { ...filaVacia('pc'), estado: 'borrador' }
    }
  }
}

describe('acciones del teléfono', () => {
  it('cobrar un informe entregado, con fecha, deja el estado pendiente', () => {
    const e = cobrar(estado(), 'i4', '2026-09-21', 'S1')
    expect(e.filas.i4).toMatchObject({ estado: 'pagado', fecha_pago: '2026-09-21', estado_hlc: 'S1', device_id: 'tel' })
    expect(e.pendientes.i4).toEqual({ contenido: false, estado: true })
  })
  it('aceptar y rechazar solo una cotización enviada; rechazar pide motivo', () => {
    expect(aceptar(estado(), 'c1', 'S').filas.c1.estado).toBe('aceptada')
    expect(rechazar(estado(), 'c1', 'muy caro', 'S').filas.c1).toMatchObject({ estado: 'rechazada', motivo_rechazo: 'muy caro' })
    expect(() => rechazar(estado(), 'c1', '  ', 'S')).toThrow(/motivo/)
    expect(() => aceptar(estado(), 'i4', 'S')).toThrow()
    expect(() => cobrar(estado(), 'c1', '2026-09-21', 'S')).toThrow()
  })
  it('un borrador nuevo es del teléfono, con las dos cosas pendientes', () => {
    const e = guardarBorrador(estado(), { id: 't1', cliente: 'Ana', equipo: 'iPhone', hoy: '2026-09-27', lineas: [{ seccion: 'Mano de obra', concepto: 'Pantalla', cantidad: 1, valorUnitario: 2500 }] }, 'S')
    expect(e.filas.t1).toMatchObject({ hecho_en: 'telefono', estado: 'borrador', tipo: 'cotizacion', cliente: 'Ana', fecha_ingreso: '2026-09-27', hlc: 'S', estado_hlc: 'S' })
    expect(e.pendientes.t1).toEqual({ contenido: true, estado: true })
    expect(puedeEditar(e.filas.t1)).toBe(true)
  })
  it('no se edita ni se borra lo que es de la PC', () => {
    expect(puedeEditar(estado().filas.pc)).toBe(false)
    expect(() => guardarBorrador(estado(), { id: 'pc', cliente: 'x', equipo: 'x', hoy: '2026-09-27', lineas: [] }, 'S')).toThrow()
    expect(() => borrarBorrador(estado(), 'pc', 'S')).toThrow()
  })
  it('borrar un borrador propio deja la lápida por subir y lo saca de la lista', () => {
    const e1 = guardarBorrador(estado(), { id: 't1', cliente: 'Ana', equipo: 'iPhone', hoy: '2026-09-27', lineas: [] }, 'S1')
    const e2 = borrarBorrador(e1, 't1', 'S2')
    expect(e2.filas.t1.deleted).toBe(true)
    expect(e2.pendientes.t1.contenido).toBe(true)
    expect(filasVisibles(e2).map((f) => f.id)).not.toContain('t1')
  })
})
```

`src/movil/sincronizarTelefono.test.ts`. Reutiliza la idea del `ServidorFalso` de C3. Para no importar de otro test, se escribe aquí una copia mínima; son 20 líneas.

```ts
import { describe, it, expect } from 'vitest'
import { sincronizarTelefono } from './sincronizarTelefono'
import { cobrar } from './acciones'
import type { EstadoTelefono } from './almacen'
import { filaVacia, type FilaCotizacion } from '../nube/fila'
import { ErrorNube, type Servidor } from '../nube/servidor'

class ServidorFalso implements Servidor {
  filas = new Map<string, FilaCotizacion>()
  n = 0
  caido = false
  async bajar(cursor: string) {
    if (this.caido) throw new ErrorNube('Sin conexión con el servidor.', 0)
    return [...this.filas.values()].filter((f) => (f.updated_at ?? '') > cursor).sort((a, b) => (a.updated_at! < b.updated_at! ? -1 : 1))
  }
  async subir(filas: Record<string, unknown>[]) {
    if (this.caido) throw new ErrorNube('Sin conexión con el servidor.', 0)
    for (const p of filas) this.poner(p)
  }
  poner(p: Record<string, unknown>) {
    const { user_id: _u, ...resto } = p
    this.n += 1
    this.filas.set(p.id as string, { ...(this.filas.get(p.id as string) ?? filaVacia(p.id as string)), ...resto, updated_at: `T${String(this.n).padStart(6, '0')}` } as FilaCotizacion)
  }
}

function montar(servidor: ServidorFalso) {
  let e: EstadoTelefono = { deviceId: 'tel', userId: 'u', cursor: '', ultimoHlc: '', filas: {}, pendientes: {} }
  return {
    get: () => e,
    set: (x: EstadoTelefono) => { e = x },
    correr: () => sincronizarTelefono({ servidor, obtener: () => e, poner: async (x) => { e = x } })
  }
}

describe('sincronizar el teléfono', () => {
  it('baja lo de la PC y sube un cobro solo con el estado', async () => {
    const s = new ServidorFalso()
    s.poner({ ...filaVacia('i4'), tipo: 'informe', numero: 'INF-4', estado: 'entregado', cliente: 'Ana', hlc: '0000000000001-00000-pc', estado_hlc: '0000000000001-00000-pc' })
    const t = montar(s)
    expect((await t.correr()).bajados).toBe(1)
    t.set(cobrar(t.get(), 'i4', '2026-09-21', '0000000000002-00000-tel'))
    // La PC corrige el cliente en el medio: el cobro no lo pisa.
    s.poner({ id: 'i4', cliente: 'Ana María', hlc: '0000000000003-00000-pc' })
    await t.correr()
    expect(s.filas.get('i4')).toMatchObject({ estado: 'pagado', fecha_pago: '2026-09-21', cliente: 'Ana María' })
    expect(t.get().pendientes).toEqual({})
  })
  it('sin señal el cambio queda pendiente y sale después', async () => {
    const s = new ServidorFalso()
    s.poner({ ...filaVacia('i4'), tipo: 'informe', estado: 'entregado', hlc: '0000000000001-00000-pc' })
    const t = montar(s)
    await t.correr()
    t.set(cobrar(t.get(), 'i4', '2026-09-21', '0000000000002-00000-tel'))
    s.caido = true
    await expect(t.correr()).rejects.toThrow('Sin conexión')
    expect(t.get().pendientes.i4).toEqual({ contenido: false, estado: true })
    s.caido = false
    await t.correr()
    expect(s.filas.get('i4')!.estado).toBe('pagado')
    expect(t.get().pendientes).toEqual({})
  })
  it('un cambio hecho durante la subida no se pierde', async () => {
    const s = new ServidorFalso()
    s.poner({ ...filaVacia('i4'), tipo: 'informe', estado: 'entregado', hlc: '0000000000001-00000-pc' })
    s.poner({ ...filaVacia('i5'), tipo: 'informe', estado: 'entregado', hlc: '0000000000001-00000-pc' })
    const t = montar(s)
    await t.correr()
    t.set(cobrar(t.get(), 'i4', '2026-09-21', '0000000000002-00000-tel'))
    const subirOriginal = s.subir.bind(s)
    s.subir = async (filas) => {
      t.set(cobrar(t.get(), 'i5', '2026-09-22', '0000000000003-00000-tel'))
      await subirOriginal(filas)
    }
    await t.correr()
    expect(t.get().pendientes.i5).toEqual({ contenido: false, estado: true })
  })
})
```

`src/movil/almacen.test.ts`: con un `Kv` en memoria (un `Map`), probar tres cosas:
- `leerTelefono` sin nada da un estado vacío con el id del generador;
- guardar y leer da lo mismo;
- la sesión se guarda, se lee y se borra.

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/nube/fusion.test.ts src/movil`. Se espera FAIL.

- [ ] **Step 3: Implementación.**

`src/nube/fusion.ts`:

```ts
/**
 * CONTRATO — Juntar una fila que bajó con la versión local del teléfono.
 *
 * Sin cambios locales pendientes, gana la remota entera. Con cambios
 * pendientes, cada grupo (contenido, estado) se queda con el lado de reloj más
 * nuevo; el grupo local que gana sigue pendiente y el que pierde deja de
 * estarlo.
 *
 * Dependencias permitidas: ./fila.
 */
import { COLUMNAS_CONTENIDO, COLUMNAS_ESTADO, type FilaCotizacion } from './fila'

export interface Pendiente { contenido: boolean; estado: boolean }

export function fusionar(
  local: FilaCotizacion | undefined,
  pendiente: Pendiente | undefined,
  remota: FilaCotizacion
): { fila: FilaCotizacion; pendiente: Pendiente | undefined } {
  if (!local || !pendiente) return { fila: remota, pendiente: undefined }
  const fila: Record<string, unknown> = { ...remota }
  const contenido = pendiente.contenido && local.hlc > remota.hlc
  const estado = pendiente.estado && local.estado_hlc > remota.estado_hlc
  if (contenido) {
    for (const c of COLUMNAS_CONTENIDO) fila[c] = local[c]
    fila.hlc = local.hlc
    fila.deleted = local.deleted
  }
  if (estado) {
    for (const c of COLUMNAS_ESTADO) fila[c] = local[c]
    fila.estado_hlc = local.estado_hlc
  }
  return { fila: fila as unknown as FilaCotizacion, pendiente: contenido || estado ? { contenido, estado } : undefined }
}
```

`src/movil/almacen.ts`: contrato con las claves de `Kv`, JSON en una sola clave y el porqué. Una clave única es atómica, y con cientos de filas de 1 KB sobra.

```ts
import type { FilaCotizacion } from '../nube/fila'
import type { Pendiente } from '../nube/fusion'
import type { Sesion } from '../nube/servidor'

export interface Kv { get(k: string): Promise<string | null>; set(k: string, v: string): Promise<void>; remove(k: string): Promise<void> }
export interface EstadoTelefono {
  deviceId: string; userId: string; cursor: string; ultimoHlc: string
  filas: Record<string, FilaCotizacion>
  pendientes: Record<string, Pendiente>
}
const CLAVE_ESTADO = 'cotizaciones.estado'
const CLAVE_SESION = 'cotizaciones.sesion'

export async function leerTelefono(kv: Kv, nuevoId: () => string = () => crypto.randomUUID()): Promise<EstadoTelefono> {
  try {
    const o = JSON.parse((await kv.get(CLAVE_ESTADO)) ?? '') as EstadoTelefono
    if (o.deviceId && o.filas && o.pendientes) return o
  } catch {
    // Nada guardado todavía.
  }
  return { deviceId: nuevoId(), userId: '', cursor: '', ultimoHlc: '', filas: {}, pendientes: {} }
}
export const guardarTelefono = (kv: Kv, e: EstadoTelefono): Promise<void> => kv.set(CLAVE_ESTADO, JSON.stringify(e))

export async function leerSesionTelefono(kv: Kv): Promise<Sesion | null> {
  try {
    const o = JSON.parse((await kv.get(CLAVE_SESION)) ?? '') as Sesion
    return o.refreshToken ? { ...o, accessToken: '' } : null
  } catch {
    return null
  }
}
export const guardarSesionTelefono = (kv: Kv, s: Sesion): Promise<void> =>
  kv.set(CLAVE_SESION, JSON.stringify({ ...s, accessToken: '' }))
export const borrarSesionTelefono = (kv: Kv): Promise<void> => kv.remove(CLAVE_SESION)
```

`src/movil/acciones.ts`:

```ts
/**
 * CONTRATO — Lo que el teléfono puede hacer. Puras: devuelven un estado nuevo.
 *
 * - aceptar / rechazar: solo una cotización enviada; rechazar exige motivo.
 * - cobrar: solo un informe entregado, con la fecha que se elige.
 * - guardarBorrador: crea o reescribe un borrador propio (hecho_en telefono,
 *   estado borrador); nunca uno de la PC.
 * - borrarBorrador: lápida sobre un borrador propio.
 * Cada cambio sella su grupo con `sello` (un HLC nuevo), pone device_id y
 * marca el grupo pendiente. Lanzan Error en español si no corresponde.
 *
 * Dependencias permitidas: ./almacen, ../nube/fila.
 */
import type { EstadoTelefono } from './almacen'
import { filaVacia, type EstadoFila, type FilaCotizacion, type LineaFila } from '../nube/fila'

function conEstado(e: EstadoTelefono, id: string, cambio: Partial<EstadoFila>, sello: string): EstadoTelefono {
  const fila = { ...e.filas[id], ...cambio, estado_hlc: sello, device_id: e.deviceId }
  const previo = e.pendientes[id] ?? { contenido: false, estado: false }
  return { ...e, filas: { ...e.filas, [id]: fila }, pendientes: { ...e.pendientes, [id]: { ...previo, estado: true } } }
}

function exigir(f: FilaCotizacion | undefined, tipo: FilaCotizacion['tipo'], estado: FilaCotizacion['estado']): void {
  if (!f || f.deleted || f.tipo !== tipo || f.estado !== estado) throw new Error('Esa acción no corresponde a este documento.')
}

export function aceptar(e: EstadoTelefono, id: string, sello: string): EstadoTelefono {
  exigir(e.filas[id], 'cotizacion', 'enviada')
  return conEstado(e, id, { estado: 'aceptada' }, sello)
}

export function rechazar(e: EstadoTelefono, id: string, motivo: string, sello: string): EstadoTelefono {
  exigir(e.filas[id], 'cotizacion', 'enviada')
  if (!motivo.trim()) throw new Error('El motivo es obligatorio para rechazar.')
  return conEstado(e, id, { estado: 'rechazada', motivo_rechazo: motivo.trim() }, sello)
}

export function cobrar(e: EstadoTelefono, id: string, fecha: string, sello: string): EstadoTelefono {
  exigir(e.filas[id], 'informe', 'entregado')
  return conEstado(e, id, { estado: 'pagado', fecha_pago: fecha }, sello)
}

export function puedeEditar(f: FilaCotizacion): boolean {
  return f.hecho_en === 'telefono' && f.estado === 'borrador' && !f.deleted
}

export function guardarBorrador(
  e: EstadoTelefono,
  d: { id: string; cliente: string; equipo: string; lineas: LineaFila[]; hoy: string },
  sello: string
): EstadoTelefono {
  const previa = e.filas[d.id]
  if (previa && !puedeEditar(previa)) throw new Error('Este documento ya es de la PC: se cambia allá.')
  const fila: FilaCotizacion = {
    ...(previa ?? { ...filaVacia(d.id), hecho_en: 'telefono', fecha_ingreso: d.hoy, estado_hlc: sello }),
    cliente: d.cliente.trim(), equipo: d.equipo.trim(), lineas: d.lineas,
    hecho_en: 'telefono', hlc: sello, device_id: e.deviceId
  }
  return {
    ...e,
    filas: { ...e.filas, [d.id]: fila },
    pendientes: { ...e.pendientes, [d.id]: { contenido: true, estado: e.pendientes[d.id]?.estado ?? !previa } }
  }
}

export function borrarBorrador(e: EstadoTelefono, id: string, sello: string): EstadoTelefono {
  const previa = e.filas[id]
  if (!previa || !puedeEditar(previa)) throw new Error('Solo se borran los borradores hechos en el teléfono.')
  return {
    ...e,
    filas: { ...e.filas, [id]: { ...previa, deleted: true, hlc: sello, device_id: e.deviceId } },
    pendientes: { ...e.pendientes, [id]: { contenido: true, estado: e.pendientes[id]?.estado ?? false } }
  }
}

export function filasVisibles(e: EstadoTelefono): FilaCotizacion[] {
  return Object.values(e.filas)
    .filter((f) => !f.deleted)
    .sort((a, b) => {
      if (!a.numero !== !b.numero) return a.numero ? 1 : -1
      return b.numero.localeCompare(a.numero)
    })
}
```

`src/movil/sincronizarTelefono.ts`:

```ts
/**
 * CONTRATO — Una corrida del teléfono: bajar y fusionar, después subir.
 *
 * Lee el estado con `obtener()` después de cada espera, así una acción que
 * el usuario hace mientras corre no se pierde. Al subir, un pendiente solo
 * se limpia si la fila todavía tiene los relojes que se mandaron; si cambió
 * en el medio, queda para la próxima. Si el servidor falla, lanza y no toca
 * los pendientes.
 *
 * Dependencias permitidas: ./almacen, ../nube/*.
 */
import type { EstadoTelefono } from './almacen'
import { fusionar } from '../nube/fusion'
import { filaParcial } from '../nube/fila'
import { Reloj } from '../nube/reloj'
import { PAGINA, type Servidor } from '../nube/servidor'

export async function sincronizarTelefono(o: {
  servidor: Servidor
  obtener: () => EstadoTelefono
  poner: (e: EstadoTelefono) => Promise<void>
}): Promise<{ subidos: number; bajados: number }> {
  let bajados = 0
  for (;;) {
    const pagina = await o.servidor.bajar(o.obtener().cursor)
    const e = o.obtener()
    const reloj = new Reloj(e.deviceId, e.ultimoHlc)
    const filas = { ...e.filas }
    const pendientes = { ...e.pendientes }
    let cursor = e.cursor
    for (const r of pagina) {
      reloj.recibir(r.hlc)
      reloj.recibir(r.estado_hlc)
      const { fila, pendiente } = fusionar(filas[r.id], pendientes[r.id], r)
      filas[r.id] = fila
      if (pendiente) pendientes[r.id] = pendiente
      else delete pendientes[r.id]
      if (r.updated_at && r.updated_at > cursor) cursor = r.updated_at
      bajados += 1
    }
    await o.poner({ ...e, filas, pendientes, cursor, ultimoHlc: reloj.ultimo() })
    if (pagina.length < PAGINA) break
  }

  const antes = o.obtener()
  const enviados = Object.entries(antes.pendientes).map(([id, grupos]) => ({
    id, grupos, hlc: antes.filas[id].hlc, estadoHlc: antes.filas[id].estado_hlc
  }))
  if (enviados.length > 0) {
    await o.servidor.subir(enviados.map((x) => filaParcial(antes.filas[x.id], x.grupos, antes.userId)))
  }
  const despues = o.obtener()
  const pendientes = { ...despues.pendientes }
  for (const x of enviados) {
    const f = despues.filas[x.id]
    if (f && f.hlc === x.hlc && f.estado_hlc === x.estadoHlc) delete pendientes[x.id]
  }
  await o.poner({ ...despues, pendientes })
  return { subidos: enviados.length, bajados }
}
```

- [ ] **Step 4: Correr las pruebas.** Ejecutar `npm test`. Se espera que todo pase.
- [ ] **Step 5: Commit.** El mensaje es `movil: estado, acciones y sincronizacion del telefono`.

### Tarea C6: las cuatro pantallas del teléfono

**Files:**
- Create:
  - `src/movil/index.html`, `main.tsx`, `App.tsx`, `movil.css` y `plataforma.ts`.
  - `src/movil/pantallas/Entrar.tsx`, `Lista.tsx`, `Detalle.tsx` y `Nueva.tsx`, cada una con su `.test.tsx`.
  - `vite.movil.config.ts`.
- Modify: `package.json`: scripts `build:movil` y `dev:movil`.

**Interfaces:**
- Consumes: C5 (acciones, estado, sincronizar), C2 (entrar, renovar, crearServidor), C1 (`totalesDeFila`) y `formatearUSD` de dominio/dinero.
- Las pantallas son componentes controlados:

```ts
// Entrar
interface PropsEntrar { onEntrar(correo: string, clave: string): void; error: string; entrando: boolean }
// Lista
type Filtro = 'cobrar' | 'abiertas' | 'todas'
interface PropsLista {
  filas: FilaCotizacion[]; pendientes: number; sinSenal: boolean
  onAbrir(id: string): void; onNueva(): void; onSalir(): void
}
// Detalle
interface PropsDetalle {
  fila: FilaCotizacion; hoy: string
  onAceptar(): void; onRechazar(motivo: string): void; onCobrar(fecha: string): void
  onEditar(): void; onBorrar(): void; onVolver(): void
}
// Nueva
interface PropsNueva {
  inicial?: FilaCotizacion
  onGuardar(d: { cliente: string; equipo: string; lineas: LineaFila[] }): void
  onCancelar(): void
}
```

Reglas de las pantallas:
- **Lista.**
  - Filtros:
    - *Por cobrar*: `tipo === 'informe' && estado === 'entregado'`. Es el filtro por defecto.
    - *Abiertas*: borrador, enviada, aceptada o entregado.
    - *Todas*.
  - Buscar filtra por cliente o equipo, sin distinguir mayúsculas ni tildes, con `normalize('NFD')` y quitando las marcas.
  - Cada fila muestra:
    - el número, o "Borrador";
    - cliente, equipo, total (`base`) y saldo;
    - la pastilla `.pastilla--<estado>`.
  - Cuando `sinSenal || pendientes > 0`, una banda arriba dice:
    - `Sin señal · N cambios por subir` si no hay señal;
    - `N cambios por subir` si hay señal.
  - Botón "Nueva".
- **Detalle.** Lo esencial, las líneas agrupadas por sección, y total, abono y saldo (`totalesDeFila`). Los botones dependen del estado:
  - cotización enviada: "Aceptada" y "Rechazada". Rechazada abre un campo de motivo con "Confirmar rechazo", deshabilitado si el motivo está vacío.
  - informe entregado: "Pagado" abre una fecha (`<input type="date">` con valor inicial `hoy`) y "Confirmar pago".
  - `puedeEditar(fila)`: "Editar" y "Borrar".
- **Nueva.**
  - Campos: cliente y equipo, más dos grupos, *Mano de obra* y *Repuestos y materiales*. Cada grupo tiene filas de concepto, cantidad y precio unitario (texto con `parsearMonto`), y "+ Línea".
  - El total se calcula con `totalesDeFila` sobre las líneas válidas.
  - "Guardar" está deshabilitado sin cliente o sin ninguna línea válida.
  - Una línea válida tiene concepto y precio.

- [ ] **Step 1: Pruebas que fallan** (testing-library):
  - `Lista.test.tsx`:
    1. Por defecto muestra solo los informes entregados.
    2. "Todas" muestra todo.
    3. Buscar "jose" encuentra "Josué".
    4. Con `sinSenal` y 2 pendientes, la banda dice "Sin señal · 2 cambios por subir".
    5. Tocar una fila llama a `onAbrir` con su id.
  - `Detalle.test.tsx`:
    1. Informe entregado → "Pagado" → la fecha vale `hoy` → "Confirmar pago" llama a `onCobrar('2026-09-27')`.
    2. Cotización enviada → "Rechazada" → "Confirmar rechazo" está deshabilitado sin motivo; con "caro" llama a `onRechazar('caro')`.
    3. Una fila `hecho_en: 'pc'` en borrador no muestra "Editar".
    4. Las líneas se agrupan bajo su sección y se ve el saldo.
  - `Nueva.test.tsx`:
    1. Guardar está deshabilitado al principio.
    2. Con cliente "Ana" y una línea "Pantalla" × 1 × "25,00" en Mano de obra, el total dice `$25.00` (con el formato de `formatearUSD`) y Guardar llama a `onGuardar` con `lineas: [{ seccion: 'Mano de obra', concepto: 'Pantalla', cantidad: 1, valorUnitario: 2500 }]`.
    3. Con `inicial`, los campos vienen llenos.
  - `Entrar.test.tsx`:
    1. Llama a `onEntrar` con el correo y la clave.
    2. Muestra `error`.

  Las pruebas se escriben con la misma forma de `Datos.test.tsx`: `render`, `fireEvent` y `screen.getByText`.

- [ ] **Step 2: Verlas fallar.** Ejecutar `npx vitest run src/movil/pantallas`. Se espera FAIL.

- [ ] **Step 3: Implementar las pantallas.**
  - Componentes con clases BEM `movil-*`: `.movil-lista`, `.movil-fila`, `.movil-banda`, `.movil-detalle`, `.movil-nueva`, `.movil-entrar` y `.pastilla`.
  - `movil.css`:
    - toma los colores de `src/renderer/src/tema.ts` (rojo de marca, fondos claro y oscuro);
    - `@media (prefers-color-scheme: dark)` para el oscuro;
    - importa Inter y Anton de `@fontsource-variable/inter` y `@fontsource/anton`, como el renderer de la PC;
    - botones de al menos 44 px de alto;
    - una sola columna.

- [ ] **Step 4: `plataforma.ts`, `App.tsx`, `main.tsx`, `index.html`.**

`plataforma.ts`:

```ts
/**
 * CONTRATO — Lo que depende del teléfono.
 *
 * - kv: @capacitor/preferences (almacenamiento privado de la app).
 * - config: la URL y la clave pública de Supabase se fijan al construir
 *   (construir-android.ps1 las lee del supabase.json de Finanzas y las pasa
 *   como VITE_SUPABASE_URL y VITE_SUPABASE_CLAVE). Nada de eso va a git.
 */
import { Preferences } from '@capacitor/preferences'
import type { Kv } from './almacen'
import type { ConfigSupabase } from '../nube/servidor'

export const kv: Kv = {
  get: async (key) => (await Preferences.get({ key })).value,
  set: (key, value) => Preferences.set({ key, value }),
  remove: (key) => Preferences.remove({ key })
}

export const configSupabase: ConfigSupabase | null =
  import.meta.env.VITE_SUPABASE_URL && import.meta.env.VITE_SUPABASE_CLAVE
    ? { url: String(import.meta.env.VITE_SUPABASE_URL).replace(/\/+$/, ''), clave: String(import.meta.env.VITE_SUPABASE_CLAVE) }
    : null
```

`App.tsx` es el dueño del estado:
- Al montar:
  - lee el estado y la sesión (`leerTelefono`, `leerSesionTelefono`);
  - sin sesión muestra Entrar;
  - con sesión muestra Lista y sincroniza.
- Guarda el estado con `guardarTelefono` en cada cambio.
- Sella las acciones con `new Reloj(e.deviceId, e.ultimoHlc).ahora(Date.now())` y guarda también `ultimoHlc`.
- Cuándo sincroniza:
  - 2 s después de cada acción;
  - cada 5 minutos;
  - al volver a la app (`document.visibilitychange` → visible).
- `sinSenal = error instanceof ErrorNube && error.status === 0`.
- Salir borra la sesión y vuelve a Entrar; conserva el estado local.
- Si `configSupabase` es null, Entrar muestra "Esta copia se construyó sin la configuración de Supabase."
- Rutas internas con un `useState<Pantalla>`: `'entrar' | 'lista' | 'detalle' | 'nueva'`, con el id abierto.

`hoy` es la fecha local:

```ts
new Date(Date.now() - new Date().getTimezoneOffset() * 60000).toISOString().slice(0, 10)
```

`main.tsx`: `createRoot(document.getElementById('raiz')!).render(<App />)` e `import './movil.css'`.

`index.html`: `<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">`, `<div id="raiz">` y `<script type="module" src="./main.tsx">`.

`vite.movil.config.ts`:

```ts
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve } from 'node:path'

export default defineConfig({
  root: resolve('src/movil'),
  base: './',
  plugins: [react()],
  build: { outDir: resolve('dist-movil'), emptyOutDir: true }
})
```

`package.json` scripts: `"build:movil": "vite build --config vite.movil.config.ts"` y `"dev:movil": "vite --config vite.movil.config.ts"`.

Agregar a `.gitignore`: `dist-movil/` y `apk/`.

Mientras no esté instalado `@capacitor/preferences` (C7), `plataforma.ts` no compila. Por eso esta tarea instala primero las dependencias de Capacitor:
- `npm i @capacitor/core@^6 @capacitor/preferences@^6`
- `npm i -D @capacitor/cli@^6 @capacitor/android@^6`

- [ ] **Step 5: Correr las pruebas y construir.** Ejecutar `npm test` y `npm run build:movil`. Se espera que todo pase y que se genere `dist-movil/index.html`.
- [ ] **Step 6: Humo en el navegador.** Servir `dist-movil` con `npx vite preview --config vite.movil.config.ts` y abrirlo con Chrome, a ancho de teléfono.
  - Esperado: se ve la pantalla Entrar. Como no hay configuración de Supabase, sale el aviso de "sin configuración".
  - Hacer una captura. No entrar con la cuenta de David.
- [ ] **Step 7: Commit.** El mensaje es `movil: las cuatro pantallas de Cotizaciones para el telefono`.

### Tarea C7: Capacitor y el APK

**Files:**
- Create: `capacitor.config.json`, `construir-android.ps1` y `android/`, que genera `npx cap add android`.
- Modify: `.gitignore`.

- [ ] **Step 1: Configuración.** Crear `capacitor.config.json`:

```json
{
  "appId": "com.dakelabs.cotizaciones",
  "appName": "Cotizaciones DakeLabs",
  "webDir": "dist-movil",
  "android": { "allowMixedContent": false }
}
```

- [ ] **Step 2: Plataforma.** Ejecutar `npm run build:movil`, luego `npx cap add android` y luego `npx cap sync android`. Se espera que se cree `android/` con el proyecto Gradle.

- [ ] **Step 3: Ícono.**
  - Generar los `ic_launcher.png` e `ic_launcher_round.png` de cada `android/app/src/main/res/mipmap-*` desde `build/icon.png`, con `System.Drawing` en PowerShell. Los tamaños son 48, 72, 96, 144 y 192.
  - Borrar los `ic_launcher_foreground` o `mipmap-anydpi-v26` que apunten al ícono de Capacitor, para que se use el PNG.

- [ ] **Step 4: El guion.** Crear `construir-android.ps1`:

```powershell
# construir-android.ps1 — arma el APK de Cotizaciones para el teléfono.
#
# Rutas escritas arriba, igual que el de Finanzas: el guion no depende del
# entorno. La URL y la clave pública de Supabase se leen del supabase.json de
# Finanzas y se pasan a Vite; no se escriben en ningún archivo del repo.
#
#   .\construir-android.ps1     compila e imprime dónde quedó el APK

$ErrorActionPreference = "Stop"

$Jdk = "C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot"
$Sdk = "C:\Android\Sdk"
$Supa = Join-Path $env:APPDATA "DakeLabs\Finanzas DakeLabs\supabase.json"

foreach ($ruta in @($Jdk, $Sdk, $Supa)) {
    if (-not (Test-Path $ruta)) { Write-Error "No está: $ruta" }
}

$env:JAVA_HOME = $Jdk
$env:ANDROID_HOME = $Sdk
$env:ANDROID_SDK_ROOT = $Sdk
$env:PATH = "$Jdk\bin;$env:PATH"

$cfg = Get-Content $Supa -Raw -Encoding UTF8 | ConvertFrom-Json
$env:VITE_SUPABASE_URL = $cfg.url
$env:VITE_SUPABASE_CLAVE = $cfg.anon_key

$raiz = $PSScriptRoot
Push-Location $raiz
try {
    Write-Output "== Web =="
    npm run build:movil
    if ($LASTEXITCODE -ne 0) { throw "Falló vite build" }

    Write-Output "== Capacitor =="
    npx cap sync android
    if ($LASTEXITCODE -ne 0) { throw "Falló cap sync" }

    Write-Output "== Gradle =="
    Push-Location (Join-Path $raiz "android")
    try {
        .\gradlew.bat assembleDebug
        if ($LASTEXITCODE -ne 0) { throw "Falló gradle" }
    } finally { Pop-Location }

    $apk = Join-Path $raiz "android\app\build\outputs\apk\debug\app-debug.apk"
    New-Item -ItemType Directory -Force (Join-Path $raiz "apk") | Out-Null
    $destino = Join-Path $raiz "apk\CotizacionesDakeLabs.apk"
    Copy-Item $apk $destino -Force
    Write-Output "APK: $destino"
} finally {
    Pop-Location
}
```

- [ ] **Step 5: Construir.** Ejecutar `powershell -NoProfile -File .\construir-android.ps1`. Se espera la línea `APK: …\apk\CotizacionesDakeLabs.apk`.
  - Si Gradle necesita bajar dependencias, bajan solas.
  - Si falla por la versión del SDK, instalar con `sdkmanager` la plataforma que pide (`platforms;android-34`) y reintentar.
  - Verificar que el bundle no filtró la clave a git: `git grep -n "sb_publishable" -- . ':!node_modules'` no devuelve nada.

- [ ] **Step 6: Commit.** Agregar `capacitor.config.json`, `construir-android.ps1`, `android` y `.gitignore`. El mensaje es `movil: Capacitor y el guion del APK`. No se commitean `apk/` ni `dist-movil/`.

---

## Cierre

- [ ] **Finanzas release.** Ejecutar `.\compilar.ps1 release`. Si Finanzas está abierta, pedirle a David que la cierre desde la bandeja. No se mata el proceso sin avisar.
- [ ] **Memoria.** Actualizar `cotizaciones-en-finanzas.md` con el estado:
  - 2a y 2b hechas en la rama `nube` de Cotizaciones;
  - SQL por correr;
  - APK en `dakelabsfactura\apk\`.
- [ ] **Revisión final** de las dos ramas, contra el spec y la Review Focus.
