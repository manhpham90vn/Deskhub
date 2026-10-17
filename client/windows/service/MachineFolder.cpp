#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "MachineFolder.h"

#include <filesystem>
#include <string>

#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"

namespace {

constexpr wchar_t kMachineFolderName[] = L"DeskHub";

std::string Narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), int(wide.size()), nullptr, 0,
        nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), int(wide.size()), out.data(), n, nullptr,
        nullptr);
    return out;
}

}

std::wstring MachineFolderPath() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"ProgramData", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return std::wstring(buf, n) + L"\\" + kMachineFolderName;
}

bool EnterMachineFolder() {
    const std::wstring folder = MachineFolderPath();
    if (folder.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(folder), ec);
    if (ec) return false;
    const std::string utf8 = Narrow(folder);
    deskhubp::SetAppDataDir(utf8);
    deskhubp::SetConfigDir(utf8);
    return !deskhubp::ConfigDir().empty();
}

bool UnattendedEnabled() {
    const std::wstring folder = MachineFolderPath();
    if (folder.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(
        std::filesystem::path(folder) / deskhubp::kHostKeyFileName, ec);
}

bool WipeMachineFolder() {
    const std::wstring folder = MachineFolderPath();
    if (folder.empty()) return false;
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(folder), ec);
    return !ec;
}
