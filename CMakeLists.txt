cmake_minimum_required(VERSION 3.22)
project(netstats-live VERSION 0.2.0 DESCRIPTION "Live network throughput and CPU monitor widget for Linux" LANGUAGES CXX)

option(USE_LAYER_SHELL "Use layer-shell-qt for KDE Wayland always-on-top overlay support when available" ON)
option(USE_KWINDOWSYSTEM "Use KF6 WindowSystem for reliable KDE Wayland tray restoration when available" ${USE_LAYER_SHELL})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus Network)

set(NSL_KWINDOWSYSTEM_FOUND OFF)
if(USE_KWINDOWSYSTEM)
    find_package(KF6WindowSystem QUIET)
    if(TARGET KF6::WindowSystem)
        set(NSL_KWINDOWSYSTEM_FOUND ON)
        message(STATUS "KF6 WindowSystem found: native KDE Wayland tray restoration enabled")
    else()
        message(STATUS "KF6 WindowSystem development package not found: tray restore will use the generic remap fallback")
    endif()
endif()

set(NSL_APP_SOURCES
    src/Core.cpp
    src/Lifecycle.cpp
    src/Settings.cpp
    src/PaneWidget.cpp
    src/TextPane.cpp
    src/GraphPane.cpp
    src/Collector.cpp
    src/TrayIconVisual.cpp
    src/TraySimulation.cpp
    src/DBusMenu.cpp
    src/StatusNotifierItem.cpp
    src/TrayIcon.cpp
    src/ClipCap.cpp
    src/MainWindow.cpp
)

