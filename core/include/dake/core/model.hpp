#pragma once
//
// dake/core/model.hpp — modelo del banco de pruebas.
//
// QUE CAMBIA RESPECTO DE LA APP ACTUAL
//
// En "Finanzas DakeLabs" las secciones (inversion, ahorro, salario) son un
// calculo: se reparte la utilidad del mes segun un porcentaje fijo. Nadie
// puede sacar plata de ahorro porque ahorro no es un lugar, es una division
// de una resta. Cuando en la vida real se saca del ahorro para comprar
// material, la app lo anota como un gasto mas, la utilidad del mes cae, y las
// tres secciones bajan en proporcion —incluida la de salario, que nadie toco.
// Los saldos dejan de parecerse a la realidad y el dueño deja de creerles.
//
// Aca los bolsillos son ENTIDADES con saldo propio, y existe el traspaso.
// "Saque 46 dolares del ahorro para comprar PLA" es un movimiento que la app
// puede representar, ver y avisar. Es la diferencia entre un tablero que
// describe la plata y uno que la inventa.
//
// Dependencias permitidas: <cstdint> <string> <string_view> <vector>
//                          "money.hpp" "date.hpp" "currency.hpp"
//
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/currency.hpp"
#include "dake/core/date.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

/// Todo identificador del dominio es un UUIDv7 en forma canonica.
using Id = std::string;

// ---------------------------------------------------------------- Bolsillos

/// Donde vive la plata. Un bolsillo NO es una categoria ni un porcentaje: es
/// un lugar con saldo, que se puede contar a mano y comparar contra la app.
/// Un saldo que no se puede cuadrar contra la realidad no se usa dos veces.
enum class PocketKind {
    Operacion,  ///< caja del dia a dia: entra lo que cobras, sale lo que pagas
    Ahorro,     ///< reserva. Sacar de aca es una decision, no un accidente
    Inversion,  ///< capital puesto a trabajar (maquinas, stock, mercado)
    Personal,   ///< lo que ya te pagaste; gastarlo no es gasto del negocio
    Emergencia  ///< el fondo para imprevistos: no se toca salvo emergencia
};

[[nodiscard]] std::string_view toString(PocketKind value) noexcept;
[[nodiscard]] PocketKind pocketKindFromString(std::string_view text);
[[nodiscard]] std::array<PocketKind, 5> allPocketKinds() noexcept;

/// true si el bolsillo es una reserva: financiar la operacion desde aca es
/// justo lo que la app tiene que saber decir en voz alta.
[[nodiscard]] bool isReserve(PocketKind value) noexcept;

/// De quien es la plata. Negocio y personal son dos cuentas separadas: lo que
/// pasa de una a la otra es sueldo, no gasto del negocio ni ingreso personal.
enum class Account { Negocio, Personal };

[[nodiscard]] std::string_view toString(Account value) noexcept;
[[nodiscard]] Account accountFromString(std::string_view text);

struct Pocket {
    Id id;
    std::string name;
    PocketKind kind = PocketKind::Operacion;
    /// Lo que habia el dia que empezaste a usar la app. Sin esto el primer
    /// saldo sale en cero y nunca cuadra contra la realidad.
    std::int64_t openingMinor = 0;
    bool archived = false;

    /// Vacio = la cuenta que corresponde al tipo (ver accountOf). Solo se
    /// guarda cuando alguien la cambia a mano, por ejemplo un ahorro personal.
    ///
    /// Vive en una tabla local y NO viaja en la sincronizacion: el servidor no
    /// conoce la columna y rechazaria la fila entera.
    std::optional<Account> accountOverride;

    std::string hlc;
    std::string deviceId;
    bool deleted = false;
};

// -------------------------------------------------------------- Movimientos

/// Tres tipos, no dos. El traspaso es el que faltaba: mover plata entre dos
/// bolsillos propios no es ingreso ni gasto, y contarlo como cualquiera de los
/// dos ensucia todos los totales del mes.
enum class MovementKind { Ingreso, Gasto, Traspaso };

[[nodiscard]] std::string_view toString(MovementKind value) noexcept;
[[nodiscard]] MovementKind movementKindFromString(std::string_view text);

enum class Recurrence { Puntual, Mensual };

[[nodiscard]] std::string_view toString(Recurrence value) noexcept;
[[nodiscard]] Recurrence recurrenceFromString(std::string_view text);

