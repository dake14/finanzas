// tests/negociotest.cpp — las preguntas del negocio: cuentas, sueldo,
// reparaciones, fijos, captura. Sin base de datos ni ventana.
//
// Mismo estilo que selftest.cpp: sin framework, una linea por comprobacion, y
// devuelve 0 si todo pasa.

#include <cstdio>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
#include <string>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/bankcsv.hpp"
#include "dake/core/capture.hpp"
#include "dake/core/fixed.hpp"
#include "dake/core/quotes.hpp"
#include "dake/core/repairs.hpp"
#include "dake/core/salary.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what) {
    std::printf("%s  %s\n", condition ? "ok  " : "FALLA", what.c_str());
    if (!condition) {
        ++gFailures;
    }
}

void checkMinor(std::int64_t got, std::int64_t want, const std::string& what) {
    if (got == want) {
        std::printf("ok    %s (%lld)\n", what.c_str(), static_cast<long long>(got));
    } else {
        std::printf("FALLA %s: esperaba %lld y salio %lld\n", what.c_str(),
                    static_cast<long long>(want), static_cast<long long>(got));
        ++gFailures;
    }
}

void checkText(const std::string& got, const std::string& want, const std::string& what) {
    if (got == want) {
        std::printf("ok    %s (%s)\n", what.c_str(), got.c_str());
    } else {
        std::printf("FALLA %s: esperaba '%s' y salio '%s'\n", what.c_str(), want.c_str(),
                    got.c_str());
        ++gFailures;
    }
}

using namespace dake::core;

const Currency kUsd = Currency::usd();

[[nodiscard]] Pocket pocket(const char* id, PocketKind kind) {
    Pocket p;
    p.id = id;
    p.name = id;
    p.kind = kind;
    return p;
}

/// Caja y ahorro del negocio, efectivo personal y un ahorro personal marcado
/// a mano (tipo Ahorro, cuenta Personal).
[[nodiscard]] std::vector<Pocket> bolsillos() {
    Pocket ahorroMio = pocket("ahorro-mio", PocketKind::Ahorro);
    ahorroMio.accountOverride = Account::Personal;
    return {pocket("caja", PocketKind::Operacion), pocket("ahorro", PocketKind::Ahorro),
            pocket("mio", PocketKind::Personal), ahorroMio};
}

[[nodiscard]] Movement gasto(const char* date, std::int64_t minor, const char* pocketId,
                             const char* category) {
    Movement m;
    m.id = std::string("g-") + date + category;
    m.date = Date::fromIso(date);
    m.name = category;
    m.kind = MovementKind::Gasto;
    m.amountMinor = minor;
    m.pocketId = pocketId;
    m.category = category;
    return m;
}

[[nodiscard]] Movement traspaso(const char* date, std::int64_t minor, const char* from,
                                const char* to) {
    Movement m;
    m.id = std::string("t-") + date + from + to;
    m.date = Date::fromIso(date);
    m.name = "traspaso";
    m.kind = MovementKind::Traspaso;
    m.amountMinor = minor;
    m.pocketId = from;
    m.targetPocketId = to;
    return m;
}

// ------------------------------------------------------------------ Cuentas

void cuentaDeCadaBolsillo() {
    std::printf("\n[cuenta de cada bolsillo]\n");
    const auto ps = bolsillos();
    check(accountOf(ps[0]) == Account::Negocio, "la caja de operacion es del negocio");
    check(accountOf(ps[1]) == Account::Negocio, "el ahorro sin marcar es del negocio");
    check(accountOf(ps[2]) == Account::Personal, "un bolsillo de tipo Personal es personal");
    check(accountOf(ps[3]) == Account::Personal, "la marca a mano gana sobre el tipo");

    Pocket raro = pocket("raro", PocketKind::Personal);
    raro.accountOverride = Account::Negocio;
    check(accountOf(raro) == Account::Negocio, "tambien al reves: Personal marcado negocio");

    check(accountOf(gasto("2026-09-01", 100, "mio", "Comida"), ps) == Account::Personal,
          "un gasto hereda la cuenta de su bolsillo");
    check(accountOf(gasto("2026-09-01", 100, "no-existe", "x"), ps) == Account::Negocio,
          "bolsillo desconocido: negocio, para que se vea en los reportes");

    checkText(std::string(toString(Account::Personal)), "Personal", "la cuenta se guarda como texto");
    check(accountFromString("Negocio") == Account::Negocio, "y vuelve del texto");
}

