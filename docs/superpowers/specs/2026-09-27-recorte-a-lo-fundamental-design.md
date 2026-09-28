# Recorte a lo fundamental (etapa 1)

Fecha: 2026-09-27 · Rama: `escritorio-cuentas-reparaciones`

## Para qué

David quiere que Finanzas haga solo lo fundamental: anotar sin nada de por
medio y contestar cuatro preguntas. Todo lo que no sirva a eso se elimina.

Las cuatro preguntas (elegidas por David):

1. ¿Cuánto tengo y dónde?
2. ¿Cuánto gané / gasté este mes?
3. ¿En qué se me va la plata?
4. ¿Cuánto me puedo pagar?

Además conserva **costeo y margen** y **sin cobrar**.

Criterio de éxito: anotar un gasto es monto, categoría, Enter; Hoy muestra
solo lo que pide acción; todo número vive en Informes; nada de lo eliminado
deja datos rotos ni rompe la sincronización del teléfono.

## Las tres etapas

Este documento es solo la **etapa 1**.

1. **Recorte** (este documento). Reparaciones queda igual.
2. **Trabajos desde Cotizaciones.** Tiene diseño aparte.
   - La cotización cobrada llega con sus líneas de piezas. En Finanzas se
     completa con las horas y el costo real, que es lo cobrado salvo que se
     escriba otro.
   - La compra de una pieza se anota cuando se hace, con el formulario. La
     pieza de la factura solo sirve para calcular el margen y nunca crea otro
     gasto: así la caja siempre cuadra y nada se cuenta dos veces.
   - Los insumos se promedian del gasto mensual en insumos. Por eso se van el
     campo "Consumibles" y "lo compré ahora", junto con la ficha manual y las
     plantillas.
3. **Cotizaciones dentro de Finanzas.** Sigue pausada.

## Decisión técnica: borrar el código, dejar los datos

Se borran pantallas, lógica y pruebas de lo eliminado. Las columnas y tablas
de la base local y de la nube **no se tocan**: no hay migración que borre,
el teléfono viejo sigue sincronizando mientras se instala el nuevo, y no se
pierde historia.

Descartado: ocultar con interruptores (código muerto que mantener) y borrar
de la base (rompe la sincronización y pierde datos).

## Qué se elimina

| Qué | Dónde vive hoy | Qué pasa con los datos |
|---|---|---|
| Intérprete de la línea de captura | `core/capture.*`, `ui/capturewidget.*` | nada que guardar |
| "Dura" / material por delante | `Movement::spreadMonths`, informes, Movimientos, teléfono | el campo queda; los informes lo ignoran; los nuevos se escriben con 1 |
| Sección Revisión y su aviso semanal | `ui/reviewpage.cpp`, bandeja, Ajustes | los pendientes que se quedan pasan a Hoy |
| Extractos del banco | `core/bankcsv.*`, `ui/bankdialog.*`, Ajustes | las tablas quedan, sin uso |
| Contador de "capturas medidas" | Ajustes → Captura, `reviewMedianMs` | se deja de medir |
| Pendientes "sin categoría", "sugerido" y "vida útil" | `core::InboxKind` | se eliminan los tipos |

Efecto visible: las tres compras de filamento repartidas en 3 y 4 meses
(46,00, 23,53, 25,67) cuentan completas en agosto de 2026.

Un movimiento viejo sin categoría aparece como "Sin categoria" en los
informes y se corrige desde Movimientos; ya no es un pendiente.

## Barra lateral

Hoy · Reparaciones · Movimientos · Bolsillos · Informes · Ajustes.

"Revisión" desaparece. "Reportes" pasa a llamarse **Informes**.

## Hoy

Solo dos cosas, en este orden: **Anotar** y **Pendientes**. Ningún número
más.

### Anotar: el formulario

Un solo widget nuevo, `EntryForm` (`ui/entryform.hpp/.cpp`), usado por Hoy y
por la ventana chica de `Ctrl+Alt+Espacio`. Reemplaza a `CaptureWidget`.

