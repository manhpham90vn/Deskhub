#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

enum class Base64Alphabet : uint8_t {
    Standard,
    Url,
};

enum class Base64Padding : uint8_t {
    None,
    Equals,
};

std::string EncodeBase64(std::span<const uint8_t> bytes,
    Base64Alphabet alphabet = Base64Alphabet::Standard,
    Base64Padding padding = Base64Padding::Equals);
std::optional<std::vector<uint8_t>> DecodeBase64(std::string_view text,
    Base64Alphabet alphabet = Base64Alphabet::Standard);

}
