#include "deskhubp/system/ConfigFileLock.h"

#include <string>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include "deskhubp/system/AppDataFile.h"

namespace deskhubp {

ConfigFileLock::ConfigFileLock(std::string_view fileName) {
    const auto path = AppDataFilePath(std::string(fileName) + ".lock");
    if (path.empty()) return;
#ifdef _WIN32
    file_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file_ == INVALID_HANDLE_VALUE) return;
    if (!LockFileEx(file_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped_)) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
#else
    file_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (file_ < 0) return;
    while (::flock(file_, LOCK_EX) != 0) {
        if (errno == EINTR) continue;
        ::close(file_);
        file_ = -1;
        break;
    }
#endif
}

ConfigFileLock::~ConfigFileLock() {
#ifdef _WIN32
    if (file_ == INVALID_HANDLE_VALUE) return;
    UnlockFileEx(file_, 0, 1, 0, &overlapped_);
    CloseHandle(file_);
#else
    if (file_ < 0) return;
    ::flock(file_, LOCK_UN);
    ::close(file_);
#endif
}

bool ConfigFileLock::Valid() const {
#ifdef _WIN32
    return file_ != INVALID_HANDLE_VALUE;
#else
    return file_ >= 0;
#endif
}

}
