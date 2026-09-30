#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/Base64.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

std::vector<uint8_t> Bytes(std::string_view text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

void TestKnownVectors() {
    std::printf("[base64] the RFC 4648 vectors encode and decode...\n");
    Check(EncodeBase64(Bytes("")).empty(), "nothing encodes to nothing");
    Check(EncodeBase64(Bytes("f")) == "Zg==", "one byte pads twice");
    Check(EncodeBase64(Bytes("fo")) == "Zm8=", "two bytes pad once");
    Check(EncodeBase64(Bytes("foo")) == "Zm9v", "three bytes need no padding");
    Check(EncodeBase64(Bytes("foob")) == "Zm9vYg==", "four bytes");
    Check(EncodeBase64(Bytes("foobar")) == "Zm9vYmFy", "six bytes");
    Check(EncodeBase64(Bytes("fo"), Base64Alphabet::Standard, Base64Padding::None) == "Zm8",
        "padding can be left out");
    for (const char* text : {"f", "fo", "foo", "foob", "fooba", "foobar"}) {
        const auto padded = DecodeBase64(EncodeBase64(Bytes(text)));
        const auto bare = DecodeBase64(
            EncodeBase64(Bytes(text), Base64Alphabet::Standard, Base64Padding::None));
        Check(padded && *padded == Bytes(text), "a padded string round-trips");
        Check(bare && *bare == Bytes(text), "an unpadded string round-trips too");
    }
}

void TestUrlAlphabet() {
    std::printf("[base64] the URL alphabet swaps + and / for - and _...\n");
    const std::vector<uint8_t> bytes = {0xFB, 0xFF, 0xBF, 0x3E, 0x3F};
    const std::string standard = EncodeBase64(bytes);
    const std::string url = EncodeBase64(bytes, Base64Alphabet::Url, Base64Padding::None);
    Check(standard.find('+') != std::string::npos || standard.find('/') != std::string::npos,
        "the sample exercises the two differing symbols");
    Check(url.find('+') == std::string::npos && url.find('/') == std::string::npos &&
              url.find('=') == std::string::npos,
        "a URL-safe string has none of the characters a URL would mangle");
    const auto back = DecodeBase64(url, Base64Alphabet::Url);
    Check(back && *back == bytes, "and it decodes with the same alphabet");
    Check(!DecodeBase64(url, Base64Alphabet::Standard).has_value() ||
              *DecodeBase64(url, Base64Alphabet::Standard) != bytes,
        "the alphabets are not interchangeable");
}

void TestRejectsDamage() {
    std::printf("[base64] damaged text is refused rather than half-decoded...\n");
    Check(!DecodeBase64("").has_value(), "empty text is not a value");
    Check(!DecodeBase64("Z").has_value(), "a lone symbol cannot hold a byte");
    Check(!DecodeBase64("Zm9v!").has_value(), "a symbol outside the alphabet is refused");
    Check(!DecodeBase64("Zh==").has_value(), "leftover bits must be zero");
    Check(!DecodeBase64("Zm8==").has_value(), "padding must match the length");
    Check(!DecodeBase64("Zg=").has_value(), "one padding symbol needs a two-symbol tail");
    Check(!DecodeBase64("Zm=9").has_value(), "padding in the middle is refused");
}

}

void RunBase64Tests() {
    TestKnownVectors();
    TestUrlAlphabet();
    TestRejectsDamage();
}
