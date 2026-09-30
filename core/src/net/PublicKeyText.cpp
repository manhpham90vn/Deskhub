#include "deskhub/net/PublicKeyText.h"

#include "deskhub/net/Base64.h"

namespace deskhub {

namespace {

constexpr std::string_view kEd25519 = "ssh-ed25519";
constexpr std::string_view kEcdsaP256 = "ecdsa-sha2-nistp256";
constexpr std::string_view kCurve = "nistp256";
constexpr size_t kMaxTextBytes = 2048;
constexpr size_t kMaxLabelBytes = 64;

std::string_view Trim(std::string_view text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::optional<std::span<const uint8_t>> ReadString(std::span<const uint8_t>& input) {
    if (input.size() < 4) return std::nullopt;
    const uint32_t length = (uint32_t(input[0]) << 24) | (uint32_t(input[1]) << 16) |
                            (uint32_t(input[2]) << 8) | uint32_t(input[3]);
    input = input.subspan(4);
    if (length > input.size()) return std::nullopt;
    const auto value = input.first(length);
    input = input.subspan(length);
    return value;
}

bool Equals(std::span<const uint8_t> bytes, std::string_view text) {
    if (bytes.size() != text.size()) return false;
    for (size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] != uint8_t(text[i])) return false;
    return true;
}

bool ValidBlob(const PublicKeyText& key) {
    std::span<const uint8_t> input(key.blob);
    const auto algorithm = ReadString(input);
    if (!algorithm) return false;
    if (key.algorithm == PublicKeyAlgorithm::Ed25519) {
        const auto bytes = ReadString(input);
        return Equals(*algorithm, kEd25519) && bytes && bytes->size() == 32 && input.empty();
    }
    const auto curve = ReadString(input);
    const auto point = ReadString(input);
    return Equals(*algorithm, kEcdsaP256) && curve && Equals(*curve, kCurve) && point &&
           point->size() == 65 && (*point)[0] == 4 && input.empty();
}

}

std::optional<PublicKeyText> ParsePublicKeyText(std::string_view text) {
    if (text.empty() || text.size() > kMaxTextBytes) return std::nullopt;
    text = Trim(text);
    const size_t typeEnd = text.find_first_of(" \t");
    if (typeEnd == std::string_view::npos) return std::nullopt;
    const std::string_view type = text.substr(0, typeEnd);
    if (type != kEd25519 && type != kEcdsaP256) return std::nullopt;
    text.remove_prefix(typeEnd);
    text = Trim(text);
    const size_t bodyEnd = text.find_first_of(" \t");
    const std::string_view body = text.substr(0, bodyEnd);
    if (bodyEnd != std::string_view::npos)
        text.remove_prefix(bodyEnd);
    else
        text = {};
    text = Trim(text);
    if (text.size() > kMaxLabelBytes) return std::nullopt;
    for (char c : text)
        if (uint8_t(c) < 32 || uint8_t(c) == 127) return std::nullopt;

    const auto blob = DecodeBase64(body);
    if (!blob) return std::nullopt;
    PublicKeyText key;
    key.algorithm = type == kEd25519 ? PublicKeyAlgorithm::Ed25519
                                     : PublicKeyAlgorithm::EcdsaP256;
    key.blob = *blob;
    key.label = text;
    if (!ValidBlob(key)) return std::nullopt;
    return key;
}

std::string FormatPublicKeyText(const PublicKeyText& key) {
    if (!ValidBlob(key) || key.label.size() > kMaxLabelBytes) return {};
    for (char c : key.label)
        if (uint8_t(c) < 32 || uint8_t(c) == 127) return {};
    const std::string_view type = key.algorithm == PublicKeyAlgorithm::Ed25519 ? kEd25519
                                                                               : kEcdsaP256;
    std::string text(type);
    text += ' ';
    text += EncodeBase64(key.blob);
    if (!key.label.empty()) {
        text += ' ';
        text += key.label;
    }
    return text;
}

}
