#include "deskhubp/system/AppDataFile.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string_view>

#include "deskhubp/system/Random.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace deskhubp {

namespace {

std::filesystem::path TemporaryPath(const std::filesystem::path& path) {
    std::array<uint8_t, 8> bytes{};
    if (!RandomBytes(bytes.data(), bytes.size())) return {};
    constexpr std::string_view digits = "0123456789abcdef";
    std::string suffix = ".tmp-";
    for (uint8_t byte : bytes) {
        suffix += digits[byte >> 4];
        suffix += digits[byte & 15];
    }
    std::filesystem::path temporary = path;
    temporary += suffix;
    return temporary;
}

bool WriteTemporary(const std::filesystem::path& path, std::string_view content) {
#ifdef _WIN32
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool okay = true;
    size_t position = 0;
    while (position < content.size()) {
        DWORD written = 0;
        const DWORD amount = DWORD(std::min<size_t>(content.size() - position, 65536));
        if (!WriteFile(file, content.data() + position, amount, &written, nullptr) || written == 0) {
            okay = false;
            break;
        }
        position += written;
    }
    if (okay) okay = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    return okay;
#else
    const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (file < 0) return false;
    bool okay = true;
    size_t position = 0;
    while (position < content.size()) {
        const ssize_t written = ::write(file, content.data() + position, content.size() - position);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            okay = false;
            break;
        }
        position += size_t(written);
    }
    if (okay) okay = ::fsync(file) == 0;
    if (::close(file) != 0) okay = false;
    return okay;
#endif
}

}

bool WriteAppDataFileAtomic(const std::string& fileName, const std::string& content) {
    const std::filesystem::path target = AppDataFilePath(fileName);
    if (target.empty()) return false;
    const std::filesystem::path temporary = TemporaryPath(target);
    if (temporary.empty()) return false;
    if (!WriteTemporary(temporary, content)) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return false;
    }
#ifdef _WIN32
    const bool saved = MoveFileExW(temporary.c_str(), target.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    const bool saved = ::rename(temporary.c_str(), target.c_str()) == 0;
#endif
    if (!saved) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
    }
    return saved;
}

}
