# Estado

Al 2026-09-04. **La sincronización funciona de punta a punta**, verificada
contra el proyecto real y no en simulacro.

---

## Qué quedó probado

Corriendo el motor de verdad —no código de prueba— con la sesión guardada:

| Qué | Cómo se comprobó |
|---|---|
| **Subida** | La cola pasó de 17 pendientes a 0. Los 4 bolsillos, 3 trabajos y 10 movimientos están en Supabase. |
| **Se marca enviado solo con confirmación** | Antes de la migración, con el servidor rechazando, la cola quedó intacta en 17. |
| **Bajada** | Se cambió un movimiento en el servidor y apareció cambiado en la base local. Después se restauró. |
| **Sin bucle de eco** | Al bajar ese cambio, la cola quedó en 0: `applyRemote` aplica sin encolar. |
| **Sin duplicados** | Después de subir y volver a bajar las 17 filas, siguen siendo 10 movimientos, no 20. |
| **Los tres cursores avanzan** | `sync.cursor.pockets`, `.jobs` y `.movements` quedaron con la marca del servidor. |
| **RLS protege de verdad** | Una escritura con la clave anónima y sin sesión devuelve `42501`. Eso es lo que hace que la clave dentro del APK no alcance para leer tus finanzas. |
| **Conflictos por HLC** | Al bajar filas con el mismo HLC que las locales, se descartaron; con un HLC mayor, pisaron. |

También: `dake_synccheck` sale 0, y `dake_selftest` y `dake_storagetest` pasan
enteros.

---

## Qué falta

**1. Instalar el APK nuevo en el teléfono.**

```
apk\FinanzasDakeLabs.apk
```

Conserva el nombre de paquete anterior, así que se instala encima sin perder lo
anotado. Pestaña **Datos** → sección **NUBE** → tu correo y contraseña. Después
sincroniza solo cada vez que abrís.

**2. Probarlo en el teléfono de verdad.** Es lo único que no puedo verificar
desde acá: el render offscreen no dibuja las barras del sistema ni el teclado.

**3. Mirar los dos botones nuevos de la barra lateral del escritorio.**
`dake_uipreview` renderiza páginas sueltas, no la ventana entera, y abrir la
aplicación roba el foco. Están bien cableados, pero no los vi.

---

## Rastros de las pruebas

- El movimiento `m-01` quedó con `hlc = 0000000000002-00000-prueba` en vez del
  `0` de la siembra. Es inofensivo: sigue por debajo de cualquier HLC real
  —que empieza con los 13 dígitos de los milisegundos— así que la próxima
  edición desde cualquier equipo le gana normalmente.
- Nada más quedó tocado. Los nombres, montos y saldos están como estaban.

---

## Si algo sale mal

```
58914b5  APK con la nube, capturas al dia
deb93fa  Etapa E: la nube en el telefono
b33b077  Los identificadores son texto, no uuid
1d6834e  Lote 3: la nube en el escritorio y el diagnostico
b0e7ed7  Lote 2: la capa de sincronizacion
e20a92a  Lote 1: la cola de salida en el repositorio
ae9432b  Andamiaje de la sincronizacion
17b94a4  El monto usa el teclado del sistema
823e1ce  Que la app deje de pelear con las barras del sistema
a05b6a5  Estado antes del rediseno del telefono
```

`git reset --hard <commit>` vuelve a cualquiera. El `a05b6a5` es la aplicación
tal como estaba antes de empezar.
