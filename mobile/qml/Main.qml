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
//
// Cinco pestañas es el maximo: por encima de eso los objetivos bajan de los
// 48 dp que necesita un dedo. Lo que se mira una vez por mes —Trabajos, Cierre
// de mes— y lo que casi nunca —la nube, el diagnostico— vive dentro de "Mas" y
// se abre encima, a pantalla completa, con el boton Atras de Android para
// volver.

ApplicationWindow {
    id: ventana

    visible: true
    width: 420
    height: 880
    title: "Finanzas DakeLabs"
    color: Estilo.fondo

    /// Con que pestaña arrancar. Solo la usa el modo captura, para poder
    /// revisar las pantallas desde la computadora.
    /// Se aplica al CAMBIAR y no en Component.onCompleted: C++ la escribe
    /// despues de que el QML termino de crearse, asi que en onCompleted todavia
    /// valdria cero y la captura saldria siempre de la primera pestaña.
    property int paginaInicial: 0
    onPaginaInicialChanged: barra.currentIndex = paginaInicial

    /// Con que pantalla apilada arrancar, para poder capturarlas tambien.
    /// Vacio es lo normal: no hay ninguna encima.
    property string pantallaInicial: ""
    onPantallaInicialChanged: {
        if (pantallaInicial !== "") {
            apilar(pantallaInicial + ".qml", pantallaInicial)
        }
    }

    Material.theme: Material.Dark
    Material.accent: Estilo.acento
    Material.background: Estilo.fondo
    Material.foreground: Estilo.texto

    function avisar(texto, tinte) {
        aviso.tinte = tinte
        aviso.texto = texto
        aviso.reiniciar()
    }

    function apilar(archivo, titulo) {
        pila.push(envoltorio, { fuente: archivo, titulo: titulo })
    }

    // El boton Atras de Android llega como un pedido de cerrar la ventana. Si
    // hay una pantalla encima, lo que quiere decir es "volve", no "cerra la
    // aplicacion": se rechaza el cierre y se saca esa pantalla. Sin esto, tocar
    // Atras dentro de Trabajos cierra la aplicacion entera, que es lo que hace
    // que la gente deje de usar el boton Atras.
    onClosing: function(cierre) {
        if (pila.depth > 0) {
            pila.pop()
            cierre.accepted = false
        }
    }

    // --- Cuando no se pudo arrancar ---------------------------------------
    //
    // Va primero y tapa todo lo demas. Si la base no abrio, ninguna de las
    // pantallas de abajo tiene nada que mostrar, y una pantalla vacia se lee
    // como una aplicacion rota sin decir por que.
    Loader {
        anchors.fill: parent
        z: 200
        active: App.fatalError !== ""
        source: "Fallo.qml"
    }

    Item {
        anchors.fill: parent
        visible: App.fatalError === ""

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

            Bolsillos {
                onMensaje: function(texto, tinte) { ventana.avisar(texto, tinte) }
            }

            Mas {
                onMensaje: function(texto, tinte) { ventana.avisar(texto, tinte) }
                onAbrir: function(pantalla, titulo) { ventana.apilar(pantalla, titulo) }
            }
        }

        // --- Navegacion ---------------------------------------------------
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
                model: ["Anotar", "Hoy", "Historial", "Bolsillos", "Mas"]

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
                            font.pixelSize: 11
                            font.weight: barra.currentIndex === pestania.index
                                         ? Font.DemiBold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // --- Las pantallas que se abren encima --------------------------------
    StackView {
        id: pila
        anchors.fill: parent
        z: 100
        // Sin esto la pila vacia sigue capturando los toques y la aplicacion
        // parece trabada: se ven las pestañas pero no responden.
        visible: depth > 0
        enabled: visible

        background: Rectangle { color: Estilo.fondo }
    }

    Component {
        id: envoltorio

        Item {
            id: raiz
            property string fuente: ""
            property string titulo: ""

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Encabezado {
                    titulo: raiz.titulo
                    onVolver: pila.pop()
                }

                Loader {
                    id: cargador
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: Qt.resolvedUrl(raiz.fuente)

                    // Las pantallas apiladas avisan igual que las de pestaña.
                    // `ignoreUnknownSignals` deja pasar a las que no tienen nada
                    // que avisar, como el diagnostico.
                    Connections {
                        target: cargador.item
                        ignoreUnknownSignals: true
                        function onMensaje(texto, tinte) { ventana.avisar(texto, tinte) }
                    }
                }
            }
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

        z: 300
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: barra.height + 14
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
}
