cmake_minimum_required(VERSION 3.25)

project(FinanzasBancoDePruebas
    VERSION 0.1.0
    DESCRIPTION "Banco de pruebas: bolsillos reales, traspasos y trabajos"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# El nombre manda sobre la carpeta de datos (QStandardPaths la arma con
# applicationName). Es DISTINTO del de la aplicacion real a proposito: este
# banco de pruebas no puede tocar ni por accidente la base que tiene los
# movimientos de verdad.
set(DAKE_APP_NAME "Finanzas DakeLabs Pruebas" CACHE STRING
    "Nombre visible de la aplicacion y de su carpeta de datos")

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(mobile)

# La interfaz de escritorio y las verificaciones no se compilan para el
# telefono: Widgets en Android produce una interfaz de escritorio metida en una
# pantalla chica, y una suite de pruebas dentro de un APK no la corre nadie.
if(NOT ANDROID)
    add_subdirectory(ui)

    enable_testing()
    add_subdirectory(tests)
endif()
