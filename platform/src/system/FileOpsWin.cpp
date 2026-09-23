#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "deskhubp/system/FileOps.h"

#include <windows.h>

namespace deskhubp {

bool CreateNewFile(const std::filesystem::path& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

MoveOutcome MoveWithoutReplacing(const std::filesystem::path& from,
    const std::filesystem::path& to) {
    if (MoveFileExW(from.c_str(), to.c_str(), 0)) return MoveOutcome::Moved;
    const DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
        return MoveOutcome::TargetExists;
    return MoveOutcome::Failed;
}

}
