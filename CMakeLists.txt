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
# applicationName). Este proyecto dejo de ser un banco de pruebas y paso a ser
# la aplicacion, asi que toma el nombre y la carpeta de la vieja.
#
# El ARCHIVO, en cambio, NO se llama finanzas.db, y eso es deliberado: en esa
# misma carpeta esta la base de la aplicacion que se descontinua, con OTRO
# esquema. Si esta abriera aquel archivo, los CREATE TABLE IF NOT EXISTS no
# recrearian nada —las tablas ya estan, con las columnas viejas— y la primera
# consulta que pidiera pocket_id reventaria contra datos reales.
set(DAKE_APP_NAME "Finanzas DakeLabs" CACHE STRING
    "Nombre visible de la aplicacion y de su carpeta de datos")
set(DAKE_DB_FILE "finanzas-v2.db" CACHE STRING
    "Nombre del archivo de base, dentro de la carpeta de datos")

# --- Credenciales de Supabase -------------------------------------------
#
# Se leen del supabase.json del equipo AL CONFIGURAR y se incrustan en el
# binario. Asi el APK del telefono llega con las credenciales puestas —alla no
# hay forma comoda de dejarle un archivo— sin que la clave entre nunca al
# repositorio.
#
# Que no haya archivo no es un error: la aplicacion compila igual y funciona
# solo en local hasta que aparezcan credenciales.
set(DAKE_SUPABASE_URL "" CACHE STRING "URL del proyecto de Supabase")
set(DAKE_SUPABASE_ANON_KEY "" CACHE STRING "Clave publicable de Supabase")

if(NOT DAKE_SUPABASE_URL AND DEFINED ENV{APPDATA})
    set(_dake_cred "$ENV{APPDATA}/DakeLabs/${DAKE_APP_NAME}/supabase.json")
    if(EXISTS "${_dake_cred}")
        file(READ "${_dake_cred}" _dake_cred_json)
        string(JSON _dake_url ERROR_VARIABLE _dake_err GET "${_dake_cred_json}" url)
        string(JSON _dake_key ERROR_VARIABLE _dake_err GET "${_dake_cred_json}" anon_key)
        if(_dake_url AND _dake_key)
            set(DAKE_SUPABASE_URL "${_dake_url}" CACHE STRING "" FORCE)
            set(DAKE_SUPABASE_ANON_KEY "${_dake_key}" CACHE STRING "" FORCE)
            message(STATUS "Supabase: credenciales tomadas de ${_dake_cred}")
        endif()
    endif()
endif()

if(NOT DAKE_SUPABASE_URL)
    message(STATUS "Supabase: sin credenciales, la aplicacion trabajara solo en local")
endif()

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(sync)
add_subdirectory(mobile)

# La interfaz de escritorio y las verificaciones no se compilan para el
# telefono: Widgets en Android produce una interfaz de escritorio metida en una
# pantalla chica, y una suite de pruebas dentro de un APK no la corre nadie.
if(NOT ANDROID)
    add_subdirectory(ui)

    enable_testing()
    add_subdirectory(tests)
endif()
