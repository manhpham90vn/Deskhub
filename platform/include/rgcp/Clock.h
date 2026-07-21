#pragma once
// Đồng hồ đơn điệu micro giây — dùng làm nowUs bơm vào core (rgc) và làm timestamp
// video phía agent. Trước đây mỗi OS một bản riêng với TÊN KHÁC NHAU (Windows
// QpcUs(), Android NowUs()) khiến code port qua lại phải sửa tay; giờ một tên duy
// nhất, khác biệt OS nằm gọn trong #ifdef dưới đây.
//
// Header này ở platform/ chứ KHÔNG ở core/: nó include header hệ điều hành, mà
// core phải giữ nguyên tắc thuần C++20 không đụng OS (xem core/CMakeLists.txt).
//
// Cả hai nhánh đều đơn điệu: không nhảy khi người dùng chỉnh giờ hệ thống. Chúng
// ĐỨNG YÊN khi máy ngủ sâu, nhưng phiên stream cũng chết lúc đó nên không ảnh hưởng.
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

inline uint64_t NowUs() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    // Tách phần nguyên / phần dư trước khi nhân: c.QuadPart * 1'000'000 tràn uint64
    // sau ~5 giờ chạy với freq 10 MHz.
    return (uint64_t)(c.QuadPart / freq.QuadPart) * 1'000'000ull +
           (uint64_t)(c.QuadPart % freq.QuadPart) * 1'000'000ull / (uint64_t)freq.QuadPart;
}

#else
#include <ctime>

inline uint64_t NowUs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1'000'000ull + uint64_t(ts.tv_nsec) / 1000ull;
}

#endif
