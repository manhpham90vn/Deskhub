#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/PublicKeyText.h"
#include "deskhub/net/AuthorizedKeys.h"

#include <cstdio>

namespace {

void AddString(std::vector<uint8_t>& output, std::string_view value) {
    const uint32_t length = uint32_t(value.size());
    output.push_back(uint8_t(length >> 24));
    output.push_back(uint8_t(length >> 16));
    output.push_back(uint8_t(length >> 8));
    output.push_back(uint8_t(length));
    output.insert(output.end(), value.begin(), value.end());
}

deskhub::PublicKeyText Ed25519Key() {
    deskhub::PublicKeyText key;
    key.algorithm = deskhub::PublicKeyAlgorithm::Ed25519;
    AddString(key.blob, "ssh-ed25519");
    AddString(key.blob, std::string(32, 'x'));
    key.label = "laptop a";
    return key;
}

deskhub::PublicKeyText P256Key() {
    deskhub::PublicKeyText key;
    key.algorithm = deskhub::PublicKeyAlgorithm::EcdsaP256;
    AddString(key.blob, "ecdsa-sha2-nistp256");
    AddString(key.blob, "nistp256");
    std::string point(65, 'x');
    point[0] = 4;
    AddString(key.blob, point);
    return key;
}

void TestValidKeysRoundTrip() {
    std::printf("[public key] OpenSSH text round trips without changing the key...\n");
    for (const auto& original : {Ed25519Key(), P256Key()}) {
        const std::string text = deskhub::FormatPublicKeyText(original);
        const auto parsed = deskhub::ParsePublicKeyText(text);
        Check(parsed.has_value(), "a valid supported key parses");
        Check(parsed && parsed->blob == original.blob &&
                  parsed->algorithm == original.algorithm && parsed->label == original.label,
            "type, key bytes and label survive");
        Check(parsed && deskhub::FormatPublicKeyText(*parsed) == text,
            "formatting a parsed key is stable");
    }
}

void TestMalformedKeysAreRejected() {
    std::printf("[public key] malformed or ambiguous key text is rejected...\n");
    const std::string good = deskhub::FormatPublicKeyText(Ed25519Key());
    Check(!deskhub::ParsePublicKeyText(""), "an empty key is invalid");
    Check(!deskhub::ParsePublicKeyText("ssh-rsa AAAA"), "unsupported key type is invalid");
    Check(!deskhub::ParsePublicKeyText("ssh-ed25519 AAAA"),
        "the blob must contain a matching algorithm and 32-byte key");
    Check(!deskhub::ParsePublicKeyText(good + "\nssh-ed25519 AAAA"),
        "multiple lines cannot be accepted as one key");
    Check(!deskhub::ParsePublicKeyText(good + "\tbad"),
        "control characters in the label are invalid");
    Check(!deskhub::ParsePublicKeyText(std::string(2049, 'a')),
        "an oversized record is invalid");

    auto mismatched = Ed25519Key();
    mismatched.algorithm = deskhub::PublicKeyAlgorithm::EcdsaP256;
    Check(deskhub::FormatPublicKeyText(mismatched).empty(),
        "the text type cannot disagree with the key blob");
    auto trailing = Ed25519Key();
    trailing.blob.push_back(0);
    Check(deskhub::FormatPublicKeyText(trailing).empty(),
        "trailing data in the key blob is invalid");
}

void TestAuthorizedKeysRejectDuplicatesAndDamage() {
    std::printf("[public key] authorized keys reject duplicates and damaged rows...\n");
    const std::string ed = deskhub::FormatPublicKeyText(Ed25519Key());
    const std::string p256 = deskhub::FormatPublicKeyText(P256Key());
    const auto keys = deskhub::ParseAuthorizedKeys("# managed list\n" + ed + "\n" + p256 + "\n");
    Check(keys && keys->Keys().size() == 2 && keys->Contains(Ed25519Key()),
        "two valid key types are retained");
    Check(keys && deskhub::ParseAuthorizedKeys(deskhub::SerializeAuthorizedKeys(*keys))
                          ->Keys()
                          .size() == 2,
        "the canonical list round trips");
    auto duplicate = Ed25519Key();
    duplicate.label = "another label";
    Check(!deskhub::ParseAuthorizedKeys(ed + "\n" +
                                        deskhub::FormatPublicKeyText(duplicate) + "\n"),
        "a different label cannot disguise a duplicate key");
    Check(!deskhub::ParseAuthorizedKeys(ed + "\nssh-rsa AAAA\n"),
        "one unsupported row invalidates the whole list");
    Check(!deskhub::ParseAuthorizedKeys(std::string(deskhub::kMaxAuthorizedKeysFileBytes + 1,
              'x')),
        "an oversized list is rejected");
    Check(deskhub::ParseAuthorizedKeys("")->Keys().empty(),
        "an empty authorized list authorizes nobody");
}

}

void RunPublicKeyTextTests() {
    TestValidKeysRoundTrip();
    TestMalformedKeysAreRejected();
    TestAuthorizedKeysRejectDuplicatesAndDamage();
}
