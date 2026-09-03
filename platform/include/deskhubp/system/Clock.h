#pragma once
#include <cstdint>
#include <ctime>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline uint64_t NowUs() {
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0) f.QuadPart = 1;
        return f;
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart / freq.QuadPart) * 1'000'000ull +
           (uint64_t)(c.QuadPart % freq.QuadPart) * 1'000'000ull / (uint64_t)freq.QuadPart;
}

inline void SleepUs(uint64_t us) {
    if (!us) return;

    struct HighResolutionTimer {
        HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        ~HighResolutionTimer() {
            if (handle) CloseHandle(handle);
        }
    };
    static thread_local HighResolutionTimer timer;

    if (timer.handle) {
        LARGE_INTEGER due;
        due.QuadPart = -int64_t(us) * 10;
        if (SetWaitableTimer(timer.handle, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer.handle, INFINITE);
            return;
        }
    }
    Sleep(DWORD((us + 999) / 1000));
}

#else
#include <cerrno>
#include <ctime>

inline uint64_t NowUs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1'000'000ull + uint64_t(ts.tv_nsec) / 1000ull;
}

inline void SleepUs(uint64_t us) {
    if (!us) return;
    timespec ts{};
    ts.tv_sec = time_t(us / 1'000'000ull);
    ts.tv_nsec = long(us % 1'000'000ull) * 1000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

#endif

inline int64_t NowUnixSeconds() {
    return int64_t(std::time(nullptr));
}

inline std::string FormatUnixMinute(int64_t unixTime) {
    if (unixTime <= 0) return {};
    const std::time_t stamp = std::time_t(unixTime);
    std::tm parts{};
#if defined(_WIN32)
    if (localtime_s(&parts, &stamp) != 0) return {};
#else
    if (localtime_r(&stamp, &parts) == nullptr) return {};
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &parts) == 0) return {};
    return std::string(buf);
}
