import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Bolsillos.qml — donde esta la plata, y el boton para cuadrarla.
//
// Es la misma pantalla que la de escritorio. Los saldos son el UNICO numero de
// toda la aplicacion que se puede contar a mano y comparar contra la realidad;
// todo lo demas se deduce de ellos. Por eso Cuadrar esta a un toque de
// distancia y no escondido en ajustes: un saldo que no cuadra vuelve falsos
// todos los reportes que salen de el, y cuanto mas tarde se cuadra mas caro
// sale encontrar de donde vino la diferencia.

Flickable {
    id: pagina

    signal mensaje(string texto, color tinte)

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

        // --- Cuanto hay en total ---------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: Estilo.espacio
            columnSpacing: Estilo.espacio

            Cifra {
                rotulo: "EN TOTAL"
                valor: App.pocketsSummary.total !== undefined ? App.pocketsSummary.total : "—"
                nota: "sumando todos los bolsillos"
                tinte: Estilo.acento
                negativo: App.pocketsSummary.negative === true
            }
            Cifra {
                rotulo: "DE ESO, RESERVA"
                valor: App.pocketsSummary.reserves !== undefined ? App.pocketsSummary.reserves : "—"
                nota: "ahorro, inversion y emergencia"
                tinte: Estilo.ahorro
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Es el unico numero de toda la aplicacion que se puede contar a mano y "
                  + "comparar. Si no cuadra, cuadralo hoy y no en tres meses: despues no hay "
                  + "forma de saber de donde salio la diferencia."
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // --- Un bolsillo por tarjeta ------------------------------------------
        Label {
            text: "BOLSILLOS"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 10
        }

        Repeater {
            model: App.pockets

            Rectangle {
                id: tarjeta
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 88
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde

                Rectangle {
                    width: 3
                    height: parent.height - 24
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: Estilo.colorBolsillo(tarjeta.modelData.kind)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    anchors.leftMargin: 16
                    spacing: Estilo.espacio

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: tarjeta.modelData.name
                            color: Estilo.texto
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            text: tarjeta.modelData.kindLabel
                            color: Estilo.textoSuave
                            font.pixelSize: 11
                        }
                        Label {
                            visible: String(tarjeta.modelData.pending) !== ""
                            text: visible ? "por cobrar " + tarjeta.modelData.pending : ""
                            color: Estilo.aviso
                            font.pixelSize: 11
                        }
                    }

                    ColumnLayout {
                        spacing: 6
                        Layout.alignment: Qt.AlignRight

                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: tarjeta.modelData.balance
                            color: tarjeta.modelData.negative ? Estilo.negativo : Estilo.texto
                            font.pixelSize: 19
                            font.weight: Font.Bold
                        }

                        Button {
                            Layout.alignment: Qt.AlignRight
                            Layout.preferredHeight: 34
                            Layout.preferredWidth: 92
                            flat: true
                            background: Rectangle {
                                color: parent.pressed ? Estilo.elevado : "transparent"
                                radius: 8
                                border.color: Estilo.borde
                            }
                            contentItem: Label {
                                text: "Cuadrar"
                                color: Estilo.textoSuave
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                dialogoCuadrar.bolsilloId = tarjeta.modelData.id
                                dialogoCuadrar.bolsilloNombre = tarjeta.modelData.name
                                dialogoCuadrar.saldoApp = tarjeta.modelData.balance
                                dialogoCuadrar.open()
                            }
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
                text: "+  Nuevo bolsillo"
                color: Estilo.texto
                font.pixelSize: 14
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: dialogoNuevo.open()
        }

        // --- Mes a mes --------------------------------------------------------
        //
        // La misma tarjeta que en el escritorio. Lo que contesta es si el mes se
        // pago solo o se pago con reserva, que no es lo mismo aunque los dos
        // terminen con la luz pagada.
        Label {
            text: "MES A MES"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 14
        }

        Label {
            Layout.fillWidth: true
            text: "Cuanto salio de las reservas cada mes. Sacar del ahorro no es una perdida y "
                  + "no aparece en ningun resultado, pero se repite hasta que no queda reserva."
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: mesesColumna.implicitHeight + 20
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            ColumnLayout {
                id: mesesColumna
                anchors.fill: parent
                anchors.margins: 10
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 6
                    visible: App.months.length === 0
                    text: "Todavia no hay meses con actividad."
                    color: Estilo.textoSuave
                    font.pixelSize: 13
                }

                Repeater {
                    // Del mes mas nuevo al mas viejo: el que interesa es el
                    // ultimo, y en un telefono lo que esta arriba es lo unico
                    // que se ve sin desplazar.
                    model: App.months.slice().reverse()

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        spacing: Estilo.espacio

                        Label {
                            Layout.preferredWidth: 64
                            Layout.leftMargin: 6
                            text: modelData.etiqueta
                            color: Estilo.texto
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Label {
                            text: modelData.resultadoTexto
                            color: modelData.resultadoNegativo ? Estilo.negativo : Estilo.positivo
                            font.pixelSize: 13
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            Layout.rightMargin: 6
                            Layout.maximumWidth: 150
                            text: modelData.fondeoTexto
                            color: modelData.fondeoNivel === 2 ? Estilo.negativo
                                 : modelData.fondeoNivel === 1 ? Estilo.positivo
                                 : Estilo.textoSuave
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }

    // ------------------------------------------------------------- Dialogos --

    Dialog {
        id: dialogoCuadrar

        property string bolsilloId: ""
        property string bolsilloNombre: ""
        property string saldoApp: ""

        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(pagina.width - 2 * Estilo.margen, 420)
        title: "Cuadrar " + bolsilloNombre
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: {
            real.text = ""
            real.forceActiveFocus()
        }

        onAccepted: {
            const error = App.reconcile(dialogoCuadrar.bolsilloId, real.text)
            pagina.mensaje(error === "" ? "Cuadrado." : error,
                           error === "" ? Estilo.positivo : Estilo.aviso)
        }

        ColumnLayout {
            width: parent.width
            spacing: Estilo.espacio

            Label {
                Layout.fillWidth: true
                text: "La aplicacion dice " + dialogoCuadrar.saldoApp + ". Conta lo que hay de "
                      + "verdad y escribilo. La diferencia queda anotada como un movimiento que "
                      + "se ve, no como un saldo corregido por debajo."
                color: Estilo.textoSuave
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            TextField {
                id: real
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "Lo que hay de verdad"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 18
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }
    }

    Dialog {
        id: dialogoNuevo

        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(pagina.width - 2 * Estilo.margen, 420)
        title: "Nuevo bolsillo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: {
            nombre.text = ""
            inicial.text = ""
            clase.currentIndex = 0
            nombre.forceActiveFocus()
        }

        onAccepted: {
            const error = App.addPocket(nombre.text, clase.currentIndex, inicial.text)
            pagina.mensaje(error === "" ? "Bolsillo creado." : error,
                           error === "" ? Estilo.positivo : Estilo.aviso)
        }

        ColumnLayout {
            width: parent.width
            spacing: Estilo.espacio

            TextField {
                id: nombre
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "Nombre"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 16
            }

            ComboBox {
                id: clase
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                // El orden es el de core::PocketKind y no otro: si aca dijera
                // "Ahorro" primero, elegir el primero guardaria "Operacion".
                model: ["Operacion", "Ahorro", "Inversion", "Personal", "Emergencia"]
            }

            TextField {
                id: inicial
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "Saldo inicial (opcional)"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 16
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            Label {
                Layout.fillWidth: true
                text: "El saldo inicial es lo que hay hoy en ese bolsillo. Si lo dejas vacio "
                      + "arranca en cero y se va llenando con movimientos."
                color: Estilo.textoSuave
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
