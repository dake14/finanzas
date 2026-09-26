import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Cifra.qml — una tarjeta de "rotulo, numero grande, nota".
//
// Es el equivalente de KpiCard en la version de escritorio, y esta aparte por
// el mismo motivo que alla: cuatro pantallas la usan, y una copia por pantalla
// es una copia que se queda vieja.
//
// La barra de color de la izquierda no es adorno: es lo que deja distinguir de
// un vistazo la caja del negocio del ahorro sin tener que leer el rotulo.

Rectangle {
    id: cifra

    property string rotulo: ""
    property string valor: "—"
    property string nota: ""
    property color tinte: Estilo.acento
    /// Cuando el numero es malo se pinta de rojo. Lo decide quien la usa, no la
    /// tarjeta: un resultado negativo es malo, pero un costo grande no lo es
    /// necesariamente.
    property bool negativo: false

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
            color: cifra.negativo ? Estilo.negativo : Estilo.texto
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
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
