#pragma once
//
// dake/core/demo.hpp — el caso real de agosto de 2026, cargado en el modelo
// nuevo.
//
// Los seis movimientos son los que hay de verdad en la base de la aplicacion
// instalada. Lo que se agrega es lo que el modelo viejo NO podia representar:
// de que bolsillo salio cada peso, a que trabajo pertenece cada gasto, y
// cuantos meses dura un rollo de filamento.
//
// Los saldos iniciales de ahorro e inversion son INVENTADOS: la aplicacion
// actual nunca los pregunto, asi que no existen en ningun lado. Estan puestos
// para que el tablero tenga contra que medir; cambiarlos por los de verdad es
// la primera cosa que habria que hacer.
//
#include <vector>

#include "dake/core/currency.hpp"
#include "dake/core/model.hpp"

namespace dake::core {

struct DemoData {
    std::vector<Pocket> pockets;
    std::vector<Job> jobs;
    std::vector<Movement> movements;
};

/// Ids fijos y legibles a proposito: un dato de ejemplo con UUID aleatorio no
/// se puede afirmar en una prueba.
[[nodiscard]] DemoData realCaseAugust2026(Currency currency);

} // namespace dake::core
