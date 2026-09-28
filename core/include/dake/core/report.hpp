#pragma once
//
// dake/core/report.hpp — las preguntas que la app tiene que saber responder.
//
// Funciones puras sobre vectores en memoria, sin base de datos de por medio.
// Un reporte que solo se puede probar abriendo un archivo termina sin probarse.
//
// El orden de este archivo es el orden de importancia:
//
//   1. pocketBalances  — cuanto hay, y donde. Lo unico que se puede cuadrar
//                        contra la realidad, y por eso lo unico que se cree.
//   2. funding         — de donde salio la plata del mes. LA pregunta.
//   3. cashFlow        — caja contra costo, separados a proposito.
//   4. jobResults      — cuanto dejo cada trabajo. La razon para cargar datos.
//   5. alerts          — lo que la app dice sin que se lo pidan.
//
// Dependencias permitidas: <cstdint> <string> <vector> "model.hpp" "money.hpp"
//
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

// ------------------------------------------------------ 1. Saldo por bolsillo

struct PocketBalance {
    Id pocketId;
    std::string name;
    PocketKind kind = PocketKind::Operacion;
    /// Saldo inicial + lo que entro - lo que salio, contando SOLO movimientos
    /// liquidados. Un ingreso facturado y no cobrado no esta en el bolsillo.
    Money balance;
    /// Facturado a este bolsillo y todavia sin cobrar.
    Money pendingIn;
};

/// Saldos en el orden en que llegan los bolsillos. `asOf` recorta: los
/// movimientos posteriores no cuentan, que es lo que permite mirar el saldo de
/// un cierre de mes sin borrar nada.
[[nodiscard]] std::vector<PocketBalance> pocketBalances(const std::vector<Pocket>& pockets,
                                                        const std::vector<Movement>& movements,
                                                        Currency currency,
                                                        Date asOf);

/// Suma de los saldos de los bolsillos de una clase.
[[nodiscard]] Money totalFor(const std::vector<PocketBalance>& balances,
                             PocketKind kind,
                             Currency currency);

/// Todo lo que es reserva (ahorro, inversion, emergencia): lo que se puede
/// consumir si el negocio no alcanza.
[[nodiscard]] Money reserveTotal(const std::vector<PocketBalance>& balances, Currency currency);

/// Suma de todos los saldos.
[[nodiscard]] Money totalAll(const std::vector<PocketBalance>& balances, Currency currency);

// -------------------------------------------------- 2. De donde salio la plata

/// La pregunta que la app actual no puede ni formular.
///
/// Un mes se sostiene con lo que se cobra. Cuando no alcanza, la plata sale de
/// algun lado: casi siempre del ahorro o de la inversion. Eso no es un gasto y
/// no aparece en ningun estado de resultados; es una transfusion, y hay que
/// verla en numeros grandes o se repite hasta que no queda reserva.
struct Funding {
    Money fromReserves;   ///< lo que ENTRO a operacion desde ahorro o inversion
    Money toReserves;     ///< lo que se logro guardar en el periodo
    Money net;            ///< fromReserves - toReserves. Positivo = te descapitalizaste
    Money reserveBalance; ///< lo que queda en ahorro + inversion al final del periodo

    /// true si en el periodo salio de las reservas mas de lo que entro.
    [[nodiscard]] bool eatingReserves() const noexcept;

    /// A este ritmo, cuantos meses aguantan las reservas. -1 cuando no aplica
    /// (no se esta consumiendo reserva) y 0 cuando ya no queda nada.
    ///
    /// `periodMonths` es la cantidad de meses que cubre el periodo medido; con
    /// menos de un mes de datos la proyeccion seria ruido y devuelve -1.
    [[nodiscard]] int monthsOfRunway(int periodMonths) const;
};

[[nodiscard]] Funding funding(const std::vector<Pocket>& pockets,
                              const std::vector<Movement>& movements,
                              Currency currency,
                              Date from,
                              Date to);

