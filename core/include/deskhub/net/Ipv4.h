#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace deskhub {

std::optional<uint32_t> ParseIPv4(std::string_view text);
std::string FormatIPv4(uint32_t ip);

}
