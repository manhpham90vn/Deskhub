cmake_minimum_required(VERSION 3.22)

if(NOT COMMAND deskhub_target_warnings)
    include(${CMAKE_CURRENT_LIST_DIR}/../cmake/DeskhubWarnings.cmake)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/DeskhubQuiche.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/DeskhubOpus.cmake)

option(DESKHUB_QUIC "Build the QUIC transport (requires third_party/quiche)" ON)
if(DESKHUB_QUIC AND NOT DESKHUB_QUICHE_FOUND)
    message(FATAL_ERROR
        "Deskhub: third_party/quiche/${DESKHUB_QUICHE_TARGET} is missing. A build without "
        "it cannot share or connect, so it fails here instead of shipping broken. Run "
        "scripts/build-quiche.sh (make bootstrap does), or pass -DDESKHUB_QUIC=OFF to "
        "build the offline stub on purpose.")
endif()
if(DESKHUB_QUIC)
    set(deskhub_platform_quic
        src/net/QuicEndpoint.cpp
        src/system/HostIdentity.cpp
        src/system/AuthProof.cpp
    )
else()
    message(STATUS
        "Deskhub: DESKHUB_QUIC=OFF — building the stub transport; this binary cannot "
        "share or connect.")
    set(deskhub_platform_quic
        src/net/QuicEndpointNone.cpp
        src/system/HostIdentityNone.cpp
        src/system/AuthProofNone.cpp
    )
endif()

option(DESKHUB_AUDIO "Build audio streaming (requires third_party/opus)" ON)
if(DESKHUB_AUDIO AND NOT DESKHUB_OPUS_FOUND)
    message(FATAL_ERROR
        "Deskhub: third_party/opus/${DESKHUB_OPUS_TARGET} is missing. A build without it "
        "shares a screen with no sound, so it fails here instead of shipping a surprise. "
        "Run scripts/build-opus.sh (make bootstrap does), or pass -DDESKHUB_AUDIO=OFF to "
        "build the silent stub on purpose.")
endif()
if(DESKHUB_AUDIO)
    set(deskhub_platform_audio src/media/OpusCodec.cpp)
