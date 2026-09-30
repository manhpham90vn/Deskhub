#pragma once
#include "deskhub/auth/Transcript.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/net/PublicKeyText.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/system/HostIdentity.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace deskhubp {

std::string IdentityPublicKeyText(const HostIdentity& identity);
std::string IdentityPublicKeyLine(const HostIdentity& identity, std::string_view label);
std::string PublicKeyTextFromSpki(std::span<const uint8_t> spkiDer);
std::vector<uint8_t> PublicKeySpkiFromText(std::string_view text);
std::optional<deskhub::Fingerprint> FingerprintOfPublicKey(std::span<const uint8_t> spkiDer);

std::vector<uint8_t> SignWithIdentity(const HostIdentity& identity,
    std::span<const uint8_t> data);
bool VerifySignature(std::span<const uint8_t> spkiDer, std::span<const uint8_t> data,
    std::span<const uint8_t> signature);

}
