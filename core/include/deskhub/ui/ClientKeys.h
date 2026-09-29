#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace deskhub::ui {

inline constexpr size_t kMaxClientKeyNameBytes = 64;

enum class ClientKeyError : uint8_t {
    None = 0,
    InvalidName = 1,
    NameInUse = 2,
    UnreadableKey = 3,
    WriteFailed = 4,
    DefaultKey = 5,
    KeyInUse = 6,
    KeyMissing = 7,
};

bool IsValidClientKeyName(std::string_view name);
std::string ClientKeyLabel(std::string_view deviceName, std::string_view keyName);
const char* ClientKeyErrorText(ClientKeyError error);

}