```
 [ Gasto ] [ Ingreso ] [ Traspaso ]
 Monto     [ 25,00 ]
 Categoría [ Comida ▾ ]          (Traspaso: De [ ▾ ]  A [ ▾ ])
 Sale de   [ Caja del negocio ▾ ]   (Ingreso: "Entra a")
 Fecha     [ 27/09/2026 ]
                                  [ Anotar ]
 ✓ Anotado: Gasto 25,00 · Comida · Caja del negocio
```

- **Tipo:** Gasto, Ingreso o Traspaso. Se elige con clic o con `Alt+G`,
  `Alt+I`, `Alt+T` (`Ctrl+número` ya cambia de sección). Arranca en Gasto.
- **Monto:** acepta `25`, `25,50` y `25.50`. Tiene que ser mayor que cero y
  con dos decimales como mucho. El foco arranca aquí.
- **Categoría** (Gasto e Ingreso):
  - Muestra las categorías de ese tipo, de la más usada a la menos usada.
  - Nunca muestra "Sin categoria" ni "Ajuste de saldo".
  - Es editable: si escribes un nombre que no existe, se crea al anotar y
    queda en la lista para la próxima. Al final hay un "+ Nueva…" que vacía el
    campo para escribir.
  - La categoría nueva toma el tipo del movimiento y la cuenta (negocio o
    personal) del bolsillo elegido, con clase General.
- **Bolsillo:** todos los bolsillos, como "Nombre · negocio" o
  "Nombre · personal". El rótulo es "Sale de" en un Gasto y "Entra a" en un
  Ingreso. Por defecto trae el último usado para ese tipo, guardado en el
  ajuste `anotar.ultimo.<tipo>`.
- **Traspaso:** en lugar de Categoría y Bolsillo aparecen **De** y **A**, y
  por defecto traen el último par usado. Se guarda con la misma lógica de
  traspaso de hoy. De un bolsillo del negocio a uno personal cuenta como
  sueldo. De la caja a Emergencia no es gasto.
- **Fecha:** por defecto hoy, con calendario. No se permiten fechas futuras.
- **Lo que se guarda:**
  - como nombre, la categoría ("Sueldo" o "Traspaso" en un traspaso): así Movimientos muestra la categoría y el núcleo, que exige nombre, no cambia;
  - cobrado siempre;
  - `spreadMonths = 1`;
  - sin trabajo.
- **Enter anota.** Después de anotar se vacía el monto, se mantienen tipo,
  categoría y bolsillo, el foco vuelve al monto y aparece la línea
  "✓ Anotado: …".
- **Errores:** van en una línea bajo el botón, sin ventanas:
  - "El monto tiene que ser mayor que cero"
  - "Falta la categoría"
  - "De y A tienen que ser distintos"
  - "La fecha no puede ser futura"
- **Ventana chica:** Enter anota y cierra, Shift+Enter anota y la deja
  abierta, Esc cierra.

### Pendientes

Es una **lista con todos los pendientes a la vista, uno por fila**, y no un
bloque que muestra uno por vez como la Revisión de ahora. Cada fila tiene la
acción que la resuelve y un botón **Después**, que la pospone 7 días con el
mecanismo actual de posponer. Al resolver una fila, desaparece y el resto se
mantiene en su lugar. Si no hay nada, dice
"Nada pendiente". El orden es el de `core::inbox`, y "bolsillo sin cuadrar"
va al final.

| Pendiente | Tipo en la bandeja | Acción en la fila |
|---|---|---|
| Cotizaciones por decidir | `Cotizaciones` | Revisar → abre el diálogo de cotizaciones actual |
| Gasto fijo por confirmar | `PorConfirmar` | Monto editable + Confirmar |
| Reparación entregada sin horas | `SinHoras` | Horas editables + Guardar |
| Repuesto sin costo | `CostoRepuesto` | Costo editable + Guardar |
| Por cobrar (entregada hace más de 7 días) | `PorCobrar` | Cobrar → el cobro actual |
| Sin entregar (en proceso hace más de 14 días) | `SinEntregar` | Abrir → la ficha en Reparaciones |
| Bolsillo sin cuadrar hace más de 30 días | nuevo: `SinCuadrar` | Cuadrar → el diálogo actual |