else()
    message(STATUS
        "Deskhub: DESKHUB_AUDIO=OFF — building the silent stub; this binary carries no "
        "sound.")
    set(deskhub_platform_audio src/media/OpusCodecNone.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(DESKHUB_PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    endif()
endif()
if(WIN32)
    set(deskhub_platform_sink src/audio/AudioSinkWasapi.cpp)
    set(deskhub_platform_capture src/audio/AudioCaptureWasapi.cpp)
elseif(APPLE)
    set(deskhub_platform_sink src/audio/AudioSinkCoreAudio.mm)
    set(deskhub_platform_capture src/audio/AudioCaptureNone.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android" AND ANDROID_PLATFORM_LEVEL GREATER_EQUAL 26)
    set(deskhub_platform_sink src/audio/AudioSinkAAudio.cpp)
    set(deskhub_platform_sink_libs aaudio)
    set(deskhub_platform_capture src/audio/AudioCaptureNone.cpp)
elseif(DESKHUB_PIPEWIRE_FOUND)
    set(deskhub_platform_sink src/audio/AudioSinkPipewire.cpp)
    set(deskhub_platform_capture src/audio/AudioCapturePipewire.cpp)
else()
    set(deskhub_platform_sink src/audio/AudioSinkNone.cpp)
    set(deskhub_platform_capture src/audio/AudioCaptureNone.cpp)
endif()

if(WIN32)
    set(deskhub_platform_pty src/system/PtyWin.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
       (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"))
    set(deskhub_platform_pty src/system/PtyPosix.cpp)
else()
    set(deskhub_platform_pty src/system/PtyNone.cpp)
endif()

if(WIN32)
    set(deskhub_platform_os
        src/net/UdpSocketWin.cpp
        src/net/NetInfoWin.cpp
        src/input/LocalInputWin.cpp
        src/input/NativeKeyMapWin.cpp
        src/media/DisplayEnumWin.cpp
        src/system/MemoryFootprintWin.cpp
        src/system/DeviceNameWin.cpp
        src/system/AutostartWin.cpp
        src/system/KeepAwakeWin.cpp
        src/system/ConsoleWin.cpp
        src/system/EnvironmentWin.cpp
    )
else()
    set(deskhub_platform_os
        src/net/UdpSocketPosix.cpp
        src/net/NetInfoPosix.cpp
        src/system/DeviceNamePosix.cpp
        src/system/ConsolePosix.cpp
        src/system/EnvironmentPosix.cpp
    )
    if(APPLE)
        list(APPEND deskhub_platform_os src/system/MemoryFootprintApple.cpp)
    else()
        list(APPEND deskhub_platform_os src/system/MemoryFootprintPosix.cpp)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND deskhub_platform_os src/input/LocalInputLinux.cpp)
    elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND deskhub_platform_os src/input/LocalInputMac.mm)
    else()
        list(APPEND deskhub_platform_os src/input/LocalInputNone.cpp)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND deskhub_platform_os src/system/AutostartLinux.cpp)
    elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND deskhub_platform_os src/system/AutostartMac.mm)
    else()
        list(APPEND deskhub_platform_os src/system/AutostartNone.cpp)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND deskhub_platform_os src/input/NativeKeyMapLinux.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND deskhub_platform_os src/input/NativeKeyMapIos.cpp)
    elseif(APPLE)
        list(APPEND deskhub_platform_os src/input/NativeKeyMapMac.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
        list(APPEND deskhub_platform_os src/input/NativeKeyMapAndroid.cpp)
    else()
        list(APPEND deskhub_platform_os src/input/NativeKeyMapNone.cpp)
    endif()

    if(APPLE)
        list(APPEND deskhub_platform_os
            src/media/VtDecoderApple.mm
            src/media/VtEncoderApple.mm
            src/ffi/ScreenFfiApple.mm
        )
    endif()

    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND deskhub_platform_os src/media/DisplayEnumMac.mm)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
        list(APPEND deskhub_platform_os src/media/DisplayEnumMobile.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(DESKHUB_PORTAL IMPORTED_TARGET gio-2.0 gio-unix-2.0)
        endif()
        if(DESKHUB_PORTAL_FOUND)
            list(APPEND deskhub_platform_os
                src/media/PortalScreenCast.cpp
                src/media/DisplayEnumLinux.cpp
            )
        else()
            message(STATUS
                "Deskhub: platform falls back to a stub display enumerator — "
                "gio-2.0 development packages are missing. Run 'make bootstrap'.")
            list(APPEND deskhub_platform_os src/media/DisplayEnumNone.cpp)
        endif()
    else()
        list(APPEND deskhub_platform_os src/media/DisplayEnumNone.cpp)
    endif()

    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND deskhub_platform_os src/system/KeepAwakeMac.cpp)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND DESKHUB_PORTAL_FOUND)
        list(APPEND deskhub_platform_os src/system/KeepAwakeLinux.cpp)
    else()
        list(APPEND deskhub_platform_os src/system/KeepAwakeNone.cpp)
    endif()
endif()

add_library(platform STATIC
    ${deskhub_platform_os}
    ${deskhub_platform_quic}
    ${deskhub_platform_pty}
    ${deskhub_platform_audio}
    ${deskhub_platform_sink}
    ${deskhub_platform_capture}
    src/audio/AudioPlayer.cpp
    src/audio/AudioBroadcaster.cpp
    src/system/TrustStoreFile.cpp
    src/system/PairedDevicesFile.cpp
    src/auth/AuthNegotiation.cpp
    src/client/HostLink.cpp
    src/host/TerminalHost.cpp
    src/client/TerminalViewer.cpp
    src/host/FileHost.cpp
    src/client/FileTransferClient.cpp
    src/client/FileUpload.cpp
    src/system/FileStore.cpp
    src/net/SessionTransport.cpp
    src/client/SourceQuery.cpp
    src/client/HostProbe.cpp
    src/client/DeviceStatusPoller.cpp
    src/client/LanScanner.cpp
    src/client/ScreenViewerLoop.cpp
    src/host/HostNetLoop.cpp
    src/host/ViewerBroadcast.cpp
    src/host/HostEngine.cpp
    src/ffi/ClientFfi.cpp
    src/ffi/ShareFfi.cpp
    src/ffi/DiscoveryFfi.cpp
    src/ffi/SendFfi.cpp
    src/ffi/TerminalFfi.cpp
)
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(platform PUBLIC cxx_std_20)
target_link_libraries(platform PUBLIC core)

if(DESKHUB_QUIC)
    target_link_libraries(platform PRIVATE deskhub::quiche)
endif()

if(DESKHUB_AUDIO)
    target_link_libraries(platform PRIVATE deskhub::opus)
endif()

if(DESKHUB_PIPEWIRE_FOUND)
    target_link_libraries(platform PRIVATE PkgConfig::DESKHUB_PIPEWIRE)
