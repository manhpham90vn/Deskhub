#pragma once
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

using AuthNonce = std::array<uint8_t, deskhub::kAuthNonceBytes>;

std::vector<uint8_t> IdentityPublicKey(const HostIdentity& identity);
std::string IdentityPublicKeyText(const HostIdentity& identity);
std::string PublicKeyTextFromSpki(std::span<const uint8_t> spkiDer);
std::vector<uint8_t> PublicKeySpkiFromText(std::string_view text);
std::optional<deskhub::Fingerprint> FingerprintOfPublicKey(std::span<const uint8_t> spkiDer);

std::vector<uint8_t> SignWithIdentity(const HostIdentity& identity,
    std::span<const uint8_t> data);
bool VerifySignature(std::span<const uint8_t> spkiDer, std::span<const uint8_t> data,
    std::span<const uint8_t> signature);

AuthNonce NewAuthNonce();

std::vector<uint8_t> AuthTranscript(std::string_view label, const AuthNonce& nonce,
    const deskhub::Fingerprint& hostFingerprint, std::span<const uint8_t> extra = {});

}
