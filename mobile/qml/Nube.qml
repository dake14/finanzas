// Nube.qml — conectarse y sincronizar, desde el telefono.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// Es un COMPONENTE, no una pantalla: se usa dentro de Datos.qml, arriba de la
// seccion "Pasar los datos". No se agrega una quinta pestaña: las cuatro
// actuales —anotar, hoy, historial, datos— ya estan decididas, y la nube es
// otra forma de lo mismo que hace exportar, asi que vive al lado.
//
// Todo lo que necesita se lo pide al singleton `App` (mobile/appbridge.hpp):
//
//   App.signedIn      bool     hay sesion
//   App.userEmail     string   correo conectado, vacio si no hay
//   App.cloudStatus   string   ultima novedad, para mostrar tal cual
//   App.pendingCount  int      cambios locales esperando subir
//   App.syncing       bool     hay una corrida en curso
//   App.signIn(correo, contrasena)
//   App.signOut()
//   App.sync()
//
// QUE TIENE QUE MOSTRAR
//
//   Sin sesion:
//     - Un campo de correo y uno de contrasena (echoMode TextInput.Password).
//     - Un boton "Conectar", deshabilitado si falta cualquiera de los dos.
//
//   Con sesion:
//     - El correo conectado.
//     - Un boton "Sincronizar", deshabilitado mientras App.syncing.
//     - Cuantos cambios esperan subir, si App.pendingCount > 0.
//     - Un boton "Desconectar", discreto: no es la accion principal.
//
//   Siempre: App.cloudStatus abajo, en letra chica y color textoSuave.
//
// REGLAS
//
//   - La contrasena se limpia (campo.text = "") apenas se llama a App.signIn.
//     No queda en pantalla ni en memoria del componente.
//   - Los dos campos usan el teclado del sistema, como el resto de la
//     aplicacion. El correo pide Qt.ImhEmailCharactersOnly.
//   - Nada de configurar URL ni clave: vienen incrustadas al compilar.
//   - Objetivos de toque de Estilo.toque como minimo, igual que todo lo demas:
//     esto se usa parado en la calle.
//   - Colores y medidas SOLO desde el singleton Estilo. Ni un color escrito a
//     mano: si aparece un "#rrggbb" en este archivo, esta mal.
//
// ACEPTACION: la pantalla Datos renderiza sin errores de QML con
// `dake_movil.exe --captura <png> --pagina 3`, sin sesion y con la nube
// visible arriba de "Pasar los datos".
//
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

ColumnLayout {
    id: control

    signal mensaje(string texto, color tinte)

    spacing: Estilo.espacio

    // --- Sin sesion ---
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Estilo.espacio
        visible: !App.signedIn

        TextField {
            id: correo
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            placeholderText: "Correo"
            color: Estilo.texto
            placeholderTextColor: Estilo.textoTenue
            font.pixelSize: 16
            inputMethodHints: Qt.ImhEmailCharactersOnly
            background: Rectangle {
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: correo.activeFocus ? Estilo.acento : Estilo.borde
            }
        }

        TextField {
            id: contrasena
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            placeholderText: "Contraseña"
            color: Estilo.texto
            placeholderTextColor: Estilo.textoTenue
            font.pixelSize: 16
            echoMode: TextInput.Password
            background: Rectangle {
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: contrasena.activeFocus ? Estilo.acento : Estilo.borde
            }
        }

        Button {
            id: botonConectar
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            enabled: correo.text.trim() !== "" && contrasena.text !== ""
            background: Rectangle {
                radius: Estilo.radio
                color: botonConectar.enabled ? Estilo.acento : Estilo.elevado
                opacity: botonConectar.pressed ? 0.8 : 1.0
            }
            contentItem: Label {
                text: "Conectar"
                color: botonConectar.enabled ? Estilo.fondo : Estilo.textoTenue
                font.pixelSize: 16
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                App.signIn(correo.text.trim(), contrasena.text)
                contrasena.text = ""
            }
        }
    }

    // --- Con sesion ---
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Estilo.espacio
        visible: App.signedIn

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: App.pendingCount > 0 ? 62 : Estilo.toque
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 1
                
                Label {
                    text: App.userEmail
                    color: Estilo.texto
                    font.pixelSize: 15
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: App.pendingCount > 0
                    text: App.pendingCount + (App.pendingCount === 1 ? " cambio pendiente" : " cambios pendientes")
                    color: Estilo.textoSuave
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Button {
            id: botonSincronizar
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            enabled: !App.syncing
            background: Rectangle {
                radius: Estilo.radio
                color: botonSincronizar.enabled ? Estilo.acento : Estilo.elevado
                opacity: botonSincronizar.pressed ? 0.8 : 1.0
            }
            contentItem: Label {
                text: "Sincronizar"
                color: botonSincronizar.enabled ? Estilo.fondo : Estilo.textoTenue
                font.pixelSize: 16
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: App.sync()
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            flat: true
            background: Rectangle {
                color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde
            }
            contentItem: Label {
                text: "Desconectar"
                color: Estilo.texto
                font.pixelSize: 14
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: App.signOut()
        }
    }

    // --- Estado de la nube ---
    Label {
        Layout.fillWidth: true
        text: App.cloudStatus
        color: Estilo.textoSuave
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }
}
