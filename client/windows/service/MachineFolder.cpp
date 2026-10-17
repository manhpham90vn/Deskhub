#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "MachineFolder.h"

#include <string>

#include "deskhubp/system/MachineFolder.h"

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
    return deskhubp::EnterMachineFolder(Narrow(MachineFolderPath()));
}

bool UnattendedEnabled() {
    return deskhubp::UnattendedEnabled(Narrow(MachineFolderPath()));
}

bool WipeMachineFolder() {
    return deskhubp::WipeMachineFolder(Narrow(MachineFolderPath()));
}
