#pragma once
//
// dake/core/fixed.hpp — lo que cuesta el taller por existir, y lo que falta
// ordenar.
//
// RECURRENTES. Luz, internet, software, alquiler: se anotan solos cada mes
// con el monto estimado y quedan "por confirmar". Confirmar es un Enter; si la
// factura vino distinta, el monto corregido pasa a ser el estimado del mes
// siguiente. Si la aplicacion estuvo cerrada, se generan todos los meses que
// faltan, uno por mes y nunca dos veces: el id sale del recurrente y del mes.
//
// HERRAMIENTAS. No generan movimientos: su costo entra como depreciacion,
// repartido en los meses de vida util. Comprar un osciloscopio no puede
// convertir un mes en perdida.
//
// TASA DE FIJOS POR HORA. Los fijos del mes (recurrentes + depreciacion)
// entre las horas que se trabajan en un mes normal. Es lo que cada hora de
// reparacion tiene que cubrir ademas de la tarifa.
//
// LA BANDEJA. Lo que la aplicacion no pudo deducir y espera una decision. Es
// una consulta, no una tabla: se calcula cada vez y no hay nada que mantener.
//
// Dependencias permitidas: <optional> <string> <vector> "model.hpp" "money.hpp"
//                          "accounts.hpp" "repairs.hpp"
//
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"
#include "dake/core/repairs.hpp"

namespace dake::core {

// ------------------------------------------------------------------ Datos

struct Recurring {
    Id id;
    std::string name;
    std::string category;
    Id pocketId;
    std::int64_t amountMinor = 0;  ///< el estimado
    int dayOfMonth = 1;            ///< 31 en febrero es el 28 (o 29)
    Date starts;                   ///< primer mes que se genera
    std::optional<Date> ends;
    bool active = true;
};

struct Tool {
    Id id;
    std::string name;
    std::int64_t costMinor = 0;
    Date bought;
    int lifeMonths = 24;
    std::optional<Date> retired;
    Id movementId;  ///< la compra, si se anoto en la aplicacion
};

/// Lo que se sabe de un movimiento ademas de el mismo. Vive en una tabla
/// local: no viaja al telefono.
struct MovementMeta {
    Id movementId;
    std::string origin;        ///< "Manual", "Recurrente", "Reparacion", "Importado"
    std::string review;        ///< "", "confirmar", "sugerido", "vida"
    Id recurringId;
    std::string period;        ///< "2026-09"
    std::string externalRef;   ///< huella de lo importado
};

/// "2026-09".
[[nodiscard]] std::string periodOf(Date date);

// ------------------------------------------------------------ Recurrentes

struct GeneratedRecurring {
    Movement movement;  ///< con id "rec-<recurrente>-<periodo>"
    MovementMeta meta;  ///< origen Recurrente, "por confirmar"
};

/// Los movimientos de recurrentes que ya vencieron hasta `today` y todavia no
/// se generaron. Uno ya generado no vuelve aunque se haya borrado: su meta
/// queda, y borrarlo fue una decision.
[[nodiscard]] std::vector<GeneratedRecurring> dueRecurring(const std::vector<Recurring>& recurring,
                                                           const std::vector<MovementMeta>& metas,
                                                           Date today);

// ------------------------------------------------------------ Herramientas

/// Lo que le toca al mes de `month` del costo de la herramienta. El reparto
/// suma exactamente el costo: ni un centavo se pierde ni se inventa.
[[nodiscard]] Money depreciationInMonth(const Tool& tool, Date month, Currency currency);
[[nodiscard]] Money depreciationInMonth(const std::vector<Tool>& tools, Date month,
                                        Currency currency);

// --------------------------------------------------------- Tasa de fijos

struct FixedRate {
    Money monthlyFixed;          ///< recurrentes activos + depreciacion del mes
    int monthlyMinutes = 0;      ///< horas trabajadas en un mes normal
    bool minutesFromSettings = false;  ///< sin historial: las de Ajustes
    Money perHour;
};

/// Los fijos del mes entre las horas reales por mes de los ultimos tres
/// meses cerrados con reparaciones entregadas (menos, si la aplicacion es mas
/// nueva). Sin horas reales, las de Ajustes.
[[nodiscard]] FixedRate fixedRate(const std::vector<Recurring>& recurring,
                                  const std::vector<Tool>& tools,
                                  const std::vector<Repair>& repairs,
                                  Date today, int fallbackMinutesPerMonth, Currency currency);

// ------------------------------------------------------------------ Bandeja

enum class InboxKind {
    SinCategoria,      ///< un movimiento sin categoria
    PorConfirmar,      ///< un recurrente generado solo
    Sugerido,          ///< un importado con categoria sugerida
    VidaUtil,          ///< una herramienta nueva: cuantos meses dura
    Cotizaciones,      ///< documentos de Cotizaciones esperando decision
    PorCobrar,         ///< entregada hace mas de 7 dias y sin cobrar
    SinEntregar,       ///< en proceso hace mas de 14 dias
    SinHoras,          ///< entregada sin horas reales
    CostoRepuesto      ///< un repuesto sin costo
};

struct InboxItem {
    InboxKind kind = InboxKind::SinCategoria;
    Id refId;  ///< el movimiento, la reparacion (jobId) o el repuesto
    int days = 0;  ///< dias de atraso, cuando aplica
};

struct InboxInput {
    std::vector<Movement> movements;
    std::vector<MovementMeta> metas;
    std::vector<Repair> repairs;
    std::vector<RepairPart> parts;
    int quoteHolds = 0;
    /// Lo pospuesto: id -> hasta cuando no se muestra.
    std::vector<std::pair<Id, Date>> snoozed;
    Date today;
};

/// En el orden en que conviene resolverlo: primero lo que ensucia los
/// numeros (sin categoria, por confirmar), despues lo que cuesta plata (por
/// cobrar), al final lo que solo afina (horas, costos de repuestos).
[[nodiscard]] std::vector<InboxItem> inbox(const InboxInput& input);

} // namespace dake::core
