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
    # Las cabeceras van listadas a proposito. AUTOMOC solo empareja un .cpp con
    # el .hpp que esta AL LADO; aca las cabeceras viven en include/dake/sync/ y
    # las fuentes en src/, asi que sin nombrarlas moc no las procesa y la
    # biblioteca compila para despues explotar al enlazar, con un "unresolved
    # external symbol" por cada señal.
    include/dake/sync/config.hpp
    include/dake/sync/supabase_client.hpp
    include/dake/sync/sync_engine.hpp
)

# SupabaseClient y SyncEngine tienen Q_OBJECT, asi que necesitan moc. Sin esto
# la biblioteca compila perfecto y explota al ENLAZAR, con "unresolved external
# symbol" en cada señal: el compilador cree la promesa del .hpp y nadie escribe
# la implementacion. core y storage no lo necesitan porque no usan QObject.
set_target_properties(dake_sync PROPERTIES AUTOMOC ON)

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
