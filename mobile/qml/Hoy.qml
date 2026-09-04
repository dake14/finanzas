import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Hoy.qml — las cuatro cifras y la frase que importa.
//
// Es la misma pantalla que la de escritorio, recortada a lo que se mira parado
// en la calle: cuanto hay, cuanto salio del ahorro, y que conviene hacer hoy.
// El analisis largo —mes a mes, margen por trabajo— se queda en la computadora,
// que es donde uno se sienta a mirar numeros.

Flickable {
    id: pagina

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

        component Tarjeta: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
            Layout.fillWidth: true
        }

        // --- Cuatro cifras -------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: Estilo.espacio
            columnSpacing: Estilo.espacio

            component Cifra: Rectangle {
                id: cifra
                property string rotulo: ""
                property string valor: "—"
                property string nota: ""
                property color tinte: Estilo.acento

                Layout.fillWidth: true
                Layout.preferredHeight: 104
                color: Estilo.tarjeta
                radius: Estilo.radio
                border.color: Estilo.borde

                Rectangle {
                    width: 3
                    height: parent.height - 24
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: cifra.tinte
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    anchors.leftMargin: 16
                    spacing: 2

                    Label {
                        text: cifra.rotulo
                        color: Estilo.textoSuave
                        font.pixelSize: 10
                        font.letterSpacing: 0.8
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: cifra.valor
                        color: Estilo.texto
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillHeight: true }
                    Label {
                        text: cifra.nota
                        color: Estilo.textoSuave
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            Cifra {
                rotulo: "CAJA DEL NEGOCIO"
                valor: App.summary.cash !== undefined ? App.summary.cash : "—"
                nota: "lo que hay para trabajar"
                tinte: Estilo.acento
            }
            Cifra {
                rotulo: "AHORRO E INVERSION"
                valor: App.summary.reserves !== undefined ? App.summary.reserves : "—"
                nota: App.summary.eating ? "bajando este mes" : "sin tocar este mes"
                tinte: Estilo.ahorro
            }
            Cifra {
                rotulo: "HECHO Y SIN COBRAR"
                valor: App.summary.pending !== undefined ? App.summary.pending : "—"
                nota: "trabajo entregado, plata no"
                tinte: Estilo.aviso
            }
            Cifra {
                rotulo: "MATERIAL POR DELANTE"
                valor: App.summary.prepaid !== undefined ? App.summary.prepaid : "—"
                nota: "pagado, sin consumir"
                tinte: Estilo.textoSuave
            }
        }

        // --- La frase ------------------------------------------------------
        Tarjeta {
            Layout.preferredHeight: frase.height + 28

            ColumnLayout {
                id: frase
                x: 14
                y: 14
                width: parent.width - 28
                spacing: 6

                Label {
                    text: "CON QUE SE PAGO ESTE MES"
                    color: Estilo.textoSuave
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }
                Label {
                    Layout.fillWidth: true
                    text: App.summary.headline !== undefined ? App.summary.headline : ""
                    color: App.summary.eating ? Estilo.negativo : Estilo.positivo
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: App.summary.headlineDetail !== undefined
                          ? App.summary.headlineDetail : ""
                    color: Estilo.textoSuave
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        // --- Caja contra resultado -----------------------------------------
        Tarjeta {
            Layout.preferredHeight: dos.height + 28

            ColumnLayout {
                id: dos
                x: 14
                y: 14
                width: parent.width - 28
                spacing: 8

                Label {
                    text: "CAJA Y RESULTADO NO SON LO MISMO"
                    color: Estilo.textoSuave
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Estilo.espacio

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            text: "bajo o subio la caja"
                            color: Estilo.textoSuave
                            font.pixelSize: 11
                        }
                        Label {
                            text: App.summary.cashDelta !== undefined ? App.summary.cashDelta : "—"
                            color: App.summary.cashDeltaNegative ? Estilo.negativo : Estilo.positivo
                            font.pixelSize: 22
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            text: "dejo el mes"
                            color: Estilo.textoSuave
                            font.pixelSize: 11
                        }
                        Label {
                            text: App.summary.result !== undefined ? App.summary.result : "—"
                            color: App.summary.resultNegative ? Estilo.negativo : Estilo.positivo
                            font.pixelSize: 22
                            font.weight: Font.Bold
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "La caja cuenta la plata que se movio. El resultado cuenta el mes que "
                          + "trabajaste, con el material repartido entre los meses que dura."
                    color: Estilo.textoSuave
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        // --- Avisos ---------------------------------------------------------
        Label {
            text: "LO QUE HAY QUE MIRAR"
            color: Estilo.textoSuave
            font.pixelSize: 10
            font.letterSpacing: 0.8
            Layout.topMargin: 6
        }

        Repeater {
            model: App.alerts

            Tarjeta {
                id: tarjetaAviso
                required property var modelData
                Layout.preferredHeight: textoAviso.height + 24

                ColumnLayout {
                    id: textoAviso
                    x: 14
                    y: 12
                    width: parent.width - 28
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        text: tarjetaAviso.modelData.title
                        color: Estilo.colorAviso(tarjetaAviso.modelData.level)
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: tarjetaAviso.modelData.detail
                        color: Estilo.textoSuave
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}