void sueldoEsUnTraspaso() {
    std::printf("\n[el sueldo es un traspaso negocio -> personal]\n");
    const auto ps = bolsillos();
    check(isSalary(traspaso("2026-09-05", 500'00, "caja", "mio"), ps), "caja -> mio es sueldo");
    check(isSalary(traspaso("2026-09-05", 500'00, "ahorro", "ahorro-mio"), ps),
          "ahorro del negocio -> ahorro personal tambien");
    check(!isSalary(traspaso("2026-09-05", 500'00, "caja", "ahorro"), ps),
          "entre bolsillos del negocio no es sueldo");
    check(!isSalary(traspaso("2026-09-05", 500'00, "mio", "caja"), ps),
          "de personal a negocio no es sueldo: es poner plata");
    check(!isSalary(gasto("2026-09-05", 500'00, "caja", "Luz"), ps), "un gasto no es sueldo");
}

void gastoCruzado() {
    std::printf("\n[gasto personal pagado con plata del negocio]\n");
    const auto ps = bolsillos();
    const Movement almuerzo = gasto("2026-09-10", 25'00, "caja", "Almuerzo");

    const auto partes = splitCrossExpense(almuerzo, ps, Account::Personal, "mio");
    check(partes.size() == 2, "se parte en dos movimientos");
    if (partes.size() == 2) {
        check(partes[0].kind == MovementKind::Traspaso, "primero un traspaso");
        checkText(partes[0].pocketId, "caja", "que sale de la caja");
        checkText(partes[0].targetPocketId, "mio", "y entra al bolsillo personal");
        checkMinor(partes[0].amountMinor, 25'00, "por el mismo monto");
        check(isSalary(partes[0], ps), "y cuenta como sueldo");
        check(partes[0].date == almuerzo.date, "el mismo dia");
        check(partes[1].kind == MovementKind::Gasto, "despues el gasto");
        checkText(partes[1].pocketId, "mio", "ahora desde el bolsillo personal");
        checkText(partes[1].category, "Almuerzo", "con su categoria");
        checkMinor(partes[1].amountMinor, 25'00, "y su monto");
        check(partes[0].isWellFormed() && partes[1].isWellFormed(), "los dos bien formados");
    }

    check(splitCrossExpense(almuerzo, ps, Account::Negocio, "mio").size() == 1,
          "categoria del negocio: queda como esta");
    const Movement desdeMio = gasto("2026-09-10", 25'00, "mio", "Almuerzo");
    check(splitCrossExpense(desdeMio, ps, Account::Personal, "mio").size() == 1,
          "ya sale de un bolsillo personal: queda como esta");
    check(splitCrossExpense(almuerzo, ps, Account::Personal, "").size() == 1,
          "sin bolsillo personal no hay a donde traspasar: queda como esta");
}

void categorias() {
    std::printf("\n[categorias]\n");
    const std::vector<Category> cats{{"Luz", Account::Negocio, CategoryClass::Fija},
                                     {"Comida", Account::Personal, CategoryClass::General}};
    const Category* luz = findCategory(cats, "luz");
    check(luz != nullptr && luz->name == "Luz", "se encuentra sin distinguir mayusculas");
    check(findCategory(cats, "Agua") == nullptr, "una que no existe da nullptr");
    checkText(std::string(toString(CategoryClass::Activo)), "Activo", "la clase se guarda como texto");
    check(categoryClassFromString("Fija") == CategoryClass::Fija, "y vuelve del texto");
}

void categoriasDeducidas() {
    std::printf("\n[categorias que ya existen, con su cuenta deducida]\n");
    const auto ps = bolsillos();
    Movement cobro = gasto("2026-09-02", 100'00, "caja", "Reparaciones");
    cobro.kind = MovementKind::Ingreso;
    const std::vector<Movement> ms{
        gasto("2026-09-01", 10'00, "mio", "Almuerzo"),
        gasto("2026-09-02", 10'00, "mio", "almuerzo"),
        gasto("2026-09-03", 10'00, "caja", "Almuerzo"),
        gasto("2026-09-03", 40'00, "caja", "Luz"),
        gasto("2026-09-04", 5'00, "caja", ""),
        gasto("2026-09-05", 5'00, "caja", "Ya conocida"),
        cobro,
    };
    const std::vector<Category> conocidas{{"Ya conocida", Account::Personal}};

    const auto nuevas = inferCategories(ms, ps, conocidas);
    check(nuevas.size() == 3, "tres nuevas: Almuerzo, Luz y Reparaciones");
    const Category* almuerzo = findCategory(nuevas, "Almuerzo");
    check(almuerzo != nullptr && almuerzo->account == Account::Personal,
          "Almuerzo es personal: dos de tres veces salio de un bolsillo personal");
    check(almuerzo != nullptr && almuerzo->name == "Almuerzo",
          "y se queda con el primer nombre escrito, sin duplicar por mayusculas");
    const Category* luz = findCategory(nuevas, "Luz");
    check(luz != nullptr && luz->account == Account::Negocio, "Luz es del negocio");
    const Category* rep = findCategory(nuevas, "Reparaciones");
    check(rep != nullptr && rep->kind == MovementKind::Ingreso, "Reparaciones es de ingreso");
    check(findCategory(nuevas, "Ya conocida") == nullptr, "las ya conocidas no se repiten");
}

void gastoPorCategoria() {
    std::printf("\n[gastos por categoria, contra el mes anterior]\n");
    const auto ps = bolsillos();
    const std::vector<Movement> ms{
        gasto("2026-08-03", 40'00, "caja", "Luz"),
        gasto("2026-09-03", 52'00, "caja", "Luz"),
        gasto("2026-09-04", 30'00, "caja", "Internet"),
        gasto("2026-08-20", 30'00, "caja", "Internet"),
        gasto("2026-09-10", 10'00, "caja", "Repuestos"),
        gasto("2026-09-11", 12'00, "mio", "Comida"),
        gasto("2026-08-11", 20'00, "mio", "Comida"),
        traspaso("2026-09-15", 500'00, "caja", "mio"),
        gasto("2026-10-01", 99'00, "caja", "Luz"),
    };

    const auto negocio = spendingByCategory(ms, ps, Account::Negocio, Date{2026, 9, 18}, kUsd);
    checkMinor(negocio.totalCurrent.minor(), 92'00, "negocio en septiembre: 52 + 30 + 10");
    checkMinor(negocio.totalPrevious.minor(), 70'00, "negocio en agosto: 40 + 30");
    check(negocio.rows.size() == 3, "tres categorias del negocio, sin el sueldo");
    if (negocio.rows.size() == 3) {
        checkText(negocio.rows[0].category, "Luz", "primero la que mas subio (+12)");
        checkText(negocio.rows[1].category, "Repuestos", "despues Repuestos (+10)");
        checkText(negocio.rows[2].category, "Internet", "Internet igual, al final");
        checkMinor(negocio.rows[0].change().minor(), 12'00, "Luz subio 12");
    }

    const auto personal = spendingByCategory(ms, ps, Account::Personal, Date{2026, 9, 1}, kUsd);
    checkMinor(personal.totalCurrent.minor(), 12'00, "personal en septiembre: solo la comida");
    checkMinor(personal.totalPrevious.minor(), 20'00, "personal en agosto");

    Movement sinCategoria = gasto("2026-09-12", 5'00, "caja", "");
    Movement borrado = gasto("2026-09-12", 7'00, "caja", "Luz");
    borrado.deleted = true;
    const auto otra =
        spendingByCategory({sinCategoria, borrado}, ps, Account::Negocio, Date{2026, 9, 1}, kUsd);
    check(otra.rows.size() == 1 && otra.rows[0].category == std::string(kUncategorized),
          "lo que no tiene categoria aparece como Sin categoria");
    checkMinor(otra.totalCurrent.minor(), 5'00, "y lo borrado no cuenta");
}


// ------------------------------------------------------------------ Captura

[[nodiscard]] Movement anotado(const char* id, const char* date, std::int64_t minor,
                               const char* pocketId, const char* name, const char* category,
                               MovementKind kind = MovementKind::Gasto) {
    Movement m;
    m.id = id;
    m.date = Date::fromIso(date);
    m.name = name;
    m.kind = kind;
    m.amountMinor = minor;
    m.pocketId = pocketId;
    m.category = category;
    return m;
}

/// Hoy es viernes 25 de septiembre de 2026. Hay dos bolsillos del negocio (el
/// banco se uso por ultima vez), uno personal, y tres reparaciones abiertas.
[[nodiscard]] CaptureContext contexto() {
    CaptureContext c;
    c.today = Date{2026, 9, 25};
    c.currency = kUsd;
    c.pockets = {pocket("caja", PocketKind::Operacion), pocket("banco", PocketKind::Operacion),
                 pocket("mio", PocketKind::Personal)};
    c.categories = {{"Almuerzo", Account::Personal},
                    {"Mercado", Account::Personal},
                    {"Comida", Account::Personal},
                    {"Luz", Account::Negocio, CategoryClass::Fija},
                    {"Repuestos", Account::Negocio},
                    {"Insumos", Account::Negocio, CategoryClass::Variable},
                    {"Consumibles", Account::Negocio, CategoryClass::Variable},
                    {"Reparaciones", Account::Negocio, CategoryClass::General,
                     MovementKind::Ingreso}};
    c.history = {
        anotado("01", "2026-09-01", 8'00, "mio", "Almuerzo", "Almuerzo"),
        anotado("02", "2026-09-02", 30'00, "mio", "super", "Mercado"),
        anotado("03", "2026-09-03", 30'00, "mio", "Super", "Mercado"),
        anotado("04", "2026-09-04", 30'00, "mio", "super!", "Comida"),
        anotado("05", "2026-09-05", 3'00, "caja", "Pasta termica", "Insumos"),
        anotado("06", "2026-09-06", 3'00, "caja", "pasta térmica", "Consumibles"),
        anotado("07", "2026-09-07", 3'00, "caja", "pasta termica 2", "Consumibles"),
        anotado("08", "2026-09-10", 80'00, "caja", "arreglo pc", "Reparaciones",
                MovementKind::Ingreso),
        anotado("09", "2026-09-24", 12'00, "banco", "cable", "Repuestos"),
    };
    c.openRepairs = {{"job-asus", "INF-2026-004", "asus x556U", "Josue Rodríguez"},
                     {"job-3080", "R-0042", "RTX 3080", "Juan Perez"},
                     {"job-3070", "R-0043", "RTX 3070", "Ana"}};
    return c;
}

void capturaDelMonto() {
    std::printf("\n[captura: el monto, donde sea que este]\n");
    const auto c = contexto();
    const auto a = parseCapture("25 almuerzo", c);
    check(a.amountMinor == 25'00, "'25 almuerzo': 25,00");
    check(parseCapture("almuerzo 25", c).amountMinor == 25'00, "'almuerzo 25': tambien al final");
    check(parseCapture("25,50 almuerzo", c).amountMinor == 25'50, "coma decimal");
    check(parseCapture("25.50 almuerzo", c).amountMinor == 25'50, "punto decimal");
    check(parseCapture("$25 almuerzo", c).amountMinor == 25'00, "con signo de pesos");
    check(parseCapture("1.200 alquiler", c).amountMinor == 1200'00, "1.200 son mil doscientos");
    check(parseCapture("cobro 120 gpu", c).amountMinor == 120'00, "en el medio, si no hay otro");
    check(!parseCapture("almuerzo", c).amountMinor, "sin numero no hay monto");
    check(!parseCapture("almuerzo", c).canSave(), "y sin monto no se puede guardar");
    check(!parseCapture("", c).canSave(), "una linea vacia tampoco");
    checkText(a.description, "almuerzo", "la descripcion es lo que queda");
}

void capturaDelTipoYLaFecha() {
    std::printf("\n[captura: tipo y fecha]\n");
    const auto c = contexto();
    check(parseCapture("25 almuerzo", c).kind == MovementKind::Gasto, "por defecto es gasto");
    check(parseCapture("+50 venta cable", c).kind == MovementKind::Ingreso, "'+' es ingreso");
    check(parseCapture("50 cobro limpieza", c).kind == MovementKind::Ingreso, "'cobro' es ingreso");
    check(parseCapture("50 cobré limpieza", c).kind == MovementKind::Ingreso, "'cobré' tambien");
    check(parseCapture("80 arreglo pc", c).kind == MovementKind::Ingreso,
          "una categoria de ingreso aprendida hace ingreso");

    const auto sueldo = parseCapture("500 sueldo", c);
    check(sueldo.salary, "'sueldo' es sueldo");
    check(sueldo.toMovement().kind == MovementKind::Traspaso, "y se guarda como traspaso");
    checkText(sueldo.pocketId, "banco", "desde el ultimo bolsillo del negocio usado");
    checkText(sueldo.targetPocketId, "mio", "al bolsillo personal");
    check(sueldo.toMovement().isWellFormed(), "bien formado");

    check(parseCapture("15 almuerzo", c).date == (Date{2026, 9, 25}), "sin fecha es hoy");
    const auto ayer = parseCapture("15 almuerzo ayer", c);
    check(ayer.date == (Date{2026, 9, 24}), "'ayer'");
    checkText(ayer.description, "almuerzo", "y 'ayer' no queda en la descripcion");
    check(parseCapture("15 almuerzo anteayer", c).date == (Date{2026, 9, 23}), "'anteayer'");
    check(parseCapture("15 almuerzo lun", c).date == (Date{2026, 9, 21}), "'lun': el lunes pasado");
    check(parseCapture("15 almuerzo vie", c).date == (Date{2026, 9, 25}), "'vie' un viernes es hoy");
    check(parseCapture("15 almuerzo sábado", c).date == (Date{2026, 9, 19}), "'sábado' con tilde");
    check(parseCapture("15 almuerzo 12/9", c).date == (Date{2026, 9, 12}), "'12/9' es dia/mes");
    check(parseCapture("15 regalo 28/12", c).date == (Date{2025, 12, 28}),
          "una fecha futura es del año pasado");
}

void capturaDeLaCategoria() {
    std::printf("\n[captura: categoria y bolsillo]\n");
    const auto c = contexto();
    const auto a = parseCapture("25 almuerzo", c);
    checkText(a.category, "Almuerzo", "'almuerzo' ya se anoto como Almuerzo");
    check(a.categorySource == CategorySource::Historial, "y lo dice el historial");
    checkText(a.pocketId, "mio", "Almuerzo es personal: sale del bolsillo personal");

    checkText(parseCapture("30 super", c).category, "Mercado", "'super': gana la mas usada (2 a 1)");
    checkText(parseCapture("3 pasta térmica", c).category, "Consumibles",
              "corregir es aprender: la correccion ya gana 2 a 1");

    const auto luz = parseCapture("40 luz de septiembre", c);
    checkText(luz.category, "Luz", "una palabra que es el nombre de una categoria");
    check(luz.categorySource == CategorySource::Nombre, "sale del nombre");
    checkText(luz.pocketId, "banco", "Luz es del negocio: el ultimo bolsillo del negocio usado");

    const auto rara = parseCapture("30 cosa rara", c);
    check(rara.category.empty() && rara.categorySource == CategorySource::Ninguna,
          "sin pistas no inventa categoria");
    check(rara.canSave() && rara.incomplete(), "pero se puede guardar, marcada por completar");
    checkText(rara.pocketId, "banco", "desde el ultimo bolsillo usado");
    checkText(rara.toMovement().category, "", "y el movimiento queda sin categoria");

    checkText(suggestedPocket(c, Account::Personal), "mio", "cambiar a personal propone 'mio'");
    checkText(suggestedPocket(c, Account::Negocio), "banco", "a negocio, el ultimo usado");
    checkText(suggestedPocket(c, std::nullopt), "banco", "sin cuenta, el ultimo de todos");
}

void capturaDeLaReparacion() {
    std::printf("\n[captura: la reparacion dentro de la frase]\n");
    const auto c = contexto();
    const auto cobro = parseCapture("120 cobro GPU 3080", c);
    checkText(cobro.jobId, "job-3080", "'3080' es la RTX 3080 de Juan");
    checkText(cobro.category, "Reparaciones", "un cobro de reparacion va a Reparaciones");
    check(cobro.categorySource == CategorySource::Reparacion, "por la reparacion");
    checkText(cobro.pocketId, "banco", "a un bolsillo del negocio");
    checkText(cobro.description, "cobro GPU 3080", "la descripcion conserva lo escrito");

    checkText(parseCapture("12 pasta 3070", c).jobId, "job-3070", "un gasto para la 3070");
    checkText(parseCapture("12 pasta 3070", c).category, "Repuestos",
              "un gasto de reparacion sin historial va a Repuestos");
    checkText(parseCapture("200 cobro INF-2026-004", c).jobId, "job-asus", "por numero de orden");
    checkText(parseCapture("10 cable #43", c).jobId, "job-3070", "'#43' es la R-0043");
    checkText(parseCapture("30 cobro josue", c).jobId, "job-asus", "por el nombre del cliente");
    checkText(parseCapture("10 rtx", c).jobId, "", "'rtx' coincide con dos: no adivina");
    checkText(parseCapture("25 almuerzo", c).jobId, "", "un almuerzo no es de ninguna");
}

void aprenderCategorias() {
    std::printf("\n[aprender categorias]\n");
    checkText(normalizeDescription("  Almuerzo 2x con Pepé! "), "almuerzo x con pepe",
              "normalizar: minusculas, sin tildes, sin numeros ni signos");
    const auto c = contexto();
    checkText(learnedCategory("SUPER", c.history), "Mercado", "aprendida sin distinguir mayusculas");
    checkText(learnedCategory("nunca visto", c.history), "", "lo nunca visto no tiene categoria");

    const auto gastos = categoriesByUse(c.categories, c.history, MovementKind::Gasto, c.today);
    check(gastos.size() == 7, "las siete de gasto");
    if (gastos.size() == 7) {
        checkText(gastos[0], "Consumibles", "primero la mas usada (2, empata con Mercado)");
        checkText(gastos[1], "Mercado", "despues Mercado");
        checkText(gastos.back(), "Luz", "al final las que no se usaron, por nombre");
    }
    const auto ingresos = categoriesByUse(c.categories, c.history, MovementKind::Ingreso, c.today);
    check(ingresos.size() == 1 && ingresos[0] == "Reparaciones", "y la de ingreso aparte");
}


// ------------------------------------------------------------- Reparaciones

/// Tarifa 15 por hora, objetivo 30%, fijos a 4 por hora.
[[nodiscard]] CostSettings ajustes() {
    return CostSettings{15'00, 3000, 4'00};
}

[[nodiscard]] Repair reparacion(const char* jobId, RepairType type, std::int64_t price,
                                int est, std::optional<int> real,
                                RepairStatus status = RepairStatus::Entregada,
                                const char* delivered = "2026-09-15") {
    Repair r;
    r.jobId = jobId;
    r.orderNo = std::string("R-") + jobId;
    r.device = jobId;
    r.type = type;
    r.priceMinor = price;
    r.estMinutes = est;
    r.realMinutes = real;
    r.status = status;
    r.received = Date{2026, 9, 1};
    if (status != RepairStatus::EnProceso) {
        r.delivered = Date::fromIso(delivered);
    }
    return r;
}

[[nodiscard]] RepairPart repuesto(const char* jobId, const char* name, std::int64_t cost,
                                  bool known = true, const char* movementId = "") {
    RepairPart p;
    p.id = std::string("p-") + jobId + name;
    p.jobId = jobId;
    p.name = name;
    p.costMinor = cost;
    p.costKnown = known;
    p.movementId = movementId;
    return p;
}

[[nodiscard]] Movement enlazado(const char* id, MovementKind kind, std::int64_t minor,
                                const char* jobId, bool settled = true,
                                const char* date = "2026-09-15") {
    Movement m;
    m.id = id;
    m.date = Date::fromIso(date);
    m.name = id;
    m.kind = kind;
    m.amountMinor = minor;
    m.pocketId = "caja";
    m.jobId = jobId;
    m.settled = settled;
    return m;
}

void costoDeUnaReparacion() {
    std::printf("\n[costo de una reparacion]\n");
    Repair a = reparacion("A", RepairType::GPU, 90'00, 120, 150);
    a.consumablesMinor = 2'00;
    a.shippingMinor = 3'00;
    const std::vector<RepairPart> partes{repuesto("A", "pasta", 5'00),
                                         repuesto("A", "pads", 8'00, true, "m-pads"),
                                         repuesto("A", "ventilador", 0, false)};
    const std::vector<Movement> ms{enlazado("m-pads", MovementKind::Gasto, 8'00, "A"),
                                   enlazado("m-tornillo", MovementKind::Gasto, 1'50, "A"),
                                   enlazado("m-otro", MovementKind::Gasto, 99'00, "B")};

    checkMinor(looseExpenses(a, partes, ms, kUsd).minor(), 1'50,
               "el tornillo anotado sin ficha cuenta como repuesto; los pads, no dos veces");
    const RepairCosting c = costRepair(a, partes, ms, ajustes(), kUsd);
    checkMinor(c.price.minor(), 90'00, "sin ingreso anotado, el precio es el de la ficha");
    checkMinor(c.parts.minor(), 14'50, "repuestos: 5 + 8 + 1,50");
    check(c.partsIncomplete, "hay un repuesto sin costo: se avisa");
    checkMinor(c.direct.minor(), 19'50, "directo: repuestos + consumibles 2 + envio 3");
    checkMinor(c.labor.minor(), 37'50, "horas: 2,5 h reales x 15");
    checkMinor(c.fixedShare.minor(), 10'00, "fijos: 2,5 h x 4");
    checkMinor(c.cost.minor(), 67'00, "costo total");
    checkMinor(c.profit.minor(), 23'00, "ganancia: 90 - 67");
    check(c.marginBps == 2556, "margen 25,56%");
    check(c.profitPerHour && c.profitPerHour->minor() == 28'20,
          "ganancia por hora: (90 - 19,50) / 2,5 h = 28,20");
    check(c.suggestedPrice && c.suggestedPrice->minor() == 95'71,
          "precio sugerido: 67 / (1 - 30%) = 95,71");
    check(c.belowTarget(3000), "queda bajo el objetivo");
    check(!c.hoursEstimated && c.minutes == 150, "con las horas reales");

    std::vector<Movement> conCobro = ms;
    conCobro.push_back(enlazado("m-cobro", MovementKind::Ingreso, 95'00, "A", false));
    checkMinor(repairPrice(a, conCobro, kUsd).minor(), 95'00,
               "con un ingreso anotado, el precio es lo que entro (cobrado o no)");

    const Repair sinHoras = reparacion("B", RepairType::GPU, 50'00, 60, std::nullopt);
    const RepairCosting b = costRepair(sinHoras, {}, {}, ajustes(), kUsd);
    check(b.hoursEstimated && b.minutes == 60, "sin horas reales usa las estimadas, y lo dice");
    checkMinor(b.labor.minor(), 15'00, "1 h estimada x 15");

    const Repair gratis = reparacion("C", RepairType::Otro, 0, 30, 30);
    const RepairCosting g = costRepair(gratis, {}, {}, ajustes(), kUsd);
    check(!g.marginBps, "precio 0 (garantia): sin margen, en vez de dividir por cero");
    checkMinor(g.profit.minor(), -9'50, "y la perdida se ve: 7,50 de horas + 2 de fijos");

    const Repair sinTiempo = reparacion("D", RepairType::Otro, 10'00, 0, 0);
    check(!costRepair(sinTiempo, {}, {}, ajustes(), kUsd).profitPerHour,
          "sin horas no hay ganancia por hora");
    checkMinor(hourlyNeeded(ajustes(), kUsd).minor(), 19'00,
               "cada hora tiene que dejar 19: tarifa 15 + fijos 4");
}

void rentabilidadPorTipo() {
    std::printf("\n[rentabilidad por tipo]\n");
    std::vector<Repair> rs{
        reparacion("G1", RepairType::GPU, 100'00, 100, 120),
        reparacion("G2", RepairType::GPU, 80'00, 120, 180),
        reparacion("G3", RepairType::GPU, 60'00, 60, 60),
        reparacion("G4", RepairType::GPU, 500'00, 60, 60, RepairStatus::EnProceso),
        reparacion("G5", RepairType::GPU, 500'00, 60, 60, RepairStatus::Cobrada, "2026-08-20"),
        reparacion("L1", RepairType::Laptop, 70'00, 60, 60, RepairStatus::Cobrada),
        reparacion("P1", RepairType::PlacaMadre, 50'00, 120, 120),
        reparacion("P2", RepairType::PlacaMadre, 50'00, 120, 120),
        reparacion("P3", RepairType::PlacaMadre, 50'00, 120, 120),
    };
    const std::vector<RepairPart> partes{
        repuesto("G1", "chip", 20'00), repuesto("G2", "chip", 10'00), repuesto("G3", "chip", 30'00),
        repuesto("P1", "x", 10'00),    repuesto("P2", "x", 10'00),    repuesto("P3", "x", 10'00)};

    const auto stats = statsByType(rs, partes, {}, ajustes(), kUsd, Date{2026, 9, 1},
                                   Date{2026, 9, 30});
    check(stats.size() == 3, "tres tipos con entregas en septiembre: GPU, Laptop, Placa");
    if (stats.size() != 3) return;

    const TypeStats& gpu = stats[0];
    check(gpu.type == RepairType::GPU, "primero GPU");
    check(gpu.count == 3, "tres GPU: la que sigue en proceso y la de agosto no cuentan");
    checkMinor(gpu.revenue.minor(), 240'00, "facturado 240");
    checkMinor(gpu.profit.minor(), 66'00, "ganancia 42 + 13 + 11");
    check(gpu.marginBps == 2750, "margen ponderado 27,5%, no el promedio de los margenes");
    check(gpu.profitPerHour && gpu.profitPerHour->minor() == 30'00, "deja 30 por hora: 180 en 6 h");
    check(gpu.hoursRatioPermille == 1286, "tarda 1,29 veces lo estimado: 360 / 280 minutos");
    checkMinor(gpu.averageCost.minor(), 58'00, "costo promedio 58");
    checkMinor(gpu.averagePrice.minor(), 80'00, "precio promedio 80");
    check(gpu.suggestedPrice && gpu.suggestedPrice->minor() == 82'86, "sugerido 58 / 0,7");
    check(gpu.verdict == Verdict::Cerca, "2,5 puntos bajo el objetivo: cerca");

    check(stats[1].type == RepairType::Laptop && stats[1].verdict == Verdict::PocosDatos,
          "una sola laptop: pocos datos, sin veredicto");
    check(stats[2].type == RepairType::PlacaMadre && stats[2].verdict == Verdict::Bajo,
          "placas al 4%: bajo");
    check(stats[2].marginBps == 400, "margen de las placas 4%");
}

void altaDesdePlantilla() {
    std::printf("\n[alta desde plantilla]\n");
    std::vector<Repair> rs{reparacion("x", RepairType::GPU, 0, 0, 0),
                           reparacion("y", RepairType::GPU, 0, 0, 0),
                           reparacion("z", RepairType::GPU, 0, 0, 0)};
    rs[0].orderNo = "R-0041";
    rs[1].orderNo = "INF-2026-004";
    rs[2].orderNo = "R-0007";
    checkText(nextOrderNumber(rs), "R-0042", "el correlativo sigue al mayor R-");
    checkText(nextOrderNumber({}), "R-0001", "y arranca en R-0001");

    RepairTemplate tpl;
    tpl.id = "tpl-gpu";
    tpl.type = RepairType::GPU;
    tpl.name = "GPU: reballing";
    tpl.priceMinor = 120'00;
    tpl.estMinutes = 180;
    tpl.consumablesMinor = 6'00;
    tpl.shippingMinor = 0;
    tpl.parts = {{"Esferas BGA", 4'00}, {"Flux", 2'50}};

    const Repair r = repairFromTemplate(tpl, "job-9", "R-0042", "RTX 3080", Date{2026, 9, 25});
    check(r.jobId == "job-9" && r.orderNo == "R-0042" && r.device == "RTX 3080",
          "con su trabajo, numero y equipo");
    check(r.type == RepairType::GPU && r.templateId == "tpl-gpu", "del tipo de la plantilla");
    check(r.status == RepairStatus::EnProceso && r.received == (Date{2026, 9, 25}),
          "en proceso, recibida hoy");
    check(r.priceMinor == 120'00 && r.estMinutes == 180 && r.consumablesMinor == 6'00,
          "precio, horas y consumibles precargados");
    check(!r.realMinutes, "las horas reales no se saben todavia");

    const auto ps = partsFromTemplate(tpl, "job-9");
    check(ps.size() == 2 && ps[0].name == "Esferas BGA" && ps[0].costMinor == 4'00 &&
              ps[0].jobId == "job-9" && ps[0].costKnown,
          "los repuestos tipicos, con costo");

    checkText(std::string(toString(RepairType::PlacaMadre)), "PlacaMadre", "el tipo como texto");
    check(repairTypeFromString("GPU") == RepairType::GPU, "y de vuelta");
    checkText(std::string(toString(RepairStatus::Cobrada)), "Cobrada", "el estado como texto");
    check(repairStatusFromString("Entregada") == RepairStatus::Entregada, "y de vuelta");
    check(allRepairTypes().size() == 4, "cuatro tipos");
}

void entregarYCobrar() {
    std::printf("\n[entregar y cobrar]\n");
    const Repair a = reparacion("A", RepairType::GPU, 80'00, 120, std::nullopt,
                                RepairStatus::EnProceso);

    const auto entregada = deliverRepair(a, {}, Date{2026, 9, 20}, 150, 90'00, false, "caja",
                                         "Reparaciones");
    check(entregada.repair.status == RepairStatus::Entregada, "entregar la deja entregada");
    check(entregada.repair.delivered == (Date{2026, 9, 20}), "con la fecha de entrega");
    check(entregada.repair.realMinutes == 150 && entregada.repair.priceMinor == 90'00,
          "con las horas reales y el precio final");
    check(entregada.income.has_value(), "y crea el ingreso");
    if (entregada.income) {
        const Movement& m = *entregada.income;
        check(m.kind == MovementKind::Ingreso && m.amountMinor == 90'00, "por el precio final");
        check(!m.settled && !m.settledDate, "por cobrar");
        check(m.jobId == "A" && m.pocketId == "caja" && m.category == "Reparaciones",
              "enlazado, en su bolsillo y categoria");
        check(m.date == (Date{2026, 9, 20}) && m.isWellFormed(), "con la fecha de entrega");
    }

    const auto deUna = deliverRepair(a, {}, Date{2026, 9, 20}, 150, 90'00, true, "caja",
                                     "Reparaciones");
    check(deUna.repair.status == RepairStatus::Cobrada, "entregar y cobrar: cobrada");
    check(deUna.income && deUna.income->settled && deUna.income->settledDate == (Date{2026, 9, 20}),
          "con el ingreso cobrado ese dia");

    const std::vector<Movement> yaAnotado{
        enlazado("ing-1", MovementKind::Ingreso, 80'00, "A", false, "2026-09-18")};
    const auto sinDuplicar = deliverRepair(a, yaAnotado, Date{2026, 9, 20}, 150, 90'00, false,
                                           "caja", "Reparaciones");
    check(sinDuplicar.income && sinDuplicar.income->id == "ing-1",
          "si ya habia un ingreso, se ajusta ese: nunca dos");
    check(sinDuplicar.income && sinDuplicar.income->amountMinor == 90'00, "al precio final");

    const auto cobrada = chargeRepair(entregada.repair, yaAnotado, Date{2026, 9, 25}, "caja",
                                      "Reparaciones");
    check(cobrada.repair.status == RepairStatus::Cobrada, "cobrar la deja cobrada");
    check(cobrada.income && cobrada.income->id == "ing-1" && cobrada.income->settled &&
              cobrada.income->settledDate == (Date{2026, 9, 25}),
          "y marca cobrado el ingreso que habia, con la fecha");

    const auto sinIngreso = chargeRepair(a, {}, Date{2026, 9, 25}, "caja", "Reparaciones");
    check(sinIngreso.income && sinIngreso.income->settled &&
              sinIngreso.income->amountMinor == 80'00,
          "sin ingreso previo, lo crea cobrado por el precio de la ficha");

    const auto porCobro = reconcileRepair(
        a, {enlazado("c", MovementKind::Ingreso, 80'00, "A", true, "2026-09-22")});
    check(porCobro.status == RepairStatus::Cobrada && porCobro.delivered == (Date{2026, 9, 22}),
          "anotar el cobro la deja cobrada y entregada ese dia");
    const auto porFactura = reconcileRepair(
        a, {enlazado("c", MovementKind::Ingreso, 80'00, "A", false, "2026-09-22")});
    check(porFactura.status == RepairStatus::Entregada, "un ingreso por cobrar la deja entregada");
    check(reconcileRepair(a, {}).status == RepairStatus::EnProceso, "sin ingreso, sigue igual");
    check(repairIncome("A", yaAnotado).has_value() && !repairIncome("B", yaAnotado),
          "el ingreso de una reparacion se encuentra por su trabajo");
}


// ------------------------------------------------------------- Cotizaciones

[[nodiscard]] QuoteDoc documento(const char* id, const char* number, QuoteKind kind,
                                 const char* status, const char* client, const char* device,
                                 std::int64_t base, const char* delivered = "2026-09-19") {
    QuoteDoc d;
    d.id = id;
    d.number = number;
    d.kind = kind;
    d.status = status;
    d.client = client;
    d.device = device;
    d.baseMinor = base;
    d.issued = Date::fromIso(delivered);
    d.received = Date{2026, 7, 17};
    if (kind == QuoteKind::Informe) d.delivered = Date::fromIso(delivered);
    return d;
}

[[nodiscard]] QuoteContext contextoCot() {
    QuoteContext c;
    c.pocketId = "caja";
    c.category = "Reparaciones";
    return c;
}

[[nodiscard]] const QuotePlan* planDe(const std::vector<QuotePlan>& plans, const char* docId) {
    for (const QuotePlan& p : plans) {
        if (p.docId == docId) return &p;
    }
    return nullptr;
}

void totalesDeCotizaciones() {
    std::printf("\n[cotizaciones: los totales con las mismas reglas]\n");
    checkMinor(quoteLineTotal(1, 30'00), 30'00, "una linea: cantidad x valor");
    checkMinor(quoteLineTotal(0.5, 3'33), 1'67, "166,5 centavos redondea lejos del cero");
    checkMinor(quoteLineTotal(2, 12'50), 25'00, "dos unidades");
    checkMinor(quoteBase(35'00, DiscountKind::Porcentaje, 7144), 10'00,
               "35 con 71,44% de descuento: 10 (el caso real del INF-2026-004)");
    checkMinor(quoteBase(35'00, DiscountKind::Monto, 2'00), 33'00, "descuento por monto");
    checkMinor(quoteBase(35'00, DiscountKind::Ninguno, 0), 35'00, "sin descuento");
}

void cotizacionAceptada() {
    std::printf("\n[cotizaciones: la cotizacion aceptada abre la reparacion]\n");
    const auto cot = documento("c1", "COT-2026-001", QuoteKind::Cotizacion, "aceptada",
                               "Josue Rodríguez", "asus x556U", 10'00);
    const auto plans = planQuotes({cot}, contextoCot());
    const QuotePlan* p = planDe(plans, "c1");
    check(p != nullptr && p->decision == QuoteDecision::Importar, "se importa");
    if (p == nullptr || !p->repair) return;
    check(p->newJob && p->repair->jobId == "cot-c1", "con un trabajo nuevo de id derivado");
    check(p->repair->status == RepairStatus::EnProceso, "en proceso");
    check(p->repair->received == (Date{2026, 7, 17}), "recibida cuando entro el equipo");
    check(p->repair->device == "asus x556U" && p->client == "Josue Rodríguez", "equipo y cliente");
    check(p->repair->sourceRef == "cot:c1", "marcada como venida de Cotizaciones");
    check(p->incomes.empty(), "y sin ingreso: todavia no hay nada que cobrar");

    // Cuando ya existe su informe, la cotizacion no escribe nada: la
    // reparacion es una sola y la maneja el informe. Si las dos la
    // escribieran, cada lectura de la carpeta la cambiaria de ida y vuelta.
    auto hijo = documento("i9", "INF-2026-009", QuoteKind::Informe, "entregado",
                          "Josue Rodríguez", "asus x556U", 10'00);
    hijo.originId = "c1";
    const auto conInforme = planQuotes({cot, hijo}, contextoCot());
    check(planDe(conInforme, "c1") && planDe(conInforme, "c1")->decision == QuoteDecision::Nada,
          "con su informe ya hecho, la cotizacion no hace nada");
    check(planDe(conInforme, "i9") && planDe(conInforme, "i9")->decision == QuoteDecision::Importar,
          "y el informe se importa");
    hijo.status = "borrador";
    const auto conBorrador = planQuotes({cot, hijo}, contextoCot());
    check(planDe(conBorrador, "c1") && planDe(conBorrador, "c1")->decision == QuoteDecision::Importar,
          "un informe en borrador todavia no la reemplaza");

    for (const char* status : {"borrador", "enviada", "rechazada"}) {
        auto d = cot;
        d.status = status;
        const auto ps = planQuotes({d}, contextoCot());
        check(ps.size() == 1 && ps[0].decision == QuoteDecision::Nada,
              std::string("una cotizacion ") + status + " no hace nada");
    }
}

void informeEntregadoYPagado() {
    std::printf("\n[cotizaciones: informe entregado, despues pagado]\n");
    QuoteContext c = contextoCot();
    Repair abierta;
    abierta.jobId = "cot-c1";
    abierta.orderNo = "COT-2026-001";
    abierta.device = "asus x556U";
    abierta.sourceRef = "cot:c1";
    abierta.type = RepairType::Laptop;
    abierta.estMinutes = 90;
    c.repairs = {abierta};

    auto inf = documento("i4", "INF-2026-004", QuoteKind::Informe, "entregado",
                         "Josue Rodríguez", "asus x556U", 10'00);
    inf.originId = "c1";
    inf.lines = {{"Mano de obra", "Diagnostico y Reparación", 30'00},
                 {"Repuestos y materiales", "Insumos", 5'00}};

    auto plans = planQuotes({inf}, c);
    const QuotePlan* p = planDe(plans, "i4");
    check(p != nullptr && p->decision == QuoteDecision::Importar, "el informe entregado se importa");
    if (p == nullptr || !p->repair) return;
    check(!p->newJob && p->repair->jobId == "cot-c1",
          "sobre la reparacion que abrio su cotizacion, no una nueva");
    check(p->repair->status == RepairStatus::Entregada, "entregada");
    check(p->repair->delivered == (Date{2026, 9, 19}), "con la fecha de entrega del informe");
    check(p->repair->priceMinor == 10'00 && p->repair->orderNo == "INF-2026-004",
          "con el precio y el numero del informe");
    check(p->repair->type == RepairType::Laptop && p->repair->estMinutes == 90,
          "sin tocar lo que manda Finanzas: tipo y horas");
    check(p->newParts.size() == 1 && p->newParts[0] == "Insumos",
          "los repuestos del informe se precargan, sin costo");
    check(p->incomes.size() == 1, "un ingreso");
    if (p->incomes.size() == 1) {
        const Movement& m = p->incomes[0];
        check(m.id == "cot-i4-saldo", "con id derivado del informe");
        check(m.amountMinor == 10'00 && !m.settled, "por cobrar, por el total");
        check(m.jobId == "cot-c1" && m.pocketId == "caja" && m.category == "Reparaciones",
              "enlazado, en el bolsillo de cobros y en Reparaciones");
        check(m.date == (Date{2026, 9, 19}) && m.kind == MovementKind::Ingreso && m.isWellFormed(),
              "fechado el dia de la entrega");
    }

    inf.status = "pagado";
    inf.paid = Date{2026, 9, 22};
    inf.depositMinor = 4'00;
    plans = planQuotes({inf}, c);
    p = planDe(plans, "i4");
    check(p != nullptr && p->repair && p->repair->status == RepairStatus::Cobrada, "pagado: cobrada");
    check(p != nullptr && p->incomes.size() == 2, "saldo y abono por separado");
    if (p != nullptr && p->incomes.size() == 2) {
        const Movement& saldo = p->incomes[0].id == "cot-i4-saldo" ? p->incomes[0] : p->incomes[1];
        const Movement& abono = p->incomes[0].id == "cot-i4-abono" ? p->incomes[0] : p->incomes[1];
        check(saldo.amountMinor == 6'00 && saldo.settled && saldo.settledDate == (Date{2026, 9, 22}),
              "el saldo, cobrado el dia del evento Pagado");
        check(abono.id == "cot-i4-abono" && abono.amountMinor == 4'00 && abono.settled &&
                  !abono.settledDate,
              "el abono cobrado, sin fecha de cobro: Cotizaciones no la guarda");
    }

    // Volvio a borrador para corregirlo, ya importado: no se toca.
    Movement importado;
    importado.id = "cot-i4-saldo";
    importado.kind = MovementKind::Ingreso;
    importado.amountMinor = 10'00;
    importado.jobId = "cot-c1";
    c.movements = {importado};
    inf.status = "borrador";
    plans = planQuotes({inf}, c);
    p = planDe(plans, "i4");
    check(p != nullptr && p->decision == QuoteDecision::Esperar && p->hold == QuoteHold::EnCorreccion,
          "en correccion despues de importado: espera, no se borra nada");
    c.movements.clear();
    plans = planQuotes({inf}, c);
    check(planDe(plans, "i4") != nullptr && planDe(plans, "i4")->decision == QuoteDecision::Nada,
          "un borrador nunca importado no hace nada");
}

void duplicadosYAnotados() {
    std::printf("\n[cotizaciones: nunca dos veces]\n");
    const auto i1 = documento("i1", "INF-2026-001", QuoteKind::Informe, "pagado", "Sr. Galván",
                              "Macbook M1", 69'83, "2026-08-16");
    const auto i2 = documento("i2", "INF-2026-002", QuoteKind::Informe, "pagado", "Sr. Galván",
                              "Macbook M1", 69'82, "2026-08-16");

    auto plans = planQuotes({i1, i2}, contextoCot());
    check(planDe(plans, "i1") && planDe(plans, "i1")->decision == QuoteDecision::Importar,
          "el primero se importa");
    const QuotePlan* dup = planDe(plans, "i2");
    check(dup && dup->decision == QuoteDecision::Esperar && dup->hold == QuoteHold::Duplicado &&
              dup->relatedNumber == "INF-2026-001",
          "el segundo, mismo cliente, equipo y un centavo de diferencia: espera como duplicado");
    check(dup && dup->incomes.empty(), "y no trae ingresos");

    QuoteContext c = contextoCot();
    c.decisions = {{"i2", "ignorar"}};
    plans = planQuotes({i1, i2}, c);
    check(planDe(plans, "i2") && planDe(plans, "i2")->decision == QuoteDecision::Nada,
          "marcado para ignorar, se ignora");
    c.decisions = {{"i2", "nuevo"}};
    plans = planQuotes({i1, i2}, c);
    check(planDe(plans, "i2") && planDe(plans, "i2")->decision == QuoteDecision::Importar,
          "marcado como nuevo, se importa igual");

    // El Macbook de agosto ya estaba anotado a mano.
    Movement aMano;
    aMano.id = "m-mac";
    aMano.date = Date{2026, 8, 17};
    aMano.name = "Reparacion Macbook";
    aMano.kind = MovementKind::Ingreso;
    aMano.amountMinor = 69'83;
    aMano.pocketId = "caja";
    aMano.category = "Reparacion";
    c = contextoCot();
    c.movements = {aMano};
    plans = planQuotes({i1}, c);
    const QuotePlan* cand = planDe(plans, "i1");
    check(cand && cand->decision == QuoteDecision::Esperar && cand->hold == QuoteHold::YaAnotado &&
              cand->candidateMovementId == "m-mac",
          "un ingreso a mano del mismo monto y fecha cercana: espera, y lo propone");

    c.decisions = {{"i1", "enlace:m-mac"}};
    plans = planQuotes({i1}, c);
    const QuotePlan* linked = planDe(plans, "i1");
    check(linked && linked->decision == QuoteDecision::Importar, "enlazado, se importa");
    check(linked && linked->incomes.size() == 1 && linked->incomes[0].id == "m-mac",
          "usando el ingreso que ya estaba, no uno nuevo");
    check(linked && linked->incomes.size() == 1 && linked->incomes[0].jobId == "cot-i1" &&
              linked->incomes[0].settled && linked->incomes[0].category == "Reparacion",
          "ahora enlazado a la reparacion, cobrado, y con su categoria de siempre");

    aMano.amountMinor = 50'00;
    c = contextoCot();
    c.movements = {aMano};
    plans = planQuotes({i1}, c);
    check(planDe(plans, "i1") && planDe(plans, "i1")->decision == QuoteDecision::Importar,
          "un ingreso a mano de otro monto no se confunde");
}

void reparacionAMano() {
    std::printf("\n[cotizaciones: una reparacion abierta a mano del mismo cliente]\n");
    QuoteContext c = contextoCot();
    Job job;
    job.id = "job-m";
    job.name = "R-0001 · Control";
    job.client = "Christian Villalobos";
    Repair aMano;
    aMano.jobId = "job-m";
    aMano.orderNo = "R-0001";
    aMano.device = "Control";
    aMano.type = RepairType::Otro;
    c.jobs = {job};
    c.repairs = {aMano};
    Movement cobro;
    cobro.id = "m-cobro";
    cobro.date = Date{2026, 9, 18};
    cobro.name = "cobro control";
    cobro.kind = MovementKind::Ingreso;
    cobro.amountMinor = 30'00;
    cobro.pocketId = "banco";
    cobro.jobId = "job-m";
    cobro.settled = false;
    c.movements = {cobro};

    const auto inf = documento("i3", "INF-2026-003", QuoteKind::Informe, "entregado",
                               "christian villalobos", "Control PS5 2 unidades", 30'00);
    auto plans = planQuotes({inf}, c);
    const QuotePlan* p = planDe(plans, "i3");
    check(p && p->decision == QuoteDecision::Importar && p->repair && !p->newJob &&
              p->repair->jobId == "job-m",
          "se enlaza a la unica reparacion abierta de ese cliente");
    check(p && p->repair && p->repair->sourceRef == "cot:i3", "y queda marcada");
    check(p && p->incomes.size() == 1 && p->incomes[0].id == "m-cobro" &&
              p->incomes[0].pocketId == "banco",
          "su ingreso ya anotado se usa, con su bolsillo: nunca dos");

    Repair otra = aMano;
    otra.jobId = "job-n";
    Job job2 = job;
    job2.id = "job-n";
    c.jobs.push_back(job2);
    c.repairs.push_back(otra);
    c.movements.clear();
    plans = planQuotes({inf}, c);
    p = planDe(plans, "i3");
    check(p && p->newJob && p->repair && p->repair->jobId == "cot-i3",
          "con dos abiertas del mismo cliente no adivina: crea una nueva");

    auto cero = inf;
    cero.baseMinor = 0;
    plans = planQuotes({cero}, contextoCot());
    check(planDe(plans, "i3") && planDe(plans, "i3")->hold == QuoteHold::SinMonto,
          "un informe en cero espera en vez de crear un ingreso de cero");
}

void anotadoConTrabajo() {
    std::printf("\n[cotizaciones: el Macbook de agosto, anotado a mano con su trabajo]\n");
    // El caso real: "Trabajo Macbook Herman Galvan" por 120,00, cobrado y
    // enlazado a un trabajo cerrado. El informe dice "Sr. Galván", Macbook M1,
    // 119,83 (69,83 de saldo y 50 de abono).
    Job viejo;
    viejo.id = "job-mac";
    viejo.name = "Reparacion Macbook";
    viejo.client = "Herman Galvan";
    viejo.closed = true;
    Repair fichaVieja;
    fichaVieja.jobId = "job-mac";
    fichaVieja.device = "Reparacion Macbook";
    fichaVieja.status = RepairStatus::Cobrada;
    Movement aMano;
    aMano.id = "m-mac";
    aMano.date = Date{2026, 8, 17};
    aMano.name = "Trabajo Macbook Herman Galvan";
    aMano.kind = MovementKind::Ingreso;
    aMano.amountMinor = 120'00;
    aMano.pocketId = "caja";
    aMano.jobId = "job-mac";
    aMano.settled = true;

    QuoteContext c = contextoCot();
    c.jobs = {viejo};
    c.repairs = {fichaVieja};
    c.movements = {aMano};
    auto inf = documento("i1", "INF-2026-001", QuoteKind::Informe, "pagado", "Sr. Galván",
                         "Macbook M1", 119'83, "2026-08-16");
    inf.depositMinor = 50'00;
    inf.paid = Date{2026, 8, 16};

    auto plans = planQuotes({inf}, c);
    const QuotePlan* p = planDe(plans, "i1");
    check(p && p->decision == QuoteDecision::Esperar && p->hold == QuoteHold::YaAnotado &&
              p->candidateMovementId == "m-mac",
          "17 centavos de diferencia y con trabajo propio: igual se reconoce como ya anotado");

    c.decisions = {{"i1", "enlace:m-mac"}};
    plans = planQuotes({inf}, c);
    p = planDe(plans, "i1");
    check(p && p->decision == QuoteDecision::Importar && p->repair && !p->newJob &&
              p->repair->jobId == "job-mac",
          "enlazado: se usa su trabajo, no se crea otra reparacion del mismo Macbook");
    check(p && p->incomes.size() == 1 && p->incomes[0].id == "m-mac" &&
              p->incomes[0].amountMinor == 119'83,
          "un solo ingreso, el de siempre, corregido al total del informe");
    check(p && p->repair && p->repair->sourceRef == "cot:i1", "y la reparacion queda marcada");

    Movement otro = aMano;
    otro.id = "m-otro";
    otro.name = "Venta de filamento";
    otro.amountMinor = 110'00;
    otro.jobId.clear();
    c = contextoCot();
    c.movements = {otro};
    plans = planQuotes({inf}, c);
    check(planDe(plans, "i1") && planDe(plans, "i1")->decision == QuoteDecision::Importar,
          "8% de diferencia sin nombrar equipo ni cliente: no es el mismo");
}

void tipoPorElEquipo() {
    std::printf("\n[cotizaciones: el tipo sale del equipo]\n");
    auto d = documento("x", "INF-9", QuoteKind::Informe, "entregado", "a", "RTX 3080", 1);
    check(guessRepairType(d) == RepairType::GPU, "RTX 3080 es GPU");
    d.device = "2 placas madres asus rog B450-f";
    check(guessRepairType(d) == RepairType::PlacaMadre, "B450 es placa madre");
    d.device = "Macbook M1";
    check(guessRepairType(d) == RepairType::Laptop, "Macbook es laptop");
    d.device = "Control PS5";
    check(guessRepairType(d) == RepairType::Otro, "un control no se adivina");
    d.lines = {{"Mano de obra", "Reballing de GPU", 1}};
    check(guessRepairType(d) == RepairType::GPU, "los conceptos tambien cuentan");
}


// -------------------------------------------------------------- Fijos y bandeja

[[nodiscard]] Recurring recurrente(const char* id, const char* name, std::int64_t amount, int day,
                                   const char* starts, bool active = true) {
    Recurring r;
    r.id = id;
    r.name = name;
    r.category = name;
    r.pocketId = "caja";
    r.amountMinor = amount;
    r.dayOfMonth = day;
    r.starts = Date::fromIso(starts);
    r.active = active;
    return r;
}

void recurrentes() {
    std::printf("\n[recurrentes: cada mes, una sola vez]\n");
    checkText(periodOf(Date{2026, 9, 25}), "2026-09", "el periodo de un dia es su mes");

    const Recurring luz = recurrente("R1", "Luz", 40'00, 10, "2026-07-01");
    MovementMeta julio;
    julio.movementId = "rec-R1-2026-07";
    julio.recurringId = "R1";
    julio.period = "2026-07";
    const auto due = dueRecurring({luz}, {julio}, Date{2026, 9, 25});
    check(due.size() == 2, "julio ya estaba: faltan agosto y septiembre");
    if (due.size() == 2) {
        check(due[0].movement.id == "rec-R1-2026-08" && due[1].movement.id == "rec-R1-2026-09",
              "con id del recurrente y el mes: nunca dos veces");
        check(due[0].movement.date == (Date{2026, 8, 10}), "el dia 10 de cada mes");
        const Movement& m = due[1].movement;
        check(m.kind == MovementKind::Gasto && m.amountMinor == 40'00 && m.category == "Luz" &&
                  m.pocketId == "caja" && m.name == "Luz" && m.isWellFormed(),
              "un gasto por el estimado, en su categoria y bolsillo");
        check(due[1].meta.origin == "Recurrente" && due[1].meta.review == "confirmar" &&
                  due[1].meta.recurringId == "R1" && due[1].meta.period == "2026-09" &&
                  due[1].meta.movementId == m.id,
              "marcado por confirmar");
    }

    const Recurring finDeMes = recurrente("R2", "Alquiler", 300'00, 31, "2026-09-01");
    check(dueRecurring({finDeMes}, {}, Date{2026, 9, 25}).empty(), "el 30 todavia no llego");
    Recurring febrero = recurrente("R3", "Software", 10'00, 31, "2026-02-01");
    febrero.ends = Date{2026, 2, 28};
    const auto feb = dueRecurring({febrero}, {}, Date{2026, 3, 15});
    check(feb.size() == 1 && feb[0].movement.date == (Date{2026, 2, 28}),
          "el 31 en febrero es el 28, y despues del fin no genera");
    check(dueRecurring({recurrente("R4", "X", 1'00, 1, "2026-01-01", false)}, {}, Date{2026, 9, 25})
              .empty(),
          "uno inactivo no genera nada");
}

void herramientas() {
    std::printf("\n[herramientas: por depreciacion, no como gasto]\n");
    Tool osciloscopio;
    osciloscopio.id = "t1";
    osciloscopio.name = "Osciloscopio";
    osciloscopio.costMinor = 1000'00;
    osciloscopio.bought = Date{2026, 8, 15};
    osciloscopio.lifeMonths = 24;
    checkMinor(depreciationInMonth(osciloscopio, Date{2026, 8, 1}, kUsd).minor(), 41'67,
               "1000 en 24 meses: 41,67 el primero");
    checkMinor(depreciationInMonth(osciloscopio, Date{2026, 7, 31}, kUsd).minor(), 0,
               "antes de comprarla, nada");
    checkMinor(depreciationInMonth(osciloscopio, Date{2028, 8, 1}, kUsd).minor(), 0,
               "despues de su vida util, nada");
    std::int64_t total = 0;
    for (int i = 0; i < 30; ++i) {
        total += depreciationInMonth(osciloscopio, Date{2026, 8, 1}.addMonths(i), kUsd).minor();
    }
    checkMinor(total, 1000'00, "sumados todos los meses, exactamente el costo");

    Tool retirada = osciloscopio;
    retirada.retired = Date{2026, 12, 5};
    checkMinor(depreciationInMonth(retirada, Date{2026, 11, 1}, kUsd).minor(), 41'67,
               "hasta el mes antes de darla de baja");
    checkMinor(depreciationInMonth(retirada, Date{2026, 12, 1}, kUsd).minor(), 0,
               "dada de baja, deja de costar");
    Tool soldador = osciloscopio;
    soldador.costMinor = 120'00;
    soldador.lifeMonths = 12;
    checkMinor(depreciationInMonth({osciloscopio, soldador}, Date{2026, 9, 1}, kUsd).minor(),
               41'67 + 10'00, "varias herramientas se suman");
}

void tasaDeFijos() {
    std::printf("\n[tasa de fijos por hora]\n");
    const std::vector<Recurring> rs{recurrente("R1", "Luz", 40'00, 10, "2026-01-01"),
                                    recurrente("R2", "Internet", 30'00, 5, "2026-01-01"),
                                    recurrente("R3", "Viejo", 99'00, 5, "2026-01-01", false)};
    Tool osciloscopio;
    osciloscopio.costMinor = 1000'00;
    osciloscopio.bought = Date{2026, 8, 15};
    osciloscopio.lifeMonths = 24;

    auto entregada = [](const char* id, const char* date, int minutes) {
        Repair r;
        r.jobId = id;
        r.status = RepairStatus::Cobrada;
        r.delivered = Date::fromIso(date);
        r.realMinutes = minutes;
        return r;
    };
    const std::vector<Repair> reps{entregada("a", "2026-06-10", 1800), entregada("b", "2026-07-10", 1200),
                                   entregada("c", "2026-08-10", 600), entregada("d", "2026-09-10", 9999),
                                   entregada("e", "2026-05-10", 9999)};
    const FixedRate rate = fixedRate(rs, {osciloscopio}, reps, Date{2026, 9, 25}, 4800, kUsd);
    checkMinor(rate.monthlyFixed.minor(), 111'67, "fijos del mes: 40 + 30 + 41,67 (sin el inactivo)");
    check(rate.monthlyMinutes == 1200 && !rate.minutesFromSettings,
          "20 h por mes: junio, julio y agosto; septiembre sigue abierto y mayo quedo afuera");
    checkMinor(rate.perHour.minor(), 5'58, "5,58 por hora");

    const FixedRate nueva = fixedRate(rs, {osciloscopio}, {entregada("c", "2026-08-10", 600)},
                                      Date{2026, 9, 25}, 4800, kUsd);
    check(nueva.monthlyMinutes == 600, "con un solo mes de historia, ese mes: no se divide por tres");

    const FixedRate sinHistoria = fixedRate(rs, {}, {}, Date{2026, 9, 25}, 4800, kUsd);
    check(sinHistoria.minutesFromSettings && sinHistoria.monthlyMinutes == 4800,
          "sin reparaciones entregadas, las horas de Ajustes");
    checkMinor(sinHistoria.perHour.minor(), 88, "70 en 80 horas: 0,875 por hora, redondeado a 0,88");
}

void bandeja() {
    std::printf("\n[la bandeja de pendientes]\n");
    InboxInput in;
    in.today = Date{2026, 9, 25};
    auto mov = [](const char* id, const char* category, MovementKind kind = MovementKind::Gasto) {
        Movement m;
        m.id = id;
        m.date = Date{2026, 9, 20};
        m.name = id;
        m.kind = kind;
        m.amountMinor = 1'00;
        m.pocketId = "caja";
        m.category = category;
        if (kind == MovementKind::Traspaso) m.targetPocketId = "mio";
        return m;
    };
    Movement borrado = mov("borrado", "");
    borrado.deleted = true;
    in.movements = {mov("sin", ""), mov("conf", "Luz"), mov("sug", "Comida"), mov("vida", "Herramientas"),
                    mov("sueldo", "", MovementKind::Traspaso), borrado};
    in.metas = {{"conf", "Recurrente", "confirmar"}, {"sug", "Importado", "sugerido"},
                {"vida", "Manual", "vida"}};
    auto rep = [](const char* id, RepairStatus status, const char* received, const char* delivered,
                  std::optional<int> real) {
        Repair r;
        r.jobId = id;
        r.status = status;
        r.received = Date::fromIso(received);
        if (delivered[0] != 0) r.delivered = Date::fromIso(delivered);
        r.realMinutes = real;
        return r;
    };
    in.repairs = {rep("A", RepairStatus::Entregada, "2026-09-01", "2026-09-10", 60),
                  rep("B", RepairStatus::Entregada, "2026-09-01", "2026-09-20", 60),
                  rep("C", RepairStatus::EnProceso, "2026-09-01", "", std::nullopt),
                  rep("D", RepairStatus::Cobrada, "2026-09-01", "2026-09-15", std::nullopt)};
    RepairPart sinCosto;
    sinCosto.id = "p1";
    sinCosto.jobId = "D";
    sinCosto.costKnown = false;
    in.parts = {sinCosto};
    in.quoteHolds = 2;

    const auto items = inbox(in);
    std::vector<InboxKind> kinds;
    for (const auto& item : items) kinds.push_back(item.kind);
    const std::vector<InboxKind> want{InboxKind::SinCategoria, InboxKind::PorConfirmar,
                                      InboxKind::Sugerido,     InboxKind::VidaUtil,
                                      InboxKind::Cotizaciones, InboxKind::PorCobrar,
                                      InboxKind::SinEntregar,  InboxKind::SinHoras,
                                      InboxKind::CostoRepuesto};
    check(kinds == want, "nueve pendientes, en el orden en que conviene resolverlos");
    if (items.size() == 9) {
        check(items[0].refId == "sin", "sin categoria: el gasto, no el traspaso ni el borrado");
        check(items[5].refId == "A" && items[5].days == 15,
              "por cobrar: la entregada hace 15 dias, no la de hace 5");
        check(items[6].refId == "C" && items[6].days == 24, "en proceso hace 24 dias");
        check(items[7].refId == "D", "cobrada sin horas reales");
        check(items[8].refId == "p1", "un repuesto sin costo");
    }

    in.snoozed = {{"A", Date{2026, 9, 30}}};
    in.quoteHolds = 0;
    const auto later = inbox(in);
    bool hasA = false;
    bool hasQuotes = false;
    for (const auto& item : later) {
        hasA = hasA || item.refId == "A";
        hasQuotes = hasQuotes || item.kind == InboxKind::Cotizaciones;
    }
    check(!hasA, "lo pospuesto hasta el 30 no aparece el 25");
    check(!hasQuotes, "sin documentos esperando, no hay renglon de Cotizaciones");
}


// ------------------------------------------------------------------- Sueldo

struct CasoSueldo {
    std::vector<Pocket> pockets;
    std::vector<Category> categories;
    std::vector<Tool> tools;
    std::vector<Movement> movements;
};

[[nodiscard]] CasoSueldo casoSueldo() {
    CasoSueldo c;
    Pocket caja = pocket("caja", PocketKind::Operacion);
    caja.openingMinor = 100'00;
    c.pockets = {caja, pocket("mio", PocketKind::Personal)};
    c.categories = {{"Herramientas", Account::Negocio, CategoryClass::Activo},
                    {"Comida", Account::Personal}};
    Tool soldador;
    soldador.costMinor = 240'00;
    soldador.bought = Date{2026, 6, 1};
    soldador.lifeMonths = 24;
    c.tools = {soldador};

    auto ingreso = [](const char* date, std::int64_t minor, bool settled = true) {
        Movement m = gasto(date, minor, "caja", "Reparaciones");
        m.id = std::string("i-") + date;
        m.kind = MovementKind::Ingreso;
        m.settled = settled;
        return m;
    };
    Movement filamento = gasto("2026-06-15", 120'00, "caja", "Filamento");
    filamento.spreadMonths = 4;
    c.movements = {
        ingreso("2026-06-10", 500'00),
        ingreso("2026-06-20", 200'00, false),
        gasto("2026-06-05", 100'00, "caja", "Varios"),
        filamento,
        gasto("2026-06-01", 240'00, "caja", "Herramientas"),
        traspaso("2026-06-28", 300'00, "caja", "mio"),
        gasto("2026-06-12", 50'00, "mio", "Comida"),
        ingreso("2026-07-10", 300'00),
        traspaso("2026-07-28", 300'00, "caja", "mio"),
        gasto("2026-07-12", 80'00, "mio", "Comida"),
        gasto("2026-08-10", 40'00, "mio", "Comida"),
        ingreso("2026-09-10", 1000'00),
    };
    return c;
}

void utilidadNetaDelMes() {
    std::printf("\n[utilidad neta del mes]\n");
    const CasoSueldo c = casoSueldo();
    const MonthNet junio = businessNet(c.movements, c.pockets, c.categories, c.tools, Date{2026, 6, 1}, kUsd);
    checkMinor(junio.income.minor(), 700'00, "ingresos de junio: 500 cobrados + 200 por cobrar");
    checkMinor(junio.cost.minor(), 140'00,
               "costo: 100 + 30 del filamento repartido + 10 de depreciacion (la compra no)");
    checkMinor(junio.net.minor(), 560'00, "utilidad: el sueldo y el gasto personal no restan");
    const MonthNet agosto = businessNet(c.movements, c.pockets, c.categories, c.tools, Date{2026, 8, 1}, kUsd);
    checkMinor(agosto.net.minor(), -40'00, "agosto sin ingresos: filamento y depreciacion, -40");
}

void sueldoRecomendado() {
    std::printf("\n[sueldo recomendado]\n");
    const CasoSueldo c = casoSueldo();
    const ProfitSplit split;
    const SalaryAdvice a =
        salaryAdvice(c.movements, c.pockets, c.categories, c.tools, split, Date{2026, 9, 25}, kUsd);
    check(a.closedMonths == 3 && !a.provisional, "tres meses cerrados: junio, julio, agosto");
    check(a.average3 && a.average3->minor() == 260'00, "promedio de 3 meses: (560 + 260 - 40) / 3");
    checkMinor(a.base.minor(), 260'00, "base 260");
    checkMinor(a.salary.minor(), 143'00, "sueldo: 55%");
    checkMinor(a.taxes.minor() + a.reinvest.minor() + a.emergency.minor() + a.salary.minor(), 260'00,
               "el reparto suma la base entera");
    checkMinor(a.taxes.minor(), 39'00, "impuestos 15%");
    check(a.personalSpend && a.personalSpend->minor() == 56'67, "gasto personal: (50 + 80 + 40) / 3");
    checkMinor(a.paidAverage.minor(), 200'00, "te pagaste 200 por mes: (300 + 300 + 0) / 3");
    check(a.margin() && a.margin()->minor() == 86'33, "te sobran 86,33");
    check(a.months.size() == 3 && a.months[0].month == (Date{2026, 8, 1}),
          "los meses, del mas nuevo al mas viejo");

    CasoSueldo conMayo = c;
    Movement mayo = gasto("2026-05-10", 1200'00, "caja", "Reparaciones");
    mayo.kind = MovementKind::Ingreso;
    conMayo.movements.push_back(mayo);
    const SalaryAdvice b = salaryAdvice(conMayo.movements, conMayo.pockets, conMayo.categories,
                                        conMayo.tools, split, Date{2026, 9, 25}, kUsd);
    check(b.average6 && b.average6->minor() == 495'00, "promedio de 6 meses con lo que hay: 4 meses, 495");
    checkMinor(b.base.minor(), 260'00, "se usa el menor: 260 y no 495");

    const SalaryAdvice p =
        salaryAdvice(c.movements, c.pockets, c.categories, c.tools, split, Date{2026, 8, 15}, kUsd);
    check(p.provisional && p.closedMonths == 2 && p.average3 && p.average3->minor() == 410'00,
          "con dos meses cerrados: provisional, (560 + 260) / 2");

    const SalaryAdvice none =
        salaryAdvice(c.movements, c.pockets, c.categories, c.tools, split, Date{2026, 6, 20}, kUsd);
    check(none.closedMonths == 0 && !none.average3 && none.base.isZero(),
          "sin meses cerrados no hay promedio: no se inventa un sueldo");

    const std::vector<Movement> perdida{gasto("2026-08-10", 100'00, "caja", "Varios")};
    const SalaryAdvice neg =
        salaryAdvice(perdida, c.pockets, c.categories, {}, split, Date{2026, 9, 10}, kUsd);
    check(neg.base.isZero() && neg.salary.isZero(), "con perdida, el sueldo sostenible es cero");
    checkMinor(neg.shortfall.minor(), 100'00, "y dice cuanto falta");
}

void flujoDeCaja() {
    std::printf("\n[flujo de caja del negocio]\n");
    const CasoSueldo c = casoSueldo();
    const auto months = businessCashFlow(c.movements, c.pockets, Date{2026, 6, 1}, Date{2026, 8, 31}, kUsd);
    check(months.size() == 3, "tres meses");
    if (months.size() != 3) return;
    checkMinor(months[0].in.minor(), 500'00, "junio entra lo cobrado; lo por cobrar no");
    checkMinor(months[0].outExpenses.minor(), 460'00,
               "salen los gastos pagados enteros: el filamento y la herramienta, el dia de la compra");
    checkMinor(months[0].outSalary.minor(), 300'00, "el sueldo sale aparte");
    checkMinor(months[0].balance.minor(), -160'00, "saldo al cierre: 100 + 500 - 460 - 300");
    checkMinor(months[1].balance.minor(), -160'00, "julio: entra 300 y sale 300 de sueldo");
    check(months[2].month == (Date{2026, 8, 1}) && months[2].in.isZero(), "agosto sin movimiento");
}


// ------------------------------------------------------------------- Bancos

void csvDelBanco() {
    std::printf("\n[bancos: leer el CSV]\n");
    check(detectSeparator("Fecha;Descripcion;Monto\n01/09/2026;\"CAFE; LECHE\";-2,50") == ';',
          "punto y coma, aunque haya uno entre comillas");
    check(detectSeparator("date,description,amount\n2026-09-01,x,1") == ',', "coma");
    check(detectSeparator("a\tb\tc") == '\t', "tabulador");

    const auto rows = parseCsv("Fecha;Descripcion;Monto\r\n01/09/2026;\"CAFE; \"\"LA ESQUINA\"\"\";-2,50\r\n\r\n"
                               "02/09/2026;PAGO CLIENTE;120,00\r\n",
                               ';');
    check(rows.size() == 3, "tres filas: la vacia no cuenta");
    if (rows.size() == 3) {
        check(rows[1].size() == 3 && rows[1][1] == "CAFE; \"LA ESQUINA\"",
              "las comillas como las escribe Excel");
        check(rows[2][2] == "120,00", "y el ultimo campo sin el fin de linea");
    }

    BankProfile perfil;
    perfil.name = "Banco General";
    const auto read = readBankRows(rows, perfil, kUsd);
    check(read.rows.size() == 2 && read.errors.empty(), "dos movimientos, sin la cabecera");
    if (read.rows.size() == 2) {
        check(read.rows[0].date == (Date{2026, 9, 1}) && read.rows[0].kind == MovementKind::Gasto &&
                  read.rows[0].amountMinor == 2'50,
              "negativo es gasto, en positivo");
        check(read.rows[1].kind == MovementKind::Ingreso && read.rows[1].amountMinor == 120'00,
              "positivo es ingreso");
        check(!read.rows[0].fingerprint.empty() && read.rows[0].fingerprint != read.rows[1].fingerprint,
              "cada uno con su huella");
    }

    // Otro banco: cargos y abonos en columnas separadas, fecha al reves.
    const auto otro = parseCsv("fecha,detalle,cargo,abono\n2026-09-03,UBER *TRIP,8.40,\n"
                               "2026-09-03,UBER *TRIP,8.40,\n2026-09-04,DEPOSITO,,\"1,250.00\"\n2026-02-30,MAL,1,\n",
                               ',');
    BankProfile dos;
    dos.dateFormat = DateFormat::AnioMesDia;
    dos.amountColumn = -1;
    dos.debitColumn = 2;
    dos.creditColumn = 3;
    const auto read2 = readBankRows(otro, dos, kUsd);
    check(read2.rows.size() == 3, "las tres filas buenas se leen");
    check(read2.errors.size() == 1, "la fecha imposible queda como error, sin frenar las demas");
    if (read2.rows.size() == 3) {
        check(read2.rows[2].kind == MovementKind::Ingreso && read2.rows[2].amountMinor == 1250'00,
              "el abono es ingreso, con separador de miles entre comillas");
        check(read2.rows[0].kind == MovementKind::Gasto && read2.rows[0].amountMinor == 8'40,
              "el cargo es gasto");
        check(read2.rows[0].fingerprint != read2.rows[1].fingerprint,
              "dos viajes iguales el mismo dia son dos viajes: huellas distintas");
    }
}

void cruzarConLoAnotado() {
    std::printf("\n[bancos: nunca dos veces]\n");
    BankRow cafe{Date{2026, 9, 1}, "CAFE LA ESQUINA", 2'50, MovementKind::Gasto, "h-cafe"};
    BankRow cobro{Date{2026, 9, 2}, "TRANSFERENCIA JUAN PEREZ", 120'00, MovementKind::Ingreso, "h-cobro"};
    BankRow uber{Date{2026, 9, 3}, "UBER *TRIP", 8'40, MovementKind::Gasto, "h-uber"};
    BankRow viejo{Date{2026, 8, 3}, "NETFLIX", 9'99, MovementKind::Gasto, "h-netflix"};

    Movement aMano;  // el cobro, anotado a mano un dia despues
    aMano.id = "m-cobro";
    aMano.date = Date{2026, 9, 3};
    aMano.name = "cobro gpu";
    aMano.kind = MovementKind::Ingreso;
    aMano.amountMinor = 120'00;
    aMano.pocketId = "banco";
    Movement uberAnterior;  // un uber importado antes, ya con categoria
    uberAnterior.id = "m-uber-viejo";
    uberAnterior.date = Date{2026, 8, 20};
    uberAnterior.name = "UBER *TRIP";
    uberAnterior.kind = MovementKind::Gasto;
    uberAnterior.amountMinor = 6'00;
    uberAnterior.category = "Transporte";
    uberAnterior.pocketId = "banco";
    Movement netflix;
    netflix.id = "m-netflix";
    netflix.date = Date{2026, 8, 3};
    netflix.name = "NETFLIX";
    netflix.kind = MovementKind::Gasto;
    netflix.amountMinor = 9'99;
    netflix.pocketId = "banco";
    const std::vector<MovementMeta> metas{{"m-netflix", "Importado", "", "", "", "h-netflix"}};

    const auto matches = matchBankRows({cafe, cobro, uber, viejo}, {aMano, uberAnterior, netflix}, metas);
    check(matches.size() == 4, "una respuesta por fila");
    if (matches.size() != 4) return;
    check(matches[0].status == BankStatus::Nuevo && matches[0].suggestedCategory.empty(),
          "el cafe es nuevo y sin historia no tiene categoria");
    check(matches[1].status == BankStatus::YaAnotado && matches[1].matchedMovementId == "m-cobro",
          "el cobro ya estaba anotado a mano un dia despues: se enlaza");
    check(matches[2].status == BankStatus::Nuevo && matches[2].suggestedCategory == "Transporte",
          "el uber es nuevo y se sugiere Transporte por lo aprendido");
    check(matches[3].status == BankStatus::YaImportado, "netflix ya se importo: su huella esta");

    // El mismo anotado a mano no se enlaza a dos filas.
    BankRow cobro2 = cobro;
    cobro2.fingerprint = "h-cobro-2";
    const auto twice = matchBankRows({cobro, cobro2}, {aMano}, {});
    check(twice.size() == 2 && twice[0].status == BankStatus::YaAnotado && twice[1].status == BankStatus::Nuevo,
          "un anotado a mano se enlaza con una sola fila");

    BankRow lejos = cobro;
    lejos.date = Date{2026, 9, 10};
    const auto far = matchBankRows({lejos}, {aMano}, {});
    check(far.size() == 1 && far[0].status == BankStatus::Nuevo, "a mas de dos dias no se confunde");
}

} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    // Una asercion de la biblioteca en modo depuracion abre un cuadro de
    // dialogo y espera un clic: la suite queda colgada para siempre. Que
    // escriba en la consola y aborte, como cualquier otra falla.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    std::printf("Banco de pruebas — las preguntas del negocio\n");

    cuentaDeCadaBolsillo();
    sueldoEsUnTraspaso();
    gastoCruzado();
    categorias();
    categoriasDeducidas();
    gastoPorCategoria();
    capturaDelMonto();
    capturaDelTipoYLaFecha();
    capturaDeLaCategoria();
    capturaDeLaReparacion();
    aprenderCategorias();
    costoDeUnaReparacion();
    rentabilidadPorTipo();
    altaDesdePlantilla();
    entregarYCobrar();
    totalesDeCotizaciones();
    cotizacionAceptada();
    informeEntregadoYPagado();
    duplicadosYAnotados();
    reparacionAMano();
    anotadoConTrabajo();
    tipoPorElEquipo();
    recurrentes();
    herramientas();
    tasaDeFijos();
    bandeja();
    utilidadNetaDelMes();
    sueldoRecomendado();
    flujoDeCaja();
    csvDelBanco();
    cruzarConLoAnotado();

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    return gFailures == 0 ? 0 : 1;
}
