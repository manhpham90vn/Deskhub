if(WIN32)
    set(deskhub_cli_signals SignalsWin.cpp)
else()
    set(deskhub_cli_signals SignalsPosix.cpp)
endif()

add_executable(deskhub_cli
    Main.cpp
    Output.cpp
    SourcesCommands.cpp
    StoreCommands.cpp
    ShareCommand.cpp
    SendCommand.cpp
    ShellCommand.cpp
    ConnectCommand.cpp
    ${deskhub_cli_signals}
)

set_target_properties(deskhub_cli PROPERTIES OUTPUT_NAME deskhub-cli)

target_include_directories(deskhub_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(deskhub_cli PRIVATE core platform)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DESKHUB_CLI_X11 REQUIRED IMPORTED_TARGET x11 xfixes)
    target_sources(deskhub_cli PRIVATE ViewerX11.cpp)
    target_link_libraries(deskhub_cli PRIVATE deskhub_linux_core PkgConfig::DESKHUB_CLI_X11)
elseif(WIN32)
    target_sources(deskhub_cli PRIVATE ViewerWin32.cpp)
    target_link_libraries(deskhub_cli PRIVATE deskhub_win_view)
elseif(APPLE)
    target_sources(deskhub_cli PRIVATE ViewerNone.cpp)
    target_link_libraries(deskhub_cli PRIVATE deskhub_mac_core)
endif()
target_compile_features(deskhub_cli PRIVATE cxx_std_20)
deskhub_target_warnings(deskhub_cli)
deskhub_target_hardening(deskhub_cli)

find_package(Threads REQUIRED)
target_link_libraries(deskhub_cli PRIVATE Threads::Threads)
