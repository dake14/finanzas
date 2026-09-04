import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Fila.qml — una linea de "rotulo a la izquierda, valor a la derecha" que se
// toca entera.
//
// El area sensible es toda la fila y no solo el texto del valor: en un telefono
// el objetivo tiene que ser lo mas grande que permita el diseño, no lo mas
// chico que se pueda dibujar.

ItemDelegate {
    id: control

    property string rotulo: ""
    property string valor: ""
    property color colorValor: Estilo.texto
    property bool mostrarFlecha: true

    Layout.fillWidth: true
    height: Estilo.toque
    padding: 0

    background: Rectangle {
        color: control.pressed ? Estilo.elevado : "transparent"
        radius: 8
    }

    contentItem: RowLayout {
        spacing: Estilo.espacio

        Label {
            text: control.rotulo
            color: Estilo.textoSuave
            font.pixelSize: 13
            Layout.leftMargin: 4
        }

        Item { Layout.fillWidth: true }

        Label {
            text: control.valor
            color: control.colorValor
            font.pixelSize: 15
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            Layout.maximumWidth: control.width * 0.6
        }

        Label {
            visible: control.mostrarFlecha
            text: "›"
            color: Estilo.textoTenue
            font.pixelSize: 20
            Layout.rightMargin: 4
        }
    }
}
