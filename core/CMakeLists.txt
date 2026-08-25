cmake_minimum_required(VERSION 3.22)

file(READ ${CMAKE_CURRENT_LIST_DIR}/../VERSION DESKHUB_VERSION_TEXT)
string(STRIP "${DESKHUB_VERSION_TEXT}" DESKHUB_VERSION_TEXT)
project(DeskhubCore LANGUAGES CXX VERSION ${DESKHUB_VERSION_TEXT})

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/../VERSION)

if(NOT COMMAND deskhub_target_warnings)
    include(${CMAKE_CURRENT_LIST_DIR}/../cmake/DeskhubWarnings.cmake)
endif()

add_library(core STATIC
    src/protocol/Wire.cpp
    src/protocol/RecordStream.cpp

    src/transport/Packetizer.cpp
    src/transport/Reassembler.cpp
    src/transport/AudioJitterBuffer.cpp
    src/transport/RetransmitCache.cpp

    src/session/host/ScreenHostSession.cpp
    src/session/host/ViewerTable.cpp
    src/session/client/ScreenClientSession.cpp
    src/session/client/ScreenClient.cpp
    src/session/host/ViewerFeedback.cpp
    src/session/host/SourcePipeline.cpp
    src/session/ClipboardSync.cpp
    src/session/TerminalSession.cpp
    src/session/client/TerminalClient.cpp
    src/session/FileTransfer.cpp
    src/session/client/FileSender.cpp
    src/session/host/FileReceiver.cpp

    src/transfer/Crc32.cpp
    src/transfer/SafeName.cpp
    src/session/LinkRecovery.cpp
    src/session/LinkPulse.cpp

    src/input/InputSender.cpp
    src/input/InputReceiver.cpp
    src/input/ClientInputQueue.cpp
    src/input/TrackpadCursor.cpp

    src/control/BitrateController.cpp
    src/control/LinkStats.cpp
    src/control/StreamSize.cpp
    src/control/FrameGate.cpp
    src/control/ClockOffset.cpp
    src/control/VideoPacer.cpp
    src/control/QualityLadder.cpp

    src/media/ViewFit.cpp
    src/media/H264Sps.cpp
    src/media/AnnexB.cpp
    src/media/RgbDownscale.cpp
    src/media/RatePlan.cpp
    src/media/H264Encode.cpp

    src/diag/ScreenClientDiag.cpp
    src/diag/ShareDiag.cpp

    src/session/host/Beacon.cpp

    src/net/LanScan.cpp
    src/net/Ipv4.cpp
    src/net/BindAddress.cpp
    src/net/TrustStore.cpp
    src/net/PairedDevices.cpp

    src/terminal/VtParser.cpp
    src/terminal/Screen.cpp
    src/terminal/Snapshot.cpp
    src/terminal/Repaint.cpp
    src/terminal/KeyEncoder.cpp
    src/terminal/Palette.cpp

    src/ui/HostRows.cpp
    src/ui/TransferView.cpp
    src/ui/DeviceRows.cpp
    src/ui/RecentDevices.cpp
    src/ui/SecretText.cpp
    src/ui/UiSettings.cpp
    src/ui/AutostartConfig.cpp
    src/ui/AutoShareGate.cpp

    src/cli/Command.cpp
    src/cli/Json.cpp
)
target_compile_features(core PUBLIC cxx_std_20)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(core PUBLIC DESKHUB_VERSION="${PROJECT_VERSION}")

deskhub_target_warnings(core)
if(MSVC)
    target_compile_options(core PRIVATE /sdl)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(deskhub_tests_default OFF)
else()
    set(deskhub_tests_default ON)
endif()
option(DESKHUB_CORE_TESTS "Build core_tests" ${deskhub_tests_default})

