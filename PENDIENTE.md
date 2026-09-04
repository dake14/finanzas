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

**1. Correr `supabase_v3_fecha_cobro.sql`** en el panel. Agrega la columna de
la fecha de cobro del lado del servidor. Sin eso, el teléfono y la computadora
van a discrepar en esa columna al sincronizar. Es un `add column if not
exists`: no borra ni pierde nada.

**2. Instalar el APK nuevo** (`apk\FinanzasDakeLabs.apk`). Se instala encima
del anterior sin perder lo anotado.

**3. Los "días en cobrar" van a decir `—` al principio**, y está bien. Ninguno
de tus movimientos tiene fecha de cobro porque el campo no existía hasta hoy.
Se va a ir llenando a medida que edites movimientos y marques cuándo cobraste.
Preferí eso antes que rellenar con la fecha de hoy, que te habría dado un
promedio inventado desde el primer día.

**4. El teléfono no podía iniciar sesión: era OpenSSL, no tu contraseña.**

Encontrado el 2026-09-04. Qt trae dos respaldos TLS para Android: `certonly`,
que solo lee certificados y no abre conexiones, y `openssl`, que carga las
bibliotecas con `dlopen` en tiempo de ejecución. Qt no las incluye y Android no
expone las suyas desde API 24, así que `QSslSocket::supportsSsl()` daba falso y
**toda petición HTTPS fallaba antes de salir del teléfono**.

En Windows nada de eso pasa: Qt usa `qschannelbackend`, el TLS del sistema
operativo. `dake_synccheck` ahora lo imprime — en esta computadora dice
`TLS disponible: si (Secure Channel, Windows 10.0.26200)`.

Ya está arreglado: el APK empaqueta `libssl_3.so` y `libcrypto_3.so`. Y si
alguna vez vuelve a faltar, la app lo dice con todas las letras en vez de
fallar como si fueran las credenciales.

**Deuda que queda:** esas bibliotecas ahora las mantenemos nosotros. Cuando
OpenSSL saque un parche de seguridad hay que bajarlas de nuevo. Los SHA-256 de
las actuales están en `third_party/android_openssl/README.md`.

**5. Si aun así no podés conectarte con tu usuario y contraseña**, lo más probable es
que la cuenta nunca haya tenido una contraseña: si la creaste con un enlace por
correo, Supabase no fija ninguna, y probar la de siempre falla con
"credenciales inválidas" sin decir por qué.

Se arregla en el panel: **Authentication → Users → tu usuario → Reset password**.
Ahí le ponés una y con esa entrás desde las dos aplicaciones.

Comprobado el 2026-09-04: el servidor responde `invalid_credentials` a una
contraseña equivocada —no "email sin confirmar" ni un error de formato—, así
que la cuenta existe, está confirmada y el login funciona. Lo único que no
coincide es la contraseña.

**6. Probar en el teléfono de verdad.** Es lo único que no puedo verificar: el
render offscreen no dibuja las barras del sistema ni el teclado.

---

## Lo que se agregó a Hoy

En las dos aplicaciones, calculado por el mismo núcleo:

| | |
|---|---|
| **PARA NO PERDER** | Cuánto facturar por mes para cubrir la estructura. El margen se **pondera por facturación**: un trabajo de 10 con 90% y uno de 1000 con 5% dan 5,84%, no el 47,5% que daría promediar. |
| **MESES DE RESERVA** | Cuánto aguantás al ritmo actual. |
| **TICKET PROMEDIO** | Cuánto deja un trabajo, y cuántos hubo. |
| **DÍAS EN COBRAR** | Cuánto tardás en cobrar lo entregado. |
| **COSTO DE ESTRUCTURA** | Cuánto cuesta el negocio existiendo, y qué % de los ingresos se come. |
| **Cinco gráficas** | Resultado por mes, ingresos contra costos, caja acumulada, gastos por categoría y margen por trabajo. |

Cuando un número no se puede saber, aparece un `—` con la explicación al lado.
Nunca un cero ni un número inventado: un dato ausente disfrazado de cifra es
peor que un guion, porque el guion se nota.

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
