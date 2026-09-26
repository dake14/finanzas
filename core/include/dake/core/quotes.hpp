#pragma once
//
// dake/core/quotes.hpp — lo que Finanzas hace con los documentos de DakeLabs
// Cotizaciones.
//
// La conexion va en UN solo sentido: Finanzas lee y nunca escribe. De cada
// documento sale lo que tiene que existir de este lado (la reparacion, su
// ingreso por cobrar o cobrado, el abono), y quien aplica el plan compara
// contra lo que ya hay: releer la carpeta cien veces deja lo mismo que una.
//
// Cotizaciones manda en cliente, equipo, precio, fechas y estado. Finanzas
// manda en tipo, horas, costos, bolsillo y categoria.
//
// NUNCA DOS VECES. Los ingresos importados tienen ids derivados del documento
// ("cot-<id>-saldo", "cot-<id>-abono"), y la reparacion, del documento raiz
// ("cot-<id de la cotizacion>"). Lo que ya esta anotado a mano no se duplica:
// se propone enlazarlo, y hasta que alguien decida, el documento espera.
//
// Dependencias permitidas: <map> <optional> <string> <vector>
//                          "model.hpp" "repairs.hpp"
//
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/repairs.hpp"

namespace dake::core {

enum class QuoteKind { Cotizacion, Informe };

struct QuoteLine {
    std::string section;  ///< "Mano de obra", "Repuestos y materiales"
    std::string item;  ///< el concepto: "Diagnostico y Reparacion", "Pieza"
    std::int64_t totalMinor = 0;
};

struct QuoteDoc {
    std::string id;
    std::string number;  ///< "INF-2026-004"
    QuoteKind kind = QuoteKind::Informe;
    /// Tal como lo escribe Cotizaciones: borrador, enviada, aceptada,
    /// rechazada (cotizacion); borrador, entregado, pagado (informe).
    std::string status;
    std::string client;
    std::string device;
    std::string originId;  ///< la cotizacion de la que salio el informe
    std::optional<Date> issued;
    std::optional<Date> received;   ///< cuando entro el equipo
    std::optional<Date> delivered;  ///< fecha de entrega
    std::optional<Date> paid;       ///< la del evento "Pagado" del historial
    std::int64_t baseMinor = 0;     ///< subtotal menos descuento
    std::int64_t depositMinor = 0;  ///< abono
    std::vector<QuoteLine> lines;

    [[nodiscard]] std::int64_t balanceMinor() const { return baseMinor - depositMinor; }
    /// El documento raiz: la cotizacion de origen, o el propio si no tiene.
    [[nodiscard]] std::string root() const { return originId.empty() ? id : originId; }
};

// -------------------------------------------------------------------- Totales
//
// Las mismas reglas que calcularTotales de Cotizaciones, en el mismo orden:
// cada linea se redondea al centavo (la mitad lejos del cero), se suman, y el
// descuento se calcula sobre el subtotal y tambien se redondea.

enum class DiscountKind { Ninguno, Porcentaje, Monto };

[[nodiscard]] std::int64_t quoteLineTotal(double quantity, std::int64_t unitMinor);
/// `value`: puntos basicos si es porcentaje (7144 = 71,44%), centavos si es monto.
[[nodiscard]] std::int64_t quoteBase(std::int64_t subtotalMinor, DiscountKind kind,
                                     std::int64_t value);

// ---------------------------------------------------------------------- Plan

/// Por que un documento espera en vez de importarse.
enum class QuoteHold {
    Ninguno,
    Duplicado,    ///< parece el mismo que otro: `relatedNumber`
    YaAnotado,    ///< hay un ingreso a mano que parece este: `candidateMovementId`
    EnCorreccion, ///< volvio a borrador despues de importado: no se toca
    SinMonto      ///< total cero o negativo
};

enum class QuoteDecision { Importar, Esperar, Nada };

struct QuotePlan {
    std::string docId;
    std::string number;
    QuoteDecision decision = QuoteDecision::Nada;
    QuoteHold hold = QuoteHold::Ninguno;
    std::string relatedNumber;
    Id candidateMovementId;

    /// La reparacion como tiene que quedar. `newJob`: hay que crear el
    /// trabajo `repair.jobId` con `client`.
    std::optional<Repair> repair;
    bool newJob = false;
    std::string client;
    /// Repuestos que el informe nombra y la ficha no tiene: sin costo.
    std::vector<std::string> newParts;
    /// Los ingresos como tienen que quedar (nuevos o existentes, por id).
    std::vector<Movement> incomes;
};

/// Lo que se decidio a mano sobre un documento que esperaba:
/// "nuevo" (importar igual), "ignorar", o "enlace:<id de movimiento>".
using QuoteDecisions = std::map<std::string, std::string>;

struct QuoteContext {
    std::vector<Repair> repairs;
    std::vector<Job> jobs;
    std::vector<Movement> movements;
    std::vector<RepairPart> parts;
    QuoteDecisions decisions;
    Id pocketId;           ///< donde entran los cobros
    std::string category;  ///< "Reparaciones"
};

[[nodiscard]] std::vector<QuotePlan> planQuotes(const std::vector<QuoteDoc>& docs,
                                                const QuoteContext& context);

/// El tipo de reparacion que sugieren el equipo y los conceptos: "RTX 3080"
/// es GPU, "B450" es placa madre, "Macbook" es laptop. Otro si no se sabe.
[[nodiscard]] RepairType guessRepairType(const QuoteDoc& doc);

} // namespace dake::core
