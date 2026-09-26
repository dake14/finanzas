#pragma once
//
// dake/core/salary.hpp — cuanto me puedo pagar, y como se movio la caja.
//
// UTILIDAD NETA DEL MES = ingresos del negocio - costo imputado del negocio.
// Ingresos por la fecha en que se hicieron, cobrados o no: el trabajo se hizo.
// Costo con los repartos (cuatro kilos de filamento son cuatro meses de
// costo), herramientas por depreciacion y no por compra, y sin traspasos: el
// sueldo nunca resta. Tampoco restan las horas x tarifa: ahi las horas no son
// un gasto que se le pague a otro, son justamente lo que se reparte.
//
// SUELDO RECOMENDADO = el menor de los promedios de 3 y 6 meses cerrados de
// utilidad neta, por el porcentaje de sueldo. El menor, porque es el
// sostenible: si el ultimo trimestre fue mejor, todavia no se sabe si se
// repite. El mes en curso no cuenta: no termino. Un mes sin movimientos
// cuenta como cero: saltearlo inflaria el promedio justo cuando el negocio
// anda peor.
//
// Dependencias permitidas: <optional> <vector> "model.hpp" "money.hpp"
//                          "accounts.hpp" "fixed.hpp"
//
#include <optional>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/fixed.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

struct MonthNet {
    Date month;  ///< primer dia del mes
    Money income;
    Money cost;
    Money net;
};

[[nodiscard]] MonthNet businessNet(const std::vector<Movement>& movements,
                                   const std::vector<Pocket>& pockets,
                                   const std::vector<Category>& categories,
                                   const std::vector<Tool>& tools, Date month, Currency currency);

/// Como se reparte la utilidad. Suman 10000.
struct ProfitSplit {
    int salaryBps = 5500;
    int taxesBps = 1500;
    int reinvestBps = 2000;
    int emergencyBps = 1000;
    [[nodiscard]] bool valid() const noexcept {
        return salaryBps >= 0 && taxesBps >= 0 && reinvestBps >= 0 && emergencyBps >= 0 &&
               salaryBps + taxesBps + reinvestBps + emergencyBps == 10000;
    }
};

struct SalaryAdvice {
    int closedMonths = 0;           ///< meses cerrados con historia (hasta 6)
    std::optional<Money> average3;  ///< vacio sin ningun mes cerrado
    std::optional<Money> average6;
    Money base;                     ///< el menor de los dos, nunca negativo
    Money shortfall;                ///< si la utilidad promedio es negativa, cuanto
    bool provisional = false;       ///< menos de 3 meses: se usa lo que hay
    Money salary;
    Money taxes;
    Money reinvest;
    Money emergency;
    std::optional<Money> personalSpend;  ///< gasto personal promedio, mismos meses que average3
    Money paidAverage;                   ///< lo que te pagaste por mes (traspasos de sueldo)
    std::vector<MonthNet> months;        ///< los meses usados, del mas nuevo al mas viejo

    /// Sueldo sostenible menos gasto personal. Vacio sin gasto personal.
    [[nodiscard]] std::optional<Money> margin() const {
        if (!personalSpend) return std::nullopt;
        return salary - *personalSpend;
    }
};

[[nodiscard]] SalaryAdvice salaryAdvice(const std::vector<Movement>& movements,
                                        const std::vector<Pocket>& pockets,
                                        const std::vector<Category>& categories,
                                        const std::vector<Tool>& tools, const ProfitSplit& split,
                                        Date today, Currency currency);

// ------------------------------------------------------------ Flujo de caja

struct CashMonth {
    Date month;
    Money in;           ///< cobrado y aportado a los bolsillos del negocio
    Money outExpenses;  ///< gastos pagados desde el negocio
    Money outSalary;    ///< lo que paso a lo personal
    Money balance;      ///< saldo de los bolsillos del negocio al cierre del mes
};

/// Mes a mes, de `from` a `to` (los dos inclusive, por mes). La caja, no el
/// costo: lo que entro y salio de verdad, en la fecha en que paso.
[[nodiscard]] std::vector<CashMonth> businessCashFlow(const std::vector<Movement>& movements,
                                                      const std::vector<Pocket>& pockets,
                                                      Date from, Date to, Currency currency);

} // namespace dake::core
