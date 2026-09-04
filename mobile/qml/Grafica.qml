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

    implicitHeight: 150

    // TODO(agy): implementar segun el contrato de arriba.
}
