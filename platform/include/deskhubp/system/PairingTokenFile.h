#pragma once

#include "deskhub/auth/PairingTokens.h"

#include <cstdint>
#include <optional>
#include <span>

namespace deskhubp {

inline constexpr const char* kPairingTokensFileName = "pairing_tokens";

std::optional<deskhub::PairingToken> IssuePairingToken(int64_t ttlSeconds);
bool RedeemPairingToken(std::span<const uint8_t> presented);
bool RevokePairingTokens();

}
