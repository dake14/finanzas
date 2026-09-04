# dake_movil — la aplicacion del telefono.
#
# Comparte `core` y `storage` con la de escritorio, sin una linea de diferencia.
# Eso no es ahorro de trabajo: es la garantia de que las dos digan lo mismo. Si
# cada pantalla calculara su propio saldo, tarde o temprano no coincidirian y no
# habria forma de saber cual de las dos miente.
#
# La interfaz es Qt Quick (QML) y no Widgets. Widgets compila en Android, pero
# produce una interfaz de escritorio metida en un telefono: objetivos de 20 px,
# sin gestos, sin integracion con el boton atras.

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Quick QuickControls2 Sql)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(dake_movil
    main.cpp
    appbridge.cpp
    appbridge.hpp
)

# Estilo.qml es un singleton, y esta marca va ANTES de qt_add_qml_module: la
# funcion lee las propiedades de los archivos cuando los incorpora, asi que
# fijarla despues no llega a tiempo y el modulo queda sin el singleton. El
# sintoma es que TODO `Estilo.algo` vale undefined y la interfaz sale sin
# colores ni medidas.
set_source_files_properties(qml/Estilo.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(dake_movil
    URI DakeMobile
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Anotar.qml
        qml/Hoy.qml
        qml/Historial.qml
        qml/Datos.qml
        qml/Nube.qml
        qml/Estilo.qml
        qml/Selector.qml
        qml/Fila.qml
)

target_include_directories(dake_movil PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dake_movil PRIVATE
    dake::core
    dake::storage
    dake_sync
    Qt6::Quick
    Qt6::QuickControls2
)

target_compile_definitions(dake_movil PRIVATE DAKE_APP_NAME="${DAKE_APP_NAME}")

if(WIN32 AND NOT ANDROID)
    # La aplicacion del telefono tambien compila para Windows, y ahi es donde se
    # la revisa antes de armar un APK. Para que arranque hace falta que
    # windeployqt copie las DLL de Qt Quick y, con --qmldir, tambien los modulos
    # QML: sin eso el ejecutable levanta y muere buscando QtQuick.Controls.
    find_program(DAKE_WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(DAKE_WINDEPLOYQT)
        add_custom_command(TARGET dake_movil POST_BUILD
            COMMAND "${DAKE_WINDEPLOYQT}" --no-translations --no-system-d3d-compiler
                    --no-compiler-runtime
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:dake_movil>"
            COMMENT "Desplegando Qt Quick junto a dake_movil")
    endif()

    # El complemento offscreen no lo copia windeployqt porque la aplicacion no
    # lo usa en su modo normal; el modo --captura si.
    add_custom_command(TARGET dake_movil POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:dake_movil>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${Qt6_DIR}/../../../plugins/platforms/qoffscreend.dll,${Qt6_DIR}/../../../plugins/platforms/qoffscreen.dll>"
                "$<TARGET_FILE_DIR:dake_movil>/platforms/"
        COMMENT "Copiando el complemento offscreen para el modo captura")
endif()

if(ANDROID)
    # Lo que ve el usuario en el cajon de aplicaciones y en la pantalla de
    # inicio. El nombre del paquete es el identificador definitivo: cambiarlo
    # despues de instalar hace que Android trate la nueva version como otra
    # aplicacion distinta y no ofrezca actualizar.
    set_target_properties(dake_movil PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.dakelabs.finanzas.pruebas"
        QT_ANDROID_APP_NAME "Finanzas DakeLabs"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
        # 24 es Android 7. Por debajo de eso Qt 6.8 no corre, y arriba de esa
        # version esta practicamente todo lo que sigue encendido.
        QT_ANDROID_MIN_SDK_VERSION 24
        # 34 y no 35 a proposito. Apuntando a 35 (Android 15) el borde a borde
        # es obligatorio: la aplicacion se dibuja POR DEBAJO de las barras del
        # sistema y la barra de navegacion de Samsung tapa las pestanas de
        # abajo. Pasado a mano en el telefono. Con 34 el sistema vuelve a
        # reservar ese espacio el solo.
        #
        # Subir esto a 35 exige antes respetar los margenes reales de Android,
        # que Qt 6.8 no expone (SafeArea llego en 6.9). Play pediria 35, pero
        # esta aplicacion se instala a mano y no se publica.
        QT_ANDROID_TARGET_SDK_VERSION 34
    )

    # La carpeta con el manifiesto y los iconos. Se genera con
    # `preparar-android.ps1` a partir del icono de la marca; si todavia no
    # existe, Qt arma uno por defecto y la aplicacion sale con el logo de Qt.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
        set_target_properties(dake_movil PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    endif()
endif()
