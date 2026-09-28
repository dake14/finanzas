# dake_ui — Qt 6 Widgets. Unica capa que conoce Qt.
#
# Las graficas se pintan a mano con QPainter en vez de traer Qt Charts: son dos
# rectangulos y una leyenda, y una dependencia mas seria mas codigo, no menos.

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)

qt_standard_project_setup()

set(DAKE_UI_SOURCES
    mainwindow.cpp
    mainwindow.hpp
    pages.hpp
    todaypage.cpp
    repairspage.cpp
    repairdialogs.cpp
    quotedialog.cpp
    quotedialog.hpp
    repairdialogs.hpp
    fields.hpp
    movementspage.cpp
    pocketspage.cpp
    reportspage.cpp
    reportsummary.cpp
    pendinglist.cpp
    pendinglist.hpp
    settingspage.cpp
    entryform.cpp
    entryform.hpp
    capturewindow.cpp
    capturewindow.hpp
    globalhotkey.cpp
    globalhotkey.hpp
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

# Las pantallas van en una biblioteca y no compiladas tres veces: la usan la
# aplicacion, el banco visual y la prueba de la interfaz (tests/uitest.cpp).
qt_add_library(dake_ui STATIC ${DAKE_UI_SOURCES})
target_include_directories(dake_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dake_ui PUBLIC dake::core dake::storage dake_sync Qt6::Widgets Qt6::Network)
target_compile_definitions(dake_ui PUBLIC DAKE_APP_NAME="${DAKE_APP_NAME}")

# Inter, Anton y el logo van dentro del ejecutable: la app tiene que verse igual
# en cualquier computadora, tenga o no esas fuentes instaladas.
qt_add_resources(dake_ui "dake_recursos"
    PREFIX "/dake"
    BASE recursos
    FILES
        recursos/fuentes/Inter-Regular.ttf
        recursos/fuentes/Inter-SemiBold.ttf
        recursos/fuentes/Inter-Bold.ttf
        recursos/fuentes/Inter-ExtraBold.ttf
        recursos/fuentes/Anton-Regular.ttf
        recursos/logo/DAke.png
)

qt_add_executable(dake_pruebas WIN32
    main.cpp
)
target_link_libraries(dake_pruebas PRIVATE dake_ui)

# --- Banco de pruebas visual ---
# Renderiza las pantallas a PNG con el complemento offscreen. Deliberadamente
# NO abre una ventana: verificar la interfaz con capturas del escritorio exige
# robar el foco y hacer clics a ciegas, y cualquier ventana que se ponga encima
# los desvia.
qt_add_executable(dake_uipreview
    preview.cpp
)
target_link_libraries(dake_uipreview PRIVATE dake_ui)

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
