# Finanzas DakeLabs

Nació como banco de pruebas de `finaldake-labs` —para probar un modelo distinto
sin tocar la aplicación instalada— y pasó a ser la aplicación. `finaldake-labs`
queda descontinuada; de ella solo se tomaron las credenciales de Supabase.

- Base de datos: `%APPDATA%\DakeLabs\Finanzas DakeLabs\finanzas-v2.db`.
  Comparte carpeta con la vieja, pero **nunca su archivo**: `finanzas.db` tiene
  otro esquema, y abrirla desde acá corrompería datos reales.
- Ejecutable: `dake_pruebas.exe`. No pisa ni el acceso directo ni la
  instalación existente, así las dos pueden convivir mientras desinstalás
  aquella.

El porqué de cada cambio está en [DIAGNOSTICO.md](DIAGNOSTICO.md).

---

## La idea, en tres frases

1. **Los bolsillos son lugares con saldo**, no porcentajes de una resta. Se
   pueden contar a mano y comparar. Existe el **traspaso**, así que "saqué del
   ahorro para comprar material" es algo que la aplicación puede escribir, ver y
   avisar.
2. **Caja y resultado son dos preguntas distintas** y se muestran por separado.
   Cuatro kilos de filamento mueven la caja hoy y el costo durante cuatro meses.
3. **Un trabajo junta el ingreso con los gastos que lo hicieron posible**, así
   que la aplicación puede decirte que la reparación del Macbook dejó 70,18 — el
   único número que paga el esfuerzo de anotar un gasto de 49,82.

---

## Las cuatro pantallas

| Pantalla | Qué contesta |
|---|---|
| **Hoy** | Con qué se pagó el mes, cuánto salió de las reservas, caja contra resultado, y los avisos. Arriba de todo, la carga rápida. |
| **Trabajos** | Cuánto dejó cada uno y con qué margen. Cuánto cuesta la estructura que ningún trabajo cubre. |
| **Movimientos** | El historial, con **de qué bolsillo salió** y **cuántos meses dura** cada compra. |
| **Bolsillos** | Dónde está la plata, y el botón **Cuadrar** para comparar contra lo que hay de verdad. Mes a mes, cuánto salió de las reservas. |

Atajos: `Ctrl+N` va a anotar, `Ctrl+F` va a buscar.

---

## Los datos con los que arranca

La primera vez siembra el **caso real de agosto de 2026**: los seis movimientos
que están de verdad en la base de la aplicación instalada, más lo que el modelo
viejo no podía representar (de qué bolsillo salió cada peso, a qué trabajo
pertenece cada gasto, cuántos meses dura un rollo de filamento) y dos
movimientos de ejemplo para mostrar el "entregado y sin cobrar".

**Los saldos iniciales de ahorro (400,00) e inversión (600,00) son inventados**:
la aplicación actual nunca los preguntó, así que no existen en ningún lado.
Cambiarlos por los de verdad es la primera cosa que habría que hacer para que
los números signifiquen algo.

El botón *Volver al caso de agosto* borra todo y vuelve a sembrar.

---

## Compilar

Lo mismo que `finaldake-labs`: Visual Studio 2022 (o las Build Tools), CMake
≥ 3.25, Ninja y Qt 6.8 para MSVC 2022 de 64 bits. La ruta de Qt está en
`CMakePresets.json`.

Desde un símbolo del sistema con el entorno de MSVC cargado (`vcvars64.bat`):

```
cmake --preset debug
cmake --build --preset debug
build\debug\bin\dake_pruebas.exe
```

## La nube

Las dos aplicaciones —la del escritorio y la del teléfono— sincronizan contra
el mismo proyecto de Supabase, así que lo que anotás parado en la calle aparece
en la computadora y al revés.

Cómo está armado:

- **Tres tablas remotas** con prefijo `v2_`: `v2_pockets`, `v2_jobs` y
  `v2_movements`. Se crean corriendo `supabase_v2.sql` en el editor SQL del
  panel. El prefijo existe para no tocar la `movements` de la aplicación vieja
  mientras se la descontinúa.
- **Seguridad por fila (RLS)**, con la política `auth.uid() = user_id`. Eso —y
  no que la clave sea secreta— es lo que protege los datos: la clave publicable
  viaja dentro del APK y es pública por diseño.
- **Las credenciales las lee CMake** de `supabase.json` al configurar y las
  incrusta en el binario, así el teléfono llega con todo puesto. Nunca entran
  al repositorio.
- **Nada se marca como enviado antes de que el servidor confirme.** Si se corta
  la red a mitad de camino, esos cambios siguen pendientes y salen en la
  próxima corrida.
