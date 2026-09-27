// tests/selftest.cpp — verificacion del nucleo, sin base de datos ni ventana.
//
// Sin framework a proposito: se compila y se corre con lo que ya hay en el
// equipo. Imprime una linea por comprobacion y devuelve 0 si todo pasa.
//
// La parte que mas importa esta al final, en `elCasoDeAgosto`: son los seis
// movimientos reales de la base instalada, y la prueba fija por escrito que el
// −20,02 que muestra la aplicacion actual como "utilidad" es en realidad la
// variacion de caja del mes.

#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

#include "dake/core/demo.hpp"
#include "dake/core/format.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"
#include "dake/core/report.hpp"

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what) {
    std::printf("%s  %s\n", condition ? "ok  " : "FALLA", what.c_str());
    if (!condition) {
        ++gFailures;
    }
}

void checkMinor(std::int64_t got, std::int64_t want, const std::string& what) {
    const bool ok = got == want;
    if (ok) {
        std::printf("ok    %s (%lld)\n", what.c_str(), static_cast<long long>(got));
    } else {
        std::printf("FALLA %s: esperaba %lld y salio %lld\n", what.c_str(),
                    static_cast<long long>(want), static_cast<long long>(got));
        ++gFailures;
    }
}

using namespace dake::core;

const Currency kUsd = Currency::usd();

[[nodiscard]] Movement expense(const char* id, const char* date, std::int64_t minor,
                               const char* pocketId, int spreadMonths = 1) {
    Movement m;
    m.id = id;
    m.date = Date::fromIso(date);
    m.name = id;
    m.kind = MovementKind::Gasto;
    m.amountMinor = minor;
    m.pocketId = pocketId;
    m.spreadMonths = spreadMonths;
    return m;
}

[[nodiscard]] Movement income(const char* id, const char* date, std::int64_t minor,
                              const char* pocketId, bool settled = true) {
    Movement m;
    m.id = id;
    m.date = Date::fromIso(date);
    m.name = id;
    m.kind = MovementKind::Ingreso;
    m.amountMinor = minor;
    m.pocketId = pocketId;
    m.settled = settled;
    return m;
}

[[nodiscard]] Movement transfer(const char* id, const char* date, std::int64_t minor,
                                const char* from, const char* to) {
    Movement m;
    m.id = id;
    m.date = Date::fromIso(date);
    m.name = id;
    m.kind = MovementKind::Traspaso;
    m.amountMinor = minor;
    m.pocketId = from;
    m.targetPocketId = to;
    return m;
}

