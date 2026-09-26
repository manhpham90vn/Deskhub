#include "deskhubp/system/AuthProof.h"

#include <cstring>

namespace deskhubp {

std::vector<uint8_t> IdentityPublicKey(const HostIdentity&) {
    return {};
}

std::string IdentityPublicKeyText(const HostIdentity&) {
    return {};
}

std::vector<uint8_t> PublicKeySpkiFromText(std::string_view) {
    return {};
}

std::optional<deskhub::Fingerprint> FingerprintOfPublicKey(std::span<const uint8_t>) {
    return std::nullopt;
}

std::vector<uint8_t> SignWithIdentity(const HostIdentity&, std::span<const uint8_t>) {
    return {};
}

bool VerifySignature(std::span<const uint8_t>, std::span<const uint8_t>,
    std::span<const uint8_t>) {
    return false;
}

AuthSalt NewAuthSalt() {
    return {};
}

AuthNonce NewAuthNonce() {
    return {};
}

PasscodeVerifier MakePasscodeVerifier(const AuthSalt&, std::string_view) {
    return {};
}

AuthMac ComputeAuthMac(std::span<const uint8_t>, std::span<const uint8_t>) {
    return {};
}

bool MacsMatch(const AuthMac&, const AuthMac&) {
    return false;
}

std::vector<uint8_t> AuthTranscript(std::string_view label, const AuthNonce& nonce,
    const deskhub::Fingerprint& hostFingerprint, std::span<const uint8_t> extra) {
    std::vector<uint8_t> out;
    out.insert(out.end(), label.begin(), label.end());
    out.push_back(0);
    out.insert(out.end(), nonce.begin(), nonce.end());
    out.insert(out.end(), hostFingerprint.bytes.begin(), hostFingerprint.bytes.end());
    out.insert(out.end(), extra.begin(), extra.end());
    return out;
}

struct Spake2Session::Impl {
    int unused = 0;
};

Spake2Session::Spake2Session() : impl_(std::make_unique<Impl>()) {
}

Spake2Session::~Spake2Session() = default;

bool Spake2Session::Start(bool, const PasscodeVerifier&, std::vector<uint8_t>&) {
    return false;
}

bool Spake2Session::Finish(std::span<const uint8_t>, PasscodeVerifier&) {
    return false;
}

}
