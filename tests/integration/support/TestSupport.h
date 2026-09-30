#pragma once
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

inline bool GrantClientKey(const deskhubp::HostIdentity& identity) {
    return deskhubp::RememberAuthorizedKey(deskhubp::IdentityPublicKeyText(identity)) ||
           deskhubp::IsClientKeyAuthorized(identity.publicKey);
}

inline bool RevokeAllClientKeys() {
    deskhubp::RemoveAppDataFile(deskhubp::kAccessRequestsFileName);
    deskhubp::RevokePairingTokens();
    return deskhubp::ClearAuthorizedKeys();
}

extern int g_failures;
void Check(bool ok, const char* what);

bool WaitFor(const std::function<bool()>& done, uint32_t timeoutMs);

std::string Hex(std::span<const uint8_t> bytes);

uint16_t NextTestPort();