[[nodiscard]] std::vector<Pocket> tresBolsillos() {
    std::vector<Pocket> pockets;
    for (const auto& [id, name, kind, opening] :
         std::vector<std::tuple<const char*, const char*, PocketKind, std::int64_t>>{
             {"caja", "Caja", PocketKind::Operacion, 100'00},
             {"ahorro", "Ahorro", PocketKind::Ahorro, 500'00},
             {"inv", "Inversion", PocketKind::Inversion, 300'00},
         }) {
        Pocket p;
        p.id = id;
        p.name = name;
        p.kind = kind;
        p.openingMinor = opening;
        pockets.push_back(p);
    }
    return pockets;
}

// ------------------------------------------------------------------ Pruebas

void formaDelMovimiento() {
    std::printf("\n-- forma del movimiento --\n");

    check(!expense("x", "2026-08-01", 0, "caja").isWellFormed(),
          "un importe en cero no es un movimiento");

    Movement negativo = expense("x", "2026-08-01", -100, "caja");
    check(!negativo.isWellFormed(), "un importe negativo se rechaza: el signo lo pone el tipo");

    Movement sinBolsillo = expense("x", "2026-08-01", 100, "");
    check(!sinBolsillo.isWellFormed(), "un gasto sin bolsillo no dice de donde salio la plata");

    Movement traspasoAsiMismo = transfer("x", "2026-08-01", 100, "caja", "caja");
    check(!traspasoAsiMismo.isWellFormed(), "un traspaso a si mismo no mueve nada");

    Movement gastoConDestino = expense("x", "2026-08-01", 100, "caja");
    gastoConDestino.targetPocketId = "ahorro";
    check(!gastoConDestino.isWellFormed(), "solo el traspaso tiene bolsillo de destino");

    check(transfer("x", "2026-08-01", 100, "caja", "ahorro").isWellFormed(),
          "un traspaso entre dos bolsillos distintos es valido");
}

void saldosPorBolsillo() {
    std::printf("\n-- saldos por bolsillo --\n");

    const auto pockets = tresBolsillos();
    const std::vector<Movement> movements{
        income("i1", "2026-08-05", 200'00, "caja"),
        income("i2", "2026-08-06", 80'00, "caja", /*settled=*/false),
        expense("g1", "2026-08-07", 50'00, "caja"),
        transfer("t1", "2026-08-08", 30'00, "ahorro", "caja"),
    };

    const auto balances =
        pocketBalances(pockets, movements, kUsd, Date::fromIso("2026-08-31"));

    checkMinor(balances[0].balance.minor(), 100'00 + 200'00 - 50'00 + 30'00,
               "la caja suma ingreso cobrado, resta gasto y recibe el traspaso");
    checkMinor(balances[0].pendingIn.minor(), 80'00,
               "el ingreso sin cobrar queda aparte y NO infla el saldo");
    checkMinor(balances[1].balance.minor(), 500'00 - 30'00,
               "el ahorro baja exactamente lo que se saco");
    checkMinor(totalAll(balances, kUsd).minor(), 100'00 + 300'00 + 500'00 + 200'00 - 50'00,
               "el traspaso no crea ni destruye plata en el total");

    const auto antes = pocketBalances(pockets, movements, kUsd, Date::fromIso("2026-08-06"));
    checkMinor(antes[0].balance.minor(), 100'00 + 200'00,
               "asOf recorta: al 6 de agosto todavia no paso el gasto del 7");
}

void deDondeSalioLaPlata() {
    std::printf("\n-- de donde salio la plata --\n");

    const auto pockets = tresBolsillos();
    const std::vector<Movement> movements{
        transfer("t1", "2026-08-08", 30'00, "ahorro", "caja"),
        transfer("t2", "2026-08-12", 70'00, "inv", "caja"),
        transfer("t3", "2026-08-20", 25'00, "caja", "ahorro"),
        transfer("t4", "2026-08-21", 40'00, "ahorro", "inv"),
    };

    const auto f =
        funding(pockets, movements, kUsd, Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));

    checkMinor(f.fromReserves.minor(), 100'00, "sumo lo que entro a la caja desde las reservas");
    checkMinor(f.toReserves.minor(), 25'00, "sumo lo que se logro guardar");
    checkMinor(f.net.minor(), 75'00, "el neto es lo que se comio de reserva");
    check(f.eatingReserves(), "el periodo se sostuvo con reserva propia");
    check(f.monthsOfRunway(1) == static_cast<int>((f.reserveBalance.minor()) / 75'00),
          "la autonomia sale del saldo de reserva dividido el ritmo");

    // ahorro -> inversion (t4) mueve plata pero no descapitaliza: sigue siendo
    // reserva. Si contara, cualquier reacomodo interno se leeria como un
    // agujero que no existe.
    const std::vector<Movement> soloInterno{transfer("t4", "2026-08-21", 40'00, "ahorro", "inv")};
    const auto interno = funding(pockets, soloInterno, kUsd, Date::fromIso("2026-08-01"),
                                 Date::fromIso("2026-08-31"));
    checkMinor(interno.net.minor(), 0, "mover ahorro a inversion no cuenta como financiarse");
}

void repartoDelCosto() {
    std::printf("\n-- reparto del costo --\n");

    const Movement pla = expense("pla", "2026-08-24", 46'00, "caja", 4);

    const std::int64_t agosto = costInMonth(pla, kUsd, 2026, 8).minor();
    const std::int64_t noviembre = costInMonth(pla, kUsd, 2026, 11).minor();
    const std::int64_t diciembre = costInMonth(pla, kUsd, 2026, 12).minor();

    checkMinor(agosto, 11'50, "cuatro kilos de PLA cargan un cuarto a agosto, no todo");
    checkMinor(noviembre, 11'50, "y el ultimo cuarto cae en noviembre");
    checkMinor(diciembre, 0, "en diciembre ya no queda nada por imputar");

    std::int64_t suma = 0;
    for (unsigned mes = 8; mes <= 11; ++mes) {
        suma += costInMonth(pla, kUsd, 2026, mes).minor();
    }
    checkMinor(suma, 46'00, "la suma de los meses es exactamente lo pagado, sin centavos sueltos");

    // Un importe que no divide exacto tampoco puede perder ni inventar plata.
    const Movement impar = expense("impar", "2026-08-20", 25'67, "caja", 3);
    std::int64_t sumaImpar = 0;
    for (unsigned mes = 8; mes <= 10; ++mes) {
        sumaImpar += costInMonth(impar, kUsd, 2026, mes).minor();
    }
    checkMinor(sumaImpar, 25'67, "25,67 repartido en tres meses sigue sumando 25,67");

    const std::vector<Movement> movements{pla};
    checkMinor(unusedPrepaid(movements, kUsd, Date::fromIso("2026-08-31")).minor(), 34'50,
               "al cierre de agosto quedan tres cuartos del rollo por delante");
    checkMinor(unusedPrepaid(movements, kUsd, Date::fromIso("2026-11-30")).minor(), 0,
               "en noviembre ya no queda material comprado por delante");
}

void resultadoPorTrabajo() {
    std::printf("\n-- resultado por trabajo --\n");

    std::vector<Job> jobs;
    Job macbook;
    macbook.id = "j";
    macbook.name = "Reparacion Macbook";
    macbook.client = "Herman Galvan";
    macbook.opened = Date::fromIso("2026-08-16");
    macbook.closed = true;
    jobs.push_back(macbook);

    Movement piezas = expense("g", "2026-08-17", 49'82, "caja");
    piezas.jobId = "j";
    Movement cobro = income("i", "2026-08-17", 120'00, "caja");
    cobro.jobId = "j";

    const auto results = jobResults(jobs, {piezas, cobro}, kUsd);
    checkMinor(results[0].margin.minor(), 70'18,
               "la reparacion del Macbook dejo 70,18, un numero que la app actual no puede dar");
    check(results[0].marginBps == 5848, "y un margen del 58,48%");
}

void avisos() {
    std::printf("\n-- avisos --\n");

    const auto demo = realCaseAugust2026(kUsd);
    const auto lista =
        alerts(demo.pockets, demo.movements, demo.jobs, kUsd, Date::fromIso("2026-08-31"));

    check(!lista.empty(), "el caso de agosto genera avisos");
    check(!lista.empty() && lista.front().level == AlertLevel::Danger,
          "y el primero de la lista es el mas grave, no el primero que se calculo");

    bool hablaDeReservas = false;
    for (const Alert& alert : lista) {
        if (alert.title.find("ahorros") != std::string::npos) {
            hablaDeReservas = true;
        }
    }
    check(hablaDeReservas, "avisa, con nombre y apellido, que el mes se sostuvo con ahorro");

    const std::vector<Movement> vacio;
    const auto silencio = alerts({}, vacio, {}, kUsd, Date::fromIso("2026-08-31"));
    check(!silencio.empty(), "una base vacia tambien tiene algo que decir");
}

void formatoDeCifras() {
    std::printf("\n-- como se escribe una cifra --\n");

    const auto usd = [](std::int64_t minor) { return Money::fromMinor(minor, kUsd); };

    check(formatAmount(usd(4600)) == "46,00", "46,00 con coma decimal");
    check(formatAmount(usd(123456789)) == "1.234.567,89", "punto para los miles");
    check(formatAmount(usd(-2002)) == "-20,02", "el menos va adelante");
    check(formatAmount(usd(0)) == "0,00", "el cero se escribe entero");
    check(formatAmount(usd(5)) == "0,05", "los centavos sueltos llevan su cero");

    // El minimo de int64 existe para reventar el negado ingenuo. Un saldo
    // absurdo tiene que salir feo, no tirar la aplicacion abajo.
    check(!formatAmount(usd(INT64_MIN)).empty(), "el minimo de int64 no rompe el formato");

    check(formatCompact(usd(123456789)) == "1,2 M", "version corta en millones");
    check(formatCompact(usd(850000)) == "8,5 k", "version corta en miles");
    check(formatCompact(usd(4600)) == "46", "por debajo de mil va entero");

    check(formatBps(5848) == "58,4%", "el margen del Macbook en porcentaje");
    check(formatBps(10000) == "100,0%", "el tope se escribe entero");
    check(formatBps(-1250) == "-12,5%", "un margen negativo tambien");
}

void diasEnSilencio() {
    std::printf("\n-- dias sin cargar nada --\n");

    const std::vector<Movement> movements{income("i", "2026-08-24", 100, "caja")};
    check(daysSinceLastEntry(movements, Date::fromIso("2026-09-02")) == 9,
          "nueve dias desde el ultimo movimiento");
    check(daysSinceLastEntry({}, Date::fromIso("2026-09-02")) == -1,
          "sin movimientos devuelve -1 y no cero");
    check(daysSinceLastEntry(movements, Date::fromIso("2026-08-01")) == 0,
          "una fecha futura no da dias negativos");
}

/// EL CASO REAL. Los seis movimientos que hay de verdad en
/// %APPDATA%\DakeLabs\Finanzas DakeLabs\finanzas.db, sin agregar ni quitar
/// ninguno.
void puntoDeEquilibrio() {
    std::printf("\n-- punto de equilibrio --\n");

    // Un trabajo que factura 1.000 y cuesta 600: deja 400, o sea 40% de margen.
    std::vector<Job> jobs;
    Job trabajo;
    trabajo.id = "j";
    trabajo.name = "Trabajo";
    trabajo.opened = Date::fromIso("2026-08-01");
    jobs.push_back(trabajo);

    Movement cobro = income("i", "2026-08-10", 1000'00, "caja");
    cobro.jobId = "j";
    Movement costo = expense("g", "2026-08-11", 600'00, "caja");
    costo.jobId = "j";
    // Sin jobId: es estructura, no cuelga de ningun trabajo.
    Movement alquiler = expense("e", "2026-08-05", 100'00, "caja");

    const auto be = breakEven(jobs, {cobro, costo, alquiler}, kUsd,
                              Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));

    checkMinor(be.overheadPerMonth.minor(), 100'00, "la estructura del mes son 100,00");
    check(be.marginBps == 4000, "y el margen ponderado, 40%");
    // 100 de estructura con 40% de margen NO se cubren facturando 100: hay que
    // facturar 250, porque de cada peso quedan 40 centavos.
    checkMinor(be.revenueNeeded.minor(), 250'00,
               "hay que facturar 250,00 al mes para cubrirla, no 100,00");
    check(!be.unknown(), "y el numero se puede saber");

    const auto vacio = breakEven({}, {}, kUsd,
                                 Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));
    check(vacio.unknown(), "sin trabajos, el punto de equilibrio se declara desconocido");
    checkMinor(vacio.revenueNeeded.minor(), 0,
               "y no se inventa un numero para llenar el hueco");
}

void margenPonderado() {
    std::printf("\n-- el margen se pondera por facturacion --\n");

    // Uno chico con margen enorme y uno grande con margen flaco. El promedio
    // simple daria 47,5%; el ponderado, mucho menos. La diferencia decide si el
    // punto de equilibrio esta bien o es una fantasia.
    std::vector<Job> jobs;
    for (const char* id : {"chico", "grande"}) {
        Job j;
        j.id = id;
        j.name = id;
        j.opened = Date::fromIso("2026-08-01");
        jobs.push_back(j);
    }

    Movement ic = income("i1", "2026-08-10", 10'00, "caja");
    ic.jobId = "chico";
    Movement gc = expense("g1", "2026-08-10", 1'00, "caja");
    gc.jobId = "chico";                      // 9,00 de 10,00 = 90%
    Movement ig = income("i2", "2026-08-11", 1000'00, "caja");
    ig.jobId = "grande";
    Movement gg = expense("g2", "2026-08-11", 950'00, "caja");
    gg.jobId = "grande";                     // 50,00 de 1000,00 = 5%

    const auto be = breakEven(jobs, {ic, gc, ig, gg}, kUsd,
                              Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));
    // (9 + 50) / (10 + 1000) = 5,84%. El promedio simple diria 47,5%.
    check(be.marginBps == 584, "el margen ponderado es 5,84% y no el 47,5% del promedio simple");
}

void ticketPromedio() {
    std::printf("\n-- ticket promedio --\n");

    std::vector<Job> jobs;
    for (const char* id : {"a", "b"}) {
        Job j;
        j.id = id;
        j.name = id;
        j.opened = Date::fromIso("2026-08-01");
        jobs.push_back(j);
    }
    Movement i1 = income("i1", "2026-08-10", 100'00, "caja");
    i1.jobId = "a";
    Movement i2 = income("i2", "2026-08-11", 300'00, "caja");
    i2.jobId = "b";

    const auto stats = ticketStats(jobs, {i1, i2}, kUsd,
                                   Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));
    check(stats.jobCount == 2, "dos trabajos con actividad en el mes");
    checkMinor(stats.averageIncome.minor(), 200'00, "y un ticket promedio de 200,00");

    const auto vacio = ticketStats({}, {}, kUsd,
                                   Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));
    check(vacio.jobCount == 0, "sin trabajos, cero");
    checkMinor(vacio.averageIncome.minor(), 0, "y ningun promedio dividido por cero");
}

void diasDeCobro() {
    std::printf("\n-- dias de cobro --\n");

    Movement rapido = income("r", "2026-08-01", 100'00, "caja");
    rapido.settled = true;
    rapido.settledDate = Date::fromIso("2026-08-06");    // 5 dias

    Movement lento = income("l", "2026-08-01", 100'00, "caja");
    lento.settled = true;
    lento.settledDate = Date::fromIso("2026-08-16");     // 15 dias

    // Cobrado, pero de antes de que existiera el campo: no se sabe cuando.
    Movement viejo = income("v", "2026-08-01", 100'00, "caja");
    viejo.settled = true;

    // Entregado y todavia sin cobrar.
    Movement pendiente = income("p", "2026-08-02", 100'00, "caja");
    pendiente.settled = false;

    const auto stats = collectionStats({rapido, lento, viejo, pendiente},
                                       Date::fromIso("2026-08-01"), Date::fromIso("2026-08-31"));

    check(stats.sampled == 2, "solo promedia los dos que tienen fecha de cobro");
    check(stats.averageDays == 10, "el promedio son 10 dias");
    check(stats.worstDays == 15, "y el que mas tardo, 15");
    check(stats.uncollected == 1, "queda uno entregado y sin cobrar");

    // El de fecha desconocida NO vale cero dias: eso bajaria el promedio a 6,67
    // y nadie entenderia por que el numero mejora al anotar cosas viejas.
    check(stats.averageDays != 7, "y el de fecha desconocida no cuenta como cobrado al instante");

    const auto vacio = collectionStats({}, Date::fromIso("2026-08-01"),
                                       Date::fromIso("2026-08-31"));
    check(vacio.sampled == 0 && vacio.averageDays == 0, "sin datos, cero y sin dividir por cero");
}

void elCasoDeAgosto() {
    std::printf("\n-- el caso real de agosto de 2026 --\n");

    std::vector<Pocket> pockets;
    Pocket caja;
    caja.id = "caja";
    caja.name = "Caja";
    caja.kind = PocketKind::Operacion;
    pockets.push_back(caja);

    const std::vector<Movement> reales{
        income("m1", "2026-08-13", 5'00, "caja"),      // IFIX MODELO 3D
        expense("m2", "2026-08-17", 49'82, "caja"),    // Teclado y backlight macbook
        income("m3", "2026-08-17", 120'00, "caja"),    // Trabajo Macbook Herman Galvan
        expense("m4", "2026-08-20", 25'67, "caja", 3), // PLA gris: tres meses
        expense("m5", "2026-08-24", 23'53, "caja", 3), // PLA negro: tres meses
        expense("m6", "2026-08-24", 46'00, "caja", 4), // PLA blanco 4 kg: cuatro meses
    };

    const Date from = Date::fromIso("2026-08-01");
    const Date to = Date::fromIso("2026-08-31");
    const CashFlow flow = cashFlow(reales, kUsd, from, to);

    checkMinor(flow.incomeCash.minor(), 125'00, "entraron 125,00");
    checkMinor(flow.outflow.minor(), 145'02, "salieron 145,02 de caja");

    // Este es el punto. La aplicacion actual calcula utilidad = ingreso -
    // gasto, que da −20,02, y con ESE numero reparte inversion, ahorro y
    // salario. Pero −20,02 no es el resultado del mes: es cuanto bajo la caja.
    checkMinor(flow.cashDelta.minor(), -20'02,
               "la caja bajo 20,02, que es exactamente la 'utilidad' que muestra la app actual");

    // Con el material repartido entre los meses que dura, agosto no fue un mes
    // en perdida: fue un mes en el que se repuso stock.
    checkMinor(flow.cost.minor(), 49'82 + 8'56 + 7'85 + 11'50, "el costo que le toca a agosto");
    checkMinor(flow.result.minor(), 47'27, "agosto cerro con 47,27 a favor, no con 20,02 en contra");

    // Y los dos numeros coinciden en cuanto se deja de repartir: la diferencia
    // no es una licencia contable, es la compra de stock.
    std::vector<Movement> sinReparto = reales;
    for (Movement& movement : sinReparto) {
        movement.spreadMonths = 1;
    }
    const CashFlow plano = cashFlow(sinReparto, kUsd, from, to);
    checkMinor(plano.result.minor(), plano.cashDelta.minor(),
               "sin reparto, resultado y caja vuelven a ser el mismo numero");
}

void elCasoCompleto() {
    std::printf("\n-- el caso de agosto con bolsillos y trabajos --\n");

    const auto demo = realCaseAugust2026(kUsd);
    const Date from = Date::fromIso("2026-08-01");
    const Date to = Date::fromIso("2026-08-31");

    const auto f = funding(demo.pockets, demo.movements, kUsd, from, to);
    checkMinor(f.fromReserves.minor(), 120'00,
               "en agosto entraron 120,00 a la caja desde ahorro e inversion");
    check(f.eatingReserves(), "el mes se sostuvo con reservas propias");

    const auto balances = pocketBalances(demo.pockets, demo.movements, kUsd, to);
    checkMinor(totalFor(balances, PocketKind::Ahorro, kUsd).minor(), 350'00,
               "el ahorro quedo en 350,00, y se ve por que");

    const auto results = jobResults(demo.jobs, demo.movements, kUsd);
    for (const JobResult& result : results) {
        if (result.jobId == "j-macbook") {
            checkMinor(result.margin.minor(), 70'18, "el Macbook dejo 70,18");
        }
        if (result.jobId == "j-lote") {
            checkMinor(result.pending.minor(), 85'00, "el lote esta entregado y sin cobrar");
            checkMinor(result.margin.minor(), 66'60, "y cuando se cobre habra dejado 66,60");
        }
    }

    const auto meses = summarizeByMonth(demo.pockets, demo.movements, kUsd);
    check(meses.size() >= 4,
          "el reparto del material hace que agosto siga costando hasta noviembre");
}

} // namespace

void bolsilloDeEmergencia() {
    std::printf("\n-- bolsillo de emergencia --\n");
    check(pocketKindFromString("Emergencia") == PocketKind::Emergencia, "'Emergencia' se lee");
    check(toString(PocketKind::Emergencia) == "Emergencia", "y se escribe igual");
    check(isReserve(PocketKind::Emergencia), "es reserva, como el ahorro");
    check(allPocketKinds().size() == 5, "hay cinco tipos de bolsillo");

    std::vector<Pocket> pockets = tresBolsillos();
    Pocket fondo;
    fondo.id = "fondo";
    fondo.name = "Fondo";
    fondo.kind = PocketKind::Emergencia;
    fondo.openingMinor = 50'00;
    pockets.push_back(fondo);
    const auto balances = pocketBalances(pockets, {}, kUsd, Date::fromIso("2026-09-27"));
    checkMinor(reserveTotal(balances, kUsd).minor(), 850'00,
               "la reserva suma ahorro, inversion y emergencia");
}

int main() {
    std::printf("Banco de pruebas — verificacion del nucleo\n");

    formaDelMovimiento();
    saldosPorBolsillo();
    deDondeSalioLaPlata();
    repartoDelCosto();
    resultadoPorTrabajo();
    avisos();
    formatoDeCifras();
    diasEnSilencio();
    puntoDeEquilibrio();
    margenPonderado();
    ticketPromedio();
    diasDeCobro();
    elCasoDeAgosto();
    elCasoCompleto();
    bolsilloDeEmergencia();

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    return gFailures == 0 ? 0 : 1;
}
