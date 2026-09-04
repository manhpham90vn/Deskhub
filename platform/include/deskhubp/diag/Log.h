#pragma once

#if defined(__ANDROID__)

#include <android/log.h>

#define DESKHUB_LOG_TAG "Deskhub"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, DESKHUB_LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, DESKHUB_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, DESKHUB_LOG_TAG, __VA_ARGS__)

#elif defined(_WIN32)

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace deskhubp {

inline void LogEmit(const char* fmt, ...) {
    constexpr const char* kTag = "[Deskhub] ";
    constexpr size_t kTagLen = 10;
    char line[1024];
    std::memcpy(line, kTag, kTagLen);

    va_list args;
    va_start(args, fmt);
    const int written = std::vsnprintf(line + kTagLen, sizeof(line) - kTagLen - 2, fmt, args);
    va_end(args);
    if (written < 0) return;

    size_t end = kTagLen + size_t(written);
    if (end > sizeof(line) - 2) end = sizeof(line) - 2;
    line[end] = '\n';
    line[end + 1] = '\0';
    std::fputs(line, stdout);
}

}

#define LOGI(...) deskhubp::LogEmit(__VA_ARGS__)
#define LOGW(...) LOGI(__VA_ARGS__)
#define LOGE(...) LOGI(__VA_ARGS__)

#else

#if __has_include(<TargetConditionals.h>)
#include <TargetConditionals.h>
#endif

#include "deskhubp/diag/LogFile.h"

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include <cstdio>
#define LOGI(...)                           \
    do {                                    \
        std::fprintf(stderr, "[Deskhub] "); \
        std::fprintf(stderr, __VA_ARGS__);  \
        std::fprintf(stderr, "\n");         \
    } while (0)
#else
#define LOGI(...) deskhubp::LogEmit(__VA_ARGS__)
#endif

#define LOGW(...) LOGI(__VA_ARGS__)
#define LOGE(...) LOGI(__VA_ARGS__)

#endif
