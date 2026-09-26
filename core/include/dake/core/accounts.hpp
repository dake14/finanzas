#pragma once
//
// dake/core/accounts.hpp — negocio y personal, dos cuentas separadas.
//
// La plata que pasa del negocio a lo personal es SUELDO: no es un gasto del
// negocio ni un ingreso personal nuevo. No hace falta un tipo de movimiento
// aparte para eso: es cualquier traspaso de un bolsillo del negocio a uno
// personal, y la aplicacion lo reconoce sola. Nunca se pregunta lo que se
// puede deducir.
//
// Dependencias permitidas: <string> <vector> "model.hpp" "money.hpp"
//
#include <string>
#include <vector>

#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

/// La cuenta de un bolsillo: la elegida a mano si la hay; si no, la que
/// corresponde al tipo (Personal es personal, el resto es del negocio).
[[nodiscard]] Account accountOf(const Pocket& pocket) noexcept;

/// La cuenta del bolsillo de un movimiento (el de origen, en un traspaso).
/// Negocio si el bolsillo no existe: un gasto huerfano del negocio se ve en
/// los reportes; uno huerfano personal desapareceria de ellos.
[[nodiscard]] Account accountOf(const Movement& movement, const std::vector<Pocket>& pockets);

/// true si el movimiento es un traspaso de un bolsillo del negocio a uno
/// personal: eso es sueldo.
[[nodiscard]] bool isSalary(const Movement& movement, const std::vector<Pocket>& pockets);

// ---------------------------------------------------------------- Categorias

/// Como entra una categoria de gasto al costo del negocio.
enum class CategoryClass {
    General,   ///< gasto comun: cae en el mes, con su reparto si lo tiene
    Fija,      ///< luz, internet, alquiler: forma la tasa de fijos por hora
    Variable,  ///< consumibles: se imputan por reparacion, no van a los fijos
    Activo     ///< herramientas: entran por depreciacion, no como gasto
};

[[nodiscard]] std::string_view toString(CategoryClass value) noexcept;
[[nodiscard]] CategoryClass categoryClassFromString(std::string_view text);

struct Category {
    std::string name;
    Account account = Account::Negocio;
    CategoryClass cls = CategoryClass::General;
    MovementKind kind = MovementKind::Gasto;
};

/// La categoria con ese nombre, o nullptr. Compara sin distinguir mayusculas:
/// "Comida" y "comida" son la misma para quien las escribe.
[[nodiscard]] const Category* findCategory(const std::vector<Category>& categories,
                                           std::string_view name);

/// Las categorias que aparecen en los movimientos y todavia no estan en
/// `known`, con la cuenta deducida: la de los bolsillos de donde salieron la
/// mayoria de sus movimientos (empate: negocio). Clase General.
///
/// Es lo que evita configurar a mano las categorias que ya existen: la cuenta
/// de "Almuerzo" se sabe mirando de donde se pago siempre.
[[nodiscard]] std::vector<Category> inferCategories(const std::vector<Movement>& movements,
                                                    const std::vector<Pocket>& pockets,
                                                    const std::vector<Category>& known);

// -------------------------------------------------------- Gasto cruzado

/// Un gasto personal pagado con plata del negocio.
///
/// Si `draft` es un gasto de categoria personal y sale de un bolsillo del
/// negocio, devuelve DOS movimientos: un traspaso del bolsillo del negocio a
/// `personalPocketId` por el mismo monto (que cuenta como sueldo) y el gasto,
/// ahora desde ese bolsillo personal. El negocio no registra un almuerzo como
/// gasto, y el sueldo real queda bien medido.
///
/// En cualquier otro caso devuelve `draft` solo. Los ids quedan como vienen:
/// los pone quien guarda.
[[nodiscard]] std::vector<Movement> splitCrossExpense(const Movement& draft,
                                                      const std::vector<Pocket>& pockets,
                                                      Account categoryAccount,
                                                      const Id& personalPocketId);

// ----------------------------------------------------- Gasto por categoria

struct CategorySpend {
    std::string category;
    Money current;   ///< gastado en el mes pedido
    Money previous;  ///< gastado en el mes anterior
    [[nodiscard]] Money change() const { return current - previous; }
};

struct SpendingReport {
    Account account = Account::Negocio;
    Money totalCurrent;
    Money totalPrevious;
    /// Primero las que mas subieron: es lo primero que hay que mirar.
    std::vector<CategorySpend> rows;
};

/// Gastos de una cuenta en el mes de `month`, contra el mes anterior.
///
/// Cuenta los GASTOS por la fecha en que se hicieron, pagados o no y sin
/// repartir: la pregunta es "cuanto gaste y en que", no "cuanto costo". Los
/// traspasos no son gasto, y por eso el sueldo nunca aparece aca.
[[nodiscard]] SpendingReport spendingByCategory(const std::vector<Movement>& movements,
                                                const std::vector<Pocket>& pockets,
                                                Account account,
                                                Date month,
                                                Currency currency);

} // namespace dake::core
