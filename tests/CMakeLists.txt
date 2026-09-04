# Verificaciones. Sin framework y sin dependencias externas: se compilan con lo
# que ya hay en el equipo, que es la unica forma de que se sigan corriendo.

# --- Logica del dominio, sin base de datos ---
add_executable(dake_selftest selftest.cpp)
target_link_libraries(dake_selftest PRIVATE dake::core)
add_test(NAME nucleo COMMAND dake_selftest)

# --- Persistencia ---
# Escribe siempre en una carpeta temporal propia: nunca toca la base real ni la
# del banco de pruebas.
find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(dake_storagetest storagetest.cpp)
target_link_libraries(dake_storagetest PRIVATE dake::storage Qt6::Core)
add_test(NAME persistencia COMMAND dake_storagetest)

# --- Diagnostico de la nube ---
# NO entra en ctest a proposito: toca la red y depende de que el proyecto de
# Supabase este arriba. Una prueba automatica que falla porque se cayo internet
# ensena a ignorar las pruebas que fallan.
find_package(Qt6 REQUIRED COMPONENTS Network)

add_executable(dake_synccheck synccheck.cpp)
target_link_libraries(dake_synccheck PRIVATE dake_sync Qt6::Core Qt6::Network)
target_compile_definitions(dake_synccheck PRIVATE DAKE_APP_NAME="${DAKE_APP_NAME}")
