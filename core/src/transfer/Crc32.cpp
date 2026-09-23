#include "deskhub/transfer/Crc32.h"

#include <array>

namespace deskhub {

namespace {

inline constexpr uint32_t kCrc32Poly = 0xEDB88320u;

constexpr std::array<uint32_t, 256> MakeTable() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int bit = 0; bit < 8; ++bit) c = (c & 1u) ? (kCrc32Poly ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
    return table;
}

constexpr std::array<uint32_t, 256> kCrc32Table = MakeTable();

}

void Crc32::Update(std::span<const uint8_t> bytes) {
    uint32_t c = state_;
    for (uint8_t b : bytes) c = kCrc32Table[(c ^ b) & 0xFFu] ^ (c >> 8);
    state_ = c;
}

}
