// DakeActivity.java — la actividad de Android de la aplicacion.
//
// EXISTE POR UN SOLO MOTIVO: la pantalla negra al volver a abrir.
//
// Sintoma: recien instalada la aplicacion abre bien. Se cierra, se vuelve a
// abrir, y queda en negro para siempre hasta desinstalarla o forzar la
// detencion desde los ajustes del telefono.
//
// Causa: Qt corre `main()` UNA sola vez por proceso, en su propio hilo. Android
// puede destruir la Activity y dejar el proceso vivo —al salir con Atras, al
// deslizarla fuera de recientes, o simplemente porque necesitaba memoria—. Al
// volver a abrir, Android reusa ese proceso y crea una Activity NUEVA con una
// superficie de dibujo NUEVA, pero el `main()` de Qt ya corrio: nadie vuelve a
// crear la ventana, nadie la conecta a esa superficie, y lo que se ve es la
// superficie vacia. Negra.
//
// Solucion: cuando la Activity se destruye de verdad, matar el proceso. Asi la
// proxima apertura SIEMPRE es un arranque limpio, con su `main()`, su ventana y
// su superficie. Es lo que el usuario cree que esta pasando igual.
//
// Por que es seguro matar el proceso en esta aplicacion:
//   - SQLite confirma cada transaccion en el momento; no hay nada en memoria
//     esperando a guardarse.
//   - Lo que falta subir a la nube vive en la tabla `outbox`, en disco, y se
//     sube en la proxima corrida. Cortar a la mitad no pierde nada: nada se
//     marca como enviado antes de que el servidor lo confirme.
//   - No hay trabajo en segundo plano, ni notificaciones, ni servicios.
//
// `isChangingConfigurations()` deja afuera el unico caso en que Android destruye
// la Activity para recrearla enseguida (girar la pantalla, cambiar el tema).
// Ahi matar el proceso cerraria la aplicacion en la cara del usuario. En la
// practica casi no pasa, porque el manifiesto declara esos cambios en
// `configChanges` y Android ni siquiera destruye nada, pero la guarda queda por
// si esa lista cambia alguna vez.

package com.dakelabs.finanzas.pruebas;

import org.qtproject.qt.android.bindings.QtActivity;

public class DakeActivity extends QtActivity
{
    @Override
    protected void onDestroy()
    {
        super.onDestroy();

        if (!isChangingConfigurations()) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }
}
