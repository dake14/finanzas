# Lo que queda pendiente

Al 2026-09-04. Este archivo existe porque el trabajo quedó terminado salvo un
paso que **solo podés dar vos**, y sin él nada de la sincronización funciona.

---

## 1. Correr la migración en Supabase — BLOQUEA TODO LO DEMÁS

```
supabase_v2_ids_a_texto.sql
```

Panel de Supabase → SQL Editor → New query → pegar → Run.

**Por qué no lo hice yo:** la única credencial que la aplicación guarda es la
clave anónima, que sirve para leer y escribir filas pero **no puede alterar
tablas**. La que sí podría es la `service_role`, que por diseño no está en
ningún archivo de este proyecto — justamente porque saltea las políticas RLS.

**Qué arregla:** la primera versión de `supabase_v2.sql` declaró los
identificadores como `uuid`. Fue un error de diseño: `core::Id` es una cadena
opaca y la siembra usa ids legibles (`p-caja`, `j-macbook`, `m-01`). El síntoma
es el error que ya viste:

```
400  22P02  invalid input syntax for type uuid: "p-caja"
```

Y como Postgres rechaza el lote **entero** ante una fila mal tipada, no sube
absolutamente nada: las 17 filas de la cola se quedan donde están.

La migración no borra ni pierde datos: solo cambia el tipo de las columnas.

---

## 2. Comprobar que quedó bien

```
build\debug\bin\dake_synccheck correo contrasena
```

Tiene que salir con código 0 e imprimir `HTTP 200` en las tres tablas. Si algo
falla, dice en qué paso exacto.

Después, en la aplicación de escritorio: botón **Conectar**, y sincroniza sola.
`Ctrl+S` para las siguientes.

---

## 3. Instalar el APK nuevo en el teléfono

```
apk\FinanzasDakeLabs.apk
```

Conserva el nombre de paquete anterior, así que se instala encima sin perder lo
anotado. Pestaña **Datos** → sección **NUBE** → tu correo y contraseña.

---

## Lo que quedó verificado, y lo que no

**Verificado de punta a punta:**

- Las tres tablas existen y responden (`dake_synccheck` sale 0).
- RLS bloquea de verdad: una escritura sin sesión devuelve `42501`. Eso es lo
  que protege los datos con la clave viajando dentro del APK.
- El teléfono restaura la sesión al arrancar y dispara la sincronización solo:
  se comprobó renderizando la pantalla Datos, que trajo el error real del
  servidor.
- `dake_selftest` y `dake_storagetest` pasan enteros.
- El APK: `targetSdk 34`, nombre "Finanzas", ícono propio, firma v2+v3, y
  **solo** el permiso de INTERNET.

**No verificado, porque no puedo:**

- Que una fila suba y baje de verdad. Necesita la migración del punto 1 y tu
  contraseña.
- Los dos botones nuevos de la barra lateral del escritorio. `dake_uipreview`
  renderiza páginas sueltas, no la ventana entera, y abrir la aplicación de
  verdad roba el foco. Están bien cableados, pero no los vi.
- Cualquier cosa que dependa de las barras del sistema en el teléfono: el
  render offscreen no las dibuja.

---

## Si algo sale mal

Cada etapa quedó en su propio commit:

```
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

`git reset --hard <commit>` vuelve a cualquiera de esos puntos. El `a05b6a5` es
la aplicación tal como estaba antes de que empezáramos.
