# dake_storage — la unica capa que conoce SQL.

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

add_library(dake_storage STATIC
    src/database.cpp
    src/exchange.cpp
    src/repository.cpp
)

target_include_directories(dake_storage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(dake_storage PUBLIC dake::core Qt6::Core Qt6::Sql)

add_library(dake::storage ALIAS dake_storage)
