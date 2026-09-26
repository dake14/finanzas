pragma Singleton
import QtQuick

// Estilo.qml — la paleta y las medidas, en un solo lugar.
//
// Son los mismos colores que la version de escritorio, que a su vez salen del
// icono de DakeLabs: fondo casi negro, cuerpo blanco, barra cian. Las dos
// aplicaciones tienen que parecer la misma cosa, porque lo son.

QtObject {
    // --- Superficies ---
    readonly property color fondo:      "#0a0a0a"
    readonly property color tarjeta:    "#141414"
    readonly property color elevado:    "#1c1c1c"
    readonly property color borde:      "#262626"

    // --- Texto ---
    readonly property color texto:      "#fafafa"
    readonly property color textoSuave: "#8a8a8a"
    readonly property color textoTenue: "#5a5a5a"

    // --- Marca y semantica ---
    readonly property color acento:     "#38bdf8"
    readonly property color positivo:   "#34d399"
    readonly property color negativo:   "#fb7185"
    readonly property color aviso:      "#fbbf24"
    readonly property color ahorro:     "#a78bfa"

    // --- Bolsillos ---
    // Los mismos cuatro colores que ui/theme.hpp, y por el mismo motivo: la
    // caja de operacion toma el cian de la marca porque es la que se mira todos
    // los dias; las reservas van en colores calidos porque tocarlas tiene que
    // destacar, no pasar desapercibido.
    readonly property color operacion:  "#38bdf8"
    readonly property color inversion:  "#fbbf24"
    readonly property color personal:   "#9ca3af"

    // Un dedo necesita 48 dp para no errarle al objetivo. Todo lo que se toca
    // respeta ese minimo: es la diferencia entre anotar el gasto ahi mismo en
    // la ferreteria y guardar el telefono para hacerlo despues, que es como se
    // pierde el movimiento.
    readonly property int toque:   52
    readonly property int margen:  16
    readonly property int espacio: 10
    readonly property int radio:   12

    // Los mismos numeros que core::MovementKind, para que el tipo que elige la
    // pantalla y el que guarda el nucleo no puedan desincronizarse.
    readonly property int ingreso:  0
    readonly property int gasto:    1
    readonly property int traspaso: 2

    function colorMovimiento(kind) {
        if (kind === ingreso) return positivo
        if (kind === gasto) return negativo
        return ahorro
    }

    /// El numero es core::PocketKind: 0 operacion, 1 ahorro, 2 inversion,
    /// 3 personal.
    function colorBolsillo(kind) {
        if (kind === 1) return ahorro
        if (kind === 2) return inversion
        if (kind === 3) return personal
        return operacion
    }

    function colorAviso(nivel) {
        if (nivel === 2) return negativo   // Danger
        if (nivel === 1) return aviso      // Warning
        return textoSuave                  // Info
    }
}
