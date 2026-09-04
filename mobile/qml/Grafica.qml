// Grafica.qml — las tres formas de grafica del telefono.
//
// CONTRATO DE LA UNIDAD
// =====================
//
// El equivalente en QML de ui/charts.hpp del escritorio. Las mismas tres
// formas, porque las cuatro graficas que se muestran se reducen a tres:
//
//   resultado por mes        modo "barras" con signo
//   ingresos contra costos   modo "barras" con dos series
//   caja acumulada           modo "linea"
//   gastos por categoria     modo "ranking"
//   margen por trabajo       modo "ranking"
//
// Se dibuja con Canvas y no con una libreria: son barras y una linea, y en un
// telefono cada dependencia que se agrega es peso en el APK.
//
// PROPIEDADES
//
//   modo      string   "barras" | "linea" | "ranking"
//   datos     var      lista de mapas {etiqueta, valor, valor2, texto}
//   conSigno  bool     solo en "barras": el cero va en el medio y lo negativo
//                      baja. Es lo que hace que un mes en perdida SE VEA como
//                      perdida y no como una barra corta.
//   serie1    string   nombre de la primera serie, para la leyenda
//   serie2    string   nombre de la segunda. Vacio = una sola serie.
//   limite    int      solo en "ranking": cuantas filas como mucho, y el resto
//                      se suma en una fila "otros". Una lista de treinta
//                      categorias no se lee.
//
// REGLAS
//
//   - Colores y medidas SOLO del singleton Estilo. Si aparece un "#rrggbb"
//     escrito a mano en este archivo, esta mal. Positivo con Estilo.positivo,
//     negativo con Estilo.negativo, la serie unica y la linea con Estilo.acento.
//   - Sin datos, o con todos los valores en cero: dibujar "sin datos todavia"
//     centrado con Estilo.textoTenue y volver. Nunca dividir por cero ni pintar
//     un eje vacio con numeros raros.
//   - Las etiquetas de abajo con letra chica y Estilo.textoTenue. Si no entran
//     todas sin pisarse, dibujar una de cada dos. Nunca superponerlas.
//   - El `texto` de cada dato viene YA FORMATEADO desde el puente. No formatear
//     plata en QML: el formato vive en dake::core::format y en ningun otro
//     lado, para que el telefono y la computadora no escriban el mismo importe
//     distinto.
//   - requestPaint() cuando cambian `datos` o el tamaño, o la grafica se queda
//     dibujada con lo viejo.
//
// ACEPTACION: la pantalla Hoy renderiza sin errores de QML en consola con
// `dake_movil.exe --captura <png> --pagina 1`, y con una lista vacia muestra
// "sin datos todavia" en vez de quedar en blanco.
//
import QtQuick
import DakeMobile

