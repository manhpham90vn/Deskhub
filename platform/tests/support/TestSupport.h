#pragma once
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/HostIdentity.h"

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

extern int g_failures;
void Check(bool ok, const char* what);
