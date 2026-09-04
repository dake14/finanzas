#include "dake/core/demo.hpp"

namespace dake::core {
namespace {

[[nodiscard]] Pocket pocket(const char* id, const char* name, PocketKind kind,
                            std::int64_t openingMinor) {
    Pocket p;
    p.id = id;
    p.name = name;
    p.kind = kind;
    p.openingMinor = openingMinor;
    p.deviceId = "demo";
    p.hlc = "0";
    return p;
}

[[nodiscard]] Job job(const char* id, const char* name, const char* client, const char* opened,
                      bool closed) {
    Job j;
    j.id = id;
    j.name = name;
    j.client = client;
    j.opened = Date::fromIso(opened);
    j.closed = closed;
    j.deviceId = "demo";
    j.hlc = "0";
    return j;
}

struct MovementSpec {
    const char* id;
    const char* date;
    const char* name;
    MovementKind kind;
    std::int64_t amountMinor;
    const char* pocketId;
    const char* targetPocketId;
    const char* category;
    const char* jobId;
    int spreadMonths;
    bool settled;
};

[[nodiscard]] Movement build(const MovementSpec& spec) {
    Movement m;
    m.id = spec.id;
    m.date = Date::fromIso(spec.date);
    m.name = spec.name;
    m.kind = spec.kind;
    m.amountMinor = spec.amountMinor;
    m.pocketId = spec.pocketId;
    m.targetPocketId = spec.targetPocketId;
    m.category = spec.category;
    m.jobId = spec.jobId;
    m.spreadMonths = spec.spreadMonths;
    m.settled = spec.settled;
    m.deviceId = "demo";
    m.hlc = "0";
    return m;
}

} // namespace

DemoData realCaseAugust2026(Currency currency) {
    (void)currency;  // los importes ya vienen en unidades minimas
    DemoData data;

    // --- Bolsillos ---------------------------------------------------------
    // "Caja" es la operacion: de ahi sale el material y ahi entra lo que se
    // cobra. Ahorro e inversion son reservas: cada peso que sale de ellas
    // hacia la caja es lo que la app tiene que saber decir en voz alta.
    data.pockets = {
        pocket("p-caja", "Caja del negocio", PocketKind::Operacion, 0),
        pocket("p-ahorro", "Ahorro", PocketKind::Ahorro, 400'00),
        pocket("p-inversion", "Inversion", PocketKind::Inversion, 600'00),
        pocket("p-personal", "Mio", PocketKind::Personal, 0),
    };

    // --- Trabajos ----------------------------------------------------------
    data.jobs = {
        job("j-macbook", "Reparacion Macbook", "Herman Galvan", "2026-08-16", true),
        job("j-ifix", "Modelo 3D", "IFIX", "2026-08-13", true),
        job("j-lote", "Lote de impresiones por encargo", "Varios", "2026-08-25", false),
    };

    // --- Movimientos -------------------------------------------------------
    // Los seis primeros son los que existen de verdad en la base instalada.
    // Los traspasos son lo que paso en la realidad y la app no tenia como
    // anotar: la plata del material no salio de lo cobrado, salio del ahorro.
    const MovementSpec specs[] = {
        {"m-01", "2026-08-13", "IFIX MODELO 3D", MovementKind::Ingreso, 500, "p-caja", "",
         "Impresion 3D", "j-ifix", 1, true},

        {"m-02", "2026-08-17", "Del ahorro, para las piezas del Macbook",
         MovementKind::Traspaso, 50'00, "p-ahorro", "p-caja", "Traspaso", "", 1, true},

        {"m-03", "2026-08-17", "Teclado y backlight macbook y envio", MovementKind::Gasto,
         49'82, "p-caja", "", "Piezas", "j-macbook", 1, true},

        {"m-04", "2026-08-17", "Trabajo Macbook Herman Galvan", MovementKind::Ingreso, 120'00,
         "p-caja", "", "Reparacion", "j-macbook", 1, true},

        // Un rollo de PLA no se consume el dia que se compra. Tres meses es lo
        // que dura a este ritmo; el costo se reparte y el mes de la compra
        // deja de parecer un desastre.
        {"m-05", "2026-08-20", "Compra de PLA GRIS", MovementKind::Gasto, 25'67, "p-caja", "",
         "Insumos", "", 3, true},

        {"m-06", "2026-08-24", "De la inversion, para reponer filamento",
         MovementKind::Traspaso, 70'00, "p-inversion", "p-caja", "Traspaso", "", 1, true},

        {"m-07", "2026-08-24", "Compra PLA Negro", MovementKind::Gasto, 23'53, "p-caja", "",
         "Insumos", "", 3, true},

        // Cuatro kilos son cuatro meses de material, no un gasto de agosto.
        {"m-08", "2026-08-24", "Compra PLA Blanco 4Kg", MovementKind::Gasto, 46'00, "p-caja",
         "", "Insumos", "", 4, true},

        // Lo que la app vieja tampoco sabia: trabajo entregado y sin cobrar.
        {"m-09", "2026-08-27", "Lote de impresiones por encargo", MovementKind::Ingreso, 85'00,
         "p-caja", "", "Impresion 3D", "j-lote", 1, false},

        {"m-10", "2026-08-28", "Boquillas y cama de repuesto", MovementKind::Gasto, 18'40,
         "p-caja", "", "Repuestos", "j-lote", 1, true},
    };

    data.movements.reserve(std::size(specs));
    for (const MovementSpec& spec : specs) {
        data.movements.push_back(build(spec));
    }
    return data;
}

} // namespace dake::core