/// Un movimiento. `amountMinor` es SIEMPRE positivo: el signo lo pone `kind`,
/// nunca el numero.
struct Movement {
    Id id;
    Date date;
    std::string name;
    MovementKind kind = MovementKind::Gasto;
    std::int64_t amountMinor = 0;

    /// Ingreso: bolsillo donde entra. Gasto: de donde sale.
    /// Traspaso: bolsillo de ORIGEN.
    Id pocketId;
    /// Solo para Traspaso: bolsillo de DESTINO. Vacio en los otros dos.
    Id targetPocketId;

    std::string category;
    /// Trabajo al que pertenece. Vacio = gasto general del negocio.
    /// Es lo que permite responder "cuanto deje en la reparacion del Macbook",
    /// que es el unico numero por el que vale la pena cargar los datos.
    Id jobId;

    /// Legado: antes repartia un gasto en varios meses. Ya no se usa; se
    /// guarda y se sincroniza para no romper al telefono viejo.
    int spreadMonths = 1;

    /// Ingreso: ya lo cobraste. Gasto: ya lo pagaste.
    /// Un ingreso sin cobrar NO suma al saldo del bolsillo: aparece aparte,
    /// como lo que es, una promesa.
    bool settled = true;

    /// CUANDO se cobro o se pago, si se sabe. Vacia cuando todavia no ocurrio,
    /// y tambien en todo lo anotado antes de que este campo existiera.
    ///
    /// `settled` dice SI, esto dice CUANDO, y son dos preguntas distintas: sin
    /// la fecha no hay forma de contestar cuanto tardas en cobrar, que es el
    /// numero que separa un negocio rentable de uno rentable en el papel y sin
    /// plata en la caja.
    ///
    /// No se rellena sola con la fecha de hoy al marcar algo como cobrado: una
    /// fecha inventada contamina el promedio y nadie se entera.
    ///
    /// Es `optional` y no una Date a secas porque una Date construida por
    /// defecto vale 1970-01-01, que es una fecha REAL: no habria forma de
    /// distinguir "no se sabe" de "se cobro en 1970", y el promedio de dias de
    /// cobro se iria a veinte mil dias sin que nadie entienda por que.
    /// `std::nullopt` significa "no se sabe", y quien promedia la saltea.
    std::optional<Date> settledDate;

    Recurrence recurrence = Recurrence::Puntual;

    std::string hlc;
    std::string deviceId;
    bool deleted = false;

    /// true si el movimiento tiene la forma que su tipo exige.
    [[nodiscard]] bool isWellFormed() const noexcept;
};

/// Nombre reservado para un movimiento sin categoria escrita.
inline constexpr std::string_view kUncategorized = "Sin categoria";

/// Categoria de los movimientos que nacen de cuadrar un bolsillo contra la
/// realidad. Vive aca para que la interfaz y el almacenamiento no escriban dos
/// textos distintos y terminen con dos categorias que el usuario ve como una.
inline constexpr std::string_view kAdjustment = "Ajuste de saldo";

// ----------------------------------------------------------------- Trabajos

/// Un trabajo concreto: una reparacion, una impresion, un encargo. Existe para
/// poder juntar el ingreso con los costos que lo hicieron posible.
struct Job {
    Id id;
    std::string name;
    std::string client;
    Date opened;
    bool closed = false;

    std::string hlc;
    std::string deviceId;
    bool deleted = false;
};

// ------------------------------------------------------------------ Helpers

/// Los movimientos vivos (sin lapida) del rango [from, to] inclusive.
[[nodiscard]] std::vector<Movement> inRange(const std::vector<Movement>& movements,
                                            Date from,
                                            Date to);

/// Los `count` movimientos mas recientes, del mas nuevo al mas viejo. El
/// desempate entre movimientos del mismo dia va por id descendente, que es el
/// orden de captura: sin un desempate estable la lista se reordena sola entre
/// refrescos y las filas saltan sin motivo.
[[nodiscard]] std::vector<Movement> recentMovements(const std::vector<Movement>& movements,
                                                    int count);

/// Dias desde el ultimo movimiento cargado hasta `asOf`. -1 si no hay ninguno.
[[nodiscard]] int daysSinceLastEntry(const std::vector<Movement>& movements, Date asOf);

} // namespace dake::core