// ------------------------------------------------------------- 3. Caja y costo

/// Dos numeros que la app actual mezcla en uno solo, y que casi nunca son
/// iguales:
///
///   * CAJA: lo que efectivamente entro y salio del bolsillo este mes.
///   * COSTO: lo que le corresponde al mes, con las compras grandes repartidas
///     entre los meses que van a durar.
///
/// Comprar cuatro kilos de PLA de golpe mueve la caja hoy y el costo durante
/// cuatro meses. Mostrar solo lo primero convierte cada reposicion en un mes en
/// perdida; mostrar solo lo segundo esconde que la plata ya no esta.
struct CashFlow {
    Money incomeCash;   ///< ingresos cobrados en el periodo
    Money incomeAccrued;///< ingresos del periodo, cobrados o no
    Money outflow;      ///< gastos pagados en el periodo (salida real de caja)
    Money cost;         ///< costo imputado al periodo, con los repartos aplicados
    Money cashDelta;    ///< incomeCash - outflow
    Money result;       ///< incomeAccrued - cost. El resultado "de verdad"
};

[[nodiscard]] CashFlow cashFlow(const std::vector<Movement>& movements,
                                Currency currency,
                                Date from,
                                Date to);

/// Lo que `movement` le cuesta al mes (year, month): el gasto entero en el mes
/// de su fecha, cero en cualquier otro. Nada se reparte en meses.
[[nodiscard]] Money costInMonth(const Movement& movement,
                                Currency currency,
                                int year,
                                unsigned month);

// ---------------------------------------------------------- 4. Por trabajo

struct JobResult {
    Id jobId;
    std::string name;
    std::string client;
    bool closed = false;
    Money income;   ///< cobrado y por cobrar, ambos: el trabajo se hizo igual
    Money pending;  ///< la parte facturada y no cobrada
    Money cost;     ///< gastos imputados a este trabajo, sin repartir
    Money margin;   ///< income - cost
    /// margin / income en puntos basicos. 0 si no hubo ingreso todavia.
    int marginBps = 0;
};

/// Un resultado por trabajo, del mas reciente al mas viejo por fecha de
/// apertura. Los trabajos con lapida se excluyen.
[[nodiscard]] std::vector<JobResult> jobResults(const std::vector<Job>& jobs,
                                                const std::vector<Movement>& movements,
                                                Currency currency);

/// Gastos del periodo que no cuelgan de ningun trabajo: la estructura del
/// negocio. Se mira aparte del margen de los trabajos porque no se reparte
/// entre ellos sin inventar un criterio.
[[nodiscard]] Money overhead(const std::vector<Movement>& movements,
                             Currency currency,
                             Date from,
                             Date to);

// --------------------------------------------------------------- 5. Alertas

enum class AlertLevel {
    Info,     ///< algo que conviene saber
    Warning,  ///< algo que conviene mirar hoy
    Danger    ///< algo que ya esta pasando y cuesta plata
};

struct Alert {
    AlertLevel level = AlertLevel::Info;
    std::string title;   ///< una linea, en numeros concretos
    std::string detail;  ///< que hacer con eso
};

/// Lo que la app dice sin que se lo pregunten.
///
/// Un libro de cuentas es pasivo: solo devuelve lo que se le mete, y una
/// herramienta que no devuelve mas de lo que cuesta alimentarla se abandona.
/// Estas alertas son la parte que devuelve algo.
///
/// El orden es por gravedad: lo primero de la lista es lo que mas cuesta.
///
/// `format` decide como se escribe cada cifra dentro del texto. El nucleo no
/// sabe de comas ni de puntos de mil —eso es presentacion— pero un aviso que
/// dice "46.00 USD" cuando toda la pantalla dice "46,00" se lee como si
/// viniera de otro programa. Por defecto usa el formato canonico.
using MoneyFormatter = std::function<std::string(const Money&)>;

