import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Diagnostico.qml — que hizo la aplicacion la ultima vez que arranco.
//
// EXISTE POR LA PANTALLA NEGRA.
//
// Cuando una aplicacion de telefono no abre, no hay consola donde mirar. Lo
// unico que queda es lo que ella misma haya dejado anotado antes de irse. Esta
// pantalla muestra ese cuaderno: cada paso del arranque con su hora, y los
// avisos de Qt que normalmente no ve nadie.
//
// Si alguna vez vuelve a quedarse en negro, la corrida que fallo esta escrita
// aca al abrir la siguiente. Eso convierte "se queda en negro" —que no se puede
// investigar— en "se quedo despues de abrir la base", que si.

Flickable {
    id: pagina

    signal mensaje(string texto, color tinte)

    property var lineas: App.startupLog()

    contentHeight: contenido.height + 2 * Estilo.margen
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: contenido
        x: Estilo.margen
        y: Estilo.margen
        width: pagina.width - 2 * Estilo.margen
        spacing: Estilo.espacio

        Label {
            Layout.fillWidth: true
            text: "Lo que la aplicacion anoto al arrancar. Si alguna vez no abre, la corrida "
                  + "que fallo queda escrita aca."
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // --- Donde vive cada cosa ---------------------------------------------
        Rectangle {
            Layout.fillWidth: true
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
                    Layout.fillWidth: true
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
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    text: "CUADERNO DE ARRANQUE"
                    color: Estilo.textoSuave
                    font.pixelSize: 9
                    font.letterSpacing: 0.6
                }
                Label {
                    Layout.fillWidth: true
                    text: App.startupLogPath()
                    color: Estilo.texto
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        // --- El cuaderno --------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            spacing: Estilo.espacio

            Label {
                text: "ULTIMAS LINEAS"
                color: Estilo.textoSuave
                font.pixelSize: 10
                font.letterSpacing: 0.8
            }
            Item { Layout.fillWidth: true }
            Button {
                Layout.preferredHeight: 34
                flat: true
                padding: 12
                background: Rectangle {
                    color: parent.pressed ? Estilo.elevado : "transparent"
                    radius: 8
                    border.color: Estilo.borde
                }
                contentItem: Label {
                    text: "Actualizar"
                    color: Estilo.textoSuave
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: pagina.lineas = App.startupLog()
            }
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
                    model: pagina.lineas

                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        // Los renglones que empiezan con === separan una corrida
                        // de la siguiente, y los que dicen FALLO o GRAVE son
                        // justamente los que se buscan. Pintarlos distinto
                        // ahorra leer cincuenta lineas iguales.
                        color: modelData.indexOf("===") === 0 ? Estilo.acento
                             : (modelData.indexOf("FALLO") >= 0
                                || modelData.indexOf("GRAVE") >= 0
                                || modelData.indexOf("FATAL") >= 0) ? Estilo.negativo
                             : modelData.indexOf("AVISO") >= 0 ? Estilo.aviso
                             : Estilo.textoSuave
                        font.pixelSize: 10
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}
