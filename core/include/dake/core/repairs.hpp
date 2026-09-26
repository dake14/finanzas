#pragma once
//
// dake/core/repairs.hpp — cuanto deja cada reparacion y cada tipo.
//
// Una reparacion es un trabajo (Job) con ficha: tipo, horas, repuestos,
// precio. La ficha vive en una tabla local; el Job sigue siendo el que
// sincroniza, asi que el telefono ve la reparacion como un trabajo mas.
//
// LAS HORAS SON TU SUELDO. El costo de una reparacion incluye las horas a la
// tarifa objetivo, asi que un margen de 0% no es perder: es cobrar justo tu
// tarifa. Todo margen positivo es ganancia por encima de eso. Por la misma
// razon, la utilidad neta del mes NO resta horas: ahi las horas no son un
// gasto que se le pague a otro.
//
// LOS FIJOS SE REPARTEN POR HORA TRABAJADA. Una placa madre de seis horas
// ocupa el taller mas que un cambio de pasta de media hora. La tasa por hora
// se calcula aparte (etapa de fijos) y llega aca como un numero.
//
// Dependencias permitidas: <optional> <string> <vector> "model.hpp" "money.hpp"
//
#include <optional>
#include <string>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

enum class RepairType { GPU, Laptop, PlacaMadre, Otro };
enum class RepairStatus { EnProceso, Entregada, Cobrada };

[[nodiscard]] std::string_view toString(RepairType value) noexcept;
[[nodiscard]] RepairType repairTypeFromString(std::string_view text);
[[nodiscard]] std::string_view toString(RepairStatus value) noexcept;
[[nodiscard]] RepairStatus repairStatusFromString(std::string_view text);

/// Los cuatro tipos, en el orden en que se muestran.
[[nodiscard]] std::vector<RepairType> allRepairTypes();

struct RepairPart {
    Id id;
    Id jobId;
    std::string name;
    std::int64_t costMinor = 0;
    /// false para lo que vino de un informe sin costo: lo que el cliente pago
    /// por la pieza no es lo que costo, y hasta que alguien lo escriba no se
    /// sabe.
    bool costKnown = true;
    /// El gasto que la pago, si se compro para esta reparacion. El costo sale
    /// de aca y no del gasto, para no contarlo dos veces.
    Id movementId;
};

struct Repair {
    Id jobId;
    std::string orderNo;
    std::string device;
    RepairType type = RepairType::Otro;
    Id templateId;
    std::optional<Date> received;
    std::optional<Date> delivered;
    RepairStatus status = RepairStatus::EnProceso;
    std::int64_t priceMinor = 0;
    std::int64_t shippingMinor = 0;
    std::int64_t consumablesMinor = 0;
    int estMinutes = 0;
    std::optional<int> realMinutes;  ///< vacio: todavia no se sabe
    /// De donde vino, si vino de afuera: "cot:<id del documento>".
    std::string sourceRef;
};

struct TemplatePart {
    std::string name;
    std::int64_t costMinor = 0;
};

struct RepairTemplate {
    Id id;
    RepairType type = RepairType::Otro;
    std::string name;
    std::int64_t priceMinor = 0;
    int estMinutes = 0;
    std::int64_t consumablesMinor = 0;
    std::int64_t shippingMinor = 0;
    std::vector<TemplatePart> parts;
};

/// Lo que se configura una vez.
struct CostSettings {
    std::int64_t hourlyRateMinor = 0;    ///< tarifa por hora objetivo
    int targetMarginBps = 3000;          ///< margen objetivo
    std::int64_t fixedPerHourMinor = 0;  ///< fijos del taller por hora trabajada
};

// --------------------------------------------------------------- Por reparacion

struct RepairCosting {
    Id jobId;
    Money price;
    Money parts;         ///< repuestos con costo conocido
    Money consumables;
    Money shipping;
    Money direct;        ///< repuestos + consumibles + envio
    Money labor;         ///< horas x tarifa
    Money fixedShare;    ///< horas x tasa de fijos
    Money cost;          ///< direct + labor + fixedShare
    Money profit;        ///< price - cost
    int minutes = 0;     ///< las reales, o las estimadas si no se saben
    bool hoursEstimated = false;
    bool partsIncomplete = false;  ///< hay repuestos sin costo todavia
    std::optional<int> marginBps;          ///< vacio si el precio es 0
    std::optional<Money> profitPerHour;    ///< (precio - directo) / horas; vacio sin horas
    std::optional<Money> suggestedPrice;   ///< costo / (1 - margen objetivo)
    [[nodiscard]] bool belowTarget(int targetBps) const noexcept {
        return marginBps && *marginBps < targetBps;
    }
};

/// El precio de una reparacion: lo que entro por ella (los ingresos
/// enlazados, cobrados o no), y si todavia no entro nada, el precio de la
/// ficha.
[[nodiscard]] Money repairPrice(const Repair& repair, const std::vector<Movement>& movements,
                                Currency currency);

