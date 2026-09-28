# Cotizaciones en la base de datos (etapas 2a y 2b)

Fecha: 2026-09-27 · Repos: `dakelabsfactura` (Cotizaciones) y
`finanzas-banco-pruebas` (Finanzas)

## Para qué

David quiere trabajar sus cotizaciones también desde el celular. Lo que haga
en la PC tiene que aparecer en el teléfono, pero **solo con los datos
esenciales**, para ahorrar espacio en la base: las descripciones, las
cláusulas y las notas pesan y no suben. Desde el teléfono tiene que poder:

- ver sus cotizaciones e informes;
- marcar estados como "pagado";
- crear una cotización rápida que después completa en la PC.

Finanzas deja de leer la carpeta de archivos y lee lo mismo desde la base.

Criterios de éxito:

- Un informe emitido en la PC aparece en el teléfono sin que David haga nada.
- Lo que David marca como pagado en el teléfono queda pagado en el documento
  de la PC, con esa fecha en el historial, y Finanzas lo ve como cobrado.
- Una cotización rápida hecha en el teléfono aparece en la PC como borrador.
- Nada se pierde por falta de señal.
- Cada documento ocupa alrededor de 1 KB en la base.

## Decisiones de David

| Pregunta | Decisión |
|---|---|
| ¿El documento completo en los dos aparatos? | No. En la base va solo lo esencial; el documento completo vive en la PC. |
| ¿Se crean documentos en el teléfono? | Sí, una cotización rápida que la PC completa. |
| ¿Sin internet? | Sí: se guarda en el aparato y se sube después. |
| ¿Fotos? | Quedan en el aparato donde se tomaron; no suben. |
| ¿Dónde vive en el teléfono? | Una app aparte de Cotizaciones, no una pestaña de Finanzas. |
| ¿En la PC? | Cotizaciones y Finanzas siguen siendo dos apps aparte. |
| ¿Todo trabajo pasa por Cotizaciones? | No: a veces hay trabajos sin factura. Finanzas conserva una forma rápida (etapa 2c). |

La serie aparte para el teléfono (COT-2026-T001), que se había elegido antes,
**ya no hace falta**: el teléfono solo crea borradores, y el número lo pone
siempre la PC al emitir.

## Etapas

| Etapa | Qué | Este documento |
|---|---|---|
| **2a** | Tabla `v2_quotes`. Cotizaciones de PC con sesión, subida de lo esencial, cola sin conexión y aplicación de lo que llega del teléfono. Finanzas lee de la base y deja de leer la carpeta. | En detalle |
| **2b** | App de Cotizaciones para Android | En detalle |
| **2c** | Finanzas: revisión del trabajo cobrado (piezas con lo cobrado, costo real, horas), insumos promediados, forma rápida para trabajos sin factura; salen la ficha actual y las plantillas | Solo anotada; tiene su propio diseño |

## Enfoque

Cotizaciones habla con Supabase directamente, con su propia cola sin
conexión, siguiendo el mismo esquema que ya usa Finanzas: reloj híbrido
(HLC) por cambio, cursor por `updated_at` y RLS por usuario.

Descartados:

- **Finanzas como puente.** Tendría que escribir en la carpeta de
  Cotizaciones, y Finanzas nunca escribe ahí. Además dependería de que
  Finanzas esté abierta.
- **Sincronizar por la red de la casa.** Solo funciona en casa.

## Qué va a la base: `v2_quotes`

Una fila por documento. Las columnas siguen las convenciones de `v2_*`: id de
texto, `user_id`, montos en centavos (`bigint`), cadena vacía en lugar de
`null` donde no hay dato, `deleted` como lápida y `updated_at` puesto por
trigger.

