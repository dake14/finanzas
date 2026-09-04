# Por qué dejaste de usar Finanzas DakeLabs

Diagnóstico de `finaldake-labs`, hecho el 2 de septiembre de 2026 sobre el
código y sobre la base de datos real de la aplicación instalada.

---

## 1. La evidencia

No es una opinión sobre la app: son los datos que tiene adentro.

**`%APPDATA%\DakeLabs\Finanzas DakeLabs\finanzas.db`** — 6 movimientos, todos
de agosto de 2026:

| Fecha | Qué | Tipo | Monto | Categoría |
|---|---|---|---|---|
| 2026-08-13 | IFIX MODELO 3D | Ingreso | 5,00 | IMPRESION 3D |
| 2026-08-17 | Teclado y backlight macbook y envio | Gasto | 49,82 | Piezas |
| 2026-08-17 | Trabajo Macbook Herman Galvan | Ingreso | 120,00 | Reparacion |
| 2026-08-20 | Compra de PLA GRIS | Gasto | 25,67 | Insumos |
| 2026-08-24 | Compra PLA Negro | Gasto | 23,53 | Insumos |
| 2026-08-24 | Compra PLA Blanco 4Kg | Gasto | 46,00 | Insumos |

Los dos últimos se cargaron con 18 segundos de diferencia: fue una sesión de
ponerse al día, no dos anotaciones del momento. Después de esa sesión no se
cargó nada más. **El último movimiento es del 24 de agosto; hoy es el 2 de
septiembre.** Nueve días.

En `settings` hay además un presupuesto y tres metas:

```
budgets : [{"category":"Consumibles","limit_minor":2000}]
goals   : Inversion 50,00 · Ahorro 50,00 · Salario 50,00
```

Con esos seis movimientos, la aplicación muestra hoy:

- Ingresos 125,00 · Gastos 145,02 · **Utilidad −20,02**
- Inversión **−8,01** · Ahorro **−6,01** · Salario **−6,00**
- Presupuesto "Consumibles": 0,00 de 20,00
- Metas: 0% las tres, "no se puede estimar"

---

## 2. El diagnóstico, en una frase

**La aplicación no puede representar lo que realmente pasó ese mes, así que los
números que muestra no coinciden con tu realidad; y como no coinciden, dejaste
de creerles; y como dejaste de creerles, dejaste de alimentarla.**

Lo que realmente pasó, en tus palabras: *gastaste plata en material y la sacaste
de ahorro e inversión.* Ninguna de esas dos cosas se puede escribir en la
aplicación. No es que las escriba mal: **no tiene dónde**.

---

## 3. Deficiencias de fondo

Ordenadas por cuánto pesan.

### 3.1. Las secciones no son lugares, así que no se puede sacar de ellas

En `core/model.hpp`, "inversión", "ahorro" y "salario" no son cuentas: son un
reparto porcentual de la utilidad del mes (`allocateProfit`), y el saldo es la
suma acumulada de esos repartos (`bucketBalanceSeries`).

Consecuencia directa: **no existe la operación "saqué 46 dólares del ahorro"**.
Cuando lo hacés, la única forma de anotarlo es como un gasto más. Ese gasto baja
la utilidad del mes, y la utilidad más baja baja las tres secciones en
proporción — incluida la de salario, que no tocaste.

Es exactamente la mitad de tu queja, y no tiene arreglo dentro del modelo
actual: hace falta que los bolsillos sean entidades con saldo y que exista el
traspaso.

### 3.2. Lo que la app llama "utilidad" es la variación de caja

`model.cpp:207` → `summary.profit = summary.income - summary.expense`.

Con tus seis movimientos eso da **−20,02**. Ese número no está mal: es
literalmente cuánto bajó la plata que tenías. Lo que está mal es **el rótulo**.
Bajó porque compraste cuatro kilos de filamento que te van a durar meses, no
porque el mes haya sido malo.

