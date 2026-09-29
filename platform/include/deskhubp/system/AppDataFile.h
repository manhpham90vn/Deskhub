#pragma once
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/Environment.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <aclapi.h>
#else
#include <fcntl.h>
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

#ifdef _WIN32
inline bool ProtectWindowsConfigDir(const std::string& dir) {
    const std::wstring wide = WidenUtf8(dir);
    if (wide.empty()) return false;
    const HANDLE directory = CreateFileW(wide.c_str(), READ_CONTROL | WRITE_DAC,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (directory == INVALID_HANDLE_VALUE) return false;
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    bool okay = GetFileInformationByHandleEx(directory, FileAttributeTagInfo, &attributes,
                    sizeof(attributes)) != 0 &&
                (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;

    HANDLE token = nullptr;
    if (okay) okay = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != 0;
    DWORD identityBytes = 0;
    if (okay) {
        GetTokenInformation(token, TokenUser, nullptr, 0, &identityBytes);
        okay = GetLastError() == ERROR_INSUFFICIENT_BUFFER && identityBytes > 0;
    }
    std::vector<std::max_align_t> identity(
        (identityBytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    if (okay)
        okay = GetTokenInformation(token, TokenUser, identity.data(), identityBytes,
                   &identityBytes) != 0;
    if (token) CloseHandle(token);

    std::array<BYTE, SECURITY_MAX_SID_SIZE> systemSid{};
    std::array<BYTE, SECURITY_MAX_SID_SIZE> adminSid{};
    DWORD systemBytes = DWORD(systemSid.size());
    DWORD adminBytes = DWORD(adminSid.size());
    if (okay) {
        okay = CreateWellKnownSid(WinLocalSystemSid, nullptr, systemSid.data(),
                   &systemBytes) != 0 &&
               CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSid.data(),
                   &adminBytes) != 0;
    }
    if (okay) {
        const PSID userSid = reinterpret_cast<TOKEN_USER*>(identity.data())->User.Sid;
        const DWORD size = sizeof(ACL) + 3 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                           GetLengthSid(userSid) + systemBytes + adminBytes;
        std::vector<std::max_align_t> buffer(
            (size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        auto* dacl = reinterpret_cast<PACL>(buffer.data());
        constexpr DWORD inherit = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
        okay = InitializeAcl(dacl, size, ACL_REVISION) != 0 &&
               AddAccessAllowedAceEx(dacl, ACL_REVISION, inherit, FILE_ALL_ACCESS,
                   userSid) != 0 &&
               AddAccessAllowedAceEx(dacl, ACL_REVISION, inherit, FILE_ALL_ACCESS,
                   systemSid.data()) != 0 &&
               AddAccessAllowedAceEx(dacl, ACL_REVISION, inherit, FILE_ALL_ACCESS,
                   adminSid.data()) != 0;
        if (okay)
            okay = SetSecurityInfo(directory, SE_FILE_OBJECT,
                       DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                       nullptr, nullptr, dacl, nullptr) == ERROR_SUCCESS;
    }
    if (!CloseHandle(directory)) okay = false;
    return okay;
}
#endif

inline bool EnsurePrivateConfigDir(const std::string& dir) {
    if (!EnsureWritableDir(dir)) return false;
#ifdef _WIN32
    return ProtectWindowsConfigDir(dir);
#else
    const int fd = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat info{};
    bool okay = fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) &&
                info.st_uid == geteuid();
    if (okay && (info.st_mode & 077) != 0) {
        okay = fchmod(fd, 0700) == 0 && fstat(fd, &info) == 0 &&
               (info.st_mode & 077) == 0;
    }
    if (close(fd) != 0) okay = false;
    if (!okay) return false;
    return true;
#endif
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

bool WriteAppDataFileAtomic(const std::string& fileName, const std::string& content);

inline bool WriteAppDataFile(const std::string& fileName, const std::string& content) {
    return WriteAppDataFileAtomic(fileName, content);
}

}
