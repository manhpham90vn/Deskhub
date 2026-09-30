#pragma once
#include "deskhub/net/TrustStore.h"

#include <optional>
#include <string_view>

namespace deskhubp {

inline constexpr const char* kTrustStoreFileName = "known_hosts";

deskhub::TrustStore LoadTrustStore();
std::optional<deskhub::TrustStore> TryLoadTrustStore();
bool ClearTrustedHosts();

deskhub::TrustVerdict CheckTrustedHost(const deskhub::Fingerprint& fingerprint);
bool RememberTrustedHost(const deskhub::Fingerprint& fingerprint, std::string_view label,
    std::string_view endpoint, int64_t nowUnix);
bool TouchTrustedHost(const deskhub::Fingerprint& fingerprint, std::string_view endpoint,
    int64_t nowUnix);
bool CreateTrustedHostProfile(const deskhub::TrustedHost& profile);
bool UpdateTrustedHostProfile(const deskhub::TrustedHost& expected,
    const deskhub::TrustedHost& profile);
bool ForgetTrustedHost(const deskhub::Fingerprint& fingerprint);

}
