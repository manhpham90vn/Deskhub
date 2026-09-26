#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/net/PublicKeyText.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/system/HostIdentity.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace deskhubp {

using AuthMac = std::array<uint8_t, deskhub::kAuthMacBytes>;
using AuthSalt = std::array<uint8_t, deskhub::kAuthSaltBytes>;
using AuthNonce = std::array<uint8_t, deskhub::kAuthNonceBytes>;
using PasscodeVerifier = std::array<uint8_t, 32>;

std::vector<uint8_t> IdentityPublicKey(const HostIdentity& identity);
std::string IdentityPublicKeyText(const HostIdentity& identity);
std::vector<uint8_t> PublicKeySpkiFromText(std::string_view text);
std::optional<deskhub::Fingerprint> FingerprintOfPublicKey(std::span<const uint8_t> spkiDer);

std::vector<uint8_t> SignWithIdentity(const HostIdentity& identity,
    std::span<const uint8_t> data);
bool VerifySignature(std::span<const uint8_t> spkiDer, std::span<const uint8_t> data,
    std::span<const uint8_t> signature);

AuthSalt NewAuthSalt();
AuthNonce NewAuthNonce();

PasscodeVerifier MakePasscodeVerifier(const AuthSalt& salt, std::string_view passcode);

AuthMac ComputeAuthMac(std::span<const uint8_t> key, std::span<const uint8_t> data);
bool MacsMatch(const AuthMac& a, const AuthMac& b);

std::vector<uint8_t> AuthTranscript(std::string_view label, const AuthNonce& nonce,
    const deskhub::Fingerprint& hostFingerprint, std::span<const uint8_t> extra = {});

class Spake2Session {
public:
    Spake2Session();
    ~Spake2Session();
    Spake2Session(const Spake2Session&) = delete;
    Spake2Session& operator=(const Spake2Session&) = delete;

    bool Start(bool asHost, const PasscodeVerifier& verifier, std::vector<uint8_t>& outMessage);
    bool Finish(std::span<const uint8_t> peerMessage, PasscodeVerifier& outKey);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