**Sin cuadrar** es un pendiente nuevo:
- Al aceptar el diálogo de cuadrar se guarda la fecha en el ajuste local
  `bolsillo.<id>.cuadrado`, aunque no haya diferencia.
- Un bolsillo que nunca se cuadró cuenta desde el primer día en que corre
  esta versión, guardado en `recorte.inicio`. Así no aparecen todos juntos
  el primer día.
- Es solo de la PC y no se sincroniza.

La lógica va en `core::inbox`. `InboxInput` gana
`std::vector<std::pair<Id, Date>> lastReconciled`, y `InboxItem::refId` pasa
a ser el id del bolsillo.

## Informes (antes Reportes)

Cinco pestañas, una por pregunta. No hay números nuevos: todo se mueve desde
donde está hoy.

| Pestaña | Contenido | Viene de |
|---|---|---|
| **Resumen** | Caja del negocio; Ahorro, Inversión y Emergencia; Meses de reserva; Con qué se pagó este mes; Lo que hay que mirar | Hoy |
| **El mes** | Resultado por mes; Ingresos contra costos; Caja acumulada | Gráficas |
| | Entradas y salidas del negocio; Caja al cierre de cada mes | Flujo de caja |
| | ¿Cuánto gasto? | Hoy |
| | Mes a mes | Bolsillos |
| | Caja y resultado no son el mismo número | Hoy |
| **Gastos** | Gastos por categoría (gráfica y tabla) | Gráficas y su pestaña |
| | Costo de estructura | Hoy |
| **Trabajos** | ¿Subir precios?; Para no perder; Ticket promedio; Hecho y sin cobrar; Días en cobrar | Hoy |
| | Margen por trabajo | Gráficas |
| | Por reparación; Por tipo | sus pestañas |
| **Sueldo** | ¿Cuánto me puedo pagar?; el reparto de la utilidad | Hoy y su pestaña |

- "Material por delante" desaparece.
- "Con qué se pagó este mes" pierde su nota "la aplicación actual no puede
  contestar esto": los traspasos ya existen.
- Cada pestaña es una página con desplazamiento vertical y las tarjetas de
  arriba hacia abajo en el orden de la tabla.

## Las otras secciones

- **Movimientos:**
  - Se quitan las columnas Dura y Trabajo.
  - El diálogo de edición pierde "cuánto dura".
  - "Cobrado" se mantiene, para los ingresos viejos que siguen por cobrar.
- **Bolsillos:** saldos, cuadrar y nuevo bolsillo (con Emergencia). "Mes a
  mes" se va a Informes.
- **Reparaciones:** sin cambios de contenido hasta la etapa 2. Solo se
  arreglan los dos errores de abajo.

### Dos errores que se arreglan

- **Celda vacía al escribir.** El campo que se abre dentro de una celda
  (repuestos, plantillas, gastos fijos) hereda de la hoja global el relleno
  `7px 10px` de `QLineEdit`, y la celda le suma `QTableWidget::item
  { padding: 7px 10px }`. Con la fila baja no queda alto para el texto: se
  escribe sin ver nada y solo aparece al terminar. Arreglo: una regla
  `QTableView QLineEdit { padding: 0 4px; border-radius: 0; }` en
  `ui/theme.cpp`, y que el editor ocupe la celda entera. Prueba: en
  `dake_uitest`, al abrir el editor de una celda, su `contentsRect()` tiene al
  menos la altura de la fuente.
- **Lentitud al elegir o editar una reparación.** Cada cambio de la ficha
  (un repuesto, un costo, un campo) dispara el `reload()` completo de la
  ventana, que recalcula todos los informes y rellena todas las páginas.
  Arreglo: medir con `QElapsedTimer` qué parte se lleva el tiempo y, en los
  cambios de la ficha, recalcular y rellenar solo la página visible; las
  demás se rellenan al mostrarse. Criterio: elegir una reparación o agregar
  un repuesto se ve en menos de 150 ms en la compilación release, con los
  datos reales.
- **Ajustes:**
  - Se quitan "Revisión de la semana", "Extractos del banco" y el contador de
    capturas.
  - Quedan Apariencia, Captura (atajo y arrancar con Windows), Costos,
    Plantillas (hasta la etapa 2), Gastos fijos, Cotizaciones y Categorías.
