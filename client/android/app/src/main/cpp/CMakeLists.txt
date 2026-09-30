cmake_minimum_required(VERSION 3.22.1)
project(deskhub LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(DESKHUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
add_subdirectory(${DESKHUB_ROOT}/core ${CMAKE_CURRENT_BINARY_DIR}/core)
add_subdirectory(${DESKHUB_ROOT}/platform ${CMAKE_CURRENT_BINARY_DIR}/platform)

add_library(deskhub SHARED
    JniBridge.cpp
    JniEnv.cpp
    HostBridge.cpp
    TerminalBridge.cpp
    HostProfileBridge.cpp
    PairingBridge.cpp
    SharingHost.cpp
    ScreenFfiAndroid.cpp

    capture/ScreenCapture.cpp
    decode/MediaCodecDecoder.cpp
    encode/MediaCodecEncoder.cpp
)

target_include_directories(deskhub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(deskhub
    PRIVATE
        core
        platform
        android
        mediandk
        log
)

target_link_options(deskhub PRIVATE -Wl,-z,max-page-size=16384)

deskhub_target_warnings(deskhub)
