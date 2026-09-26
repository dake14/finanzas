#include "dake/core/quotes.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

#include "dake/core/capture.hpp"

namespace dake::core {
namespace {

[[nodiscard]] std::int64_t divRound(std::int64_t a, std::int64_t b) {
    const std::int64_t half = b / 2;
    return a >= 0 ? (a + half) / b : -((-a + half) / b);
}

[[nodiscard]] std::vector<std::string> tokens(std::string_view text) {
    std::vector<std::string> out;
    std::string current;
    for (const char c : foldText(text)) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current.push_back(c);
        } else if (!current.empty()) {
            out.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

[[nodiscard]] bool isChipset(const std::string& token) {
    // b450, x570, z690, h610, a520...: una letra y tres cifras.
    return token.size() == 4 && std::string("abhxz").find(token[0]) != std::string::npos &&
           std::all_of(token.begin() + 1, token.end(),
                       [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

[[nodiscard]] bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

[[nodiscard]] bool sameText(const std::string& a, const std::string& b) {
    return !a.empty() && foldText(a) == foldText(b);
}

[[nodiscard]] std::int64_t distanceDays(Date a, Date b) {
    const std::int64_t d = a.toEpochDays() - b.toEpochDays();
    return d < 0 ? -d : d;
}

[[nodiscard]] const Movement* findMovement(const std::vector<Movement>& movements, const Id& id) {
    for (const Movement& m : movements) {
        if (!m.deleted && m.id == id) return &m;
    }
    return nullptr;
}

[[nodiscard]] const Job* findJob(const std::vector<Job>& jobs, const Id& id) {
    for (const Job& j : jobs) {
        if (j.id == id) return &j;
    }
    return nullptr;
}

/// La reparacion de un documento raiz: la marcada con el, o la del trabajo de
/// id derivado.
[[nodiscard]] const Repair* repairForRoot(const std::vector<Repair>& repairs, const std::string& root) {
    for (const Repair& r : repairs) {
        if (r.sourceRef == "cot:" + root) return &r;
    }
    for (const Repair& r : repairs) {
        if (r.jobId == "cot-" + root) return &r;
    }
    return nullptr;
}

/// La unica reparacion abierta, sin documento, de ese cliente. Con dos o mas
/// no se adivina.
[[nodiscard]] const Repair* adoptableRepair(const QuoteContext& c, const std::string& client) {
    const Repair* found = nullptr;
    for (const Repair& r : c.repairs) {
        if (r.status == RepairStatus::Cobrada || !r.sourceRef.empty()) continue;
        const Job* job = findJob(c.jobs, r.jobId);
        if (job == nullptr || job->deleted || !sameText(job->client, client)) continue;
        if (found != nullptr) return nullptr;
        found = &r;
    }
    return found;
}

/// Un ingreso ya enlazado a esa reparacion que no vino de Cotizaciones.
[[nodiscard]] const Movement* manualIncome(const std::vector<Movement>& movements, const Id& jobId) {
    for (const Movement& m : movements) {
        if (!m.deleted && m.kind == MovementKind::Ingreso && m.jobId == jobId &&
            !startsWith(m.id, "cot-")) {
            return &m;
        }
    }
    return nullptr;
}

/// Un ingreso anotado a mano que parece ser el de este informe. Puede tener
/// trabajo, siempre que ese trabajo no venga ya de otro documento. Tiene que
/// ser de un mes alrededor de la entrega y por lo mismo: hasta 1% de
/// diferencia, o hasta 10% si la descripcion nombra el equipo o el cliente
/// (alguien que anoto 120 por un informe de 119,83).
[[nodiscard]] const Movement* candidateIncome(const QuoteContext& c, const QuoteDoc& doc) {
    const Date anchor = doc.delivered.value_or(doc.issued.value_or(Date{}));
    std::set<std::string> names;
    for (const std::string& w : tokens(doc.device + " " + doc.client)) {
        if (w.size() >= 4) names.insert(w);
    }
    const Movement* best = nullptr;
    std::int64_t bestDiff = 0;
    for (const Movement& m : c.movements) {
        if (m.deleted || m.kind != MovementKind::Ingreso || startsWith(m.id, "cot-")) continue;
        if (distanceDays(m.date, anchor) > 30) continue;
        if (!m.jobId.empty()) {
            const auto owner = std::find_if(c.repairs.begin(), c.repairs.end(),
                                            [&m](const Repair& r) { return r.jobId == m.jobId; });
            if (owner != c.repairs.end() && !owner->sourceRef.empty()) continue;
        }
        const std::int64_t diff = std::min(std::llabs(m.amountMinor - doc.baseMinor),
                                           std::llabs(m.amountMinor - doc.balanceMinor()));
        std::string text = m.name;
        if (const Job* job = findJob(c.jobs, m.jobId)) text += " " + job->name + " " + job->client;
        bool named = false;
        for (const std::string& w : tokens(text)) {
            if (names.contains(w)) named = true;
        }
        const std::int64_t onePercent = std::max<std::int64_t>(1, doc.baseMinor / 100);
        if (diff > onePercent && !(named && diff <= doc.baseMinor / 10)) continue;
        if (best == nullptr || diff < bestDiff) {
            best = &m;
            bestDiff = diff;
        }
    }
    return best;
}

[[nodiscard]] int statusRank(RepairStatus s) {
    switch (s) {
        case RepairStatus::EnProceso: return 0;
        case RepairStatus::Entregada: return 1;
        case RepairStatus::Cobrada: return 2;
    }
    return 0;
}

/// La reparacion de este documento, como tiene que quedar. Lo de Finanzas
/// (tipo, horas, costos) no se toca si ya existia.
[[nodiscard]] Repair desiredRepair(const QuoteDoc& doc, const Repair* existing, const Id& jobId,
                                   RepairStatus minimum) {
    Repair r;
    if (existing != nullptr) {
        r = *existing;
    } else {
        r.jobId = jobId;
        r.type = guessRepairType(doc);
    }
    r.sourceRef = "cot:" + doc.root();
    if (!doc.device.empty()) r.device = doc.device;
    // El numero es el del informe; el de la cotizacion vale hasta que llegue.
    if (doc.kind == QuoteKind::Informe || r.orderNo.empty() || startsWith(r.orderNo, "COT-")) {
        r.orderNo = doc.number;
    }
    if (doc.received) r.received = doc.received;
    else if (!r.received) r.received = doc.issued;
    if (doc.kind == QuoteKind::Informe) {
        r.priceMinor = doc.baseMinor;
        if (doc.delivered) r.delivered = doc.delivered;
    }
    if (statusRank(minimum) > statusRank(r.status)) r.status = minimum;
    return r;
}

} // namespace

// -------------------------------------------------------------------- Totales

std::int64_t quoteLineTotal(double quantity, std::int64_t unitMinor) {
    return static_cast<std::int64_t>(std::llround(quantity * static_cast<double>(unitMinor)));
}

std::int64_t quoteBase(std::int64_t subtotalMinor, DiscountKind kind, std::int64_t value) {
    switch (kind) {
        case DiscountKind::Ninguno: return subtotalMinor;
        case DiscountKind::Monto: return subtotalMinor - value;
        case DiscountKind::Porcentaje: return subtotalMinor - divRound(subtotalMinor * value, 10000);
    }
    return subtotalMinor;
}

// ---------------------------------------------------------------------- Tipo

RepairType guessRepairType(const QuoteDoc& doc) {
    std::vector<std::string> words = tokens(doc.device);
    for (const QuoteLine& line : doc.lines) {
        for (const std::string& w : tokens(line.item)) words.push_back(w);
    }
    static const std::set<std::string> kGpu{"rtx", "gtx", "gpu", "radeon", "rx", "grafica",
                                            "graficas", "video", "vga", "geforce", "quadro"};
    static const std::set<std::string> kBoard{"placa", "placas", "motherboard", "mainboard",
                                              "mobo"};
    static const std::set<std::string> kLaptop{"laptop", "notebook", "macbook", "portatil",
                                               "ultrabook", "thinkpad", "ideapad", "vivobook",
                                               "zenbook", "pavilion", "inspiron", "latitude",
                                               "chromebook"};
    for (const std::string& w : words) if (kGpu.contains(w)) return RepairType::GPU;
    for (const std::string& w : words) if (kBoard.contains(w) || isChipset(w)) return RepairType::PlacaMadre;
    for (const std::string& w : words) if (kLaptop.contains(w)) return RepairType::Laptop;
    return RepairType::Otro;
}

// ---------------------------------------------------------------------- Plan

std::vector<QuotePlan> planQuotes(const std::vector<QuoteDoc>& docs, const QuoteContext& c) {
    auto decisionFor = [&c](const std::string& id) -> std::string {
        const auto it = c.decisions.find(id);
        return it == c.decisions.end() ? std::string() : it->second;
    };

    std::vector<QuotePlan> plans;
    for (const QuoteDoc& doc : docs) {
        QuotePlan p;
        p.docId = doc.id;
        p.number = doc.number;
        p.client = doc.client;
        const std::string decision = decisionFor(doc.id);
        if (decision == "ignorar") {
            plans.push_back(p);
            continue;
        }

        const Repair* existing = repairForRoot(c.repairs, doc.root());
        const Repair* adopted = existing == nullptr ? adoptableRepair(c, doc.client) : nullptr;
        const Repair* base = existing != nullptr ? existing : adopted;

        // Enlazado a mano con un ingreso que ya tenia su trabajo: la
        // reparacion es ese trabajo, no una nueva del mismo equipo.
        if (existing == nullptr && startsWith(decision, "enlace:")) {
            if (const Movement* linked = findMovement(c.movements, decision.substr(7))) {
                const auto owner = std::find_if(c.repairs.begin(), c.repairs.end(),
                                                [linked](const Repair& r) {
                                                    return !linked->jobId.empty() &&
                                                           r.jobId == linked->jobId &&
                                                           r.sourceRef.empty();
                                                });
                if (owner != c.repairs.end()) base = &*owner;
            }
        }
        const Id jobId = base != nullptr ? base->jobId : "cot-" + doc.root();

        // --- Cotizacion: solo la aceptada, y solo abre la reparacion ----------
        if (doc.kind == QuoteKind::Cotizacion) {
            // Si ya hay un informe hecho a partir de ella, la reparacion la
            // maneja el informe: dos documentos escribiendo la misma ficha la
            // cambiarian de ida y vuelta en cada lectura.
            const bool superseded = std::any_of(docs.begin(), docs.end(), [&doc](const QuoteDoc& d) {
                return d.kind == QuoteKind::Informe && d.originId == doc.id && d.status != "borrador";
            });
            if (doc.status == "aceptada" && !superseded) {
                p.decision = QuoteDecision::Importar;
                p.repair = desiredRepair(doc, base, jobId, RepairStatus::EnProceso);
                p.newJob = base == nullptr;
            }
            plans.push_back(p);
            continue;
        }

        // --- Informe ------------------------------------------------------------
        const std::string saldoId = "cot-" + doc.id + "-saldo";
        const std::string abonoId = "cot-" + doc.id + "-abono";
        const bool imported = findMovement(c.movements, saldoId) != nullptr ||
                              findMovement(c.movements, abonoId) != nullptr;

        if (doc.status == "borrador") {
            if (imported) {
                p.decision = QuoteDecision::Esperar;
                p.hold = QuoteHold::EnCorreccion;
            }
            plans.push_back(p);
            continue;
        }
        if (doc.status != "entregado" && doc.status != "pagado") {
            plans.push_back(p);
            continue;
        }
        if (doc.baseMinor <= 0) {
            p.decision = QuoteDecision::Esperar;
            p.hold = QuoteHold::SinMonto;
            plans.push_back(p);
            continue;
        }

        const bool decided = decision == "nuevo" || startsWith(decision, "enlace:");
        if (!imported && !decided) {
            // Otro informe del mismo cliente y equipo, por lo mismo (un
            // centavo de tolerancia), con numero anterior: este es el repetido.
            const QuoteDoc* twin = nullptr;
            for (const QuoteDoc& other : docs) {
                if (other.id == doc.id || other.kind != QuoteKind::Informe ||
                    decisionFor(other.id) == "ignorar" || !(other.number < doc.number)) {
                    continue;
                }
                if (sameText(other.client, doc.client) && sameText(other.device, doc.device) &&
                    std::llabs(other.baseMinor - doc.baseMinor) <= 1) {
                    twin = &other;
                    break;
                }
            }
            if (twin != nullptr) {
                p.decision = QuoteDecision::Esperar;
                p.hold = QuoteHold::Duplicado;
                p.relatedNumber = twin->number;
                plans.push_back(p);
                continue;
            }
        }

        // El ingreso: el ya enlazado a la reparacion, el elegido a mano, o
        // uno nuevo. Si hay uno suelto que parece este, se espera la decision.
        const Movement* adoptedIncome = base != nullptr ? manualIncome(c.movements, base->jobId) : nullptr;
        if (startsWith(decision, "enlace:")) {
            adoptedIncome = findMovement(c.movements, decision.substr(7));
        }
        if (!imported && !decided && adoptedIncome == nullptr) {
            if (const Movement* candidate = candidateIncome(c, doc)) {
                p.decision = QuoteDecision::Esperar;
                p.hold = QuoteHold::YaAnotado;
                p.candidateMovementId = candidate->id;
                plans.push_back(p);
                continue;
            }
        }

        const bool paid = doc.status == "pagado";
        p.decision = QuoteDecision::Importar;
        p.repair = desiredRepair(doc, base, jobId,
                                 paid ? RepairStatus::Cobrada : RepairStatus::Entregada);
        p.newJob = base == nullptr;

        // Repuestos que nombra el informe y la ficha no tiene.
        std::set<std::string> known;
        for (const RepairPart& part : c.parts) {
            if (part.jobId == p.repair->jobId) known.insert(foldText(part.name));
        }
        for (const QuoteLine& line : doc.lines) {
            if (foldText(line.section).find("repuesto") == std::string::npos) continue;
            const std::string key = foldText(line.item);
            if (!key.empty() && known.insert(key).second) p.newParts.push_back(line.item);
        }

        const std::string name = doc.number + (doc.device.empty() ? "" : " · " + doc.device);
        auto startFrom = [&](const std::string& id, Date date) {
            if (const Movement* existingMovement = findMovement(c.movements, id)) {
                return *existingMovement;
            }
            Movement m;
            m.id = id;
            m.kind = MovementKind::Ingreso;
            m.pocketId = c.pocketId;
            m.category = c.category;
            m.name = name;
            m.date = date;
            return m;
        };
        const Date deliveredDate = doc.delivered.value_or(doc.issued.value_or(Date{}));

        if (adoptedIncome != nullptr) {
            Movement m = *adoptedIncome;
            m.jobId = p.repair->jobId;
            m.amountMinor = doc.baseMinor;
            if (paid) {
                m.settled = true;
                if (!m.settledDate) m.settledDate = doc.paid;
            }
            p.incomes.push_back(m);
        } else {
            if (doc.balanceMinor() > 0) {
                Movement saldo = startFrom(saldoId, deliveredDate);
                saldo.jobId = p.repair->jobId;
                saldo.amountMinor = doc.balanceMinor();
                saldo.date = deliveredDate;
                saldo.settled = paid;
                saldo.settledDate = paid ? doc.paid : std::nullopt;
                p.incomes.push_back(saldo);
            }
            if (doc.depositMinor > 0) {
                Movement abono = startFrom(abonoId, doc.received.value_or(deliveredDate));
                abono.jobId = p.repair->jobId;
                abono.amountMinor = doc.depositMinor;
                abono.settled = true;
                p.incomes.push_back(abono);
            }
        }
        plans.push_back(p);
    }
    return plans;
}

} // namespace dake::core
