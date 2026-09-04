import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Teclado.qml — el teclado numerico propio de la aplicacion.
//
// No es capricho de diseño: el teclado del sistema aparece y desaparece, empuja
// el formulario hacia arriba y tapa justo el boton de guardar. Un teclado
// propio y siempre visible hace que el gesto sea siempre el mismo —monto,
// nombre, guardar— y ese es todo el punto de una aplicacion de captura.
//
// Ademas evita el problema de la coma: en varios teclados de Android el
// separador decimal esta escondido detras de una tecla larga.

GridLayout {
    id: control

    signal digito(string valor)
    signal coma()
    signal borrar()

    columns: 3
    rowSpacing: 6
    columnSpacing: 6

    component Tecla: Button {
        id: tecla
        property string etiqueta: ""
        property color colorTexto: Estilo.texto

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Estilo.toque
        flat: true

        background: Rectangle {
            color: tecla.pressed ? Estilo.elevado : Estilo.tarjeta
            radius: 10
            border.color: Estilo.borde
        }

        contentItem: Label {
            text: tecla.etiqueta
            color: tecla.colorTexto
            font.pixelSize: 22
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Repeater {
        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]
        Tecla {
            required property string modelData
            etiqueta: modelData
            onClicked: control.digito(modelData)
        }
    }

    Tecla {
        etiqueta: ","
        onClicked: control.coma()
    }

    Tecla {
        etiqueta: "0"
        onClicked: control.digito("0")
    }

    Tecla {
        etiqueta: "⌫"
        colorTexto: Estilo.negativo
        onClicked: control.borrar()
        // Mantener apretado borra todo: corregir un monto de seis digitos a
        // golpe de toque es la clase de detalle que hace que uno deje de
        // corregir y guarde cualquier cosa.
        onPressAndHold: {
            for (var i = 0; i < 24; ++i) {
                control.borrar()
            }
        }
    }
}
