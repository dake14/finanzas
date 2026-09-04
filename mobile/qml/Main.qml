import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import DakeMobile

// Main.qml — la ventana.
//
// La barra de navegacion va ABAJO y no arriba: es donde llega el pulgar. Y la
// primera pestaña es Anotar, no el tablero, porque el 90% de las veces que uno
// abre esto en la calle es para anotar algo antes de olvidarlo.

ApplicationWindow {
    id: ventana

    visible: true
    width: 420
    height: 880
    title: "Finanzas DakeLabs"
    color: Estilo.fondo

    /// Con que pestaña arrancar. Solo la usa el modo captura, para poder
    /// revisar las cuatro pantallas desde la computadora.
    /// Se aplica al CAMBIAR y no en Component.onCompleted: C++ la escribe
    /// despues de que el QML termino de crearse, asi que en onCompleted todavia
    /// valdria cero y la captura saldria siempre de la primera pestaña.
    property int paginaInicial: 0
    onPaginaInicialChanged: barra.currentIndex = paginaInicial

    Material.theme: Material.Dark
    Material.accent: Estilo.acento
    Material.background: Estilo.fondo
    Material.foreground: Estilo.texto

    function avisar(texto, tinte) {
        aviso.tinte = tinte
        aviso.texto = texto
        aviso.reiniciar()
    }

    StackLayout {
        id: paginas
        anchors.fill: parent
        anchors.bottomMargin: barra.height
        currentIndex: barra.currentIndex

        Anotar {
            onMensaje: function(texto, tinte) { ventana.avisar(texto, tinte) }
        }

        Hoy {}

        Historial {
            onMensaje: function(texto, tinte) { ventana.avisar(texto, tinte) }
        }

        Datos {
            onMensaje: function(texto, tinte) { ventana.avisar(texto, tinte) }
        }
    }

    // --- Aviso pasajero ---------------------------------------------------
    // Un cartel que se va solo y no tapa nada: confirmar que se guardo tiene
    // que costar cero toques, o se deja de mirar y se anota dos veces.
    Rectangle {
        id: aviso

        property string texto: ""
        property color tinte: Estilo.texto
        function reiniciar() { opacity = 1; reloj.restart() }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: barra.top
        anchors.bottomMargin: 14
        width: Math.min(etiqueta.implicitWidth + 32, ventana.width - 2 * Estilo.margen)
        height: etiqueta.implicitHeight + 22
        radius: height / 2
        color: Estilo.elevado
        border.color: Estilo.borde
        opacity: 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 220 } }

        Label {
            id: etiqueta
            anchors.centerIn: parent
            width: aviso.width - 32
            text: aviso.texto
            color: aviso.tinte
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Timer {
            id: reloj
            interval: 2600
            onTriggered: aviso.opacity = 0
        }
    }

    // --- Navegacion -------------------------------------------------------
    TabBar {
        id: barra
        anchors.bottom: parent.bottom
        width: parent.width
        height: 62
        currentIndex: 0

        background: Rectangle {
            color: Estilo.tarjeta
            Rectangle {
                width: parent.width
                height: 1
                color: Estilo.borde
            }
        }

        Repeater {
            model: ["Anotar", "Hoy", "Historial", "Datos"]

            TabButton {
                id: pestania
                required property string modelData
                required property int index

                background: Rectangle { color: "transparent" }
                contentItem: ColumnLayout {
                    spacing: 3
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 22
                        height: 3
                        radius: 2
                        color: barra.currentIndex === pestania.index
                               ? Estilo.acento : "transparent"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: pestania.modelData
                        color: barra.currentIndex === pestania.index
                               ? Estilo.texto : Estilo.textoSuave
                        font.pixelSize: 12
                        font.weight: barra.currentIndex === pestania.index
                                     ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
