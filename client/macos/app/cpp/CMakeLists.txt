add_library(deskhub_mac_core STATIC
    SharingHost.cpp
    Permissions.mm
    capture/ScreenCapture.mm
    input/InputInjector.mm
)

target_include_directories(deskhub_mac_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(deskhub_mac_core PUBLIC core platform)
target_link_libraries(deskhub_mac_core PUBLIC
    "-framework AppKit"
    "-framework ApplicationServices"
    "-framework CoreGraphics"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework ScreenCaptureKit"
)
target_compile_options(deskhub_mac_core PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_compile_features(deskhub_mac_core PUBLIC cxx_std_20)
deskhub_target_warnings(deskhub_mac_core)