Y sobre ese número mal rotulado se calcula todo lo demás: `allocateProfit(−20,02)`
reparte la pérdida 40/30/30 y deja las tres secciones en rojo. Un mes en el que
repusiste stock se presenta como un mes en el que te fue mal en las tres áreas.

### 3.3. Comprar material se carga entero al mes de la compra

"Compra PLA Blanco 4Kg · 46,00" es material para varios meses. La aplicación lo
cuenta 100% en agosto.

Resultado: **cada mes en que reponés se ve como un desastre y cada mes en que
imprimís de lo que ya tenías se ve como ganancia pura.** La señal que da el
tablero es ruido con el signo cambiado. Si mirás ese tablero para decidir si
subir precios, te va a decir lo contrario de lo que corresponde.

### 3.4. No hay saldos: nada se puede verificar contra la realidad

La aplicación no sabe cuánta plata tenés. Solo suma y resta movimientos desde
cero. No hay saldo inicial, no hay cuentas, no hay nada que puedas contar a mano
y comparar.

**Un número que no se puede verificar nunca llega a creerse del todo.** Es la
razón estructural por la que un tablero deja de mirarse aunque todo lo demás
esté bien.

### 3.5. Una pérdida se reparte entre las tres secciones

`allocateProfit` reparte también los negativos: con −20,02 da inversión −8,01,
ahorro −6,01, salario −6,00.

En la vida real una pérdida **no sale de tres lados en proporción**: sale de uno,
casi siempre del ahorro. El reparto proporcional produce tres saldos que no
corresponden a ninguna plata que exista.

### 3.6. No existe "hecho y sin cobrar"

Un trabajo entregado y todavía impago solo tiene dos opciones: cargarlo (y la app
dice que tenés plata que no tenés) o no cargarlo (y se olvida). Para un taller de
reparaciones y encargos, esto es la mitad del negocio.

### 3.7. No hay trabajos: el ingreso y su costo son dos filas sin relación

En tu propia base están:

- `Trabajo Macbook Herman Galvan` — Ingreso 120,00
- `Teclado y backlight macbook y envio` — Gasto 49,82

Son obviamente la misma reparación. La aplicación **no puede decirte que dejó
70,18, el 58,5%**. Ese número es el único por el que vale la pena sentarse a
anotar un gasto de 49,82 — y es justo el que no da.

Nota: la versión anterior (`finanzasdakelabs`) sí tenía `Project` y `Client` con
`profitByProject`. En `finaldake-labs` se cayeron. Se perdió la respuesta a la
única pregunta que paga el esfuerzo de cargar datos.

---

## 4. Fricciones de uso

Más chicas, pero se pagan una vez por fila cargada.

### 4.1. La carga rápida no tiene fecha

`mainwindow.cpp:352` → `movement.date = today_;`

La plata se gasta en la calle y se anota en la computadora dos días después. Hoy
hay que agregar el movimiento y **después** buscarlo en la lista, abrirlo y
corregir la fecha a mano. Tus dos últimas cargas (18 segundos de diferencia) son
justo el patrón que necesita fecha.

### 4.2. Las categorías son texto libre sin normalizar

En seis movimientos ya hay `IMPRESION 3D` (en mayúsculas), `Reparacion`,
`Insumos` y `Piezas`. "Insumos" y "Piezas" son la misma idea partida en dos, y
`IMPRESION 3D` va a chocar con "Impresion 3D" la próxima vez.

### 4.3. El presupuesto está puesto sobre una categoría que no existe

`budgets: [{"category":"Consumibles", "limit_minor":2000}]`. Ningún movimiento
usa "Consumibles". La tarjeta de presupuestos muestra 0,00 de 20,00 para
siempre, y **la aplicación nunca dice que ese presupuesto no está midiendo
nada**. Es una pantalla entera de peso muerto.

### 4.4. Las metas no tienen forma de cumplirse ni de explicarse

Tres metas de 50,00. Con utilidad negativa el avance es 0% y la estimación es
"no se puede estimar". La app no dice por qué ni qué haría falta. Una meta que
solo sabe decir "no" es una fuente de desánimo, no de dirección.

