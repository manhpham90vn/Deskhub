#pragma once
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace deskhubp {

inline constexpr const char* kHostKeyFileName = "host_key.pem";
inline constexpr const char* kHostCertFileName = "host_cert.pem";

struct HostIdentity {
    std::string certPem{};
    std::string keyPem{};
    std::string certPath{};
    std::string keyPath{};
    deskhub::Fingerprint fingerprint{};

    bool Valid() const {
        return !certPem.empty() && !keyPem.empty() && !deskhub::IsZero(fingerprint);
    }
};

bool QuicAvailable();

HostIdentity LoadHostIdentity();
HostIdentity LoadOrCreateHostIdentity(std::string_view commonName);
std::optional<deskhub::Fingerprint> FingerprintOfCertDer(std::span<const uint8_t> der);
}
