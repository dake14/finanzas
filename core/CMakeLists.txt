# dake_core — nucleo portable.
#
# REGLA: sin Qt, sin SQL, sin API de Windows. Solo STL. Es lo que permite
# probar toda la logica de plata sin abrir una ventana ni un archivo.

add_library(dake_core STATIC
    src/currency.cpp
    src/money.cpp
    src/date.cpp
    src/uuid.cpp
    src/hlc.cpp
    src/format.cpp
    src/model.cpp
    src/report.cpp
    src/demo.cpp
    src/accounts.cpp
    src/capture.cpp
    src/repairs.cpp
)

target_include_directories(dake_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(dake_core PUBLIC cxx_std_20)

add_library(dake::core ALIAS dake_core)
