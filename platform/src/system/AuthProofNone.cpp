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

}
