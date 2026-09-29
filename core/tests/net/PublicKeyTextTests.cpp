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

void TestKeyTextIsNormalised() {
    std::printf("[public key] pasted key text is normalised to one canonical line...\n");
    const auto original = Ed25519Key();
    const std::string canonical = deskhub::FormatPublicKeyText(original);
    const std::string body = canonical.substr(0, canonical.find(" laptop a"));
    const auto messy = deskhub::ParsePublicKeyText("  \t" + body + " \t  laptop a \r\n");
    Check(messy && messy->blob == original.blob && messy->label == "laptop a",
        "surrounding and separating whitespace does not change the key or its label");
    Check(messy && deskhub::FormatPublicKeyText(*messy) == canonical,
        "and it is written back in the canonical single-space form");
    const auto unlabelled = deskhub::ParsePublicKeyText(body);
    Check(unlabelled && unlabelled->label.empty() &&
              deskhub::FormatPublicKeyText(*unlabelled) == body,
        "a key without a comment has an empty label and no trailing space");

    const auto list = deskhub::ParseAuthorizedKeys("\r\n# a comment\r\n\r\n" + canonical +
                                                   "\r\n");
    Check(list && list->Keys().size() == 1,
        "CRLF line endings, blank lines and comments are accepted in authorized_keys");
    Check(list && deskhub::SerializeAuthorizedKeys(*list) == canonical + "\n",
        "and the list is rewritten without them");
}

void TestDamagedKeyBytesAreRejected() {
    std::printf("[public key] damaged key bytes and labels are refused...\n");
    const std::string canonical = deskhub::FormatPublicKeyText(Ed25519Key());
    const std::string body = canonical.substr(0, canonical.find(" laptop a"));

    std::string foreign = body;
    foreign[20] = '!';
    Check(!deskhub::ParsePublicKeyText(foreign),
        "a character outside the base64 alphabet is refused");
    Check(!deskhub::ParsePublicKeyText(body.substr(0, body.size() - 4)),
        "a truncated key blob is refused");
    Check(!deskhub::ParsePublicKeyText("ssh-ed25519"), "a type with no key is refused");
    Check(!deskhub::ParsePublicKeyText("ecdsa-sha2-nistp256 " + body.substr(12)),
        "the text type must name the algorithm inside the blob");
    Check(!deskhub::ParsePublicKeyText(body + " " + std::string(65, 'x')),
        "an over-long label is refused");
    Check(deskhub::ParsePublicKeyText(body + " " + std::string(64, 'x')).has_value(),
        "a label at the limit is kept");
    Check(!deskhub::ParsePublicKeyText(body + " a\x7f" + "b"),
        "a DEL character in the label is refused");
    Check(!deskhub::ParsePublicKeyText(body + " a\x01" + "b"),
        "a control character in the label is refused");

    auto compressed = P256Key();
    compressed.blob[compressed.blob.size() - 65] = 2;
    Check(deskhub::FormatPublicKeyText(compressed).empty(),
        "a compressed P-256 point is not a supported key");
    auto otherCurve = deskhub::PublicKeyText{};
    otherCurve.algorithm = deskhub::PublicKeyAlgorithm::EcdsaP256;
    AddString(otherCurve.blob, "ecdsa-sha2-nistp256");
    AddString(otherCurve.blob, "nistp384");
    std::string point(65, 'x');
    point[0] = 4;
    AddString(otherCurve.blob, point);
    Check(deskhub::FormatPublicKeyText(otherCurve).empty(),
        "a P-256 key type naming another curve is refused");
    auto shortKey = deskhub::PublicKeyText{};
    AddString(shortKey.blob, "ssh-ed25519");
    AddString(shortKey.blob, std::string(31, 'x'));
    Check(deskhub::FormatPublicKeyText(shortKey).empty(), "a 31-byte Ed25519 key is refused");
    auto overstated = deskhub::PublicKeyText{};
    AddString(overstated.blob, "ssh-ed25519");
    overstated.blob.insert(overstated.blob.end(), {0, 0, 1, 0});
    overstated.blob.insert(overstated.blob.end(), 32, 'x');
    Check(deskhub::FormatPublicKeyText(overstated).empty(),
        "a length prefix longer than the data is refused");
}

deskhub::PublicKeyText NumberedKey(size_t index) {
    auto key = Ed25519Key();
    key.blob[key.blob.size() - 1] = uint8_t(index);
    key.blob[key.blob.size() - 2] = uint8_t(index >> 8);
    key.label = {};
    return key;
}

void TestAuthorizedKeysAreBounded() {
    std::printf("[public key] the authorized list is bounded and edits by key...\n");
    deskhub::AuthorizedKeys keys;
    for (size_t i = 0; i < deskhub::kMaxAuthorizedKeys; ++i)
        Check(keys.Add(NumberedKey(i)), "keys up to the limit are accepted");
    Check(!keys.Add(NumberedKey(deskhub::kMaxAuthorizedKeys)),
        "one more key than the limit is refused");
    const std::string full = deskhub::SerializeAuthorizedKeys(keys);
    Check(deskhub::ParseAuthorizedKeys(full).has_value(), "a full list reads back");
    Check(!deskhub::ParseAuthorizedKeys(
              full + deskhub::FormatPublicKeyText(NumberedKey(deskhub::kMaxAuthorizedKeys)) +
              "\n"),
        "a file with more keys than the limit is refused as a whole");

    auto relabelled = NumberedKey(3);
    relabelled.label = "renamed";
    Check(!keys.Add(relabelled), "the same key under another label is a duplicate");
    Check(keys.Remove(relabelled), "removal matches the key bytes, not the label");
    Check(!keys.Contains(NumberedKey(3)), "and the key is gone");
    Check(!keys.Remove(NumberedKey(3)), "removing it twice reports nothing removed");

    deskhub::PublicKeyText invalid;
    Check(!keys.Add(invalid), "an empty key blob is never added");
}

}

void RunPublicKeyTextTests() {
    TestValidKeysRoundTrip();
    TestMalformedKeysAreRejected();
    TestAuthorizedKeysRejectDuplicatesAndDamage();
    TestKeyTextIsNormalised();
    TestDamagedKeyBytesAreRejected();
    TestAuthorizedKeysAreBounded();
}
