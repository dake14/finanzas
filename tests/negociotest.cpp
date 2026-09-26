// tests/negociotest.cpp — las preguntas del negocio: cuentas, sueldo,
// reparaciones, fijos, captura. Sin base de datos ni ventana.
//
// Mismo estilo que selftest.cpp: sin framework, una linea por comprobacion, y
// devuelve 0 si todo pasa.

#include <cstdio>
#include <string>
#include <vector>

#include "dake/core/accounts.hpp"
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

} // namespace

int main() {
    std::printf("Banco de pruebas — las preguntas del negocio\n");

    cuentaDeCadaBolsillo();
    sueldoEsUnTraspaso();
    gastoCruzado();
    categorias();
    categoriasDeducidas();
    gastoPorCategoria();

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    return gFailures == 0 ? 0 : 1;
}
