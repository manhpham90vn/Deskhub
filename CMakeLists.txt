cmake_minimum_required(VERSION 3.25)
project(Deskhub LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(DeskhubWarnings)

include(DeskhubQuiche)
include(DeskhubOpus)

option(DESKHUB_MSVC_STATIC_RUNTIME "Link the MSVC runtime statically" ON)
if(MSVC AND DESKHUB_QUICHE_FOUND)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 _HAS_ITERATOR_DEBUGGING=0)
    add_compile_options("$<$<CONFIG:Debug>:/U_DEBUG>")
    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
elseif(MSVC AND DESKHUB_MSVC_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/Zi /FS)
    add_link_options(/DEBUG /OPT:REF /OPT:ICF)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(platform)

if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(deskhub_integration_tests_default OFF)
else()
    set(deskhub_integration_tests_default ON)
endif()
option(DESKHUB_INTEGRATION_TESTS "Build integration_tests"
    ${deskhub_integration_tests_default})
if(DESKHUB_INTEGRATION_TESTS)
    add_subdirectory(tests/integration)
endif()

option(DESKHUB_LINUX_APP "Build the Ubuntu desktop app" ON)
option(DESKHUB_WINDOWS_APP "Build the Windows desktop app" ON)
option(DESKHUB_CLI "Build the command-line client" OFF)
option(DESKHUB_MACOS_DAEMON "Build the macOS daemon the app embeds for access before sign-in" OFF)

if(WIN32)
    add_subdirectory(client/windows/cpp)
    if(DESKHUB_WINDOWS_APP)
        add_subdirectory(client/windows/win32)
        add_subdirectory(client/windows/service)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(DESKHUB_LINUX_APP OR DESKHUB_CLI)
        add_subdirectory(client/linux)
    endif()
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(DESKHUB_CLI OR DESKHUB_MACOS_DAEMON)
        add_subdirectory(client/macos/app/cpp)
    endif()
    if(DESKHUB_MACOS_DAEMON)
        add_subdirectory(client/macos/daemon)
    endif()
endif()

if(DESKHUB_CLI)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
       (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"))
        add_subdirectory(client/cli)
    else()
        message(FATAL_ERROR
            "Deskhub: DESKHUB_CLI is only built for Linux, Windows and macOS - "
            "the phones have no command line to run it from.")
    endif()
endif()
