#pragma once
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

inline bool GrantClientKey(const deskhubp::ClientIdentity& identity) {
    return deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(identity)) ||
           deskhubp::IsClientKeyAuthorized(identity.publicKey);
}

inline bool RevokeAllClientKeys() {
    return deskhubp::ClearAuthorizedKeys();
}

extern int g_failures;
void Check(bool ok, const char* what);

bool WaitFor(const std::function<bool()>& done, uint32_t timeoutMs);

std::string Hex(std::span<const uint8_t> bytes);

uint16_t NextTestPort();