add_executable(netstats-live src/main.cpp ${NSL_APP_SOURCES})
target_include_directories(netstats-live PRIVATE src)
target_link_libraries(netstats-live PRIVATE Qt6::Widgets Qt6::DBus Qt6::Network)
target_compile_options(netstats-live PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(netstats-live PRIVATE QT_NO_KEYWORDS)

if(NSL_KWINDOWSYSTEM_FOUND)
    target_link_libraries(netstats-live PRIVATE KF6::WindowSystem)
    target_compile_definitions(netstats-live PRIVATE NSL_HAS_KWINDOWSYSTEM=1)
endif()

set(NSL_LAYER_SHELL_FOUND OFF)
if(USE_LAYER_SHELL)
    find_package(LayerShellQt 6 QUIET)
    if(TARGET LayerShellQt::Interface)
        set(NSL_LAYER_SHELL_FOUND ON)
        target_link_libraries(netstats-live PRIVATE LayerShellQt::Interface)
        target_compile_definitions(netstats-live PRIVATE NSL_HAS_LAYER_SHELL=1)
        message(STATUS "Qt 6 layer-shell-qt found: KDE Wayland overlay support enabled")
    else()
        message(STATUS "Qt 6 layer-shell-qt development package not found: always-on-top will use Qt hint/KWin rule fallback")
    endif()
endif()

add_executable(nsl_core_tests tests/test_core.cpp src/Core.cpp)
target_include_directories(nsl_core_tests PRIVATE src)
target_compile_options(nsl_core_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nsl_collector_tests
    tests/test_collector.cpp
    src/Core.cpp
    src/Settings.cpp
    src/Collector.cpp
    src/TraySimulation.cpp
)
target_include_directories(nsl_collector_tests PRIVATE src)
target_link_libraries(nsl_collector_tests PRIVATE Qt6::Core Qt6::Network)
target_compile_options(nsl_collector_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_collector_tests PRIVATE QT_NO_KEYWORDS)

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

add_executable(nsl_clipcap_tests tests/test_clipcap.cpp src/ClipCap.cpp)
target_include_directories(nsl_clipcap_tests PRIVATE src)
target_link_libraries(nsl_clipcap_tests PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_options(nsl_clipcap_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_clipcap_tests PRIVATE QT_NO_KEYWORDS)

add_executable(nsl_status_notifier_tests
    tests/test_status_notifier_item.cpp
    src/StatusNotifierItem.cpp
)
target_include_directories(nsl_status_notifier_tests PRIVATE src)
target_link_libraries(nsl_status_notifier_tests PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_options(nsl_status_notifier_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_status_notifier_tests PRIVATE QT_NO_KEYWORDS)

add_executable(nsl_dbus_menu_tests
    tests/test_dbus_menu.cpp
    src/DBusMenu.cpp
)
target_include_directories(nsl_dbus_menu_tests PRIVATE src)
target_link_libraries(nsl_dbus_menu_tests PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_options(nsl_dbus_menu_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_dbus_menu_tests PRIVATE QT_NO_KEYWORDS)

add_executable(nsl_tray_backend_tests
    tests/test_tray_backend.cpp
    src/Core.cpp
    src/DBusMenu.cpp
    src/StatusNotifierItem.cpp
    src/TrayIcon.cpp
    src/TrayIconVisual.cpp
)
target_include_directories(nsl_tray_backend_tests PRIVATE src)
target_link_libraries(nsl_tray_backend_tests PRIVATE Qt6::Widgets Qt6::DBus)
target_compile_options(nsl_tray_backend_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_tray_backend_tests PRIVATE QT_NO_KEYWORDS)

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
target_link_libraries(nsl_lifecycle_tests PRIVATE Qt6::Core Qt6::DBus)
target_compile_options(nsl_lifecycle_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_lifecycle_tests PRIVATE QT_NO_KEYWORDS)
add_dependencies(nsl_lifecycle_tests netstats-live)

add_executable(nsl_main_window_tests tests/test_main_window.cpp ${NSL_APP_SOURCES})
target_include_directories(nsl_main_window_tests PRIVATE src)
target_link_libraries(nsl_main_window_tests PRIVATE Qt6::Widgets Qt6::DBus Qt6::Network)
target_compile_options(nsl_main_window_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nsl_main_window_tests PRIVATE QT_NO_KEYWORDS)
if(NSL_KWINDOWSYSTEM_FOUND)
    target_link_libraries(nsl_main_window_tests PRIVATE KF6::WindowSystem)
    target_compile_definitions(nsl_main_window_tests PRIVATE NSL_HAS_KWINDOWSYSTEM=1)
endif()
if(NSL_LAYER_SHELL_FOUND)
    target_link_libraries(nsl_main_window_tests PRIVATE LayerShellQt::Interface)
    target_compile_definitions(nsl_main_window_tests PRIVATE NSL_HAS_LAYER_SHELL=1)
endif()

enable_testing()
add_test(NAME nsl_core_tests COMMAND nsl_core_tests)
add_test(NAME nsl_collector_tests COMMAND nsl_collector_tests)
add_test(NAME nsl_tray_tests COMMAND nsl_tray_tests)
add_test(NAME nsl_visual_theme_tests COMMAND nsl_visual_theme_tests)
add_test(NAME nsl_lifecycle_tests COMMAND nsl_lifecycle_tests)
add_test(NAME nsl_migration_logging_tests COMMAND nsl_lifecycle_tests --migration-copy-failure-logging-check)
add_test(NAME nsl_autostart_symlink_migration_tests COMMAND nsl_lifecycle_tests --autostart-symlink-migration-check)
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    add_test(NAME nsl_status_notifier_tests
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:nsl_status_notifier_tests>)
    set_tests_properties(nsl_status_notifier_tests PROPERTIES TIMEOUT 30)
    add_test(NAME nsl_dbus_menu_tests
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:nsl_dbus_menu_tests>)
    set_tests_properties(nsl_dbus_menu_tests PROPERTIES TIMEOUT 30)
    add_test(NAME nsl_tray_backend_tests
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:nsl_tray_backend_tests>)
    set_tests_properties(nsl_tray_backend_tests PROPERTIES TIMEOUT 30)
    add_test(NAME nsl_dbus_failure_tests
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:nsl_lifecycle_tests> --dbus-failure-semantics-check)
    set_tests_properties(nsl_dbus_failure_tests PROPERTIES TIMEOUT 30)
    add_test(NAME nsl_clipcap_tests
        COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:nsl_clipcap_tests>)
    set_tests_properties(nsl_clipcap_tests PROPERTIES TIMEOUT 30)
else()
    message(STATUS "dbus-run-session not found: isolated DBus lifecycle and ClipCap tests disabled")
endif()
add_test(NAME nsl_main_window_tests COMMAND nsl_main_window_tests)
set_tests_properties(nsl_tray_tests nsl_visual_theme_tests nsl_main_window_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(nsl_collector_tests PROPERTIES TIMEOUT 15)

include(GNUInstallDirs)
find_program(GZIP_EXECUTABLE gzip REQUIRED)
set(NSL_DEBIAN_COPYRIGHT "${CMAKE_CURRENT_BINARY_DIR}/copyright")
file(WRITE "${NSL_DEBIAN_COPYRIGHT}"
    "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/\n"
    "Upstream-Name: NetStats-Live\n"
    "Source: https://github.com/DeviousVon/NetStats-Live\n\n"
    "Files: *\n"
    "Copyright: 2026 DeviousVon\n"
    "License: GPL-3+\n"
    " This program is free software: you can redistribute it and/or modify it\n"
    " under the terms of the GNU General Public License as published by the\n"
    " Free Software Foundation, either version 3 of the License, or any later version.\n"
    " .\n"
    " On Debian systems, the complete license text is available at\n"
    " /usr/share/common-licenses/GPL-3.\n")
set(NSL_NEWS_GZ "${CMAKE_CURRENT_BINARY_DIR}/NEWS.gz")
execute_process(COMMAND "${GZIP_EXECUTABLE}" -9 -n -c
    INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
    OUTPUT_FILE "${NSL_NEWS_GZ}"
    COMMAND_ERROR_IS_FATAL ANY)
set(NSL_DEBIAN_CHANGELOG "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian")
file(WRITE "${NSL_DEBIAN_CHANGELOG}"
    "netstats-live (0.2.0) UNRELEASED; urgency=medium\n\n"
    "  * Prepare the private production release candidate.\n"
    "  * See NEWS.gz for the complete upstream changelog.\n\n"
    " -- DeviousVon <DeviousVon@users.noreply.github.com>  Mon, 24 Aug 2026 00:00:00 +0000\n")
set(NSL_DEBIAN_CHANGELOG_GZ "${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz")
execute_process(COMMAND "${GZIP_EXECUTABLE}" -9 -n -c
    INPUT_FILE "${NSL_DEBIAN_CHANGELOG}"
    OUTPUT_FILE "${NSL_DEBIAN_CHANGELOG_GZ}"
    COMMAND_ERROR_IS_FATAL ANY)
set(NSL_MANPAGE "${CMAKE_CURRENT_BINARY_DIR}/netstats-live.1")
file(WRITE "${NSL_MANPAGE}"
    ".TH NETSTATS-LIVE 1 \"2026-08-24\" \"NetStats-Live 0.2.0\" \"User Commands\"\n"
    ".SH NAME\nnetstats-live \\- compact live network throughput and CPU monitor\n"
    ".SH SYNOPSIS\n.B netstats-live\n.RI [ options ]\n"
    ".SH DESCRIPTION\nNetStats-Live is a Qt 6 desktop monitor for Linux network traffic, transfer totals, remote reachability, thread count, and CPU activity.\n"
    ".SH OPTIONS\nRun \\fBnetstats-live --help\\fR for the current command-line options.\n"
    ".SH FILES\n\\fI~/.config/netstats-live/netstats-live.conf\\fR stores per-user settings and totals.\n"
    ".SH SECURITY\nReport vulnerabilities through the private process documented in SECURITY.md at the project homepage.\n"
    ".SH HOMEPAGE\nhttps://github.com/DeviousVon/NetStats-Live\n")
set(NSL_MANPAGE_GZ "${CMAKE_CURRENT_BINARY_DIR}/netstats-live.1.gz")
execute_process(COMMAND "${GZIP_EXECUTABLE}" -9 -n -c
    INPUT_FILE "${NSL_MANPAGE}"
    OUTPUT_FILE "${NSL_MANPAGE_GZ}"
    COMMAND_ERROR_IS_FATAL ANY)
install(TARGETS netstats-live RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/netstats-live.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES config/netstats-live.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    RENAME io.github.deviousvon.netstats-live.metainfo.xml)
install(FILES assets/icons/hicolor/64x64/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps)
install(FILES assets/icons/hicolor/128x128/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps)
install(FILES assets/icons/hicolor/256x256/apps/netstats-live.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES "${NSL_DEBIAN_COPYRIGHT}" DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/netstats-live RENAME copyright)
install(FILES "${NSL_NEWS_GZ}" DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/netstats-live RENAME NEWS.gz)
install(FILES "${NSL_DEBIAN_CHANGELOG_GZ}" DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/netstats-live RENAME changelog.gz)
install(FILES "${NSL_MANPAGE_GZ}" DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 RENAME netstats-live.1.gz)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "netstats-live")
set(CPACK_PACKAGE_VENDOR "NetStats-Live")
set(CPACK_PACKAGE_CONTACT "DeviousVon <DeviousVon@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Live network throughput and CPU monitor widget for Linux")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Compact Qt 6 desktop network monitor for Linux\n NetStats-Live displays live traffic, transfer totals, remote reachability,\n thread count, and CPU activity in an always-available vertical panel.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/DeviousVon/NetStats-Live")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_DESCRIPTION
    "Compact Qt 6 desktop network monitor for Linux. NetStats-Live displays live traffic, transfer totals, remote reachability, thread count, and CPU activity in an always-available vertical panel.")
set(CPACK_RPM_SPEC_MORE_DEFINE
    "%define _buildhost reproducible.netstats-live\n%define use_source_date_epoch_as_buildtime 1\n%define clamp_mtime_to_source_date_epoch 1")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_BREAKS "nsl-linux (<< 0.2.0)")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "nsl-linux (<< 0.2.0)")
set(CPACK_DEBIAN_PACKAGE_REPLACES "nsl-linux (<< 0.2.0)")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/outputs/final")
set(NSL_PACKAGE_FLAVOR "generic")
if(NSL_LAYER_SHELL_FOUND OR NSL_KWINDOWSYSTEM_FOUND)
    set(NSL_PACKAGE_FLAVOR "kde")
endif()
set(NSL_LAYER_SHELL_JSON false)
if(NSL_LAYER_SHELL_FOUND)
    set(NSL_LAYER_SHELL_JSON true)
endif()
set(NSL_KWINDOWSYSTEM_JSON false)
if(NSL_KWINDOWSYSTEM_FOUND)
    set(NSL_KWINDOWSYSTEM_JSON true)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/nsl-build-features.json" CONTENT
    "{\n  \"package_flavor\": \"${NSL_PACKAGE_FLAVOR}\",\n  \"layer_shell_qt6\": ${NSL_LAYER_SHELL_JSON},\n  \"kf6_window_system\": ${NSL_KWINDOWSYSTEM_JSON}\n}\n")
execute_process(COMMAND dpkg --print-architecture
    OUTPUT_VARIABLE NSL_DEB_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT NSL_DEB_ARCH)
    set(NSL_DEB_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "NetStats-Live_${PROJECT_VERSION}_${NSL_PACKAGE_FLAVOR}_${NSL_DEB_ARCH}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "iputils-ping, traceroute")
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
include(CPack)
