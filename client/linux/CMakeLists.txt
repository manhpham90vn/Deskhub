cmake_minimum_required(VERSION 3.22)

option(DESKHUB_REQUIRE_LINUX_APP "Fail instead of skipping when the app cannot be built" OFF)
if(DESKHUB_REQUIRE_LINUX_APP OR DESKHUB_CLI)
    set(deskhub_skip_level FATAL_ERROR)
else()
    set(deskhub_skip_level STATUS)
endif()

add_subdirectory(cpp)

if(NOT DESKHUB_LINUX_APP OR NOT TARGET deskhub_linux_core)
    return()
endif()

add_subdirectory(daemon)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DESKHUB_GTK IMPORTED_TARGET gtk+-3.0)
if(NOT DESKHUB_GTK_FOUND)
    message(${deskhub_skip_level}
        "Deskhub: skipping the Ubuntu app — libgtk-3-dev is missing. "
        "Run 'make bootstrap' to install it.")
    return()
endif()

pkg_check_modules(DESKHUB_TRAY IMPORTED_TARGET ayatana-appindicator3-0.1)
if(NOT DESKHUB_TRAY_FOUND)
    message(${deskhub_skip_level}
        "Deskhub: the Ubuntu app builds without a tray icon — "
        "libayatana-appindicator3-dev is missing. Run 'make bootstrap'.")
endif()

pkg_check_modules(DESKHUB_WAYLAND IMPORTED_TARGET gtk+-wayland-3.0 wayland-client)
if(NOT DESKHUB_WAYLAND_FOUND)
    message(${deskhub_skip_level}
        "Deskhub: the Ubuntu app builds without keyboard-shortcuts-inhibit on Wayland — "
        "the GTK Wayland backend or libwayland-dev is missing. Run 'make bootstrap'.")
endif()

add_executable(deskhub_app
    gtk/main.cpp
    gtk/FileSendDialog.cpp
    gtk/MainWindow.cpp
    gtk/TerminalWindow.cpp
    gtk/TrayIcon.cpp
    gtk/ViewerWindow.cpp
    gtk/WaylandShortcutsInhibitor.cpp
)

set_target_properties(deskhub_app PROPERTIES OUTPUT_NAME deskhub)

target_include_directories(deskhub_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(deskhub_app PRIVATE deskhub_linux_core PkgConfig::DESKHUB_GTK)
if(DESKHUB_TRAY_FOUND)
    target_compile_definitions(deskhub_app PRIVATE DESKHUB_HAVE_APPINDICATOR)
    target_link_libraries(deskhub_app PRIVATE PkgConfig::DESKHUB_TRAY)
endif()
if(DESKHUB_WAYLAND_FOUND)
    target_compile_definitions(deskhub_app PRIVATE DESKHUB_HAVE_WAYLAND_SHORTCUTS_INHIBIT)
    target_link_libraries(deskhub_app PRIVATE PkgConfig::DESKHUB_WAYLAND)
endif()
target_compile_features(deskhub_app PRIVATE cxx_std_20)
deskhub_target_warnings(deskhub_app)
deskhub_target_hardening(deskhub_app)

function(deskhub_place_fallback_icon_beside_uninstalled_binary target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/icons/deskhub-256.png
            $<TARGET_FILE_DIR:${target}>/deskhub-256.png)
endfunction()
deskhub_place_fallback_icon_beside_uninstalled_binary(deskhub_app)
