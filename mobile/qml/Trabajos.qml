import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Trabajos.qml — cuanto dejo cada trabajo, y cuanto cuesta la estructura.
//
// En el escritorio esto es una tabla de siete columnas. En un telefono una
// tabla de siete columnas no se lee: cada trabajo es una tarjeta, y las tres
// cifras que importan —lo que entro, lo que costo, lo que quedo— van una al
// lado de la otra donde el ojo las compara sin desplazar nada.
//
// El orden y los numeros salen de core::jobResults, el mismo que usa el
// escritorio. Si esta pantalla calculara su propio margen, tarde o temprano no
// coincidiria con el de la computadora y no habria forma de saber cual miente.

Flickable {
    id: pagina

    signal mensaje(string texto, color tinte)

    /// Cuando esta apagado se esconden los trabajos cerrados. Arranca apagado
    /// porque lo que se mira habitualmente es lo que esta en curso.
    property bool verCerrados: false

    contentHeight: contenido.height + 2 * Estilo.margen
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    function visibles() {
        if (verCerrados) {
            return App.allJobs
        }
        return App.allJobs.filter(function(t) { return !t.closed })
    }

    ColumnLayout {
        id: contenido
        x: Estilo.margen
        y: Estilo.margen
        width: pagina.width - 2 * Estilo.margen
        spacing: Estilo.espacio

        // --- Lo que dejaron y lo que cuesta existir --------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: Estilo.espacio
            columnSpacing: Estilo.espacio

            Cifra {
                rotulo: "DEJARON EN TOTAL"
                valor: App.jobsSummary.margin !== undefined ? App.jobsSummary.margin : "—"
                nota: (App.jobsSummary.count !== undefined ? App.jobsSummary.count : 0) + " trabajos"
                tinte: Estilo.positivo
            }
            Cifra {
                rotulo: "ESTRUCTURA DEL MES"
                valor: App.jobsSummary.structure !== undefined ? App.jobsSummary.structure : "—"
                nota: "lo que no cuelga de ningun trabajo"
                tinte: Estilo.negativo
            }
        }

        Label {
            Layout.fillWidth: true
            text: "La estructura es el piso que los trabajos tienen que cubrir antes de dejar "
                  + "algo."
                  + (String(App.jobsSummary.pending) !== ""
                     ? "  Hay " + App.jobsSummary.pending + " entregados y sin cobrar."
                     : "")
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // --- Filtro ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: Estilo.espacio

            Label {
                text: "TRABAJOS"
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
                    text: pagina.verCerrados ? "Ocultar cerrados" : "Ver cerrados"
                    color: Estilo.textoSuave
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: pagina.verCerrados = !pagina.verCerrados
            }
        }

        Label {
            Layout.fillWidth: true
            visible: pagina.visibles().length === 0
            text: pagina.verCerrados
                  ? "Todavia no hay ningun trabajo anotado."
                  : "No hay trabajos abiertos. Los cerrados se ven con el boton de arriba."
            color: Estilo.textoSuave
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: pagina.visibles()

            Rectangle {
                id: tarjeta
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: cuerpo.implicitHeight + 28
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde
                opacity: tarjeta.modelData.closed ? 0.62 : 1.0

                Rectangle {
                    width: 3
                    height: parent.height - 24
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: tarjeta.modelData.marginNegative ? Estilo.negativo : Estilo.positivo
                }

                ColumnLayout {
                    id: cuerpo
                    anchors.fill: parent
                    anchors.margins: 14
                    anchors.leftMargin: 16
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Estilo.espacio

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                Layout.fillWidth: true
                                text: tarjeta.modelData.name
                                color: Estilo.texto
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: String(tarjeta.modelData.client) !== ""
                                text: tarjeta.modelData.client
                                color: Estilo.textoSuave
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }

                        Label {
                            text: tarjeta.modelData.state
                            color: tarjeta.modelData.stateLevel === 1 ? Estilo.aviso
                                                                      : Estilo.textoSuave
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            Layout.maximumWidth: 150
                        }
                    }

                    // --- Entro, costo, quedo ---
                    //
                    // Las tres en una fila y no una debajo de otra: el numero
                    // que importa es el tercero, y solo significa algo al lado
                    // de los dos primeros.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        component Columna: ColumnLayout {
                            id: columna
                            property string rotulo: ""
                            property string valor: ""
                            property color tinte: Estilo.texto
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: columna.rotulo
                                color: Estilo.textoTenue
                                font.pixelSize: 9
                                font.letterSpacing: 0.6
                            }
                            Label {
                                Layout.fillWidth: true
                                text: columna.valor
                                color: columna.tinte
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }

                        Columna {
                            rotulo: "ENTRO"
                            valor: tarjeta.modelData.income
                            tinte: Estilo.positivo
                        }
                        Columna {
                            rotulo: "COSTO"
                            valor: tarjeta.modelData.cost
                            tinte: Estilo.negativo
                        }
                        Columna {
                            rotulo: "QUEDO"
                            valor: tarjeta.modelData.margin + "  " + tarjeta.modelData.marginBps
                            tinte: tarjeta.modelData.marginNegative ? Estilo.negativo
                                                                    : Estilo.texto
                        }
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 100
                        flat: true
                        background: Rectangle {
                            color: parent.pressed ? Estilo.elevado : "transparent"
                            radius: 8
                            border.color: Estilo.borde
                        }
                        contentItem: Label {
                            text: tarjeta.modelData.closed ? "Reabrir" : "Cerrar"
                            color: Estilo.textoSuave
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            const error = App.setJobClosed(tarjeta.modelData.id,
                                                           !tarjeta.modelData.closed)
                            pagina.mensaje(
                                error === ""
                                    ? (tarjeta.modelData.closed ? "Trabajo reabierto."
                                                                : "Trabajo cerrado.")
                                    : error,
                                error === "" ? Estilo.positivo : Estilo.aviso)
                        }
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.topMargin: 4
            flat: true
            background: Rectangle {
                color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                radius: 10
                border.color: Estilo.borde
            }
            contentItem: Label {
                text: "+  Nuevo trabajo"
                color: Estilo.texto
                font.pixelSize: 14
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: dialogoNuevo.open()
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 6
            text: "Cerrar un trabajo solo lo saca del selector al anotar. Sus numeros siguen "
                  + "contando en los reportes, porque el trabajo se hizo igual."
            color: Estilo.textoTenue
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: 8 }
    }

    Dialog {
        id: dialogoNuevo

        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(pagina.width - 2 * Estilo.margen, 420)
        title: "Nuevo trabajo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: {
            nombre.text = ""
            cliente.text = ""
            nombre.forceActiveFocus()
        }

        onAccepted: {
            const error = App.addJob(nombre.text, cliente.text)
            pagina.mensaje(error === "" ? "Trabajo creado." : error,
                           error === "" ? Estilo.positivo : Estilo.aviso)
        }

        ColumnLayout {
            width: parent.width
            spacing: Estilo.espacio

            TextField {
                id: nombre
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "Que trabajo es"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 16
            }

            TextField {
                id: cliente
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "Para quien (opcional)"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 16
            }
        }
    }
}
