set(NVENC_INTERFACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/nvenc-13.0/include/ffnvcodec"
    CACHE PATH "Directory containing nvEncodeAPI.h")

if(NOT EXISTS "${NVENC_INTERFACE_DIR}/nvEncodeAPI.h")
    message(FATAL_ERROR
        "nvEncodeAPI.h not found in '${NVENC_INTERFACE_DIR}' - "
        "run 'git submodule update --init' (or 'make bootstrap') first, "
        "or point NVENC_INTERFACE_DIR at it")
endif()

add_library(deskhub_win_core STATIC
    SharingHost.cpp
    ElevatedShare.cpp

    net/Firewall.cpp

    capture/ScreenCapture.cpp
    capture/Downscaler.cpp

    gpu/GpuSelect.cpp
    gpu/D3D11VideoProcessor.cpp

    encode/MfEncoder.cpp
    encode/NvencEncoder.cpp
    encode/EncoderFactory.cpp

    decode/MfDecoder.cpp
    decode/PanelRenderer.cpp
    decode/WinVideoDecoder.cpp

    input/InputInjector.cpp
)

target_compile_definitions(deskhub_win_core PUBLIC UNICODE _UNICODE)
target_include_directories(deskhub_win_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${NVENC_INTERFACE_DIR}"
)
target_link_libraries(deskhub_win_core PUBLIC core platform)
target_link_libraries(deskhub_win_core PUBLIC ws2_32 iphlpapi)

deskhub_target_warnings(deskhub_win_core)
if(MSVC)
    target_compile_options(deskhub_win_core PUBLIC /utf-8)
    target_compile_options(deskhub_win_core PRIVATE /sdl)
endif()

option(DESKHUB_ENCODER_BENCH "Build the encoder bake-off harness" ON)
if(DESKHUB_ENCODER_BENCH)
    add_executable(deskhub_encbench bench/EncoderBench.cpp)
    target_link_libraries(deskhub_encbench PRIVATE deskhub_win_core)
    deskhub_target_warnings(deskhub_encbench)
    if(MSVC)
        target_compile_options(deskhub_encbench PRIVATE /utf-8 /sdl)
    endif()
endif()

add_library(deskhub_win_view STATIC
    ../win32/FileSendWindow.cpp
    ../win32/Viewer.cpp
    ../win32/ViewerInput.cpp
    ScreenFfiWin.cpp
)
target_include_directories(deskhub_win_view PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../win32")
target_link_libraries(deskhub_win_view PUBLIC deskhub_win_core comctl32 comdlg32)
deskhub_target_warnings(deskhub_win_view)
if(MSVC)
    target_compile_options(deskhub_win_view PRIVATE /sdl)
    target_link_options(deskhub_win_view INTERFACE
        "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
endif()
