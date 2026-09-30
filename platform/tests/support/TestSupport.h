#pragma once
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"

#include <filesystem>
#include <string>

inline bool ForgetHostIdentity() {
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    return deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName).empty();
}

inline bool GrantClientKey(const deskhubp::HostIdentity& identity) {
    return deskhubp::RememberAuthorizedKey(deskhubp::IdentityPublicKeyText(identity)) ||
           deskhubp::IsClientKeyAuthorized(identity.publicKey);
}

inline bool RevokeAllClientKeys() {
    deskhubp::RemoveAppDataFile(deskhubp::kAccessRequestsFileName);
    deskhubp::RevokePairingTokens();
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
