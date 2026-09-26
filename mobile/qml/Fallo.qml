import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Fallo.qml — la pantalla que se ve cuando la aplicacion no pudo arrancar.
//
// EXISTE PARA QUE NUNCA MAS HAYA UNA PANTALLA NEGRA.
//
// Antes, si abrir la base fallaba, la excepcion salia de main() y en Android
// eso mata el proceso antes de que exista una sola ventana: lo que veia el
// usuario era negro, sin una palabra, y no habia forma de saber por que. Un
// libro de cuentas que se abre en negro se desinstala.
//
// Ahora ese fallo llega hasta aca escrito, con la ruta del archivo y el
// cuaderno de arranque debajo. No arregla el problema, pero convierte "no abre"
// en "no abre PORQUE", que es la unica diferencia que importa cuando la
// aplicacion esta en otro telefono y en otra ciudad.

Rectangle {
    id: pagina

    color: Estilo.fondo

    Flickable {
        anchors.fill: parent
        contentHeight: contenido.height + 2 * Estilo.margen
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: contenido
            x: Estilo.margen
            y: Estilo.margen + 20
            width: pagina.width - 2 * Estilo.margen
            spacing: Estilo.espacio

            Label {
                text: "No se pudo abrir"
                color: Estilo.texto
                font.pixelSize: 26
                font.weight: Font.Bold
            }

            Label {
                Layout.fillWidth: true
                text: "Tus datos NO se tocaron. La aplicacion se detuvo antes de escribir nada, "
                      + "que es lo correcto: seguir adelante sobre una base que no abre bien es "
                      + "como se pierden meses de anotaciones."
                color: Estilo.textoSuave
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            // --- El motivo, con todas las letras --------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.preferredHeight: motivo.implicitHeight + 28
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.negativo

                ColumnLayout {
                    id: motivo
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    Label {
                        text: "QUE PASO"
                        color: Estilo.negativo
                        font.pixelSize: 9
                        font.letterSpacing: 0.6
                    }
                    Label {
                        Layout.fillWidth: true
                        text: App.fatalError
                        color: Estilo.texto
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 10
                text: "QUE HACER"
                color: Estilo.textoSuave
                font.pixelSize: 10
                font.letterSpacing: 0.8
            }

            Label {
                Layout.fillWidth: true
                text: "1. Cerra la aplicacion del todo y volve a abrirla. Un fallo pasajero "
                      + "—el telefono sin espacio, la base a medio escribir por un apagon— se "
                      + "arregla solo en el segundo intento.\n\n"
                      + "2. Fijate que le quede espacio libre al telefono.\n\n"
                      + "3. Si sigue igual, la base esta en la ruta de abajo. Al lado suyo hay "
                      + "copias `.bak` fechadas de antes de cada cambio de esquema: una de esas "
                      + "sirve para recuperar lo anotado."
                color: Estilo.textoSuave
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Layout.preferredHeight: rutas.implicitHeight + 24
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde

                ColumnLayout {
                    id: rutas
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "BASE DE DATOS"
                        color: Estilo.textoSuave
                        font.pixelSize: 9
                        font.letterSpacing: 0.6
                    }
                    Label {
                        Layout.fillWidth: true
                        text: App.dbPath
                        color: Estilo.texto
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            // --- El cuaderno de arranque -----------------------------------------
            Label {
                Layout.fillWidth: true
                Layout.topMargin: 14
                text: "LO ULTIMO QUE HIZO"
                color: Estilo.textoSuave
                font.pixelSize: 10
                font.letterSpacing: 0.8
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: registro.implicitHeight + 20
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde

                ColumnLayout {
                    id: registro
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2

                    Repeater {
                        model: App.startupLog()

                        Label {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            color: modelData.indexOf("FALLO") >= 0
                                   || modelData.indexOf("GRAVE") >= 0
                                   ? Estilo.negativo : Estilo.textoSuave
                            font.pixelSize: 10
                            font.family: "monospace"
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
