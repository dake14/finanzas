import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Cierre.qml — como termino un mes que ya paso.
//
// La misma pantalla que la de escritorio, con los mismos cinco numeros y la
// misma comparacion contra el mes anterior. Es la unica pantalla que mira
// hacia atras: todas las demas contestan "como estoy hoy", y esta contesta
// "como me fue".
//
// Los meses van del mas nuevo al mas viejo porque el cierre que uno abre es
// siempre el ultimo, y arranca en ese.

Flickable {
    id: pagina

    /// Indice dentro de `meses`. Cero es el mes mas reciente con actividad.
    property int mesElegido: 0
    property var meses: App.closingMonths()
    property var datos: App.closing(mesElegido)

    // Los dos se recalculan cuando cambian los datos: anotar un movimiento de
    // un mes nuevo agrega un mes a la lista, y sin esto el selector seguiria
    // mostrando la lista vieja.
    Connections {
        target: App
        function onDataChanged() {
            pagina.meses = App.closingMonths()
            if (pagina.mesElegido >= pagina.meses.length) {
                pagina.mesElegido = 0
            }
            pagina.datos = App.closing(pagina.mesElegido)
        }
    }

    onMesElegidoChanged: datos = App.closing(mesElegido)

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

        // --- Que mes se esta mirando ------------------------------------------
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            visible: pagina.meses.length > 0
            flat: true
            background: Rectangle {
                color: parent.pressed ? Estilo.elevado : Estilo.tarjeta
                radius: 10
                border.color: Estilo.borde
            }
            contentItem: RowLayout {
                spacing: Estilo.espacio
                Label {
                    Layout.leftMargin: 6
                    text: "MES"
                    color: Estilo.textoSuave
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: pagina.datos.etiqueta !== undefined ? pagina.datos.etiqueta : "—"
                    color: Estilo.texto
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.rightMargin: 6
                    text: "▾"
                    color: Estilo.textoTenue
                    font.pixelSize: 14
                }
            }
            onClicked: selectorMes.open()
        }

        Label {
            Layout.fillWidth: true
            text: pagina.datos.contra !== undefined ? pagina.datos.contra : ""
            color: Estilo.textoSuave
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // --- Los cinco numeros -------------------------------------------------
        //
        // Resultado arriba de todo y solo, porque es el que contesta la
        // pregunta. Los otros cuatro existen para poder no creerle: son de
        // donde sale.
        Cifra {
            visible: pagina.datos.vacio !== true
            rotulo: "RESULTADO DEL MES"
            valor: pagina.datos.resultado !== undefined ? pagina.datos.resultado : "—"
            nota: "facturado del mes menos el costo que le toca"
            tinte: Estilo.acento
            negativo: pagina.datos.resultadoNegativo === true
        }

        GridLayout {
            Layout.fillWidth: true
            visible: pagina.datos.vacio !== true
            columns: 2
            rowSpacing: Estilo.espacio
            columnSpacing: Estilo.espacio

            Cifra {
                rotulo: "FACTURADO"
                valor: pagina.datos.facturado !== undefined ? pagina.datos.facturado : "—"
                nota: "cobrado o no"
                tinte: Estilo.positivo
            }
            Cifra {
                rotulo: "COSTO IMPUTADO"
                valor: pagina.datos.costo !== undefined ? pagina.datos.costo : "—"
                nota: "con las compras grandes repartidas"
                tinte: Estilo.negativo
            }
            Cifra {
                rotulo: "VARIACION DE CAJA"
                valor: pagina.datos.caja !== undefined ? pagina.datos.caja : "—"
                nota: "lo que de verdad entro y salio"
                tinte: Estilo.inversion
                negativo: pagina.datos.cajaNegativa === true
            }
            Cifra {
                rotulo: "QUEDO POR COBRAR"
                valor: pagina.datos.porCobrar !== undefined ? pagina.datos.porCobrar : "—"
                nota: "al cerrar ese mes"
                tinte: Estilo.ahorro
            }
        }

        Label {
            Layout.fillWidth: true
            visible: pagina.datos.vacio !== true
            text: "Resultado y caja casi nunca son iguales, y no es un error: cuatro kilos de "
                  + "material mueven la caja hoy y el costo durante los meses que duran."
            color: Estilo.textoTenue
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        // --- En que se fue -----------------------------------------------------
        Label {
            visible: pagina.datos.vacio !== true
            text: "EN QUE SE FUE"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 12
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: categoriasColumna.implicitHeight + 20
            visible: pagina.datos.vacio !== true
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            ColumnLayout {
                id: categoriasColumna
                anchors.fill: parent
                anchors.margins: 10
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 6
                    visible: pagina.datos.categorias === undefined
                             || pagina.datos.categorias.length === 0
                    text: "Ese mes no tuvo gastos anotados."
                    color: Estilo.textoSuave
                    font.pixelSize: 13
                }

                Repeater {
                    model: pagina.datos.categorias !== undefined ? pagina.datos.categorias : []

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        spacing: Estilo.espacio

                        Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 6
                            text: modelData.etiqueta
                            color: Estilo.texto
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.rightMargin: 6
                            text: modelData.texto
                            color: Estilo.negativo
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: pagina.datos.vacio === true
            text: "Todavia no hay ningun mes con movimientos. Anota algo y el cierre aparece "
                  + "solo."
            color: Estilo.textoSuave
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: 8 }
    }

    Selector {
        id: selectorMes
        title: "Que mes"
        modelo: pagina.meses
        onElegido: function(valor, texto) {
            pagina.mesElegido = pagina.meses.indexOf(texto)
        }
    }
}
