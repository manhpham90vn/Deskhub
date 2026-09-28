#pragma once

#include "deskhub/protocol/Wire.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace deskhub {

enum class AuthRole : uint8_t {
    Client = 1,
    Host = 2,
};

using AuthSessionId = std::array<uint8_t, kAuthSessionIdBytes>;

std::vector<uint8_t> AuthTranscript(AuthRole role,
    const AuthSessionId& sessionId,
    std::span<const uint8_t> clientPublicKey, const Fingerprint& hostFingerprint);

}
