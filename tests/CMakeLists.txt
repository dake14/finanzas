# Verificaciones. Sin framework y sin dependencias externas: se compilan con lo
# que ya hay en el equipo, que es la unica forma de que se sigan corriendo.

# --- Logica del dominio, sin base de datos ---
add_executable(dake_selftest selftest.cpp)
target_link_libraries(dake_selftest PRIVATE dake::core)
add_test(NAME nucleo COMMAND dake_selftest)

# --- Las preguntas del negocio: cuentas, sueldo, reparaciones, fijos ---
add_executable(dake_negociotest negociotest.cpp)
target_link_libraries(dake_negociotest PRIVATE dake::core)
add_test(NAME negocio COMMAND dake_negociotest)

# --- Persistencia ---
# Escribe siempre en una carpeta temporal propia: nunca toca la base real ni la
# del banco de pruebas.
find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(dake_storagetest storagetest.cpp)
target_link_libraries(dake_storagetest PRIVATE dake::storage Qt6::Core)
add_test(NAME persistencia COMMAND dake_storagetest)

# --- La interfaz, manejada con el teclado ---
# Sin pantalla (offscreen) y con las teclas mandadas por QTest a los widgets de
# la propia aplicacion. Nunca teclas del sistema: esas van a la ventana que
# tenga el foco, sea cual sea.
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(dake_uitest uitest.cpp)
target_link_libraries(dake_uitest PRIVATE dake_ui Qt6::Test)
add_test(NAME interfaz COMMAND dake_uitest)
if(WIN32)
    add_custom_command(TARGET dake_uitest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${Qt6_DIR}/../../../bin/Qt6Testd.dll,${Qt6_DIR}/../../../bin/Qt6Test.dll>"
                "$<TARGET_FILE_DIR:dake_uitest>/"
        COMMENT "Copiando Qt6Test junto a la prueba de la interfaz")
endif()

# --- Diagnostico de la nube ---
# NO entra en ctest a proposito: toca la red y depende de que el proyecto de
# Supabase este arriba. Una prueba automatica que falla porque se cayo internet
# ensena a ignorar las pruebas que fallan.
find_package(Qt6 REQUIRED COMPONENTS Network)

add_executable(dake_synccheck synccheck.cpp)
target_link_libraries(dake_synccheck PRIVATE dake_sync Qt6::Core Qt6::Network)
target_compile_definitions(dake_synccheck PRIVATE DAKE_APP_NAME="${DAKE_APP_NAME}")
