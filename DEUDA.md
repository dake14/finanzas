# Deuda

Cosas que hoy funcionan y se van a romper solas si nadie las toca. No son
tareas pendientes: son compromisos que ya asumimos y que tienen fecha de
vencimiento aunque nadie la haya escrito.

---

## 1. OpenSSL en el teléfono lo mantenemos nosotros

**Qué.** El APK empaqueta `libssl_3.so` y `libcrypto_3.so` bajadas a mano de
[KDAB/android_openssl](https://github.com/KDAB/android_openssl). Están en
`third_party/android_openssl/arm64-v8a/` y sus SHA-256 en el `README.md` de esa
carpeta.

**Por qué existe.** Qt para Android no trae OpenSSL, y Android no expone la
suya a las aplicaciones desde API 24. Sin esas dos bibliotecas
`QSslSocket::supportsSsl()` da falso y **toda** petición HTTPS falla antes de
salir del teléfono — o sea, no hay sincronización. En Windows no pasa porque
ahí Qt usa el TLS del sistema operativo.

**Qué se rompe si nadie hace nada.** Nada, visiblemente. Ese es el problema:
cuando OpenSSL publique un parche de seguridad, el teléfono va a seguir usando
la versión vieja sin avisar, y el APK se instala una vez y queda.

**Qué hacer.** Cuando salga un parche de OpenSSL 3.x: bajar las bibliotecas
nuevas de KDAB, reemplazar las dos, anotar los SHA-256 nuevos, y volver a
generar el APK con `construir-android.ps1`. Verificar después con
`dake_synccheck`, que imprime el respaldo TLS que quedó activo.

**Cómo se revisa.** No hay aviso automático. La forma barata es mirar
[openssl-library.org/news/vulnerabilities](https://openssl-library.org/news/vulnerabilities/)
cada tanto, o suscribirse a `openssl-announce`.

---

## 2. La clave anónima viaja dentro del APK

**Qué.** `DAKE_SUPABASE_ANON_KEY` se incrusta al compilar, así que cualquiera
que abra el APK la puede leer.

**Por qué está bien igual.** Es el diseño de Supabase: esa clave sola no
alcanza para nada. Lo que protege los datos es RLS, y está verificado — una
escritura sin sesión devuelve `42501`.

**Cuándo deja de estar bien.** Si algún día se agrega una tabla sin política de
RLS, esa tabla queda abierta a cualquiera con el APK. La regla es: **ninguna
tabla nueva sin su política**, y comprobarlo con una petición sin sesión antes
de dar por hecho que quedó protegida.

---

## 3. El token de sesión se guarda en claro

**Qué.** `sync.refresh_token` vive en la tabla `settings` de la base local, sin
cifrar, en las dos aplicaciones.

**Qué implica.** Quien tenga el archivo tiene la sesión hasta que se revoque.
En la computadora está bajo tu usuario de Windows; en el teléfono, dentro del
almacenamiento privado de la app, que otra aplicación no puede leer sin root.

**Por qué se dejó así.** Cifrarlo requiere una clave que también hay que
guardar en algún lado, y en un equipo personal eso mueve el problema sin
resolverlo. Si alguna vez la app se instala en una máquina compartida, esto
deja de ser aceptable y hay que pasar a `QtKeychain` o al almacén de
credenciales de Windows.

---

## 4. Rastro de las pruebas de sincronización

El movimiento `m-01` quedó con `hlc = 0000000000002-00000-prueba` en vez del
`0` de la siembra. Es inofensivo —sigue por debajo de cualquier HLC real, que
empieza con los trece dígitos de los milisegundos— así que la próxima edición
desde cualquier equipo le gana normalmente. Se anota para que nadie lo
investigue de nuevo dentro de seis meses.
