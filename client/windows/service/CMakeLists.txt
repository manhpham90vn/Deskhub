add_executable(deskhub_service
    main.cpp
    ServiceControl.cpp
    ServiceBroker.cpp
    MachineFolder.cpp
    UnattendedHost.cpp
    Agent.cpp
    DuplicationCapture.cpp
    PointerCompositor.cpp
)
target_link_libraries(deskhub_service PRIVATE deskhub_win_core deskhub_win_secure platform d3d11 dxgi advapi32 user32 wtsapi32)
set_target_properties(deskhub_service PROPERTIES
    OUTPUT_NAME "DeskhubService"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/client/windows/win32")
deskhub_target_warnings(deskhub_service)
deskhub_target_hardening(deskhub_service)
if(MSVC)
    target_compile_options(deskhub_service PRIVATE /utf-8)
    set_property(TARGET deskhub_service APPEND_STRING PROPERTY LINK_FLAGS
        " /MANIFESTUAC:\"level='asInvoker' uiAccess='false'\"")
endif()