- **Un cursor por tabla**, no uno compartido: con uno solo, bajar movimientos
  adelantaría el reloj de los bolsillos y los cambios de bolsillo hechos en el
  medio no bajarían nunca.
- **Los conflictos los resuelve el HLC**, no la fecha. Gana el reloj más alto,
  que es lo que hace converger a dos equipos con relojes distintos.

Para diagnosticar sin abrir ninguna ventana:

```
build\debug\bin\dake_synccheck                      proyecto y tablas
build\debug\bin\dake_synccheck correo contrasena    ademas la sesion
```

Devuelve 0 si todo está bien. No entra en `ctest` a propósito: toca la red, y
una prueba que falla porque se cayó internet enseña a ignorar las pruebas que
fallan.

---

## Compilar para el teléfono

La aplicación del teléfono vive en `mobile/`: Qt Quick (QML) en vez de Widgets,
y comparte `core` y `storage` con la de escritorio sin una línea de diferencia,
para que las dos no puedan decir números distintos.

```
.\construir-android.ps1              deja el APK en apk\FinanzasDakeLabs.apk
.\construir-android.ps1 -Limpiar     borra build\android y empieza de cero
```

Sale firmado con la clave de depuración —la correcta para instalarlo a mano en
el teléfono propio— porque `androiddeployqt` lo entrega **sin firmar** y Android
rechaza un paquete sin firma sin explicar por qué. Publicarlo en Play exigiría
una clave de verdad, que es otra conversación.

Las cuatro pantallas también se revisan sin teléfono y sin abrir una ventana,
igual que las de escritorio:

```
build\debug\bin\dake_movil.exe --captura docs\movil\anotar.png --pagina 0
```

Las páginas van en el orden de las pestañas: `0` anotar, `1` hoy, `2` historial,
`3` datos. La aplicación abre en **anotar**, que es lo que se hace parado en la
calle. Las capturas de las cuatro están en `docs/movil/`.

---

## Verificar

```
build\debug\bin\dake_selftest.exe     # la logica de plata, sin base ni ventana
build\debug\bin\dake_storagetest.exe  # ida y vuelta a SQLite, en una carpeta temporal
```

Las dos devuelven 0 si todo pasa e imprimen una línea por comprobación. También
se corren juntas con `ctest` desde `build\debug`.

`dake_selftest` incluye el caso real de agosto y fija por escrito el hallazgo
central: que el −20,02 que la aplicación actual muestra como "utilidad" es la
variación de caja del mes, y que con el material repartido entre los meses que
dura, agosto cerró con **+47,27** a favor.

### Ver las pantallas sin abrir una ventana

```
build\debug\bin\dake_uipreview.exe todas docs\capturas 1280 1200
```

Renderiza cada pantalla a PNG con el complemento *offscreen*. Existe por lo
mismo que en la aplicación real: verificar con capturas del escritorio obliga a
robar el foco y hacer clics a ciegas.

---

## Cómo está organizado

```
core/      Modelo, dinero, fechas, reportes y avisos. No conoce Qt ni SQL.
           money/date/currency/uuid/hlc vienen tal cual de finaldake-labs.
storage/   SQLite. Unica capa que conoce SQL.
ui/        Qt Widgets. Unica capa que conoce Qt.
tests/     Las dos verificaciones.
docs/      Capturas rendereadas.
```

No hay capa de sincronización: es un banco de pruebas y la nube no es lo que
está en discusión. Los campos `hlc`, `deviceId` y `deleted` están igual desde el
día uno, porque agregarlos después sobre datos reales duele y agregarles el
transporte encima es trivial.

---

## Lo que NO resuelve

Para no venderlo por más de lo que es:

- **La aplicación del teléfono nunca se probó en un teléfono de verdad.** El
  APK compila y firma, y las cuatro pantallas se revisaron renderizadas, pero
  eso no es lo mismo que instalarla y anotar un gasto parado en la calle.
- **Las dos mitades no se hablan.** El teléfono tiene su propia base y la
  computadora la suya. Pasar los datos de una a otra hoy no se puede.
- **No hay nube ni sincronización.**
- **El reparto del costo es de grano mensual**, no por consumo real. Es una
  aproximación deliberada: llevar inventario de gramos de filamento cuesta más
  trabajo del que devuelve.
- **Las categorías siguen siendo texto libre.** La normalización (fricción 4.2)
  no está resuelta; sí está resuelto que el desplegable aprenda de lo ya usado y
  sugiera por subcadena.
