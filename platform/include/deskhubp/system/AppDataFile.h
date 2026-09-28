#pragma once
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/Environment.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace deskhubp {

inline std::string& ConfigDirRef() {
    static std::string dir;
    return dir;
}

inline void SetConfigDir(std::string dir) {
    ConfigDirRef() = std::move(dir);
}

inline bool EnsurePrivateConfigDir(const std::string& dir) {
    if (!EnsureWritableDir(dir)) return false;
#ifndef _WIN32
    struct stat info{};
    if (lstat(dir.c_str(), &info) != 0 || !S_ISDIR(info.st_mode) ||
        info.st_uid != geteuid()) return false;
    if ((info.st_mode & 077) != 0 && chmod(dir.c_str(), 0700) != 0) return false;
#endif
    return true;
}

inline std::string ConfigDir() {
    const std::string& configured = ConfigDirRef();
    if (!configured.empty()) return EnsurePrivateConfigDir(configured) ? configured : std::string();
    const std::string fromEnvironment = EnvValue("DESKHUB_CONFIG_DIR");
    if (!fromEnvironment.empty())
        return EnsurePrivateConfigDir(fromEnvironment) ? fromEnvironment : std::string();
    const std::string fallback = LogDir();
    return EnsurePrivateConfigDir(fallback) ? fallback : std::string();
}

inline std::filesystem::path AppDataFilePath(const std::string& fileName) {
    const std::string dir = ConfigDir();
    if (dir.empty()) return {};
    const std::u8string dirU8(dir.begin(), dir.end());
    const std::u8string nameU8(fileName.begin(), fileName.end());
    return std::filesystem::path(dirU8) / std::filesystem::path(nameU8);
}

inline std::string ReadAppDataFile(const std::string& fileName) {
    const std::filesystem::path path = AppDataFilePath(fileName);
    if (path.empty()) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

inline void RemoveAppDataFile(const std::string& fileName) {
    const std::filesystem::path path = AppDataFilePath(fileName);
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

inline bool WriteAppDataFile(const std::string& fileName, const std::string& content) {
    const std::filesystem::path path = AppDataFilePath(fileName);
    if (path.empty()) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return bool(out);
}

bool WriteAppDataFileAtomic(const std::string& fileName, const std::string& content);

}
