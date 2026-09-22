#include "deskhubp/system/FolderOpen.h"

#include <windows.h>

#include <shellapi.h>

#include "deskhubp/diag/Log.h"

namespace deskhubp {

bool OpenFolder(const std::filesystem::path& folder) {
    const HINSTANCE result =
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    const auto code = reinterpret_cast<INT_PTR>(result);
    if (code <= 32) {
        LOGW("[FolderOpen] ShellExecuteW failed: %td", code);
        return false;
    }
    return true;
}

}
