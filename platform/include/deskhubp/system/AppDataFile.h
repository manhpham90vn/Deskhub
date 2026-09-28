#pragma once
#include "deskhubp/diag/LogFile.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace deskhubp {

inline std::filesystem::path AppDataFilePath(const std::string& fileName) {
    const std::string dir = LogDir();
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
