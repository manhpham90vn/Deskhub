add_executable(deskhub_daemon
    main.cpp
    Daemon.cpp
    HostSupervisor.cpp
    PeerRights.cpp
    UnattendedHost.cpp
)

set_target_properties(deskhub_daemon PROPERTIES OUTPUT_NAME deskhubd)
target_link_libraries(deskhub_daemon PRIVATE deskhub_linux_core)
target_compile_features(deskhub_daemon PRIVATE cxx_std_20)
deskhub_target_warnings(deskhub_daemon)
deskhub_target_hardening(deskhub_daemon)