| Columna | Tipo | De dónde sale en el documento |
|---|---|---|
| `id` | text pk | `id` (uuid del documento; no cambia nunca) |
| `user_id` | uuid | la sesión |
| `numero` | text, `''` si es borrador | `numero` |
| `tipo` | text: `cotizacion` \| `informe` | `tipo` |
| `forma` | text: `servicio` \| `proyecto` | `forma` |
| `cliente` | text | `clienteCongelado.nombre` |
| `equipo` | text | `equipo.descripcion` o, en un proyecto, el nombre del proyecto |
| `lineas` | jsonb | `[{seccion, concepto, cantidad, valorUnitario}]`, con `seccion` = nombre de la categoría |
| `descuento_tipo` | text: `''` \| `porcentaje` \| `monto` | `descuento.tipo` |
| `descuento_valor` | bigint | `descuento.valor` |
| `abono_minor` | bigint | `abono` |
| `fecha_emision` | text ISO, `''` si no hay | `fechaEmision` |
| `fecha_ingreso` | text ISO | `equipo.fechaIngreso` |
| `origen_id` | text | `origenId` (la cotización de la que salió el informe) |
| `hecho_en` | text: `pc` \| `telefono` | quién la creó |
| `hlc` | text | reloj del **contenido** (solo lo escribe quien es dueño: ver abajo) |
| `estado` | text | `estado` |
| `fecha_entrega` | text ISO | `fechaEntrega` |
| `fecha_pago` | text ISO | la fecha del evento "Pagado" del historial |
| `motivo_rechazo` | text | `motivoRechazo` |
| `estado_hlc` | text | reloj del **estado** |
| `device_id` | text | aparato que escribió por última vez |
| `deleted` | boolean | lápida |
| `updated_at` | timestamptz | trigger |

**No van:**

- el resumen, las notas, la forma de pago y la garantía;
- las cláusulas;
- los datos del cliente (identificación, teléfono, correo, dirección);
- el historial, salvo la fecha de pago;
- las fotos y el PDF;
- la revisión, la validez y el consecutivo.

La migración SQL (`supabase_v4_cotizaciones.sql`, en el repo de Finanzas,
junto a las otras) crea la tabla, el índice `(user_id, updated_at)`, el
trigger y las mismas cuatro políticas RLS que `v2_movements`. Es un `create
table if not exists`: no toca nada de lo que ya existe.

## Dos relojes por fila

El contenido y el estado los escriben lados distintos, y cada grupo tiene su
reloj:

- **Contenido** (`numero` … `hecho_en`, reloj `hlc`). Lo escribe el dueño
  del documento: la PC en todo lo que hizo o ya tomó; el teléfono solo en un
  borrador suyo que la PC todavía no tomó.
- **Estado** (`estado`, `fecha_entrega`, `fecha_pago`, `motivo_rechazo`,
  reloj `estado_hlc`). Lo escriben la PC y el teléfono.

Cómo se resuelven los choques:

- En cada grupo gana el reloj más nuevo.
- El teléfono cambia el estado con un `PATCH` que manda solo las columnas del
  estado. Nunca reescribe el contenido de un documento de la PC, así que no
  puede pisar una línea que la PC corrigió después.
- La PC, antes de subir un documento, baja los cambios. Si el `estado_hlc`
  remoto es más nuevo que el suyo, aplica primero ese estado al documento
  local y recién después sube.

## Qué puede hacer cada uno

| Acción | PC | Teléfono |
|---|---|---|
| Crear un borrador | Sí | Sí (cotización rápida; ver 2b) |
| Editar un borrador | Sí | Solo uno suyo, mientras la PC no lo tomó |
| Emitir (enviar una cotización, entregar un informe: pone número) | Sí | No |
| Aceptar o rechazar una cotización enviada | Sí | Sí; rechazar pide motivo |
| Cobrar un informe entregado (pagado, con fecha) | Sí | Sí, con fecha (hoy por defecto) |
| Corregir (vuelve a borrador, revisión + 1) | Sí | No |
| Borrar | Sí | Solo un borrador suyo que la PC no tomó |

"Entregado" **no** es una acción del teléfono: en Cotizaciones, entregar un
informe es emitirlo (le pone número y fecha de emisión), y la numeración es
solo de la PC.

Que la PC "toma" un borrador del teléfono significa que lo abre y lo guarda.
Desde ese momento es dueña del contenido y la fila pasa a `hecho_en = pc`.

## Etapa 2a: la PC y Finanzas

### Cotizaciones de PC (Electron, proceso principal)

- **Sesión.** Nueva sección "Nube" en la pantalla Datos: correo y contraseña
  de la misma cuenta de Supabase que usa Finanzas, "Conectar" /
  "Desconectar" y el estado de la última sincronización.
  - La URL y la clave pública se leen del mismo `supabase.json` que usa
    Finanzas (`%APPDATA%\DakeLabs\Finanzas DakeLabs\supabase.json`).
  - La contraseña no se guarda. El token de sesión va cifrado con
    `safeStorage` de Electron.
