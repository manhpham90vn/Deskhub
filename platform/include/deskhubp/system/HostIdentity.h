#pragma once
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

inline constexpr const char* kHostKeyFileName = "host_key.pem";

struct HostIdentity {
    std::string keyPem{};
    std::string keyPath{};
    std::vector<uint8_t> publicKey{};
    deskhub::Fingerprint fingerprint{};

    bool Valid() const {
        return !keyPem.empty() && !publicKey.empty() && !deskhub::IsZero(fingerprint);
    }
};

bool QuicAvailable();

HostIdentity LoadHostIdentity();
HostIdentity LoadOrCreateHostIdentity();
std::string TransportCertificatePem(const HostIdentity& identity);
std::optional<deskhub::Fingerprint> FingerprintOfCertDer(std::span<const uint8_t> der);
}
