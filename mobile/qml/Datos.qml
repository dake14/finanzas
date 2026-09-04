import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DakeMobile

// Datos.qml — los bolsillos, y como sacar los datos del telefono.
//
// Lo segundo no es un extra: lo que se anota en la calle existe solamente hasta
// que llega a la computadora. Exportar escribe un archivo con TODO —incluidas
// las lapidas de lo borrado— y del otro lado se importa. Gana siempre el reloj
// mas alto, asi que importar dos veces el mismo archivo no cambia nada y no hay
// que acordarse de si ya se hizo.

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

        // --- Bolsillos ------------------------------------------------------
        Label {
            text: "BOLSILLOS"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
        }

        Repeater {
            model: App.pockets

            Rectangle {
                id: tarjetaBolsillo
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 62
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 8
                    spacing: Estilo.espacio

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: tarjetaBolsillo.modelData.name
                            color: Estilo.texto
                            font.pixelSize: 15
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: tarjetaBolsillo.modelData.kindLabel
                                  + (tarjetaBolsillo.modelData.pending !== ""
                                     ? "  ·  sin cobrar " + tarjetaBolsillo.modelData.pending
                                     : "")
                            color: Estilo.textoSuave
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Label {
                        text: tarjetaBolsillo.modelData.balance
                        color: tarjetaBolsillo.modelData.negative ? Estilo.negativo : Estilo.texto
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Button {
                        Layout.preferredWidth: 78
                        Layout.preferredHeight: 36
                        flat: true
                        background: Rectangle {
                            color: parent.pressed ? Estilo.elevado : "transparent"
                            radius: 8
                            border.color: Estilo.borde
                        }
                        contentItem: Label {
                            text: "Cuadrar"
                            color: Estilo.textoSuave
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: cuadrar.pedir(tarjetaBolsillo.modelData.id,
                                                 tarjetaBolsillo.modelData.name,
                                                 tarjetaBolsillo.modelData.balance)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Estilo.espacio

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                text: "Nuevo bolsillo"
                flat: true
                background: Rectangle {
                    color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                    radius: 10
                    border.color: Estilo.borde
                }
                contentItem: Label {
                    text: parent.text
                    color: Estilo.texto
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: nuevoBolsillo.open()
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                text: "Nuevo trabajo"
                flat: true
                background: Rectangle {
                    color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                    radius: 10
                    border.color: Estilo.borde
                }
                contentItem: Label {
                    text: parent.text
                    color: Estilo.texto
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: nuevoTrabajo.open()
            }
        }

        // --- Pasar los datos -------------------------------------------------
        Label {
            text: "PASAR LOS DATOS A LA COMPUTADORA"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 10
        }

        Label {
            Layout.fillWidth: true
            text: "Exportar escribe un archivo con todo. Pasalo a la computadora como quieras "
                  + "—cable, correo, la nube que uses— y ahi importalo. Importar dos veces el "
                  + "mismo archivo no cambia nada."
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Estilo.espacio

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                background: Rectangle {
                    color: parent.pressed ? "#0ea5e9" : Estilo.acento
                    radius: 10
                }
                contentItem: Label {
                    text: "Exportar"
                    color: Estilo.fondo
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    dialogoExportar.currentFile = "file:///" + App.suggestedFileName()
                    dialogoExportar.open()
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                flat: true
                background: Rectangle {
                    color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                    radius: 10
                    border.color: Estilo.borde
                }
                contentItem: Label {
                    text: "Importar"
                    color: Estilo.texto
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: dialogoImportar.open()
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 14
            text: "Base de datos:\n" + App.dbPath
            color: Estilo.textoTenue
            font.pixelSize: 10
            wrapMode: Text.WrapAnywhere
        }

        Item { Layout.preferredHeight: 8 }
    }

    // ------------------------------------------------------------- Dialogos --

    FileDialog {
        id: dialogoExportar
        title: "Guardar el archivo"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Cambios de DakeLabs (*.jsonl)"]
        onAccepted: pagina.mensaje(App.exportTo(selectedFile), Estilo.positivo)
    }

    FileDialog {
        id: dialogoImportar
        title: "Elegir el archivo a importar"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Cambios de DakeLabs (*.jsonl)", "Todos los archivos (*)"]
        onAccepted: pagina.mensaje(App.importFrom(selectedFile), Estilo.positivo)
    }

    Dialog {
        id: cuadrar
        property string bolsilloId: ""

        title: "Cuadrar con la realidad"
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 2 * Estilo.margen, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        function pedir(id, nombre, saldo) {
            bolsilloId = id
            dice.text = "La aplicacion dice que en \"" + nombre + "\" hay " + saldo + "."
            real.text = ""
            open()
        }

        onAccepted: {
            var error = App.reconcile(bolsilloId, real.text)
            pagina.mensaje(error === "" ? "Ajuste anotado." : error,
                           error === "" ? Estilo.positivo : Estilo.aviso)
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label {
                id: dice
                Layout.fillWidth: true
                color: Estilo.texto
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            TextField {
                id: real
                Layout.fillWidth: true
                placeholderText: "Cuanto hay de verdad"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            Label {
                Layout.fillWidth: true
                text: "La diferencia queda anotada como un movimiento con fecha y nombre, no "
                      + "como un saldo corregido por debajo."
                color: Estilo.textoSuave
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: nuevoBolsillo
        title: "Nuevo bolsillo"
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 2 * Estilo.margen, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: { nombreBolsillo.text = ""; saldoBolsillo.text = "" }
        onAccepted: {
            var error = App.addPocket(nombreBolsillo.text, claseBolsillo.currentIndex,
                                      saldoBolsillo.text)
            pagina.mensaje(error === "" ? "Bolsillo creado." : error,
                           error === "" ? Estilo.positivo : Estilo.negativo)
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            TextField {
                id: nombreBolsillo
                Layout.fillWidth: true
                placeholderText: "Caja del negocio"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
            }
            ComboBox {
                id: claseBolsillo
                Layout.fillWidth: true
                // El orden es el de core::PocketKind, para que el indice que
                // manda la pantalla sea el que entiende el nucleo.
                model: ["Operacion — el dia a dia", "Ahorro — reserva",
                        "Inversion — capital trabajando", "Personal — lo que ya te pagaste"]
            }
            TextField {
                id: saldoBolsillo
                Layout.fillWidth: true
                placeholderText: "Cuanto hay hoy"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            Label {
                Layout.fillWidth: true
                text: "Sin el saldo de hoy, ningun numero va a cuadrar nunca contra lo que "
                      + "tenes en la mano."
                color: Estilo.textoSuave
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: nuevoTrabajo
        title: "Nuevo trabajo"
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 2 * Estilo.margen, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: { nombreTrabajo.text = ""; clienteTrabajo.text = "" }
        onAccepted: {
            var error = App.addJob(nombreTrabajo.text, clienteTrabajo.text)
            pagina.mensaje(error === "" ? "Trabajo creado." : error,
                           error === "" ? Estilo.positivo : Estilo.negativo)
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            TextField {
                id: nombreTrabajo
                Layout.fillWidth: true
                placeholderText: "Reparacion Macbook"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
            }
            TextField {
                id: clienteTrabajo
                Layout.fillWidth: true
                placeholderText: "Cliente"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
            }
        }
    }
}