- **Módulo nuevo `src/main/nube/`.** Sin dependencias nuevas salvo
  `@supabase/supabase-js`. Contiene:
  - `esencial.ts`: `esencialDe(documento) → FilaCotizacion` y el camino
    inverso para un borrador del teléfono. Es puro y tiene pruebas.
  - `reloj.ts`: el HLC, compatible con el formato de Finanzas.
  - `cola.ts`: la cola de subidas pendientes, persistida en un archivo JSON
    con escritura atómica en la carpeta de datos.
  - `sincronizar.ts`: una corrida hace bajar → aplicar → subir, con cursor
    por `updated_at`.
- **Cuándo se sincroniza:**
  - al conectar y al abrir la app;
  - 5 segundos después de cada cambio local (se agrupan);
  - cada 5 minutos;
  - con "Sincronizar ahora".
- **Aplicar lo que baja:**
  - **Estado más nuevo en un documento de la PC:** se usa la misma
    `aplicarAccion` de `dominio/estados.ts`, con la fecha que trae la fila y
    no "hoy". Para eso `aplicarAccion` gana un parámetro opcional `fecha`.
    Queda el evento en el historial, igual que si se hubiera hecho en la PC.
  - **Borrador nuevo del teléfono:** se crea el documento local en borrador
    con cliente, equipo y líneas en las categorías "Mano de obra" y
    "Repuestos y materiales"; el resto con los valores por defecto de un
    documento nuevo. Aparece en el listado con la marca "del teléfono".
  - **Lápida de un borrador del teléfono:** se borra el documento local si
    todavía es un borrador sin tomar.
- **Subir:** cada guardado, emisión o acción agrega el id a la cola. La
  corrida sube la fila esencial con `upsert`. El contenido lo sube con reloj
  nuevo solo si cambió; el estado, igual.
- **Primera conexión:** sube la fila esencial de todos los documentos que
  existen hoy (son 7).
- **Si algo falla:** el aviso de siempre (`dake:error`) y la cola queda
  intacta. Nunca se marca subido sin la confirmación del servidor.

### Finanzas (Qt)

- `quotes` se suma a las tablas que sincroniza `SyncEngine`, después de
  `movements`. Queda una tabla local `quotes` con las mismas columnas y el
  mismo trato de reloj y lápida que las demás.
- Un lector nuevo, `storage::readQuoteRows(repository) →
  std::vector<core::QuoteDoc>`, arma cada `QuoteDoc` desde la fila:
  - la base sale de las líneas y el descuento, con `quoteLineTotal` y
    `quoteBase`, que ya existen;
  - la fecha de pago sale de `fecha_pago`;
  - la fecha de ingreso, de `fecha_ingreso`.
- `MainWindow::importQuotes` usa ese lector en lugar de
  `readQuoteFolder`. Todo lo demás del importador (`planQuotes`, las
  decisiones guardadas en `cot.decisiones`, la revisión de dudosos) no cambia.
