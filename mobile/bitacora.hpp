#pragma once
//
// mobile/bitacora.hpp — el cuaderno de arranque.
//
// EXISTE PORQUE UNA PANTALLA NEGRA NO DICE NADA.
//
// En la computadora, cuando algo falla al arrancar, el mensaje sale por
// consola y se lee. En el telefono no hay consola: la aplicacion abre o no
// abre, y si no abre lo unico que queda es adivinar. Adivinar sobre el equipo
// de otra persona, a distancia, es como se pierden tardes enteras.
//
// Esto escribe una linea por cada paso del arranque en un archivo dentro del
// almacenamiento privado de la aplicacion. Si la proxima apertura funciona, el
// archivo cuenta exactamente hasta donde llego la anterior y con que error se
// fue. Se lee desde la propia aplicacion, en Mas → Diagnostico, sin cables ni
// adb ni tener el telefono a mano.
//
// Tambien captura los qWarning/qCritical de Qt: los avisos del motor de QML
// —un archivo que no compila, una propiedad que no existe— son justamente los
// que dejan la ventana vacia, y en el telefono no los ve nadie.
//
// El archivo se recorta solo. Un registro que crece sin limite en un telefono
// termina siendo un problema en vez de una ayuda.
//
#include <QString>
#include <QStringList>

namespace dake::mobile::bitacora {

/// Ruta del archivo. Dentro del almacenamiento privado de la aplicacion, al
/// lado de la base.
[[nodiscard]] QString ruta();

/// Empieza una corrida nueva y toma el control de los mensajes de Qt.
/// Se llama una sola vez, lo antes posible dentro de main().
void instalar();

/// Una linea con la hora delante. Es todo lo que hace falta para saber cuanto
/// tardo cada paso y en cual se quedo.
void anotar(const QString& texto);

/// Las ultimas `cuantas` lineas, de la mas vieja a la mas nueva, para mostrar
/// en pantalla.
[[nodiscard]] QStringList ultimas(int cuantas);

} // namespace dake::mobile::bitacora
