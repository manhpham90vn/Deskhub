add_executable(deskhub_daemon
    main.mm
    Daemon.mm
    UnattendedHost.mm
    PeerTrust.mm
    LoginWindowAgent.mm
)

set_target_properties(deskhub_daemon PROPERTIES OUTPUT_NAME "DeskhubDaemon")
target_link_libraries(deskhub_daemon PRIVATE deskhub_mac_core
    "-framework Security"
    "-framework IOSurface"
)
target_compile_options(deskhub_daemon PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
deskhub_target_warnings(deskhub_daemon)
deskhub_target_hardening(deskhub_daemon)