/// Los gastos enlazados a la reparacion que ningun repuesto reclama: son los
/// repuestos anotados antes de que existiera la ficha, y cuentan como tales.
[[nodiscard]] Money looseExpenses(const Repair& repair, const std::vector<RepairPart>& parts,
                                  const std::vector<Movement>& movements, Currency currency);

[[nodiscard]] RepairCosting costRepair(const Repair& repair,
                                       const std::vector<RepairPart>& parts,
                                       const std::vector<Movement>& movements,
                                       const CostSettings& settings,
                                       Currency currency);

// ------------------------------------------------------------------ Por tipo

enum class Verdict { PocosDatos, Bien, Cerca, Bajo };

struct TypeStats {
    RepairType type = RepairType::Otro;
    int count = 0;
    Money revenue;
    Money profit;
    Money averagePrice;
    Money averageCost;
    std::optional<int> marginBps;          ///< ponderado por facturacion
    std::optional<Money> profitPerHour;
    /// Horas reales / estimadas en milesimas: 1400 = tardas 1,4 veces lo
    /// presupuestado. Solo con reparaciones que tienen las dos.
    std::optional<int> hoursRatioPermille;
    std::optional<Money> suggestedPrice;   ///< costo promedio / (1 - objetivo)
    Verdict verdict = Verdict::PocosDatos;
};

/// Lo que tiene que dejar cada hora: tarifa + fijos por hora. Una reparacion
/// que deja menos que esto por hora pierde plata aunque parezca buena.
[[nodiscard]] Money hourlyNeeded(const CostSettings& settings, Currency currency);

/// Estadisticas de las reparaciones entregadas (o cobradas) cuya fecha de
/// entrega cae en [from, to]. Un tipo sin reparaciones no aparece.
///
/// El veredicto: Bien en o sobre el objetivo, Cerca hasta 5 puntos debajo,
/// Bajo mas abajo, PocosDatos con menos de 3 reparaciones.
[[nodiscard]] std::vector<TypeStats> statsByType(const std::vector<Repair>& repairs,
                                                 const std::vector<RepairPart>& parts,
                                                 const std::vector<Movement>& movements,
                                                 const CostSettings& settings,
                                                 Currency currency,
                                                 Date from,
                                                 Date to);

// ---------------------------------------------------------- Plantillas y alta

/// El proximo numero de orden propio: "R-0001", "R-0002"... Los que vienen de
/// otro lado (INF-2026-004) no cuentan.
[[nodiscard]] std::string nextOrderNumber(const std::vector<Repair>& repairs);

/// Una reparacion nueva desde una plantilla: en proceso, recibida hoy, con
/// precio, horas, consumibles y envio de la plantilla.
[[nodiscard]] Repair repairFromTemplate(const RepairTemplate& tpl, const Id& jobId,
                                        const std::string& orderNo, const std::string& device,
                                        Date today);

/// Las plantillas con las que arranca la aplicacion, una por tipo, sin id.
/// Sin precio: el precio es de quien cobra, y un numero inventado aca
/// terminaria cobrandose. Horas y consumibles tipicos, para ajustar.
[[nodiscard]] std::vector<RepairTemplate> defaultTemplates();

/// Los repuestos tipicos de la plantilla, sin id: los pone quien guarda.
[[nodiscard]] std::vector<RepairPart> partsFromTemplate(const RepairTemplate& tpl,
                                                        const Id& jobId);

// ---------------------------------------------------------- Entregar y cobrar

/// El ingreso de una reparacion, si ya hay uno enlazado (el primero).
[[nodiscard]] std::optional<Movement> repairIncome(const Id& jobId,
                                                   const std::vector<Movement>& movements);

struct DeliveryResult {
    Repair repair;
    /// El ingreso nuevo o actualizado. Vacio si no hace falta tocarlo.
    std::optional<Movement> income;
};

/// Entregar: fecha de entrega, horas reales y precio final. Crea el ingreso
/// POR COBRAR (o, con `charged`, ya cobrado) en `pocketId`, categoria
/// `category`. Si la reparacion ya tiene un ingreso enlazado —porque se anoto
/// el cobro antes—, no crea otro: le ajusta el monto al precio final.
[[nodiscard]] DeliveryResult deliverRepair(const Repair& repair,
                                           const std::vector<Movement>& movements,
                                           Date date, int realMinutes,
                                           std::int64_t priceMinor, bool charged,
                                           const Id& pocketId, const std::string& category);

/// Cobrar: marca el ingreso cobrado con fecha. Si no habia ingreso, lo crea
/// ya cobrado por el precio de la ficha.
[[nodiscard]] DeliveryResult chargeRepair(const Repair& repair,
                                          const std::vector<Movement>& movements, Date date,
                                          const Id& pocketId, const std::string& category);

/// El estado que dicen los movimientos: un ingreso cobrado enlazado la deja
/// cobrada (y entregada, si no tenia fecha); uno por cobrar, entregada. Es la
/// otra puerta: anotar "120 cobro GPU 3080" cierra la reparacion igual que
/// el boton Cobrar.
[[nodiscard]] Repair reconcileRepair(const Repair& repair, const std::vector<Movement>& movements);

} // namespace dake::core
