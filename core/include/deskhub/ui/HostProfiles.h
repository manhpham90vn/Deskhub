#pragma once
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace deskhub::ui {

enum class HostProfileError : uint8_t {
    None = 0,
    InvalidAlias = 1,
    InvalidAddress = 2,
    MissingHostKey = 3,
    InvalidHostKey = 4,
    AliasExists = 6,
    AliasMissing = 7,
    AliasAmbiguous = 8,
    StoreUnreadable = 10,
    StoreFull = 11,
    WriteFailed = 12,
    KeyExists = 13,
};

enum class HostProfileMode : uint8_t {
    Add,
    Update,
};

struct HostProfileRequest {
    std::string alias{};
    std::string address{};
    std::optional<Fingerprint> hostKey{};
};

struct HostProfileLookup {
    HostProfileError error = HostProfileError::None;
    std::optional<TrustedHost> host{};
};

struct HostProfilePlan {
    HostProfileError error = HostProfileError::None;
    std::optional<TrustedHost> existing{};
    TrustedHost profile{};
};

bool IsValidHostAlias(std::string_view alias);
std::optional<std::string> CanonicalHostEndpoint(std::string_view address);
std::string SuggestHostAlias(const TrustStore& store, std::string_view endpoint);
HostProfileLookup FindHostProfile(const TrustStore& store, std::string_view alias);
HostProfilePlan PlanHostProfile(const TrustStore& store, HostProfileMode mode,
    const HostProfileRequest& request);
const char* HostProfileErrorText(HostProfileError error);

}