endif()

if(APPLE)
    set(deskhub_bundled_libs)
    if(DESKHUB_QUIC)
        list(APPEND deskhub_bundled_libs ${DESKHUB_QUICHE_LIB})
    endif()
    if(DESKHUB_AUDIO)
        list(APPEND deskhub_bundled_libs ${DESKHUB_OPUS_LIB})
    endif()
    if(deskhub_bundled_libs)
        add_custom_command(TARGET platform POST_BUILD
            COMMAND libtool -static -no_warning_for_no_symbols
                -o $<TARGET_FILE_DIR:platform>/libplatform_bundled.a
                $<TARGET_FILE:platform> ${deskhub_bundled_libs})
    else()
        add_custom_command(TARGET platform POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:platform>
                $<TARGET_FILE_DIR:platform>/libplatform_bundled.a)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(platform PUBLIC util)
endif()

if(APPLE)
    target_compile_options(platform PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)

    target_link_libraries(platform PUBLIC
        "-framework Foundation"
        "-framework AudioToolbox"
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework CoreGraphics"
        "-framework VideoToolbox"
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_link_libraries(platform PUBLIC "-framework UIKit")
    else()
        target_link_libraries(platform PUBLIC
            "-framework AppKit"
            "-framework ScreenCaptureKit"
            "-framework ServiceManagement"
            "-framework IOKit"
        )
    endif()
endif()

deskhub_target_warnings(platform)
if(WIN32)
    target_link_libraries(platform PUBLIC ws2_32 psapi ole32)
    target_compile_options(platform PRIVATE /sdl)
endif()

if(DESKHUB_PORTAL_FOUND)
    target_link_libraries(platform PRIVATE PkgConfig::DESKHUB_PORTAL)
endif()

if(ANDROID)
    target_link_libraries(platform PUBLIC log ${deskhub_platform_sink_libs})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(deskhub_platform_tests_default OFF)
else()
    set(deskhub_platform_tests_default ON)
endif()
option(DESKHUB_PLATFORM_TESTS "Build platform_tests" ${deskhub_platform_tests_default})

if(DESKHUB_PLATFORM_TESTS)
    add_executable(platform_tests
        tests/TestMain.cpp
        tests/support/TestSupport.cpp

        tests/diag/LogFileTests.cpp
        tests/ffi/DiscoveryFfiTests.cpp
        tests/ffi/FfiTextTests.cpp
        tests/ffi/TerminalFfiTests.cpp
        tests/input/LocalInputGateTests.cpp
        tests/media/AudioBroadcasterTests.cpp
        tests/media/OpusCodecTests.cpp
        tests/net/NetAddrTests.cpp
        tests/net/SourceQueryTests.cpp
        tests/net/HostProbeTests.cpp
        tests/net/LanScannerTests.cpp
        tests/net/UdpSocketTests.cpp
        tests/system/AppDataFileTests.cpp
        tests/system/HostIdentityTests.cpp
        tests/system/AuthProofTests.cpp
        tests/system/PtyTests.cpp
        tests/net/QuicEndpointTests.cpp
        tests/net/SessionTransportTests.cpp
        tests/session/SharingHostTests.cpp
        tests/session/ScreenHostCallbackTests.cpp
        tests/session/AuthNegotiationTests.cpp
        tests/session/HostLinkTests.cpp
        tests/session/TerminalHostTests.cpp
        tests/session/FileTransferTests.cpp
        tests/system/ClockTests.cpp
        tests/system/DeviceNameTests.cpp
        tests/system/MemoryFootprintTests.cpp
        tests/system/RandomTests.cpp
    )
    target_link_libraries(platform_tests PRIVATE platform)
    target_include_directories(platform_tests PRIVATE tests)
    deskhub_target_warnings(platform_tests)

    include(CTest)
    add_test(NAME platform_tests COMMAND platform_tests)
endif()

option(DESKHUB_PLATFORM_PERF "Build platform_perf" ${deskhub_platform_tests_default})

if(DESKHUB_PLATFORM_PERF)
    add_executable(platform_perf
        perf/PerfMain.cpp
        perf/QuicPerf.cpp
        perf/UdpPerf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/perf/PerfHarness.cpp
    )
    target_link_libraries(platform_perf PRIVATE platform)
    target_include_directories(platform_perf PRIVATE perf ${CMAKE_CURRENT_SOURCE_DIR}/../core/perf)
    deskhub_target_warnings(platform_perf)
    if(MSVC)
        target_compile_definitions(platform_perf PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()
