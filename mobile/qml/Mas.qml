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
}
