# platform — lớp MỎNG bọc những thứ core/ không được phép chạm (header hệ điều
# hành). Dùng chung giữa các client OS; API giữ nguyên tên giữa mọi nền tảng để
# code port qua lại đọc y hệt.
#
# Hiện chỉ có Clock.h (header-only) nên đây là INTERFACE library — không sinh
# .lib/.a nào. Khi UdpSocket được gộp về đây (mỗi OS một .cpp) thì đổi thành
# STATIC và thêm source theo CMAKE_SYSTEM_NAME.
cmake_minimum_required(VERSION 3.20)

add_library(platform INTERFACE)
target_include_directories(platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(platform INTERFACE cxx_std_20)