- **Bandeja del sistema:** se quita el aviso semanal de revisión.

## Bolsillo Emergencia

- **Núcleo:**
  - `core::PocketKind::Emergencia`, con el texto "Emergencia".
  - `allPocketKinds()` pasa a devolver 5 valores.
  - `isReserve(Emergencia) == true`, así que entra en "Meses de reserva",
    en "Con qué se pagó" y en el Resumen.
- **Tema:** papel nuevo `Papel::Emergencia` (rosa: claro `#D6336C`, oscuro
  `#F06595`); `kPapeles = 19`, con el alias `theme::kEmergencia` y
  `pocketColor` asignado.
- **Nube:** `v2_pockets.kind` es texto sin restricción, así que no hace falta
  correr SQL.
- **Orden obligatorio:**
  1. Instalar el APK nuevo.
  2. Recién entonces crear el primer bolsillo de Emergencia.

  El APK viejo lanza una excepción en `pocketKindFromString` con un tipo que
  no conoce. En la sección Bolsillos, el diálogo de nuevo bolsillo lo
  recuerda con una línea bajo el tipo cuando se elige Emergencia y todavía no
  existe ninguno.

## Teléfono (APK nuevo)

- **Anotar.qml:**
  - Tiene los mismos campos que la PC: tipo, monto, categoría con "+ Nueva…"
    (abre un campo de texto), bolsillo (De y A en traspaso) y fecha.
  - Se quitan "Trabajo", "Cuánto dura" y "Ya lo pagué / Ya me lo pagaron".
  - Siempre manda `spreadMonths = 1`, `settled = true` y `jobId` vacío.
- **Historial.qml:** deja de mostrar "N meses".
- **Bolsillos.qml, Estilo.qml, appbridge:** conocen Emergencia, con su color
  y su rótulo.
- Hoy, Trabajos, Cierre y Más no cambian.
- Una categoría nueva escrita en el teléfono llega a la PC como texto, y
  `core::inferCategories` la da de alta (cuenta según los bolsillos, clase
  General), como ya ocurre hoy.

## Pruebas

- **Núcleo (`dake_selftest`):**
  - Emergencia se guarda y se lee de vuelta, y `isReserve` es verdadero.
  - Un movimiento con `spreadMonths = 3` cuenta completo en su mes en todos
    los informes.
  - La bandeja no produce `SinCategoria`, `Sugerido` ni `VidaUtil`.
  - `SinCuadrar` aparece a los 31 días y no a los 30. Usa `recorte.inicio`
    cuando no hay fecha, y respeta posponer.
  - Se borran las pruebas del intérprete y de los extractos.
- **Persistencia (`dake_storagetest`):** una base con filas viejas (dura > 1,
  por cobrar, tablas del banco con datos) abre y se lee sin errores, y
  Emergencia hace el viaje de ida y vuelta por `wire`.
- **Interfaz (`dake_uitest`, offscreen, solo teclado, sin teclas globales):**
  - Un gasto con categoría nueva que queda en la lista.
  - Un ingreso con la fecha cambiada.
  - Una fecha futura que se rechaza.
  - Un traspaso negocio → personal que aparece en Sueldo.
  - Un traspaso a Emergencia que no suma a gastos.
  - Tras anotar, el formulario conserva tipo y bolsillo y vuelve al monto.
  - Pendientes: un gasto fijo que se confirma desde Hoy y un bolsillo sin
    cuadrar que desaparece al cuadrarlo.
  - La barra sin Revisión, Informes con 5 pestañas y Hoy sin tarjetas de
    números.
  - La ventana chica usa `EntryForm`.
- **Teléfono:** revisión a mano después de instalar el APK.
- Compilar con `.\compilar.ps1 debug -Probar` sin ninguna advertencia.

## Fuera de alcance

- Todo lo de la etapa 2: trabajos desde Cotizaciones, horas y costo real al
  cobrar, insumos promediados, y quitar la ficha y las plantillas.
- La etapa 3.
- Borrar columnas o tablas de la base o de la nube.
- Cambios en Hoy, Trabajos, Cierre o Más del teléfono.
