#pragma once

#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxPairingTokens = 4;
inline constexpr int64_t kPairingTokenTtlSeconds = 300;
inline constexpr size_t kMaxPairingTokensFileBytes = 4096;

struct IssuedPairingToken {
    PairingToken token{};
    int64_t expiresUnix = 0;

    bool operator==(const IssuedPairingToken&) const = default;
};

class PairingTokens {
public:
    bool Issue(const PairingToken& token, int64_t expiresUnix, int64_t nowUnix);
    bool Consume(std::span<const uint8_t> presented, int64_t nowUnix);
    size_t Expire(int64_t nowUnix);
    void Clear();
    const std::vector<IssuedPairingToken>& Tokens() const {
        return tokens_;
    }

private:
    std::vector<IssuedPairingToken> tokens_{};
};

std::optional<PairingTokens> ParsePairingTokens(std::string_view text, int64_t nowUnix);
std::string SerializePairingTokens(const PairingTokens& tokens);

}
