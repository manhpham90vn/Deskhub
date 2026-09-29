#pragma once
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/HostIdentity.h"

#include <filesystem>
#include <string>

inline bool ForgetHostIdentity() {
    deskhubp::RemoveAppDataFile(deskhubp::kHostCertFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    return deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName).empty();
}

inline bool GrantClientKey(const deskhubp::ClientIdentity& identity) {
    return deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(identity)) ||
           deskhubp::IsClientKeyAuthorized(identity.publicKey);
}

inline bool RevokeAllClientKeys() {
    return deskhubp::ClearAuthorizedKeys();
}

std::filesystem::path UniqueTempDir(const std::string& prefix);
std::filesystem::path TestFixturesDir();

struct IsolatedAppData {
    std::filesystem::path dir{};
    std::string previous = deskhubp::AppDataDirRef();

    explicit IsolatedAppData(const std::string& prefix);
    ~IsolatedAppData();

    IsolatedAppData(const IsolatedAppData&) = delete;
    IsolatedAppData& operator=(const IsolatedAppData&) = delete;
};

extern int g_failures;
void Check(bool ok, const char* what);
