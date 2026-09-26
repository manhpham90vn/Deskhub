#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

enum class PublicKeyAlgorithm : uint8_t {
    Ed25519,
    EcdsaP256,
};

struct PublicKeyText {
    PublicKeyAlgorithm algorithm = PublicKeyAlgorithm::Ed25519;
    std::vector<uint8_t> blob{};
    std::string label{};
};

std::optional<PublicKeyText> ParsePublicKeyText(std::string_view text);
std::string FormatPublicKeyText(const PublicKeyText& key);

}
