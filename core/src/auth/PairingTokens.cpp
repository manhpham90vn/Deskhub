#include "deskhub/auth/PairingTokens.h"

#include "deskhub/net/Base64.h"
#include "deskhub/net/PairingInvite.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr size_t kMaxPairingTokenLineBytes = 96;

uint8_t ConstantTimeDifference(std::span<const uint8_t> a, const PairingToken& b) {
    uint8_t difference = 0;
    for (size_t i = 0; i < b.size(); ++i) difference |= uint8_t(a[i] ^ b[i]);
    return difference;
}

bool ParseUnixTime(std::string_view text, int64_t& out) {
    if (text.empty() || text.size() > 19) return false;
    int64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

}

bool PairingTokens::Issue(const PairingToken& token, int64_t expiresUnix, int64_t nowUnix) {
    Expire(nowUnix);
    if (IsZero(token) || expiresUnix <= nowUnix) return false;
    for (const IssuedPairingToken& issued : tokens_)
        if (issued.token == token) return false;
    if (tokens_.size() >= kMaxPairingTokens) {
        const auto soonest = std::min_element(tokens_.begin(), tokens_.end(),
            [](const IssuedPairingToken& a, const IssuedPairingToken& b) {
                return a.expiresUnix < b.expiresUnix;
            });
        tokens_.erase(soonest);
    }
    tokens_.push_back(IssuedPairingToken{token, expiresUnix});
    return true;
}

bool PairingTokens::Consume(std::span<const uint8_t> presented, int64_t nowUnix) {
    Expire(nowUnix);
    if (presented.size() != kPairingTokenBytes) return false;
    size_t matched = tokens_.size();
    for (size_t i = 0; i < tokens_.size(); ++i) {
        const uint8_t difference = ConstantTimeDifference(presented, tokens_[i].token);
        const size_t same = size_t(difference == 0);
        matched = same * i + (1 - same) * matched;
    }
    if (matched == tokens_.size()) return false;
    tokens_.erase(tokens_.begin() + ptrdiff_t(matched));
    return true;
}

size_t PairingTokens::Expire(int64_t nowUnix) {
    const size_t before = tokens_.size();
    std::erase_if(tokens_, [&](const IssuedPairingToken& issued) {
        return issued.expiresUnix <= nowUnix;
    });
    return before - tokens_.size();
}

void PairingTokens::Clear() {
    tokens_.clear();
}

std::optional<PairingTokens> ParsePairingTokens(std::string_view text, int64_t nowUnix) {
    if (text.size() > kMaxPairingTokensFileBytes) return std::nullopt;
    PairingTokens tokens;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) continue;
        if (line.size() > kMaxPairingTokenLineBytes) return std::nullopt;
        const size_t space = line.find(' ');
        if (space == std::string_view::npos) return std::nullopt;
        const std::optional<std::vector<uint8_t>> bytes =
            DecodeBase64(line.substr(0, space), Base64Alphabet::Url);
        int64_t expiresUnix = 0;
        if (!bytes || bytes->size() != kPairingTokenBytes ||
            !ParseUnixTime(line.substr(space + 1), expiresUnix))
            return std::nullopt;
        PairingToken token{};
        std::copy(bytes->begin(), bytes->end(), token.begin());
        if (expiresUnix <= nowUnix) continue;
        if (!tokens.Issue(token, expiresUnix, nowUnix)) return std::nullopt;
    }
    return tokens;
}

std::string SerializePairingTokens(const PairingTokens& tokens) {
    std::string out;
    for (const IssuedPairingToken& issued : tokens.Tokens()) {
        out += EncodeBase64(issued.token, Base64Alphabet::Url, Base64Padding::None);
        out += ' ';
        out += std::to_string(issued.expiresUnix);
        out += '\n';
    }
    return out;
}

}
