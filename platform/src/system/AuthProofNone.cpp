#include "deskhubp/system/AuthProof.h"

namespace deskhubp {

std::vector<uint8_t> IdentityPublicKey(const HostIdentity&) {
    return {};
}

std::string IdentityPublicKeyText(const HostIdentity&) {
    return {};
}

std::string PublicKeyTextFromSpki(std::span<const uint8_t>) {
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

AuthNonce NewAuthNonce() {
    return {};
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

}
