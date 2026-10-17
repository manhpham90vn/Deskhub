#include "deskhubp/system/MachineFolder.h"

#include <filesystem>
#include <system_error>

#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"

namespace deskhubp {
namespace {

std::filesystem::path FolderPath(const std::string& path) {
    const std::u8string utf8(path.begin(), path.end());
    return std::filesystem::path(utf8);
}

}

bool EnterMachineFolder(const std::string& path) {
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(FolderPath(path), error);
    if (error) return false;
    SetAppDataDir(path);
    SetConfigDir(path);
    return !ConfigDir().empty();
}

bool UnattendedEnabled(const std::string& path) {
    if (path.empty()) return false;
    std::error_code error;
    return std::filesystem::is_regular_file(FolderPath(path) / kHostKeyFileName, error);
}

bool WipeMachineFolder(const std::string& path) {
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::remove_all(FolderPath(path), error);
    return !error;
}

}
