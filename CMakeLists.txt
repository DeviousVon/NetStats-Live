cmake_minimum_required(VERSION 3.24)
project(nsl-linux VERSION 0.1.0 LANGUAGES CXX)

option(USE_LAYER_SHELL "Use layer-shell-qt for KDE Wayland always-on-top overlay support when available" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus Network)

set(NSL_SOURCES
    src/main.cpp
    src/Core.cpp
    src/Settings.cpp
    src/PaneWidget.cpp
    src/TextPane.cpp
    src/GraphPane.cpp
    src/Collector.cpp
    src/TrayIcon.cpp
    src/ClipCap.cpp
    src/MainWindow.cpp
)

add_executable(nsl-linux ${NSL_SOURCES})
target_include_directories(nsl-linux PRIVATE src)
target_link_libraries(nsl-linux PRIVATE Qt6::Widgets Qt6::DBus Qt6::Network)
target_compile_options(nsl-linux PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl-linux PRIVATE QT_NO_KEYWORDS)

if(USE_LAYER_SHELL)
    find_package(LayerShellQt QUIET)
    if(LayerShellQt_FOUND)
        target_link_libraries(nsl-linux PRIVATE LayerShellQt::Interface)
        target_compile_definitions(nsl-linux PRIVATE NSL_HAS_LAYER_SHELL=1)
        message(STATUS "layer-shell-qt found: KDE Wayland overlay support enabled")
    else()
        message(STATUS "layer-shell-qt development package not found: always-on-top will use Qt hint/KWin rule fallback")
    endif()
endif()

add_executable(nsl_core_tests tests/test_core.cpp src/Core.cpp)
target_include_directories(nsl_core_tests PRIVATE src)
target_compile_options(nsl_core_tests PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME nsl_core_tests COMMAND nsl_core_tests)

include(GNUInstallDirs)
install(TARGETS nsl-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/nsl-linux.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
