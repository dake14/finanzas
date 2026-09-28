#pragma once
//
// dake/core/capture.hpp — lo que necesita el formulario de anotar.
//
// Anotar es un formulario: tipo, monto, categoria, bolsillo, fecha. No hay
// interprete que adivine a partir de una frase. Lo que queda aca son las
// piezas que el formulario usa para arrancar con lo que corresponde: leer un
// monto escrito como se dice, proponer el bolsillo y ordenar las categorias.
//
// Dependencias permitidas: <optional> <string> <string_view> <vector>
//                          "model.hpp" "accounts.hpp" "money.hpp" "date.hpp"
//
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dake/core/accounts.hpp"
#include "dake/core/date.hpp"
#include "dake/core/model.hpp"
#include "dake/core/money.hpp"

namespace dake::core {

/// Un monto escrito como se dice: "25", "25,50", "25.50", "$25", "1.200".
/// Vacio si no es un monto mayor que cero, o si trae algo mas que el monto.
[[nodiscard]] std::optional<std::int64_t> parseAmount(std::string_view text, Currency currency);

/// El bolsillo que se propone para una cuenta: el ultimo usado de esa cuenta
/// (sin contar traspasos ni archivados), o el primero que haya. Sin cuenta, el
/// ultimo usado de cualquiera.
[[nodiscard]] Id suggestedPocket(const std::vector<Pocket>& pockets,
                                 const std::vector<Movement>& history,
                                 std::optional<Account> account);

/// Minusculas y sin tildes, sobre UTF-8. "Rodríguez" -> "rodriguez". Es lo
/// que se usa para comparar nombres escritos por personas.
[[nodiscard]] std::string foldText(std::string_view text);

/// Las categorias de un tipo de movimiento, de la mas usada a la menos en los
/// ultimos 90 dias hasta `today`, y despues las que no se usaron.
[[nodiscard]] std::vector<std::string> categoriesByUse(const std::vector<Category>& categories,
                                                       const std::vector<Movement>& history,
                                                       MovementKind kind, Date today);

} // namespace dake::core