- **Sale la lectura de la carpeta:**
  - `storage/quotefolder.*` y su vigilante de archivos (`quoteWatcher_`,
    `quoteDebounce_`);
  - el campo de la carpeta en Ajustes. La tarjeta "DakeLabs Cotizaciones"
    queda con "Revisar" y el estado ("N documentos desde la nube, la última
    sincronización hace X").
- La sincronización del teléfono de Finanzas **no** baja `quotes`, porque ese
  teléfono no importa cotizaciones. Como `SyncEngine` es el mismo en los dos
  aparatos, su lista de tablas (`kTables`, hoy fija) pasa a ser un parámetro
  del constructor:
  - el escritorio usa `{pockets, jobs, movements, quotes}`;
  - el teléfono de Finanzas sigue con `{pockets, jobs, movements}`.

### Migración

- La primera conexión de Cotizaciones de PC sube los 7 documentos.
- Finanzas los baja y los planifica como hoy. Las decisiones se guardan por
  id de documento, así que INF-2026-002 sigue ignorado.
- Los movimientos ya importados conservan sus ids (`cot-<id>-saldo`), así que
  releer no duplica nada. La prueba "leer otra vez no agrega nada" se conserva
  con la base como fuente.

## Etapa 2b: Cotizaciones en Android

- **Cómo se construye:**
  - Capacitor empaqueta el renderer de React en un APK aparte.
  - Un punto de entrada nuevo, `src/movil/`, reusa `dominio/` (dinero,
    totales, estados, numeración) y el tema, pero no el editor completo ni la
    impresión.
  - La interfaz del teléfono es propia: cuatro pantallas simples.
- **Guardado en el teléfono:**
  - la fila esencial de cada documento, en `@capacitor/preferences` (un JSON
    por documento más un índice);
  - la cola de cambios y el cursor.
  - Sin fotos ni PDF.
- **Sincronización:** el mismo `sincronizar.ts` de la PC, sin el paso
  "aplicar al documento completo": en el teléfono el documento es la fila.
- **Pantallas:**
  1. **Entrar.** Correo y contraseña, una sola vez; el token va en el
     almacenamiento seguro del sistema.
  2. **Lista.**
     - Cada fila: número (o "Borrador"), cliente, equipo, total, saldo y la
       pastilla de estado, con los colores de Cotizaciones.
     - Filtros: *Por cobrar* (informes entregados sin pagar) · *Abiertas*
       (borradores, enviadas, aceptadas, entregadas) · *Todas*.
     - Búsqueda por cliente o equipo.
     - Arriba: "Sin señal · N cambios por subir" cuando corresponde.
     - Botón "Nueva".
  3. **Detalle.**
     - Lo esencial y las líneas agrupadas por sección, con total, abono y
       saldo.
     - Botones según el estado:
       - cotización *enviada*: **Aceptada** / **Rechazada** (pide motivo);
       - informe *entregado*: **Pagado** (fecha, hoy por defecto).
     - En un borrador del teléfono que la PC no tomó: **Editar** y **Borrar**.
  4. **Nueva rápida.**
     - Cliente, equipo y líneas (concepto, cantidad, precio unitario) en dos
       grupos: *Mano de obra* y *Repuestos y materiales*.
     - El total se suma solo, con las mismas reglas de redondeo que la PC.
     - Guardar deja un borrador `hecho_en = telefono`.
- **Aspecto:** el mismo de Cotizaciones (Inter/Anton, rojo de marca, claro u
  oscuro según el sistema).
- **Construcción:** un script `construir-android.ps1` en el repo de
  Cotizaciones, igual que el de Finanzas. El APK se llama
  `CotizacionesDakeLabs.apk`.

## Pruebas

- **Cotizaciones (vitest):**
  - `esencial.ts`, de ida y vuelta: un documento con descuento, abono y
    líneas de dos secciones da la fila esperada, y el borrador que vuelve
    trae cliente, equipo y líneas.
  - Nada de lo que no va (resumen, notas, cláusulas, datos del cliente)
    aparece en la fila.
  - Reloj: con dos cambios de estado, gana el más nuevo; un cambio de estado
    del teléfono no pisa las líneas que la PC corrigió después.
  - `aplicarAccion` con fecha: cobrar con fecha deja el evento "Pagado" con
    esa fecha.
  - La cola: lo subido sin confirmación queda en la cola; con confirmación,
    sale.
  - La sincronización contra un cliente de Supabase simulado: bajar, aplicar
    y subir; primera conexión con 7 documentos; lápida de un borrador.
- **Finanzas:**
  - `storage`: una fila `quotes` se lee como `QuoteDoc` con base, abono y
    fechas correctas (mismos casos que las pruebas actuales de la carpeta).
  - `uitest`: el bloque "[DakeLabs Cotizaciones]" pasa con filas en la tabla
    local en lugar de archivos: el entregado entra por cobrar, el pagado entra
    cobrado con su fecha, el repetido espera y releer no duplica.
- **Contra Supabase real:** en un proyecto o esquema de prueba, nunca con los
  datos de David. Una corrida de humo: la PC sube, el teléfono marca pagado,
  la PC lo aplica y Finanzas lo ve cobrado.
- **Teléfono:** revisión a mano en el celular.

## Lo que David tiene que hacer

1. Correr `supabase_v4_cotizaciones.sql` en el panel de Supabase.
2. En Cotizaciones de PC: Datos → Nube → Conectar, con su cuenta.
3. Después de la 2b: instalar `CotizacionesDakeLabs.apk`.

## Fuera de alcance

- Todo lo de la etapa 2c.
- Subir fotos, PDF o el documento completo.
- Emitir, corregir o numerar desde el teléfono.
- Clientes y cláusulas en el teléfono. La cotización rápida lleva solo el
  nombre del cliente; la PC completa el resto al tomarla.
- Que Finanzas del teléfono muestre cotizaciones.
