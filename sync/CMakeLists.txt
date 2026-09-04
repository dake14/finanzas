# sync/ — la capa que habla con Supabase.
#
# Depende de storage (necesita el repositorio y la forma JSON de un registro) y
# de Qt Network. NO depende de ui ni de mobile: el motor no sabe si lo maneja
# una ventana de escritorio o una pantalla de telefono, y por eso los dos lo
# pueden usar sin duplicar una linea.

find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_library(dake_sync STATIC
    src/config.cpp
    src/supabase_client.cpp
    src/sync_engine.cpp
)

target_include_directories(dake_sync PUBLIC include)

target_link_libraries(dake_sync
    PUBLIC
        dake_core
        dake_storage
        Qt6::Core
        Qt6::Network
)

# Las credenciales incrustadas.
#
# Se definen SIEMPRE las dos macros, aunque sea con la cadena vacia: asi el
# codigo no necesita #ifdef y compila igual con o sin credenciales. Los valores
# los pone el CMakeLists raiz leyendo el supabase.json del equipo, asi que no
# entran nunca al repositorio.
target_compile_definitions(dake_sync PRIVATE
    DAKE_SUPABASE_URL="${DAKE_SUPABASE_URL}"
    DAKE_SUPABASE_ANON_KEY="${DAKE_SUPABASE_ANON_KEY}"
)
