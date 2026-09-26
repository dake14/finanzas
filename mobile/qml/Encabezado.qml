import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Encabezado.qml — el titulo y la flecha de volver de una pantalla apilada.
//
// Las pantallas de pestaña no lo llevan: no hay a donde volver desde ellas. Lo
// llevan las que se abren desde Mas —Trabajos, Cierre, Nube, Diagnostico—,
// donde la flecha tiene que estar arriba a la izquierda aunque el pulgar no
// llegue, porque es donde todo el mundo la busca. El boton Atras de Android
// hace lo mismo y es el que se usa de verdad.

Rectangle {
    id: control

    property string titulo: ""
    signal volver()

    Layout.fillWidth: true
    implicitHeight: 56
    color: Estilo.fondo

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: Estilo.margen
        spacing: 4

        ItemDelegate {
            Layout.preferredWidth: Estilo.toque
            Layout.preferredHeight: Estilo.toque
            padding: 0
            background: Rectangle {
                color: parent.pressed ? Estilo.elevado : "transparent"
                radius: 8
            }
            contentItem: Label {
                text: "‹"
                color: Estilo.texto
                font.pixelSize: 28
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: control.volver()
        }

        Label {
            Layout.fillWidth: true
            text: control.titulo
            color: Estilo.texto
            font.pixelSize: 18
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Estilo.borde
    }
}
