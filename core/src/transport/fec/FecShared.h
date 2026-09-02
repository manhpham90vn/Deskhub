#pragma once
#include "deskhub/transport/FecScheme.h"

#include <algorithm>
#include <cstdint>
#include <span>

namespace deskhub::fec {

inline size_t LongestMember(std::span<const std::span<const uint8_t>> group) {
    size_t longest = 0;
    for (const std::span<const uint8_t>& member : group) {
        if (member.empty() || member.size() > kMaxVideoPayload) return 0;
        longest = std::max(longest, member.size());
    }
    return longest;
}

inline void AccumulateMember(std::span<uint8_t> into, std::span<const uint8_t> member) {
    into[0] ^= uint8_t(member.size() >> 8);
    into[1] ^= uint8_t(member.size() & 0xFF);
    for (size_t b = 0; b < member.size(); ++b) into[kFecLenPrefix + b] ^= member[b];
}

inline size_t LengthPrefixOf(std::span<const uint8_t> symbol) {
    return (size_t(symbol[0]) << 8) | symbol[1];
}

}
