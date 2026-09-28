#pragma once

#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace deskhubp {

class ConfigFileLock {
public:
    explicit ConfigFileLock(std::string_view fileName);
    ~ConfigFileLock();
    ConfigFileLock(const ConfigFileLock&) = delete;
    ConfigFileLock& operator=(const ConfigFileLock&) = delete;

    bool Valid() const;

private:
#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped_{};
#else
    int file_ = -1;
#endif
};

}
