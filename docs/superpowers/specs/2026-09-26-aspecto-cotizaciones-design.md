# Finanzas DakeLabs — el aspecto de Cotizaciones

Aprobado en conversación el 2026-09-26. Es la parte 1 de 3; las otras dos
(Cotizaciones dentro de Finanzas, y cotizar desde el teléfono) tienen su propio
diseño aparte.

Maquetas de referencia: `.superpowers/brainstorm/831-1790443018/content/`
(`color-gastos.html`, opción A; `hoy-completa.html`, opción A).

## 1. Qué cambia

La aplicación de escritorio pasa del tema oscuro propio al sistema visual de
DakeLabs Cotizaciones: barra lateral azul-noche con el logo, superficies claras,
rojo de marca, Inter y Anton. Con dos temas, **claro** y **oscuro**, elegibles
en Ajustes. Al cambiar de tema, todo se repinta en el acto, sin reiniciar.

## 2. Lo que no cambia

- La app del teléfono (`mobile/`), sus colores incluidos.
- Los datos, el esquema y la sincronización. El tema se guarda en la tabla
  local `settings`, que no sincroniza.
- La estructura de las pantallas: mismas páginas, mismas tarjetas, mismo orden.
  Las únicas piezas nuevas son el logo y las pastillas de estado (sección 6).

## 3. Temas

Dos valores: `claro` y `oscuro`. Clave `ui.tema` en `settings`; si falta o no se
reconoce, `claro`. Se elige en Ajustes con un selector de dos opciones y se
aplica al soltarlo. El cambio alcanza también a la ventana mini de
`Ctrl+Alt+Espacio` y a los diálogos abiertos.

Sin tema automático por horario: David lo descartó.

## 4. Paleta

Cada color es un **papel**. Las pantallas piden el papel; el tema dice qué color
tiene.

| Papel | Claro | Oscuro | Para qué |
|---|---|---|---|
| `Fondo` | `#F7F8FA` | `#23253A` | fondo de la ventana |
| `Superficie` | `#FFFFFF` | `#2B2D42` | tarjetas, tablas, diálogos |
| `SuperficieAlzada` | `#F1F3F7` | `#33364F` | campos, hover, fila elegida |
| `Barra` | `#2B2D42` | `#1A1C2B` | barra lateral (en oscuro, más oscura que la superficie, o se funde) |
| `Borde` | `#E4E7EE` | `#3A3D55` | bordes y separadores |
| `Texto` | `#2B2D42` | `#EDF2F4` | texto principal |
| `Tenue` | `#6B7690` | `#A9B1C4` | texto secundario |
| `Apagado` | `#8D99AE` | `#737C96` | pistas, ayudas, marcas de eje |
| `Marca` | `#EF233C` | `#EF233C` | logo, menú activo, rótulos de tarjeta, botón principal, sueldo |
| `MarcaSuave` | `#FDE8EB` | `#4A2332` | fondo del menú activo y de la pastilla de marca |
| `Ingreso` | `#1F9D6B` | `#3ECF8E` | lo que entra |
| `Gasto` | `#E8590C` | `#FF922B` | lo que sale y los números negativos |
| `Aviso` | `#B7791F` | `#F6C453` | advertencias que no son error (hoy usan el ámbar de inversión) |
| `Serie` | `#4361EE` | `#7B93FF` | gráficas sin significado propio (caja acumulada, reinversión) |
| `Operacion` | `#4361EE` | `#7B93FF` | bolsillo de operación |
| `Ahorro` | `#7B2CBF` | `#B57BFF` | bolsillo de ahorro |
| `Inversion` | `#0F8FA3` | `#3CC8DC` | bolsillo de inversión |
| `Personal` | `#8D99AE` | `#A9B1C4` | bolsillo personal |

Reglas:

- El rojo es solo de la marca. Un gasto o un número negativo nunca va en rojo:
  va en `Gasto` (naranja).
- `Aviso` e `Inversion` se separan. Hoy `kInversion` hace de las dos cosas;
  cada uso se reasigna según lo que quiere decir.
- Las gráficas no usan `Marca` para datos: una línea de caja en rojo se leería
  como alarma. Usan `Serie`, `Ingreso` o `Gasto`.

## 5. Letra

- **Inter** para todo el texto y los montos de tablas y listas, con cifras de
  ancho tabular (`tnum`) para que las columnas queden alineadas.
- **Anton** para las cifras grandes de las tarjetas (`KpiCard`, los titulares de
  Hoy y de Reportes) y para `DAKE`/`LABS` en la barra lateral. Nunca por debajo
  de 16 pt: en tamaño chico se lee mal.
