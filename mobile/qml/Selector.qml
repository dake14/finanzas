import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Selector.qml — elegir uno de una lista, a pantalla casi completa.
//
// Un ComboBox de escritorio abre un menu de items de 24 px que en un telefono
// se erran con el dedo. Esto es una hoja que sube desde abajo con filas del
// tamaño de un dedo, que es como se elige en un telefono.

Dialog {
    id: control

    property alias modelo: lista.model
    /// Nombre de la propiedad que se muestra en cada fila.
    property string campoTexto: "label"
    /// Nombre de la propiedad que se devuelve al elegir.
    property string campoValor: "id"
    /// Texto opcional debajo del principal.
    property string campoDetalle: ""

    signal elegido(var valor, string texto)

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 2 * Estilo.margen : 360, 460)
    height: Math.min(parent ? parent.height * 0.7 : 500, 620)
    padding: 0
    standardButtons: Dialog.Cancel

    background: Rectangle {
        color: Estilo.tarjeta
        radius: Estilo.radio
        border.color: Estilo.borde
    }

    header: Label {
        text: control.title
        color: Estilo.texto
        font.pixelSize: 16
        font.weight: Font.DemiBold
        padding: Estilo.margen
    }

    contentItem: ListView {
        id: lista
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            required property var modelData
            width: lista.width
            height: Estilo.toque

            contentItem: ColumnLayout {
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: modelData[control.campoTexto] !== undefined
                          ? modelData[control.campoTexto] : String(modelData)
                    color: Estilo.texto
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    visible: control.campoDetalle !== ""
                             && modelData[control.campoDetalle] !== undefined
                             && String(modelData[control.campoDetalle]) !== ""
                    text: visible ? modelData[control.campoDetalle] : ""
                    color: Estilo.textoSuave
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            onClicked: {
                var valor = modelData[control.campoValor] !== undefined
                            ? modelData[control.campoValor] : modelData
                var texto = modelData[control.campoTexto] !== undefined
                            ? modelData[control.campoTexto] : String(modelData)
                control.elegido(valor, texto)
                control.close()
            }
        }
    }
}
