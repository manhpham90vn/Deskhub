#pragma once
#include "deskhub/net/TrustStore.h"

#include <optional>

namespace deskhubp {

inline constexpr const char* kTrustStoreFileName = "known_hosts";

deskhub::TrustStore LoadTrustStore();
std::optional<deskhub::TrustStore> TryLoadTrustStore();
bool ClearTrustedHosts();

deskhub::TrustVerdict CheckTrustedHost(std::string_view endpoint,
    const deskhub::Fingerprint& fingerprint);
bool RememberTrustedHost(std::string_view endpoint, std::string_view label,
    const deskhub::Fingerprint& fingerprint, int64_t nowUnix);
bool RememberTrustedHostProfile(std::string_view endpoint, std::string_view label,
    const deskhub::Fingerprint& fingerprint, std::string_view identityName,
    int64_t nowUnix);
bool CreateTrustedHostProfile(std::string_view endpoint, std::string_view label,
    const deskhub::Fingerprint& fingerprint, std::string_view identityName);
bool UpdateTrustedHostProfile(const deskhub::TrustedHost& expected, std::string_view endpoint,
    std::string_view label, const deskhub::Fingerprint& fingerprint,
    std::string_view identityName);
bool ForgetTrustedHost(std::string_view endpoint);

}
