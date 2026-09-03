#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt")

inline bool RandomBytes(void* out, size_t n) {
    if (!out || n == 0) return false;
    const NTSTATUS st = BCryptGenRandom(nullptr, static_cast<PUCHAR>(out), static_cast<ULONG>(n),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return st >= 0;
}

#elif defined(__APPLE__)
#include <cstdlib>

inline bool RandomBytes(void* out, size_t n) {
    if (!out || n == 0) return false;
    arc4random_buf(out, n);
    return true;
}

#else
#include <cerrno>
#include <cstdio>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

inline bool RandomBytes(void* out, size_t n) {
    if (!out || n == 0) return false;
    auto* p = static_cast<unsigned char*>(out);
    size_t got = 0;

#if defined(__linux__) && defined(SYS_getrandom)
    while (got < n) {
        const long r = syscall(SYS_getrandom, p + got, n - got, 0);
        if (r > 0) {
            got += static_cast<size_t>(r);
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        break;
    }
    if (got == n) return true;
#endif

    std::FILE* f = std::fopen("/dev/urandom", "rb");
    if (!f) return false;
    const size_t rd = std::fread(p + got, 1, n - got, f);
    std::fclose(f);
    return got + rd == n;
}

#endif

inline uint32_t RandomU32() {
    uint32_t v = 0;
    if (!RandomBytes(&v, sizeof(v))) return 0;
    return v;
}
