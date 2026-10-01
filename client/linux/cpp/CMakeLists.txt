find_package(PkgConfig REQUIRED)
pkg_check_modules(DESKHUB_MEDIA IMPORTED_TARGET
    epoxy
    egl
    libdrm
    libva
    libva-drm
    libpipewire-0.3
)

if(NOT DESKHUB_MEDIA_FOUND)
    message(${deskhub_skip_level}
        "Deskhub: skipping capture, encode and decode on Ubuntu — development packages "
        "are missing. Run 'make bootstrap' to install them.")
    return()
endif()

set(FFMPEG_MIN_DIR ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-min)
if(NOT EXISTS ${FFMPEG_MIN_DIR}/lib/libavcodec.a)
    message(${deskhub_skip_level}
        "Deskhub: skipping capture, encode and decode on Ubuntu — "
        "third_party/ffmpeg-min is missing. Run 'scripts/build-ffmpeg.sh' "
        "(or 'make bootstrap').")
    return()
endif()

add_library(ffmpeg_min INTERFACE)
target_include_directories(ffmpeg_min SYSTEM INTERFACE ${FFMPEG_MIN_DIR}/include)
target_link_libraries(ffmpeg_min INTERFACE
    ${FFMPEG_MIN_DIR}/lib/libavcodec.a
    ${FFMPEG_MIN_DIR}/lib/libavutil.a
)

option(DESKHUB_NVENC "Encode on NVIDIA GPUs through NVENC when one is present" ON)
set(NVENC_HEADERS_DIR ${CMAKE_SOURCE_DIR}/third_party/nvenc-13.0/include)
if(DESKHUB_NVENC AND NOT EXISTS ${NVENC_HEADERS_DIR}/ffnvcodec/nvEncodeAPI.h)
    message(${deskhub_skip_level}
        "Deskhub: skipping capture, encode and decode on Ubuntu — the "
        "third_party/nvenc-13.0 submodule is not checked out. Run "
        "'git submodule update --init', or pass -DDESKHUB_NVENC=OFF to build for "
        "VA-API only.")
    return()
endif()

add_library(deskhub_linux_core STATIC
    SharingHost.cpp

    capture/ScreenCapture.cpp

    encode/VaDisplay.cpp
    encode/VaEncoder.cpp
    $<$<BOOL:${DESKHUB_NVENC}>:encode/NvEncoder.cpp>

    decode/AvDecoder.cpp
    render/VideoRenderer.cpp

    input/InputInjector.cpp
)

target_include_directories(deskhub_linux_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(deskhub_linux_core PUBLIC core platform PkgConfig::DESKHUB_MEDIA ffmpeg_min)

if(DESKHUB_NVENC)
    target_compile_definitions(deskhub_linux_core PUBLIC DESKHUB_HAVE_NVENC)
    target_include_directories(deskhub_linux_core SYSTEM PUBLIC ${NVENC_HEADERS_DIR})
    target_link_libraries(deskhub_linux_core PUBLIC ${CMAKE_DL_LIBS})
endif()

target_compile_features(deskhub_linux_core PUBLIC cxx_std_20)
deskhub_target_warnings(deskhub_linux_core)
deskhub_target_hardening(deskhub_linux_core)