[[nodiscard]] std::vector<Alert> alerts(const std::vector<Pocket>& pockets,
                                        const std::vector<Movement>& movements,
                                        const std::vector<Job>& jobs,
                                        Currency currency,
                                        Date asOf,
                                        const MoneyFormatter& format = {});

// --------------------------------------------------------- Resumen por mes

struct MonthSummary {
    int year = 1970;
    unsigned month = 1;
    Money incomeAccrued;
    Money cost;
    Money result;
    Money netFunding; ///< lo que salio de las reservas ese mes

    /// "ago 2026"
    [[nodiscard]] std::string label() const;
};

/// Un resumen por cada mes con actividad, del mas viejo al mas nuevo.
[[nodiscard]] std::vector<MonthSummary> summarizeByMonth(const std::vector<Pocket>& pockets,
                                                         const std::vector<Movement>& movements,
                                                         Currency currency);

// ------------------------------------------------- 6. Salud del negocio

/// Cuanto hay que facturar por mes para que el negocio se pague solo.
///
/// No es "cuanto gasto": es cuanto ingreso hace falta para cubrir la estructura
/// SABIENDO que cada peso facturado deja solo una parte. Con 40% de margen, una
/// estructura de 100 no se cubre facturando 100 sino 250.
struct BreakEven {
    Money overheadPerMonth;  ///< estructura que ningun trabajo paga, por mes
    int marginBps = 0;       ///< margen promedio de los trabajos, en puntos basicos
    Money revenueNeeded;     ///< facturacion mensual para llegar a cero

    /// true cuando no se puede calcular: sin trabajos con margen positivo, la
    /// division no existe y cualquier numero que se muestre seria inventado.
    [[nodiscard]] bool unknown() const noexcept { return marginBps <= 0; }
};

/// `periodMonths` reparte la estructura del rango entre esos meses.
[[nodiscard]] BreakEven breakEven(const std::vector<Job>& jobs,
                                  const std::vector<Movement>& movements,
                                  Currency currency,
                                  Date from,
                                  Date to);

/// Cuanto deja un trabajo en promedio y cuantos hubo. Sirve para saber si el
/// problema es que cobras poco o que haces pocos, que tienen soluciones
/// distintas.
struct TicketStats {
    int jobCount = 0;        ///< trabajos con actividad en el rango
    Money averageIncome;     ///< facturacion promedio por trabajo
    Money averageMargin;     ///< lo que deja cada uno, en promedio
};

[[nodiscard]] TicketStats ticketStats(const std::vector<Job>& jobs,
                                      const std::vector<Movement>& movements,
                                      Currency currency,
                                      Date from,
                                      Date to);

/// Cuanto tardas en cobrar lo que entregaste.
///
/// Solo entran los ingresos con `settled == true` Y `settledDate` con valor
/// —o sea, distinto de std::nullopt—. Los anotados antes de que existiera el
/// campo no se cuentan: inventarles una fecha ensuciaria el promedio sin que
/// nadie se entere.
/// `sampled` dice sobre cuantos se calculo, para poder mostrar "sobre 3
/// trabajos" y que el numero se lea con la desconfianza que merece.
struct CollectionStats {
    int sampled = 0;         ///< cuantos ingresos entraron en el promedio
    int averageDays = 0;     ///< promedio, redondeado
    int worstDays = 0;       ///< el que mas tardo
    int uncollected = 0;     ///< entregados y todavia sin cobrar
};

[[nodiscard]] CollectionStats collectionStats(const std::vector<Movement>& movements,
                                              Date from,
                                              Date to);

struct CategoryTotal {
    std::string category;
    Money total;
};

/// Gastos del rango agrupados por categoria, de mayor a menor. Usa el COSTO
/// imputado, no la salida de caja: es la vista que sirve para decidir.
[[nodiscard]] std::vector<CategoryTotal> costByCategory(const std::vector<Movement>& movements,
                                                        Currency currency,
                                                        Date from,
                                                        Date to);

} // namespace dake::core
