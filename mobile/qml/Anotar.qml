import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DakeMobile

// Anotar.qml — la pantalla que justifica que esto exista.
//
// El diagnostico de la aplicacion de escritorio decia que la friccion no esta
// en los reportes sino en el momento de anotar: la plata se gasta en la calle y
// se anota en la computadora dos dias despues, y lo que se posterga se olvida.
// Todo aca esta ordenado para que el camino corto —monto, nombre, guardar— sean
// tres gestos, y el resto quede en lo que se dejo la vez anterior.

Item {
    id: pagina

    // --- Estado del formulario -------------------------------------------
    property string monto: ""
    property int tipo: Estilo.gasto
    property string fecha: App.today
    property string pocketId: ""
    property string pocketNombre: ""
    property string targetId: ""
    property string targetNombre: ""
    property string categoria: ""
    property string jobId: ""
    property string jobNombre: ""
    property int duracion: 1
    property bool pagado: true

    readonly property bool esTraspaso: tipo === Estilo.traspaso
    readonly property bool esGasto: tipo === Estilo.gasto

    /// Un archivo QML no ve los `id` del archivo que lo instancia, asi que el
    /// mensaje sale por una señal y la ventana decide como mostrarlo.
    signal mensaje(string texto, color tinte)

    function primerBolsillo() {
        if (App.pockets.length > 0) {
            pocketId = App.pockets[0].id
            pocketNombre = App.pockets[0].name
        }
        if (App.pockets.length > 1) {
            targetId = App.pockets[1].id
            targetNombre = App.pockets[1].name
        }
    }

    Component.onCompleted: primerBolsillo()

    function limpiar() {
        // Se limpian el monto y el nombre; el resto se conserva. Cargar cinco
        // compras de insumos seguidas no puede costar cinco veces elegir el
        // mismo bolsillo.
        monto = ""
        nombre.text = ""
        nombre.forceActiveFocus()
    }

    function guardar() {
        var error = App.saveMovement({
            "name": nombre.text,
            "amount": monto,
            "kind": tipo,
            "date": fecha,
            "pocketId": pocketId,
            "targetPocketId": targetId,
            "category": categoria,
            "jobId": jobId,
            "spreadMonths": duracion,
            "settled": pagado
        })
        if (error === "") {
            limpiar()
            pagina.mensaje("Anotado.", Estilo.positivo)
        } else {
            pagina.mensaje(error, Estilo.negativo)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Estilo.margen
        spacing: Estilo.espacio

        // --- Tipo ---------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            // Un layout anidado dentro de otro trae fillHeight en true por
            // defecto, al reves que un Item. Sin apagarlo se estira y se come el
            // espacio de las filas de contexto de mas abajo.
            Layout.fillHeight: false
            spacing: 6

            Repeater {
                model: [
                    { "texto": "Gasto",    "valor": Estilo.gasto,    "color": Estilo.negativo },
                    { "texto": "Ingreso",  "valor": Estilo.ingreso,  "color": Estilo.positivo },
                    { "texto": "Traspaso", "valor": Estilo.traspaso, "color": Estilo.ahorro }
                ]

                Button {
                    id: botonTipo
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    flat: true
                    checkable: true
                    checked: pagina.tipo === botonTipo.modelData.valor

                    background: Rectangle {
                        radius: 10
                        color: botonTipo.checked ? botonTipo.modelData.color : Estilo.tarjeta
                        border.color: botonTipo.checked ? botonTipo.modelData.color : Estilo.borde
                    }
                    contentItem: Label {
                        text: botonTipo.modelData.texto
                        color: botonTipo.checked ? Estilo.fondo : Estilo.textoSuave
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: pagina.tipo = botonTipo.modelData.valor
                }
            }
        }

        // --- Monto --------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde

            Label {
                anchors.centerIn: parent
                text: pagina.monto === "" ? "0" : pagina.monto
                color: pagina.monto === ""
                       ? Estilo.textoTenue
                       : Estilo.colorMovimiento(pagina.tipo)
                font.pixelSize: 40
                font.weight: Font.Bold
            }
        }

        // --- Nombre -------------------------------------------------------
        TextField {
            id: nombre
            Layout.fillWidth: true
            Layout.preferredHeight: Estilo.toque
            placeholderText: "Que fue"
            color: Estilo.texto
            placeholderTextColor: Estilo.textoTenue
            font.pixelSize: 16
            background: Rectangle {
                color: Estilo.tarjeta
                radius: 10
                border.color: nombre.activeFocus ? Estilo.acento : Estilo.borde
            }
            onAccepted: pagina.guardar()
        }

        // --- Contexto -----------------------------------------------------
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            contentHeight: filas.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: filas
                width: parent.width
                spacing: 0

                Fila {
                    rotulo: "Fecha"
                    valor: App.relativeDate(pagina.fecha)
                    onClicked: fechaSelector.open()
                }

                Fila {
                    rotulo: pagina.tipo === Estilo.ingreso ? "Entra a" : "Sale de"
                    valor: pagina.pocketNombre
                    onClicked: { bolsilloSelector.destino = false; bolsilloSelector.open() }
                }

                Fila {
                    visible: pagina.esTraspaso
                    rotulo: "Va a"
                    valor: pagina.targetNombre
                    colorValor: Estilo.ahorro
                    onClicked: { bolsilloSelector.destino = true; bolsilloSelector.open() }
                }

                Fila {
                    visible: !pagina.esTraspaso
                    rotulo: "Categoria"
                    valor: pagina.categoria === "" ? "—" : pagina.categoria
                    onClicked: categoriaSelector.abrir(pagina.tipo)
                }

                Fila {
                    visible: !pagina.esTraspaso
                    rotulo: "Trabajo"
                    valor: pagina.jobNombre === "" ? "sin trabajo" : pagina.jobNombre
                    onClicked: trabajoSelector.open()
                }

                Fila {
                    visible: pagina.esGasto
                    rotulo: "Cuanto dura"
                    valor: pagina.duracion === 1
                           ? "se gasta este mes"
                           : pagina.duracion + " meses"
                    colorValor: pagina.duracion === 1 ? Estilo.texto : Estilo.ahorro
                    onClicked: duracionSelector.open()
                }

                Fila {
                    visible: !pagina.esTraspaso
                    rotulo: pagina.esGasto ? "Ya lo pague" : "Ya me lo pagaron"
                    valor: pagina.pagado ? "si" : "todavia no"
                    colorValor: pagina.pagado ? Estilo.texto : Estilo.aviso
                    mostrarFlecha: false
                    onClicked: pagina.pagado = !pagina.pagado
                }
            }
        }

        // --- Teclado ------------------------------------------------------
        Teclado {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: 4 * 52
            Layout.maximumHeight: 4 * 52

            onDigito: function(valor) {
                // Dos decimales como maximo: mas alla de eso el monto ya no se
                // puede representar en centavos y el nucleo lo rechazaria.
                var coma = pagina.monto.indexOf(",")
                if (coma >= 0 && pagina.monto.length - coma > 2) {
                    return
                }
                if (pagina.monto === "0") {
                    pagina.monto = valor
                } else {
                    pagina.monto += valor
                }
            }
            onComa: {
                if (pagina.monto.indexOf(",") < 0) {
                    pagina.monto = (pagina.monto === "" ? "0" : pagina.monto) + ","
                }
            }
            onBorrar: pagina.monto = pagina.monto.slice(0, -1)
        }

        // --- Guardar ------------------------------------------------------
        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            enabled: pagina.monto !== "" && nombre.text.trim() !== ""

            background: Rectangle {
                radius: Estilo.radio
                color: parent.enabled
                       ? (parent.pressed ? "#0ea5e9" : Estilo.acento)
                       : Estilo.elevado
            }
            contentItem: Label {
                text: "Guardar"
                color: parent.enabled ? Estilo.fondo : Estilo.textoTenue
                font.pixelSize: 17
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: pagina.guardar()
        }
    }

    // ------------------------------------------------------------ Dialogos --

    Selector {
        id: bolsilloSelector
        property bool destino: false
        title: destino ? "A que bolsillo va" : "De que bolsillo sale"
        modelo: App.pockets
        campoTexto: "name"
        campoDetalle: "balance"
        onElegido: function(valor, texto) {
            if (destino) {
                pagina.targetId = valor
                pagina.targetNombre = texto
            } else {
                pagina.pocketId = valor
                pagina.pocketNombre = texto
            }
        }
    }

    Selector {
        id: trabajoSelector
        title: "A que trabajo pertenece"
        modelo: [{ "id": "", "label": "— sin trabajo —" }].concat(App.jobs)
        campoTexto: "label"
        onElegido: function(valor, texto) {
            pagina.jobId = valor
            pagina.jobNombre = valor === "" ? "" : texto
        }
    }

    Selector {
        id: duracionSelector
        title: "Cuantos meses dura"
        modelo: [
            { "id": 1,  "label": "se gasta este mes" },
            { "id": 2,  "label": "2 meses" },
            { "id": 3,  "label": "3 meses" },
            { "id": 4,  "label": "4 meses" },
            { "id": 6,  "label": "6 meses" },
            { "id": 12, "label": "12 meses" }
        ]
        onElegido: function(valor, texto) { pagina.duracion = valor }
    }

    Selector {
        id: categoriaSelector
        title: "Categoria"
        campoTexto: "label"

        function abrir(tipo) {
            var usadas = App.categoriesFor(tipo)
            var items = [{ "id": "__nueva__", "label": "＋ escribir una nueva" }]
            for (var i = 0; i < usadas.length; ++i) {
                items.push({ "id": usadas[i], "label": usadas[i] })
            }
            modelo = items
            open()
        }

        onElegido: function(valor, texto) {
            if (valor === "__nueva__") {
                nuevaCategoria.open()
            } else {
                pagina.categoria = valor
            }
        }
    }

    Dialog {
        id: nuevaCategoria
        title: "Categoria nueva"
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 2 * Estilo.margen, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: Estilo.tarjeta
            radius: Estilo.radio
            border.color: Estilo.borde
        }

        onOpened: campoCategoria.forceActiveFocus()
        onAccepted: {
            if (campoCategoria.text.trim() !== "") {
                pagina.categoria = campoCategoria.text.trim()
            }
            campoCategoria.text = ""
        }

        TextField {
            id: campoCategoria
            width: parent.width
            placeholderText: "Insumos, Piezas, Repuestos…"
            color: Estilo.texto
            placeholderTextColor: Estilo.textoTenue
        }
    }

    Selector {
        id: fechaSelector
        title: "Cuando fue"
        campoTexto: "label"
        modelo: {
            // Una lista de los ultimos catorce dias: en una aplicacion de
            // captura la fecha que se corrige es siempre de esta semana, y una
            // lista se recorre con el pulgar mas rapido que un calendario.
            var items = []
            var hoy = new Date(App.today)
            for (var i = 0; i < 14; ++i) {
                var d = new Date(hoy)
                d.setDate(hoy.getDate() - i)
                var iso = Qt.formatDate(d, "yyyy-MM-dd")
                var etiqueta = i === 0 ? "hoy" : (i === 1 ? "ayer" : Qt.formatDate(d, "ddd d MMM"))
                items.push({ "id": iso, "label": etiqueta, "detalle": iso })
            }
            return items
        }
        campoDetalle: "detalle"
        onElegido: function(valor, texto) { pagina.fecha = valor }
    }

    // Cuando cambian los bolsillos (por ejemplo al crear uno) hay que asegurar
    // que la seleccion siga existiendo.
    Connections {
        target: App
        function onDataChanged() {
            if (pagina.pocketId === "") {
                pagina.primerBolsillo()
            }
        }
    }
}
