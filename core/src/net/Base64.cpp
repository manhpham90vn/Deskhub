#include "deskhub/net/Base64.h"

namespace deskhub {

namespace {

constexpr std::string_view kStandardAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::string_view kUrlAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr int kBitsPerSymbol = 6;
constexpr int kBitsPerByte = 8;
constexpr size_t kSymbolsPerGroup = 4;

std::string_view AlphabetOf(Base64Alphabet alphabet) {
    return alphabet == Base64Alphabet::Url ? kUrlAlphabet : kStandardAlphabet;
}

int SymbolValue(char symbol, std::string_view alphabet) {
    const size_t at = alphabet.find(symbol);
    return at == std::string_view::npos ? -1 : int(at);
}

size_t PaddingLength(std::string_view text) {
    if (text.ends_with("==")) return 2;
    if (text.ends_with('=')) return 1;
    return 0;
}

}

std::string EncodeBase64(std::span<const uint8_t> bytes, Base64Alphabet alphabet,
    Base64Padding padding) {
    const std::string_view symbols = AlphabetOf(alphabet);
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * kSymbolsPerGroup);
    uint32_t bits = 0;
    int count = 0;
    for (uint8_t byte : bytes) {
        bits = (bits << kBitsPerByte) | byte;
        count += kBitsPerByte;
        while (count >= kBitsPerSymbol) {
            count -= kBitsPerSymbol;
            out.push_back(symbols[(bits >> count) & 63]);
        }
    }
    if (count != 0) out.push_back(symbols[(bits << (kBitsPerSymbol - count)) & 63]);
    if (padding == Base64Padding::Equals)
        while (out.size() % kSymbolsPerGroup != 0) out.push_back('=');
    return out;
}

std::optional<std::vector<uint8_t>> DecodeBase64(std::string_view text,
    Base64Alphabet alphabet) {
    if (text.empty() || text.size() % kSymbolsPerGroup == 1) return std::nullopt;
    const size_t padding = PaddingLength(text);
    if (padding != 0 && text.size() % kSymbolsPerGroup != 0) return std::nullopt;
    const std::string_view symbols = AlphabetOf(alphabet);
    const size_t length = text.size() - padding;
    std::vector<uint8_t> out;
    out.reserve(text.size() * 3 / 4);
    uint32_t bits = 0;
    int count = 0;
    for (size_t i = 0; i < length; ++i) {
        const int value = SymbolValue(text[i], symbols);
        if (value < 0) return std::nullopt;
        bits = (bits << kBitsPerSymbol) | uint32_t(value);
        count += kBitsPerSymbol;
        if (count >= kBitsPerByte) {
            count -= kBitsPerByte;
            out.push_back(uint8_t((bits >> count) & 0xff));
        }
    }
    const bool leftoverBitsSet = count != 0 && (bits & ((1u << count) - 1)) != 0;
    const bool paddingMismatch = (padding == 1 && count != 2) || (padding == 2 && count != 4);
    if (leftoverBitsSet || paddingMismatch) return std::nullopt;
    return out;
}

}