- Las dos van **empaquetadas** como `.ttf` estáticos en un recurso de Qt
  (`ui/recursos/fuentes/`) y se registran al arrancar con
  `QFontDatabase::addApplicationFont`. Así no depende de lo que tenga instalado
  la computadora. Fuente: Google Fonts, licencia SIL OFL (se copia el
  `OFL.txt` al lado).
- Si una fuente no carga, se cae a Segoe UI como hoy, y el diagnóstico lo dice.

## 6. Piezas nuevas

**Logo.** Arriba de la barra lateral: `DAke.png` (copiado de
`dakelabsfactura/src/renderer/src/recursos/`) a 28 px, y al lado "DAKE" en
blanco y "LABS" en `Marca`, en Anton. Es igual en los dos temas, porque la barra
siempre es oscura.

**Menú activo.** La opción elegida de la barra lateral lleva una raya de 3 px en
`Marca` a la izquierda y un fondo de `Marca` al 18 %. Es lo mismo que en
Cotizaciones.

**Pastillas de estado en Reparaciones.** El estado de cada reparación, en la
lista y en la ficha, pasa de texto de color a pastilla redondeada:

| Estado | Texto | Pastilla |
|---|---|---|
| `EnProceso` | En proceso | fondo `Texto`, letra `Superficie` (como "Enviada") |
| `Entregada` | Por cobrar | fondo `Aviso` al 20 %, letra `Aviso` (como "Vencida") |
| `Cobrada` | Cobrada | fondo `Ingreso` al 15 %, letra `Ingreso` (como "Aceptada") |

**Botón principal.** El botón que guarda o crea, uno por pantalla o diálogo, va
en `Marca` con letra blanca. El resto de los botones van con borde, sin relleno.

## 7. Cómo se arma por dentro

**Papeles en vez de colores.** En `ui/theme.hpp`, los `inline const QColor kX`
pasan a ser valores de `enum class Tono`. Los nombres actuales se conservan
como alias (`kNegative` → `Tono::Gasto`, `kTextMuted` → `Tono::Tenue`, …), así
que la mayoría de los ~280 usos no cambian de texto. Donde hace falta un
`QColor` de verdad (pintar, `setForeground` de una tabla), se pide con
`theme::color(Tono)`. El compilador marca cada lugar que necesita ese ajuste.
Los usos de `kInversion` y `kAccent` se revisan uno por uno contra la
sección 4.

**Textos.** `setLabelColor(widget, Tono)` ya no escribe un color: pone la
propiedad dinámica `tono` en el widget. La hoja de estilo global tiene una regla
por papel (`QLabel[tono="gasto"] { color: … }`). Al cambiar de tema se
regenera la hoja de estilo y Qt vuelve a pintar todas las etiquetas solo.

**Lo que se pinta a mano** (gráficas, `Card`, `KpiCard`, `FundingBar`,
`SalaryScale`, `SplitBar`, el ícono de la bandeja): pide
`theme::color(papel)` dentro de `paintEvent`, nunca lo guarda. Al cambiar de
tema basta un `update()`.

**Tablas.** Los colores de celda se ponen al llenarlas. Al cambiar de tema, la
ventana principal vuelve a pasar la foto actual (`setSnapshot`) por todas las
páginas, y las tablas se rellenan con los colores nuevos. Es lo mismo que ya
pasa después de cada cambio local.

**El cambio de tema**, en un solo lugar: `theme::setTheme(Tema)` actualiza la
paleta activa y regenera la hoja de estilo de la aplicación. Después
`MainWindow` repinta los widgets propios y re-pasa la foto. Ajustes solo llama a
eso y guarda `ui.tema`.

## 8. Pruebas

- `dake_uitest`, offscreen: arranca en claro, anota un gasto, cambia a oscuro
  desde Ajustes y comprueba que (a) la etiqueta del monto del gasto tiene
  `tono == "gasto"`, (b) su color efectivo es `#FF922B`, y (c) `ui.tema`
  quedó en `oscuro`. Reabre la ventana y comprueba que arranca en oscuro.
- `dake_uitest`: la pastilla de una reparación cobrada dice "Cobrada".
- `dake_uipreview` gana un argumento de tema y genera todas las pantallas en
  claro y en oscuro, para revisarlas a ojo antes de dar la parte por
  terminada.
- `compilar.ps1 debug -Probar` entero, cero advertencias.

## 9. Fuera de esta parte

- Tema automático por horario.
- Fichas de filtro con contador (David eligió solo pastillas y logo).
- Todo lo de Cotizaciones dentro de Finanzas: parte 2.
- El teléfono: parte 3.
