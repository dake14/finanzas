# dake_ui — Qt 6 Widgets. Unica capa que conoce Qt.
#
# Las graficas se pintan a mano con QPainter en vez de traer Qt Charts: son dos
# rectangulos y una leyenda, y una dependencia mas seria mas codigo, no menos.

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

qt_standard_project_setup()

set(DAKE_UI_SOURCES
    mainwindow.cpp
    mainwindow.hpp
    pages.hpp
    todaypage.cpp
    jobspage.cpp
    movementspage.cpp
    pocketspage.cpp
    quickentry.cpp
    quickentry.hpp
    dialogs.cpp
    dialogs.hpp
    cards.cpp
    cards.hpp
    charts.cpp
    charts.hpp
    categorybox.cpp
    categorybox.hpp
    theme.cpp
    theme.hpp
    snapshot.hpp
    tables.hpp
)

qt_add_executable(dake_pruebas WIN32
    main.cpp
    ${DAKE_UI_SOURCES}
)

target_include_directories(dake_pruebas PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dake_pruebas PRIVATE dake::core dake::storage dake_sync Qt6::Widgets)
target_compile_definitions(dake_pruebas PRIVATE DAKE_APP_NAME="${DAKE_APP_NAME}")

# --- Banco de pruebas visual ---
# Renderiza las pantallas a PNG con el complemento offscreen. Deliberadamente
# NO abre una ventana: verificar la interfaz con capturas del escritorio exige
# robar el foco y hacer clics a ciegas, y cualquier ventana que se ponga encima
# los desvia.
qt_add_executable(dake_uipreview
    preview.cpp
    ${DAKE_UI_SOURCES}
)
target_include_directories(dake_uipreview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dake_uipreview PRIVATE dake::core dake::storage dake_sync Qt6::Widgets)
target_compile_definitions(dake_uipreview PRIVATE DAKE_APP_NAME="${DAKE_APP_NAME}")

if(WIN32)
    find_program(DAKE_WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(DAKE_WINDEPLOYQT)
        add_custom_command(TARGET dake_pruebas POST_BUILD
            COMMAND "${DAKE_WINDEPLOYQT}" --no-translations --no-system-d3d-compiler
                    --no-compiler-runtime "$<TARGET_FILE:dake_pruebas>"
            COMMENT "Desplegando las DLL de Qt junto a dake_pruebas")
    endif()

    # windeployqt copia solo el complemento de plataforma que usa la app
    # ("windows"). El banco visual necesita ademas "offscreen": sin el, Qt
    # aborta con un cuadro de dialogo que en modo headless deja el proceso
    # colgado esperando un clic que nadie va a dar.
    add_custom_command(TARGET dake_uipreview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:dake_uipreview>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${Qt6_DIR}/../../../plugins/platforms/qoffscreend.dll,${Qt6_DIR}/../../../plugins/platforms/qoffscreen.dll>"
                "$<TARGET_FILE_DIR:dake_uipreview>/platforms/"
        COMMENT "Copiando el complemento offscreen para el banco visual")
endif()