Item {
    id: control

    property string modo: "barras"
    property var datos: []
    property bool conSigno: false
    property string serie1: ""
    property string serie2: ""
    property int limite: 6

    implicitHeight: {
        if (modo === "ranking") {
            var count = datos ? datos.length : 0
            if (count === 0) return 40
            var safeLimit = Math.max(1, limite)
            count = Math.min(count, safeLimit)
            return Math.max(40, count * 30 - 6)
        }
        return 150
    }

    onDatosChanged: canvas.requestPaint()
    onModoChanged: canvas.requestPaint()
    onConSignoChanged: canvas.requestPaint()
    onSerie1Changed: canvas.requestPaint()
    onSerie2Changed: canvas.requestPaint()
    onLimiteChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)

            var d = control.datos || []
            var hasData = false
            var maxVal = 0.0

            for (var i = 0; i < d.length; ++i) {
                var p = d[i]
                var v1 = p.valor || 0.0
                var v2 = p.valor2 || 0.0
                if (v1 !== 0.0 || v2 !== 0.0) hasData = true
                maxVal = Math.max(maxVal, Math.abs(v1), Math.abs(v2))
            }

            if (!hasData || d.length === 0 || maxVal === 0.0) {
                ctx.fillStyle = Estilo.textoTenue
                ctx.font = "12px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText("sin datos todavia", width / 2, height / 2)
                return
            }

            if (modo === "barras") {
                dibujarBarras(ctx, d, maxVal)
            } else if (modo === "linea") {
                dibujarLinea(ctx, d, maxVal)
            } else if (modo === "ranking") {
                dibujarRanking(ctx, d, maxVal)
            }
        }

        function dibujarBarras(ctx, datos, maxVal) {
            var labelFont = "11px sans-serif"
            ctx.font = labelFont
            var labelHeight = 14

            var topMargin = 4
            var hasLegend = control.serie2 !== ""
            if (hasLegend) {
                topMargin = labelHeight + 16
            }

            var chartH = height - topMargin - labelHeight - 8
            if (chartH <= 0) return

            var zeroY = control.conSigno ? topMargin + chartH / 2.0 : topMargin + chartH
            var scale = control.conSigno ? (chartH / 2.0) / maxVal : chartH / maxVal

            if (hasLegend) {
                ctx.font = labelFont
                ctx.textBaseline = "middle"
                var w1 = ctx.measureText(control.serie1).width
                var w2 = ctx.measureText(control.serie2).width
                var spacing = 20
                var totalW = 12 + w1 + spacing + 12 + w2
                var startX = (width - totalW) / 2
                var legendY = 4 + (labelHeight - 8) / 2.0

                ctx.fillStyle = Estilo.positivo
                ctx.fillRect(startX, legendY, 8, 8)
                ctx.fillStyle = Estilo.textoTenue
                ctx.textAlign = "left"
                ctx.fillText(control.serie1, startX + 12, 4 + labelHeight / 2.0)

                startX += 12 + w1 + spacing
                ctx.fillStyle = Estilo.negativo
                ctx.fillRect(startX, legendY, 8, 8)
                ctx.fillStyle = Estilo.textoTenue
                ctx.fillText(control.serie2, startX + 12, 4 + labelHeight / 2.0)
            }

            ctx.strokeStyle = Estilo.borde
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(0, zeroY)
            ctx.lineTo(width, zeroY)
            ctx.stroke()

            var N = datos.length
            var step = width / N
            var barGroupWidth = step * 0.7

            for (var i = 0; i < N; ++i) {
                var p = datos[i]
                var cx = (i + 0.5) * step
                var v1 = p.valor || 0.0
                var v2 = p.valor2 || 0.0

                if (hasLegend) {
                    var barW = barGroupWidth / 2.0
                    var h1 = Math.abs(v1) * scale
                    var y1 = v1 >= 0 ? zeroY - h1 : zeroY
                    if (!control.conSigno) y1 = zeroY - h1
                    ctx.fillStyle = Estilo.positivo
                    ctx.fillRect(cx - barW, y1, barW - 1, h1)

                    var h2 = Math.abs(v2) * scale
                    var y2 = v2 >= 0 ? zeroY - h2 : zeroY
                    if (!control.conSigno) y2 = zeroY - h2
                    ctx.fillStyle = Estilo.negativo
                    ctx.fillRect(cx, y2, barW - 1, h2)
                } else {
                    var barW = barGroupWidth
                    var h = Math.abs(v1) * scale
                    var y = v1 >= 0 ? zeroY - h : zeroY
                    if (!control.conSigno) y = zeroY - h

                    if (control.conSigno) {
                        ctx.fillStyle = v1 >= 0 ? Estilo.positivo : Estilo.negativo
                    } else {
                        ctx.fillStyle = Estilo.acento
                    }
                    ctx.fillRect(cx - barW / 2.0, y, barW, h)
                }
            }

            ctx.font = labelFont
            ctx.fillStyle = Estilo.textoTenue
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            var skip = 1
            var maxW = 0
            for (var i = 0; i < N; ++i) {
                maxW = Math.max(maxW, ctx.measureText(datos[i].etiqueta || "").width)
            }
            if (step > 0 && maxW + 8 > step) {
                skip = Math.ceil((maxW + 8) / step)
            }

            for (var i = 0; i < N; i += skip) {
                var cx = (i + 0.5) * step
                ctx.fillText(datos[i].etiqueta || "", cx, height - labelHeight / 2.0 - 2)
            }
        }

        function dibujarLinea(ctx, datos, maxVal) {
            var labelFont = "11px sans-serif"
            ctx.font = labelFont
            var labelHeight = 14

            var topMargin = 8.0
            var chartH = height - labelHeight - 8 - topMargin
            if (chartH <= 0) return
            
            // El eje va del minimo al maximo REALES, no de cero al maximo del
            // valor absoluto. La caja acumulada puede ser negativa —es lo que
            // pasa cuando se estan consumiendo las reservas— y dibujar su valor
            // absoluto la mostraria subiendo justo cuando el negocio va peor.
            var vMin = 0.0
            var vMax = 0.0
            for (var k = 0; k < datos.length; ++k) {
                var vk = datos[k].valor || 0.0
                if (k === 0) { vMin = vk; vMax = vk }
                vMin = Math.min(vMin, vk)
                vMax = Math.max(vMax, vk)
            }
            // El cero siempre entra en el rango: si no, una serie toda positiva
            // arrancaria pegada al piso y pareceria que empezo en cero.
            vMin = Math.min(vMin, 0.0)
            vMax = Math.max(vMax, 0.0)
            var rango = vMax - vMin
            if (rango <= 0) rango = 1

            var zeroY = topMargin + chartH - ((0.0 - vMin) / rango) * chartH
            var scale = chartH / rango

            var N = datos.length
            var step = width / N

            var polyX = []
            var polyY = []
            for (var i = 0; i < N; ++i) {
                polyX.push((i + 0.5) * step)
                var v = datos[i].valor || 0.0
                polyY.push(topMargin + chartH - ((v - vMin) / rango) * chartH)
            }

            if (polyX.length > 0) {
                ctx.beginPath()
                ctx.moveTo(polyX[0], polyY[0])
                for (var i = 1; i < polyX.length; ++i) {
                    ctx.lineTo(polyX[i], polyY[i])
                }
                ctx.lineTo(polyX[polyX.length - 1], zeroY)
                ctx.lineTo(polyX[0], zeroY)
                ctx.closePath()
                
                ctx.globalAlpha = 0.16
                ctx.fillStyle = Estilo.acento
                ctx.fill()
                ctx.globalAlpha = 1.0

                ctx.beginPath()
                ctx.moveTo(polyX[0], polyY[0])
                for (var i = 1; i < polyX.length; ++i) {
                    ctx.lineTo(polyX[i], polyY[i])
                }
                ctx.strokeStyle = Estilo.acento
                ctx.lineWidth = 2
                ctx.stroke()
                
                ctx.fillStyle = Estilo.acento
                for (var i = 0; i < polyX.length; ++i) {
                    ctx.beginPath()
                    ctx.arc(polyX[i], polyY[i], 3, 0, 2 * Math.PI)
                    ctx.fill()
                }
            }

            ctx.strokeStyle = Estilo.borde
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(0, zeroY)
            ctx.lineTo(width, zeroY)
            ctx.stroke()

            ctx.font = labelFont
            ctx.fillStyle = Estilo.textoTenue
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            var skip = 1
            var maxW = 0
            for (var i = 0; i < N; ++i) {
                maxW = Math.max(maxW, ctx.measureText(datos[i].etiqueta || "").width)
            }
            if (step > 0 && maxW + 8 > step) {
                skip = Math.ceil((maxW + 8) / step)
            }

            for (var i = 0; i < N; i += skip) {
                var cx = (i + 0.5) * step
                ctx.fillText(datos[i].etiqueta || "", cx, height - labelHeight / 2.0 - 2)
            }
        }

        function dibujarRanking(ctx, datos, maxVal) {
            // Se ordena aca y no en la pantalla que la usa: el contrato de este
            // componente dice "de mayor a menor", y una promesa que cada
            // usuario tiene que cumplir por su cuenta se rompe en el segundo.
            // App.categories ya viene ordenado; App.jobMargins viene por fecha
            // de apertura del trabajo.
            datos = datos.slice().sort(function (a, b) {
                return Math.abs(b.valor || 0.0) - Math.abs(a.valor || 0.0)
            })

            var rows = []
            var safeLimit = Math.max(1, control.limite)
            if (datos.length > safeLimit) {
                for (var i = 0; i < safeLimit - 1; ++i) {
                    rows.push(datos[i])
                }
                var otros = 0.0
                for (var i = safeLimit - 1; i < datos.length; ++i) {
                    otros += (datos[i].valor || 0.0)
                }
                rows.push({
                    etiqueta: "Otros",
                    valor: otros,
                    texto: ""
                })
            } else {
                rows = datos
            }

            var rMax = 0.0
            for (var i = 0; i < rows.length; ++i) {
                rMax = Math.max(rMax, Math.abs(rows[i].valor || 0.0))
            }
            if (rMax === 0.0) {
                ctx.fillStyle = Estilo.textoTenue
                ctx.font = "12px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText("sin datos todavia", width / 2, height / 2)
                return
            }

            ctx.font = "12px sans-serif" 
            
            var gutter = 0
            for (var i = 0; i < rows.length; ++i) {
                var t = rows[i].texto || ""
                if (t !== "") {
                    gutter = Math.max(gutter, ctx.measureText(t).width)
                }
            }
            gutter += 12
            var barArea = Math.max(20, width - gutter)

            var y = 0
            ctx.textBaseline = "middle"
            for (var i = 0; i < rows.length; ++i) {
                var r = rows[i]
                var val = r.valor || 0.0
                var ratio = Math.abs(val) / rMax
                var barW = barArea * ratio

                ctx.globalAlpha = 0.60
                ctx.fillStyle = Estilo.acento
                ctx.fillRect(0, y, barW, 24)
                ctx.globalAlpha = 1.0

                ctx.fillStyle = Estilo.texto
                ctx.textAlign = "left"
                ctx.fillText(r.etiqueta || "", 6, y + 12)

                var txt = r.texto || ""
                if (txt !== "") {
                    ctx.fillStyle = Estilo.textoTenue
                    ctx.textAlign = "right"
                    ctx.fillText(txt, width - 6, y + 12)
                }

                y += 30
            }
        }
    }
}
