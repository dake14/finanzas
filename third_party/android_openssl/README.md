# OpenSSL para Android

`libssl_3.so` y `libcrypto_3.so` compiladas para `arm64-v8a`, tomadas de
[KDAB/android_openssl](https://github.com/KDAB/android_openssl), Apache-2.0.
Es la fuente a la que apunta la documentación de Qt.

Descargadas el 2026-09-04 de la rama `master`, carpeta `ssl_3/arm64-v8a`:

| Archivo | Tamaño | SHA-256 |
|---|---|---|
| `libssl_3.so` | 634 680 | `01d2bd0baac626efd3309f35f99c4b826dd9b885a7e4d14d5b12b3603d3a407f` |
| `libcrypto_3.so` | 4 030 424 | `1e6c12ae0c2dadfe9d178d7f80f0ab248a1877a234066098bf5594e5205e5740` |

## Por qué están acá

Qt trae dos respaldos TLS para Android: `qcertonlybackend`, que solo sabe leer
certificados y no abre conexiones, y `qopensslbackend`, que **carga OpenSSL en
tiempo de ejecución con `dlopen`**. Esas bibliotecas Qt no las incluye, y
Android no expone las suyas a las aplicaciones desde API 24.

El resultado, sin estos archivos, es que `QSslSocket::supportsSsl()` devuelve
falso y **toda petición HTTPS falla** — el síntoma que se veía era que iniciar
sesión no funcionaba nunca desde el teléfono, mientras que desde Windows
andaba perfecto. En Windows, Qt usa `qschannelbackend`, el TLS del propio
sistema operativo, y por eso ahí no hace falta configurar nada.

## Lo que esto cuesta

**Actualizarlas es responsabilidad nuestra.** Cuando OpenSSL saque un arreglo
de seguridad, estas copias siguen siendo las viejas hasta que alguien las
reemplace. Hay que volver a bajarlas de KDAB y comprobar los SHA-256 nuevos.

La alternativa sin esa deuda es hablar por JNI con `HttpsURLConnection`, la
pila del sistema, que Android mantiene parcheada sola. Se evaluó el 2026-09-04
y se descartó por ahora: es escribir un transporte nuevo solo para Android y
dejar el motor de sincronización con dos caminos según la plataforma.

## Solo arm64-v8a

Es la única arquitectura que compila `construir-android.ps1`
(`QT_ANDROID_ABIS=arm64-v8a`), que es la de cualquier teléfono de los últimos
diez años. Si algún día se agrega otra, hay que bajar también su carpeta.
