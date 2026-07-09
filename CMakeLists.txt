cmake_minimum_required(VERSION 3.24)
project(netstats-live VERSION 0.1.0 DESCRIPTION "Live network throughput and CPU monitor widget for Linux" LANGUAGES CXX)

option(USE_LAYER_SHELL "Use layer-shell-qt for KDE Wayland always-on-top overlay support when available" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus Network)

set(NSL_SOURCES
    src/main.cpp
    src/Core.cpp
    src/Lifecycle.cpp
    src/Settings.cpp
    src/PaneWidget.cpp
    src/TextPane.cpp
    src/GraphPane.cpp
    src/Collector.cpp
    src/TrayIconVisual.cpp
    src/TraySimulation.cpp
    src/TrayIcon.cpp
    src/ClipCap.cpp
    src/MainWindow.cpp
)

add_executable(netstats-live ${NSL_SOURCES})
target_include_directories(netstats-live PRIVATE src)
target_link_libraries(netstats-live PRIVATE Qt6::Widgets Qt6::DBus Qt6::Network)
target_compile_options(netstats-live PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(netstats-live PRIVATE QT_NO_KEYWORDS)

if(USE_LAYER_SHELL)
    find_package(LayerShellQt QUIET)
    if(LayerShellQt_FOUND)
        target_link_libraries(netstats-live PRIVATE LayerShellQt::Interface)
        target_compile_definitions(netstats-live PRIVATE NSL_HAS_LAYER_SHELL=1)
        message(STATUS "layer-shell-qt found: KDE Wayland overlay support enabled")
    else()
        message(STATUS "layer-shell-qt development package not found: always-on-top will use Qt hint/KWin rule fallback")
    endif()
endif()

add_executable(nsl_core_tests tests/test_core.cpp src/Core.cpp)
target_include_directories(nsl_core_tests PRIVATE src)
target_compile_options(nsl_core_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nsl_tray_tests
    tests/test_tray_icon.cpp
    src/Core.cpp
    src/TrayIconVisual.cpp
    src/TraySimulation.cpp
)
target_include_directories(nsl_tray_tests PRIVATE src)
target_link_libraries(nsl_tray_tests PRIVATE Qt6::Widgets)
target_compile_options(nsl_tray_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_tray_tests PRIVATE QT_NO_KEYWORDS)

add_executable(nsl_visual_theme_tests
    tests/test_visual_theme.cpp
    src/Core.cpp
    src/PaneWidget.cpp
    src/GraphPane.cpp
)
target_include_directories(nsl_visual_theme_tests PRIVATE src)
target_link_libraries(nsl_visual_theme_tests PRIVATE Qt6::Widgets)
target_compile_options(nsl_visual_theme_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_visual_theme_tests PRIVATE QT_NO_KEYWORDS)
add_dependencies(nsl_visual_theme_tests netstats-live)

add_executable(nsl_lifecycle_tests
    tests/test_lifecycle.cpp
    src/Lifecycle.cpp
    src/Settings.cpp
)
target_include_directories(nsl_lifecycle_tests PRIVATE src)
target_link_libraries(nsl_lifecycle_tests PRIVATE Qt6::Core)
target_compile_options(nsl_lifecycle_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_lifecycle_tests PRIVATE QT_NO_KEYWORDS)
add_dependencies(nsl_lifecycle_tests netstats-live)

enable_testing()
add_test(NAME nsl_core_tests COMMAND nsl_core_tests)
add_test(NAME nsl_tray_tests COMMAND nsl_tray_tests)
add_test(NAME nsl_visual_theme_tests COMMAND nsl_visual_theme_tests)
add_test(NAME nsl_lifecycle_tests COMMAND nsl_lifecycle_tests)
set_tests_properties(nsl_tray_tests nsl_visual_theme_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

include(GNUInstallDirs)
install(TARGETS netstats-live RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/netstats-live.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/icons/hicolor/64x64/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps)
install(FILES assets/icons/hicolor/128x128/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps)
install(FILES assets/icons/hicolor/256x256/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "netstats-live")
set(CPACK_PACKAGE_VENDOR "NetStats-Live")
set(CPACK_PACKAGE_CONTACT "NetStats-Live maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Live network throughput and CPU monitor widget for Linux")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/DeviousVon/NetStats-Live")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_BREAKS "nsl-linux (<< 0.2.0)")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "nsl-linux (<< 0.2.0)")
set(CPACK_DEBIAN_PACKAGE_REPLACES "nsl-linux (<< 0.2.0)")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/outputs/final")
set(NSL_PACKAGE_FLAVOR "generic")
if(LayerShellQt_FOUND)
    set(NSL_PACKAGE_FLAVOR "kde")
endif()
execute_process(COMMAND dpkg --print-architecture
    OUTPUT_VARIABLE NSL_DEB_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT NSL_DEB_ARCH)
    set(NSL_DEB_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "NetStats-Live_${PROJECT_VERSION}_${NSL_PACKAGE_FLAVOR}_${NSL_DEB_ARCH}")
set(NSL_DEBIAN_DEPENDS "libc6 (>= 2.38), libstdc++6 (>= 13), libqt6core6t64 (>= 6.4) | libqt6core6 (>= 6.4), libqt6gui6 (>= 6.4), libqt6widgets6 (>= 6.4), libqt6dbus6 (>= 6.4), libqt6network6 (>= 6.4)")
if(LayerShellQt_FOUND)
    string(APPEND NSL_DEBIAN_DEPENDS ", liblayershellqtinterface6 (>= 6.4), libqt6waylandclient6 (>= 6.4)")
endif()
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${NSL_DEBIAN_DEPENDS}")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "iputils-ping, traceroute")
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
include(CPack)
