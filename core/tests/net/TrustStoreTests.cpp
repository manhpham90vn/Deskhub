#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/TrustStore.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

Fingerprint MakeFingerprint(uint8_t seed) {
    Fingerprint fp;
    for (size_t i = 0; i < kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(seed + i * 7 + 1);
    return fp;
}

void TestFingerprintText() {
    std::printf("[trust] a fingerprint reads back exactly as it was shown...\n");
    const Fingerprint fp = MakeFingerprint(3);
    const std::string text = FormatFingerprint(fp);
    Check(text.compare(0, kFingerprintPrefix.size(), kFingerprintPrefix) == 0,
        "the text names the hash that made it");
    Check(text.size() == kFingerprintPrefix.size() + kFingerprintTextBytes,
        "and is a fixed width the user can compare by eye");
    const auto back = ParseFingerprint(text);
    Check(back && *back == fp, "the printed form round-trips");
    Check(ParseFingerprint("  " + text + "\n").has_value(),
        "whitespace around a pasted fingerprint is forgiven");

    Check(!ParseFingerprint("").has_value(), "an empty string is not a fingerprint");
    Check(!ParseFingerprint(text.substr(1)).has_value(), "a missing prefix is refused");
    Check(!ParseFingerprint(text.substr(0, text.size() - 1)).has_value(),
        "a truncated fingerprint is refused");
    Check(!ParseFingerprint(text + "A").has_value(), "an over-long one is refused");
    Check(!ParseFingerprint(std::string(kFingerprintPrefix) +
                            std::string(kFingerprintTextBytes, '!'))
              .has_value(),
        "characters outside the alphabet are refused");
    Check(!ParseFingerprint(std::string(kFingerprintPrefix) +
                            text.substr(kFingerprintPrefix.size(), kFingerprintTextBytes - 1) + "B")
              .has_value(),
        "a last character with stray low bits cannot decode to 32 bytes");

    Fingerprint zero;
    Check(IsZero(zero), "an unset fingerprint is recognised as unset");
    Check(!IsZero(fp), "a real one is not");
}

void TestThreeTrustStates() {
    std::printf("[trust] a machine is new, known, or has changed its key...\n");
    TrustStore store;
    const Fingerprint mine = MakeFingerprint(1);
    const Fingerprint theirs = MakeFingerprint(200);

    Check(store.Check("192.168.1.10:47777", mine) == TrustVerdict::Unknown,
        "a machine we have never met is unknown");
    store.Remember("192.168.1.10:47777", "Workstation", mine, 1000);
    Check(store.Check("192.168.1.10:47777", mine) == TrustVerdict::Trusted,
        "once trusted it stays trusted");
    Check(store.Check("192.168.1.10:47777", theirs) == TrustVerdict::Changed,
        "a different key at a known address is the man-in-the-middle case");

    Check(store.Check("192.168.1.10:47777", Fingerprint{}) == TrustVerdict::Unknown,
        "a host that offered no key is never trusted");

    Check(store.Check("10.0.0.9:47777", mine) == TrustVerdict::Trusted,
        "the same machine on a new address is recognised by its key");
    Check(store.Check("10.0.0.9:47777", theirs) == TrustVerdict::Unknown,
        "an unknown key at an unknown address is simply new");
}

void TestRememberAndForget() {
    std::printf("[trust] the trusted list can be read, refreshed and emptied...\n");
    TrustStore store;
    store.Remember("a:1", "A", MakeFingerprint(1), 100);
    store.Remember("b:1", "B", MakeFingerprint(2), 200);
    Check(store.Size() == 2, "two machines are remembered");

    const auto a = store.Find("a:1");
    Check(a && a->label == "A" && a->firstSeenUnix == 100 && a->lastSeenUnix == 100,
        "the first meeting is recorded");

    store.Remember("a:1", "A renamed", MakeFingerprint(1), 500);
    const auto again = store.Find("a:1");
    Check(store.Size() == 2, "meeting it again does not add a second row");
    Check(again && again->lastSeenUnix == 500 && again->label == "A renamed",
        "the last-seen time and the name are refreshed");
    Check(again && again->firstSeenUnix == 100, "but the first meeting is not rewritten");

    store.Remember("c:1", "", MakeFingerprint(1), 600);

    Check(!store.Forget("nothing:1"), "forgetting a stranger reports nothing happened");
    Check(store.Forget("a:1") && store.Size() == 2, "forgetting a machine removes exactly it");
    Check(!store.Find("a:1").has_value(), "and it is gone from the list");

    store.Remember("", "junk", MakeFingerprint(9), 1);
    store.Remember("d:1", "junk", Fingerprint{}, 1);
    Check(store.Size() == 2, "a blank address or an unset key is never stored");

    store.Clear();
    Check(store.Size() == 0 && store.Hosts().empty(), "the whole list can be emptied");
}

void TestCapEvictsOldest() {
    std::printf("[trust] the list is capped and drops the machine seen longest ago...\n");
    TrustStore store;
    for (size_t i = 0; i < kMaxTrustedHosts; ++i)
        store.Remember("host" + std::to_string(i) + ":1", "", MakeFingerprint(uint8_t(i)),
            int64_t(1000 + i));
    Check(store.Size() == kMaxTrustedHosts, "the list fills to the cap");

    Fingerprint unseen = MakeFingerprint(1);
    unseen.bytes[0] ^= 0xFF;
    store.Remember("newcomer:1", "", unseen, 9999);
    Check(store.Size() == kMaxTrustedHosts, "and never grows past it");
    Check(!store.Find("host0:1").has_value(), "the least recently seen machine is dropped");
    Check(store.Find("newcomer:1").has_value(), "the newcomer is kept");
}

void TestSerializeRoundTrip() {
    std::printf("[trust] the known-hosts file survives a round trip...\n");
    TrustStore store;
    store.Remember("192.168.1.10:47777", "Workstation", MakeFingerprint(5), 111);
    store.Remember("10.0.0.9:47777", "", MakeFingerprint(6), 222);
    store.Remember("[fe80::1]:47777", "Laptop with spaces", MakeFingerprint(7), 333);

    const std::string text = SerializeTrustStore(store);
    const TrustStore back = ParseTrustStore(text);
    Check(back.Size() == store.Size(), "every row comes back");
    Check(SerializeTrustStore(back) == text, "serialising is a fixpoint");

    const auto one = back.Find("192.168.1.10:47777");
    Check(one && one->label == "Workstation" && one->fingerprint == MakeFingerprint(5),
        "address, name and key all survive");
    Check(one && one->firstSeenUnix == 111 && one->lastSeenUnix == 111,
        "and so do both timestamps");
    const auto spaced = back.Find("[fe80::1]:47777");
    Check(spaced && spaced->label == "Laptop with spaces",
        "a name with spaces is kept whole because it is the last field");

    Check(ParseTrustStore("").Size() == 0, "an empty file yields an empty list");
}

void TestParseJunk() {
    std::printf("[trust] a hand-edited or corrupt known-hosts file cannot break us...\n");
    const std::string good = FormatFingerprint(MakeFingerprint(4)) + " 1 2 host:1 Name\n";
    Check(ParseTrustStore(good).Size() == 1, "a well-formed line parses");

    const char* junk[] = {
        "\n\n\n",
        "# a comment line\n",
        "not-a-fingerprint 1 2 host:1\n",
        "SHA256:short 1 2 host:1\n",
        "onlyonefield\n",
        "two fields\n",
    };
    for (const char* text : junk)
        Check(ParseTrustStore(text).Size() == 0, "a malformed line is dropped, not trusted");

    Check(ParseTrustStore(FormatFingerprint(MakeFingerprint(4)) + " x 2 host:1\n").Size() == 0,
        "a non-numeric timestamp drops the line");
    Check(ParseTrustStore(FormatFingerprint(MakeFingerprint(4)) + " 1 y host:1\n").Size() == 0,
        "so does a non-numeric last-seen");
    Check(ParseTrustStore(FormatFingerprint(MakeFingerprint(4)) + " 1 2\n").Size() == 0,
        "a line with no address drops too");

    const TrustStore mixed = ParseTrustStore(std::string("garbage\n") + good + "more garbage\n");
    Check(mixed.Size() == 1, "good lines survive alongside bad ones");

    const std::string control =
        FormatFingerprint(MakeFingerprint(8)) + " 1 2 host:2 na\x01me\x7f\n";
    const auto host = ParseTrustStore(control).Find("host:2");
    Check(host && host->label == "name", "control characters are stripped from a stored name");

    std::string over = FormatFingerprint(MakeFingerprint(9)) + " 1 2 host:3 ";
    over.append(kMaxTrustLabelBytes + 40, 'x');
    over += '\n';
    const auto capped = ParseTrustStore(over).Find("host:3");
    Check(capped && capped->label.size() == kMaxTrustLabelBytes, "and a long one is bounded");

    for (int i = 0; i < 400; ++i) {
        std::string soup(Rnd() % 120, ' ');
        for (char& c : soup) c = char(Rnd() % 96 + 32);
        const TrustStore parsed = ParseTrustStore(soup);
        Check(SerializeTrustStore(ParseTrustStore(SerializeTrustStore(parsed))) ==
                  SerializeTrustStore(parsed),
            "parsing junk is stable under a second round trip");
    }
}

}

void RunTrustStoreTests() {
    TestFingerprintText();
    TestThreeTrustStates();
    TestRememberAndForget();
    TestCapEvictsOldest();
    TestSerializeRoundTrip();
    TestParseJunk();
}
