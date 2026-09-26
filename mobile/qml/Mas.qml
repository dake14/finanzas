import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DakeMobile

// Mas.qml — lo que no se mira todos los dias.
//
// Cinco pestañas abajo es el maximo que entra sin que los objetivos bajen de
// los 48 dp que necesita un dedo. Anotar, Hoy, Historial y Bolsillos son
// diarias o semanales; Trabajos y Cierre se miran una vez por mes, y la nube y
// el diagnostico casi nunca. Esas viven aca y se abren a pantalla completa.
//
// Exportar no es un extra: lo que se anota en la calle existe solamente hasta
// que llega a la computadora. El archivo lleva TODO —incluidas las lapidas de
// lo borrado— y gana siempre el reloj mas alto, asi que importarlo dos veces no
// cambia nada y no hay que acordarse de si ya se hizo.

Flickable {
    id: pagina

    signal mensaje(string texto, color tinte)
    /// Le pide a Main.qml que apile una pantalla. El nombre es el del archivo.
    signal abrir(string pantalla, string titulo)

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

        // --- Las pantallas que se miran una vez por mes -----------------------
        Label {
            text: "MIRAR"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: navegacion.implicitHeight + 12
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            ColumnLayout {
                id: navegacion
                anchors.fill: parent
                anchors.margins: 6
                spacing: 0

                Fila {
                    rotulo: "Trabajos"
                    valor: (App.jobsSummary.count !== undefined ? App.jobsSummary.count : 0)
                           + " anotados"
                    onClicked: pagina.abrir("Trabajos.qml", "Trabajos")
                }
                Fila {
                    rotulo: "Cierre de mes"
                    valor: App.closingMonths().length > 0 ? App.closingMonths()[0] : "—"
                    onClicked: pagina.abrir("Cierre.qml", "Cierre de mes")
                }
            }
        }

        // --- Nube --------------------------------------------------------------
        Label {
            text: "NUBE"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 12
        }

        Nube {
            Layout.fillWidth: true
            onMensaje: function(texto, tinte) { pagina.mensaje(texto, tinte) }
        }

        // --- Pasar los datos a la computadora ----------------------------------
        Label {
            text: "PASAR LOS DATOS A LA COMPUTADORA"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 12
        }

        Label {
            Layout.fillWidth: true
            text: "Con la nube conectada esto no hace falta. Sigue estando para cuando no hay "
                  + "internet: exportar escribe un archivo con todo, se pasa como se quiera y "
                  + "del otro lado se importa. Importar dos veces el mismo archivo no cambia "
                  + "nada."
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

        // --- Cuando algo no anda ------------------------------------------------
        Label {
            text: "CUANDO ALGO NO ANDA"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 12
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: mantenimiento.implicitHeight + 12
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            ColumnLayout {
                id: mantenimiento
                anchors.fill: parent
                anchors.margins: 6
                spacing: 0

                Fila {
                    rotulo: "Diagnostico"
                    valor: "que hizo al arrancar"
                    onClicked: pagina.abrir("Diagnostico.qml", "Diagnostico")
                }
            }
        }

        // --- Borrar todo ---------------------------------------------------------
        //
        // Esta abajo de todo y a un paso mas de distancia que cualquier otra
        // cosa, con el nombre escrito a mano para confirmar. No alcanza con un
        // "seguro?": un dialogo de si o no se acepta sin leerlo, y esto no
        // tiene vuelta desde el telefono.
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.topMargin: 14
            flat: true
            background: Rectangle {
                color: parent.pressed ? "#2a1216" : "transparent"
                radius: 10
                border.color: Estilo.negativo
            }
            contentItem: Label {
                text: "Borrar todo"
                color: Estilo.negativo
                font.pixelSize: 14
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: dialogoBorrar.open()
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
        id: dialogoBorrar

        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(pagina.width - 2 * Estilo.margen, 420)
        title: "Borrar todo"
        standardButtons: Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.negativo
        }

        onOpened: {
            confirmacion.text = ""
            confirmacion.forceActiveFocus()
        }

        ColumnLayout {
            width: parent.width
            spacing: Estilo.espacio

            Label {
                Layout.fillWidth: true
                text: "Se van todos los bolsillos, trabajos y movimientos. Cada uno queda con "
                      + "lapida, asi que el borrado tambien viaja a la nube y a la computadora: "
                      + "no es solo en este telefono.\n\n"
                      + "Antes de hacerlo, exporta."
                color: Estilo.textoSuave
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: "Escribi BORRAR para confirmar:"
                color: Estilo.texto
                font.pixelSize: 13
            }

            TextField {
                id: confirmacion
                Layout.fillWidth: true
                Layout.preferredHeight: Estilo.toque
                placeholderText: "BORRAR"
                color: Estilo.texto
                placeholderTextColor: Estilo.textoTenue
                font.pixelSize: 16
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                enabled: confirmacion.text === "BORRAR"
                opacity: enabled ? 1.0 : 0.4
                background: Rectangle {
                    color: parent.pressed ? "#2a1216" : "transparent"
                    radius: 10
                    border.color: Estilo.negativo
                }
                contentItem: Label {
                    text: "Borrar todo, sin vuelta atras"
                    color: Estilo.negativo
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    const resultado = App.eraseAll()
                    dialogoBorrar.close()
                    pagina.mensaje(resultado, Estilo.aviso)
                }
            }
        }
    }
}
