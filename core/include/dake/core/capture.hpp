#pragma once
//
// dake/core/capture.hpp — el interprete de la captura rapida.
//
// "25 almuerzo" o "120 cobro GPU 3080" se convierten en un movimiento sin
// preguntar nada mas. Monto y categoria son lo unico que hace falta; lo demas
// se deduce (fecha de hoy, el ultimo bolsillo usado de esa cuenta, la
// reparacion abierta que coincide) o queda por completar. El interprete nunca
// falla por falta de un dato: devuelve lo que pudo y marca lo que falta.
//
// Aprender una categoria es solo corregirla: la sugerencia es la categoria mas
// usada con esa misma descripcion, asi que la proxima vez gana la corregida
// sin configurar ninguna regla.
//
// Dependencias permitidas: <optional> <string> <string_view> <vector>
//                          "model.hpp" "accounts.hpp" "money.hpp" "date.hpp"
//
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/date.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

/// Lo que el interprete necesita saber de una reparacion abierta para
/// reconocerla dentro de una frase.
struct RepairRef {
    Id jobId;
    std::string orderNo;  ///< "INF-2026-004", "R-0042"
    std::string device;   ///< "RTX 3080"
    std::string client;   ///< "Juan Perez"
};

struct CaptureContext {
    Date today;
    Currency currency = Currency::usd();
    std::vector<Pocket> pockets;
    std::vector<Category> categories;
    /// Los movimientos ya anotados: de aca sale lo que se aprende.
    std::vector<Movement> history;
    std::vector<RepairRef> openRepairs;
};

/// De donde salio la categoria, para decirlo en la vista previa.
enum class CategorySource {
    Ninguna,     ///< no se pudo deducir: queda por completar
    Historial,   ///< la mas usada con esa misma descripcion
    Nombre,      ///< una palabra de la frase es el nombre de una categoria
    Reparacion   ///< la frase se enlazo a una reparacion
};

struct CaptureDraft {
    std::optional<std::int64_t> amountMinor;  ///< vacio: no hay monto todavia
    MovementKind kind = MovementKind::Gasto;
    bool salary = false;  ///< traspaso negocio -> personal
    Date date;
    std::string description;
    std::string category;
    CategorySource categorySource = CategorySource::Ninguna;
    Id pocketId;
    Id targetPocketId;  ///< solo en sueldo
    Id jobId;

    /// Se puede guardar: tiene monto y un bolsillo. Sin categoria tambien se
    /// guarda, marcado para la bandeja.
    [[nodiscard]] bool canSave() const noexcept;
    /// Le falta algo que se completa despues (hoy: la categoria).
    [[nodiscard]] bool incomplete() const noexcept;

    /// El movimiento que resulta, sin id ni hlc.
    [[nodiscard]] Movement toMovement() const;
};

[[nodiscard]] CaptureDraft parseCapture(std::string_view text, const CaptureContext& context);

/// El bolsillo que se propone para una cuenta: el ultimo usado de esa cuenta,
/// o el primero que haya. Sin cuenta, el ultimo usado de cualquiera. Es lo que
/// usa el interprete, expuesto para cuando alguien cambia la categoria a mano.
[[nodiscard]] Id suggestedPocket(const CaptureContext& context, std::optional<Account> account);

/// Minusculas y sin tildes, sobre UTF-8. "Rodríguez" -> "rodriguez". Es lo
/// que se usa para comparar nombres escritos por personas.
[[nodiscard]] std::string foldText(std::string_view text);

/// La descripcion reducida a lo que se compara para aprender: minusculas, sin
/// tildes, sin numeros ni signos, espacios simples. "Almuerzo 2x" y
/// "almuerzo" son la misma descripcion.
[[nodiscard]] std::string normalizeDescription(std::string_view text);

/// La categoria que se sugiere para una descripcion, segun el historial. Vacia
/// si esa descripcion nunca se anoto con categoria.
[[nodiscard]] std::string learnedCategory(std::string_view description,
                                          const std::vector<Movement>& history);

/// Las categorias de un tipo de movimiento, de la mas usada a la menos en los
/// ultimos 90 dias hasta `today`, y despues las que no se usaron.
[[nodiscard]] std::vector<std::string> categoriesByUse(const std::vector<Category>& categories,
                                                       const std::vector<Movement>& history,
                                                       MovementKind kind, Date today);

} // namespace dake::core