if(DESKHUB_CORE_TESTS)
    add_executable(core_tests
        tests/TestMain.cpp
        tests/support/TestSupport.cpp

        tests/protocol/ByteOrderTests.cpp
        tests/protocol/WireTests.cpp

        tests/transport/ReassemblerTests.cpp
        tests/transport/AudioJitterBufferTests.cpp
        tests/transport/FecTests.cpp
        tests/transport/RetransmitCacheTests.cpp
        tests/transport/PacerTests.cpp
        tests/transport/LossGoodputTests.cpp

        tests/session/ScreenSessionTests.cpp
        tests/session/ScreenClientTests.cpp
        tests/session/ClipboardSyncTests.cpp
        tests/session/ViewerFeedbackTests.cpp
        tests/session/SourcePipelineTests.cpp
        tests/session/ViewerTableTests.cpp
        tests/session/ConnectFlowTests.cpp
        tests/session/ShareFlowTests.cpp
        tests/session/OpenViewersTests.cpp
        tests/session/SourcePipelineStateTests.cpp
        tests/session/TerminalSessionTests.cpp
        tests/session/FileTransferTests.cpp
        tests/session/LinkRecoveryTests.cpp
        tests/session/LinkPulseTests.cpp
        tests/session/AuthThrottleTests.cpp

        tests/transfer/Crc32Tests.cpp
        tests/transfer/SafeNameTests.cpp

        tests/input/InputTests.cpp
        tests/input/PressedInputTests.cpp
        tests/input/ClientInputQueueTests.cpp
        tests/input/ScancodeTableTests.cpp
        tests/input/Set1ScancodeTests.cpp
        tests/input/HotkeysTests.cpp
        tests/input/PointerMapTests.cpp
        tests/input/PointerLockStateTests.cpp
        tests/input/TrackpadCursorTests.cpp

        tests/control/ControlTests.cpp
        tests/control/StreamSizeTests.cpp
        tests/control/FrameGateTests.cpp
        tests/control/ClockOffsetTests.cpp
        tests/control/VideoPacerTests.cpp
        tests/control/QualityLadderTests.cpp

        tests/diag/DiagTests.cpp

        tests/media/ContractTests.cpp
        tests/media/BitWriterTests.cpp
        tests/media/H264SpsTests.cpp
        tests/media/AnnexBTests.cpp
        tests/media/RatePlanTests.cpp
        tests/media/H264EncodeTests.cpp
        tests/media/QualityPresetTests.cpp
        tests/media/ViewFitTests.cpp
        tests/media/SourceLabelTests.cpp
        tests/media/ViewerTitleTests.cpp
        tests/media/FrameMailboxTests.cpp
        tests/media/RgbDownscaleTests.cpp
        tests/media/PcmRingTests.cpp

        tests/session/BeaconTests.cpp

        tests/net/LanScanTests.cpp
        tests/net/Ipv4Tests.cpp
        tests/net/BindAddressTests.cpp
        tests/net/TrustStoreTests.cpp
        tests/net/PairedDevicesTests.cpp

        tests/terminal/VtParserTests.cpp
        tests/terminal/ScreenTests.cpp
        tests/terminal/SnapshotTests.cpp
        tests/terminal/RepaintTests.cpp
        tests/terminal/KeyEncoderTests.cpp
        tests/terminal/PaletteTests.cpp
        tests/terminal/ScrollAnchorTests.cpp

        tests/ui/StringsTests.cpp
        tests/ui/HostRowsTests.cpp
        tests/ui/TransferViewTests.cpp
        tests/ui/DeviceRowsTests.cpp
        tests/ui/RecentDevicesTests.cpp
        tests/ui/SecretTextTests.cpp
        tests/ui/UiSettingsTests.cpp
        tests/ui/AutostartConfigTests.cpp
        tests/ui/AutoShareGateTests.cpp

        tests/cli/CommandTests.cpp
        tests/cli/JsonTests.cpp

        tests/fuzz/FuzzTests.cpp
    )
    target_link_libraries(core_tests PRIVATE core)
    target_include_directories(core_tests PRIVATE tests)
    deskhub_target_warnings(core_tests)

    include(CTest)
    add_test(NAME core_tests COMMAND core_tests)
endif()

option(DESKHUB_CORE_PERF "Build core_perf" ${deskhub_tests_default})

if(DESKHUB_CORE_PERF)
    add_executable(core_perf
        perf/PerfMain.cpp
        perf/PerfHarness.cpp

        perf/VideoPerf.cpp
        perf/TransferPerf.cpp
        perf/TerminalPerf.cpp
        perf/ControlPerf.cpp
        perf/AudioPerf.cpp
        perf/InputPerf.cpp
        perf/StreamPerf.cpp
    )
    target_link_libraries(core_perf PRIVATE core)
    target_include_directories(core_perf PRIVATE perf)
    deskhub_target_warnings(core_perf)
    if(MSVC)
        target_compile_definitions(core_perf PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

option(DESKHUB_FUZZ "Build libFuzzer fuzz targets (clang only)" OFF)

if(DESKHUB_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "DESKHUB_FUZZ requires clang (libFuzzer)")
    endif()

    set(DESKHUB_FUZZER_LINK -fsanitize=fuzzer)
    if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        file(GLOB DESKHUB_FUZZER_RUNTIMES
            /opt/homebrew/opt/llvm/lib/clang/*/lib/darwin/libclang_rt.fuzzer_osx.a
            /usr/local/opt/llvm/lib/clang/*/lib/darwin/libclang_rt.fuzzer_osx.a)
        list(SORT DESKHUB_FUZZER_RUNTIMES COMPARE NATURAL)
        list(POP_BACK DESKHUB_FUZZER_RUNTIMES DESKHUB_FUZZER_LINK)
        if(NOT DESKHUB_FUZZER_LINK)
            message(FATAL_ERROR
                "libFuzzer runtime not found - Apple clang does not ship one.\n"
                "  Install Homebrew LLVM: brew install llvm   (or: make bootstrap)")
        endif()
    endif()

    function(deskhub_fuzz_target name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE core)
        target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
        target_link_options(${name} PRIVATE ${DESKHUB_FUZZER_LINK})
        deskhub_target_warnings(${name})
    endfunction()
    deskhub_fuzz_target(fuzz_wire fuzz/FuzzWire.cpp)
    deskhub_fuzz_target(fuzz_annexb fuzz/FuzzAnnexB.cpp)
    deskhub_fuzz_target(fuzz_h264sps fuzz/FuzzH264Sps.cpp)
    deskhub_fuzz_target(fuzz_reassembler fuzz/FuzzReassembler.cpp)
    deskhub_fuzz_target(fuzz_session fuzz/FuzzSession.cpp)
    deskhub_fuzz_target(fuzz_uitext fuzz/FuzzUiText.cpp)
    deskhub_fuzz_target(fuzz_term fuzz/FuzzTerm.cpp)
endif()
