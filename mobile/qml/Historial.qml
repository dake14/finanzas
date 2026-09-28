import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Historial.qml — lo ultimo que se anoto.
//
// Existe sobre todo para una cosa: darse cuenta en el momento de que un
// movimiento salio mal y poder borrarlo ahi mismo. Una lista sin forma de
// corregir obliga a esperar a llegar a la computadora, y para entonces ya no se
// acuerda uno de cual estaba mal.

Item {
    id: pagina

    signal mensaje(string texto, color tinte)

    ListView {
        id: lista
        anchors.fill: parent
        anchors.margins: Estilo.margen
        model: App.recent
        spacing: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        header: Label {
            width: lista.width
            text: App.recent.length === 0
                  ? "Todavia no hay movimientos."
                  : App.recent.length + " movimientos · manten apretado para borrar"
            color: Estilo.textoSuave
            font.pixelSize: 11
            bottomPadding: 8
        }

        delegate: ItemDelegate {
            id: fila
            required property var modelData
            width: lista.width
            height: 66
            padding: 0

            background: Rectangle {
                color: fila.pressed ? Estilo.elevado : Estilo.tarjeta
                radius: 10
                border.color: Estilo.borde
            }

            contentItem: RowLayout {
                spacing: Estilo.espacio

                // Franja del color del tipo: verde entra, rojo sale, violeta se
                // mueve entre bolsillos propios. Se lee sin leer.
                Rectangle {
                    Layout.leftMargin: 10
                    Layout.preferredWidth: 3
                    Layout.preferredHeight: 40
                    radius: 2
                    color: Estilo.colorMovimiento(fila.modelData.kind)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: fila.modelData.name
                              + (fila.modelData.settled ? "" : "  · sin cobrar")
                        color: Estilo.texto
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: {
                            var partes = [App.relativeDate(fila.modelData.date),
                                          fila.modelData.where]
                            if (fila.modelData.job !== "") partes.push(fila.modelData.job)
                            return partes.join(" · ")
                        }
                        color: Estilo.textoSuave
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Label {
                    Layout.rightMargin: 12
                    text: fila.modelData.amount
                    color: Estilo.colorMovimiento(fila.modelData.kind)
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
            }

            // Mantener apretado y no un boton de basura por fila: el boton
            // pequeño al lado de una fila que se toca para otra cosa es como se
            // borran movimientos sin querer.
            onPressAndHold: confirmar.pedir(fila.modelData.id, fila.modelData.name)
        }
    }

    Dialog {
        id: confirmar
        property string idMovimiento: ""

        title: "Borrar movimiento"
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 2 * Estilo.margen, 420)
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        function pedir(id, nombre) {
            idMovimiento = id
            texto.text = "¿Borrar \"" + nombre + "\"?"
            open()
        }

        onAccepted: {
            var error = App.removeMovement(idMovimiento)
            pagina.mensaje(error === "" ? "Borrado." : error,
                           error === "" ? Estilo.positivo : Estilo.negativo)
        }

        Label {
            id: texto
            width: parent.width
            color: Estilo.texto
            wrapMode: Text.WordWrap
        }
    }
}
