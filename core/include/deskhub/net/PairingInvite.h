#pragma once

#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr std::string_view kPairingInvitePrefix = "deskhub://pair/";
inline constexpr uint8_t kPairingInviteVersion = 1;
inline constexpr size_t kMaxPairingEndpoints = 4;
inline constexpr size_t kMaxPairingHostNameBytes = 32;
inline constexpr size_t kMaxPairingInviteChars = 180;

struct PairingEndpoint {
    uint32_t ip = 0;
    uint16_t port = 0;

    bool operator==(const PairingEndpoint&) const = default;
};

struct PairingInvite {
    std::vector<PairingEndpoint> endpoints{};
    Fingerprint hostKey{};
    PairingToken token{};
    std::string hostName{};

    bool operator==(const PairingInvite&) const = default;
};

bool IsZero(const PairingToken& token);
bool IsPairingInvite(std::string_view text);
std::optional<PairingEndpoint> MakePairingEndpoint(std::string_view ip, uint16_t port);
std::string FormatPairingEndpoint(const PairingEndpoint& endpoint);
std::string FormatPairingInvite(const PairingInvite& invite);
std::optional<PairingInvite> ParsePairingInvite(std::string_view text);

}