### 4.5. Solo corre en el escritorio

El gasto ocurre en la ferretería, con el teléfono en la mano. La aplicación es
un `.exe` de Windows instalado en una máquina. **Toda anotación es diferida, y
una anotación diferida es una anotación que se olvida.** El hueco de nueve días
sale de acá.

### 4.6. La aplicación nunca pide nada ni avisa nada

Es un libro de cuentas puro: devuelve exactamente lo que se le mete y ni un
gramo más. No hay un aviso, no hay un recordatorio, no hay una sola frase que
aparezca sin que se la pidas. Nada te trae de vuelta.

### 4.7. Un ingreso no puede ser mensual

`quickentry.cpp` deshabilita "Fijo mensual" para ingresos. Un cliente con
retainer no se puede proyectar.

---

## 5. La conclusión

Con seis movimientos cargados, la aplicación te devuelve: una pérdida, tres
secciones en rojo, un presupuesto que mide una categoría inexistente y tres
metas que dicen "no se puede estimar".

**Devuelve menos de lo que cuesta alimentarla.** Ese es el motivo real del
abandono, y las siete deficiencias de arriba son las que lo producen. No es
falta de disciplina.

---

## 6. Lo que NO hay que tocar

Vale decirlo, porque es mucho y es bueno:

- **El dinero como `int64` en centavos, sin un solo `double`.** Toda la
  aritmética, el reparto sin perder centavos (`Money::allocate`), el redondeo
  half-even. Está bien hecho y el programa hermano lo reusa tal cual.
- **Las fechas civiles sin zona horaria**, con el algoritmo de Hinnant.
- **UUIDv7 + HLC + lápidas desde el día uno.** Es la decisión que hace posible
  sincronizar sin dolor.
- **Las capas** (`ui → sync → storage → core`) y que `core` no conozca Qt.
- **`dake_uipreview`**, el render offscreen a PNG. El programa hermano lo copió
  porque es la única forma sensata de verificar una interfaz sin robar el foco.

El problema nunca fue la ingeniería. Fue **qué decidió representar el modelo**.

---

## 7. Qué hace distinto el programa hermano

`finanzas-banco-pruebas/` — mismo `core` monetario, modelo nuevo.

| Deficiencia | Qué cambia |
|---|---|
| 3.1 secciones que no son lugares | **Bolsillos** con saldo real y **traspasos**. "Saqué del ahorro" es un movimiento. |
| 3.2 caja rotulada como utilidad | Se muestran **los dos números, separados y explicados**: caja −38,42 y resultado +113,87. |
| 3.3 material cargado al mes | **"Me dura N meses"** en la carga. La caja se mueve hoy; el costo se reparte. |
| 3.4 nada verificable | Pantalla de **Bolsillos** con saldo inicial y botón **Cuadrar**: escribís lo que hay de verdad y la diferencia queda anotada como movimiento visible. |
| 3.5 pérdida repartida | No hay reparto automático. El financiamiento se **mide** de los traspasos reales. |
| 3.6 sin cobrar | Casilla **"todavía no me lo pagaron"**: cuenta para el resultado, no para el saldo. |
| 3.7 sin trabajos | **Trabajos** con margen: el Macbook dejó 70,18, 58,5%. |
| 4.1 sin fecha | Botones **Hoy / Ayer** y calendario en la carga rápida. |
| 4.6 no avisa nada | **Avisos** ordenados por gravedad, encabezados por el que importa. |

Y la pantalla principal contesta primero la pregunta que la app actual no puede
ni formular:

> **120,00 de lo que gastaste este mes salió de tus ahorros.**
> Eso no es una pérdida ni una ganancia: es capital tuyo tapando un hueco, y por
> eso no aparece en ningún estado de resultados. Si vuelve a pasar el mes que
> viene, el problema no es el mes: es el precio de los trabajos.

Ver [README.md](README.md) para cómo compilarlo y correrlo.
