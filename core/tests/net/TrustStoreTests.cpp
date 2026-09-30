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

void TestTrustFollowsTheKey() {
    std::printf("[trust] a machine is known by its key, not by its address...\n");
    TrustStore store;
    const Fingerprint mine = MakeFingerprint(1);
    const Fingerprint theirs = MakeFingerprint(200);

    Check(store.Check(mine) == TrustVerdict::Unknown, "a machine we have never met is unknown");
    store.Remember(mine, "Workstation", "192.168.1.10:47777", 1000);
    Check(store.Check(mine) == TrustVerdict::Trusted, "once trusted it stays trusted");
    Check(store.Check(theirs) == TrustVerdict::Unknown,
        "a different key is simply another machine, wherever it answers from");
    Check(store.Check(Fingerprint{}) == TrustVerdict::Unknown,
        "a host that offered no key is never trusted");

    Check(store.Touch(mine, "10.0.0.9:47777", 2000), "the same key at a new address is touched");
    const auto moved = store.Find(mine);
    Check(moved && moved->endpoint == "10.0.0.9:47777" && moved->lastSeenUnix == 2000,
        "and the row now remembers the new address as the last one it answered at");
    Check(moved && moved->label == "Workstation" && moved->firstSeenUnix == 1000,
        "without touching its name or first meeting");
    Check(!store.Touch(theirs, "10.0.0.9:47777", 2000), "touching a stranger changes nothing");

    const auto previous = store.FindByEndpoint("10.0.0.9:47777");
    Check(previous && previous->fingerprint == mine,
        "the store can say which machine last answered at an address");
    Check(!store.FindByEndpoint("172.16.0.1:1").has_value(), "and that no one did at a new one");
}

void TestRememberAndForget() {
    std::printf("[trust] the trusted list can be read, refreshed and emptied...\n");
    TrustStore store;
    store.Remember(MakeFingerprint(1), "A", "a:1", 100);
    store.Remember(MakeFingerprint(2), "B", "b:1", 200);
    Check(store.Size() == 2, "two machines are remembered");

    const auto a = store.Find(MakeFingerprint(1));
    Check(a && a->label == "A" && a->firstSeenUnix == 100 && a->lastSeenUnix == 100,
        "the first meeting is recorded");

    store.Remember(MakeFingerprint(1), "A renamed", "a:2", 500);
    const auto again = store.Find(MakeFingerprint(1));
    Check(store.Size() == 2, "meeting it again does not add a second row");
    Check(again && again->lastSeenUnix == 500 && again->label == "A renamed" &&
              again->endpoint == "a:2",
        "the last-seen time, the name and the address are refreshed");
    Check(again && again->firstSeenUnix == 100, "but the first meeting is not rewritten");

    store.Remember(MakeFingerprint(1), "", "", 600);
    const auto kept = store.Find(MakeFingerprint(1));
    Check(kept && kept->label == "A renamed" && kept->endpoint == "a:2",
        "meeting it without a name or address keeps what was known");

    Check(!store.Forget(MakeFingerprint(9)), "forgetting a stranger reports nothing happened");
    Check(store.Forget(MakeFingerprint(1)) && store.Size() == 1,
        "forgetting a machine removes exactly it");
    Check(!store.Find(MakeFingerprint(1)).has_value(), "and it is gone from the list");

    store.Remember(Fingerprint{}, "junk", "d:1", 1);
    Check(store.Size() == 1, "an unset key is never stored");
    Check(store.SetLabel(MakeFingerprint(2), "Renamed"), "a machine can be renamed");
    Check(!store.SetLabel(MakeFingerprint(2), "bad\x01name"), "but not to a damaged name");
    Check(!store.SetLabel(MakeFingerprint(7), "x"), "and a stranger cannot be renamed");

    store.Clear();
    Check(store.Size() == 0 && store.Hosts().empty(), "the whole list can be emptied");
}

void TestCapEvictsOldest() {
    std::printf("[trust] the list is capped and drops the machine seen longest ago...\n");
    TrustStore store;
    for (size_t i = 0; i < kMaxTrustedHosts; ++i) {
        Fingerprint fp = MakeFingerprint(uint8_t(i));
        fp.bytes[31] = uint8_t(i >> 8);
        store.Remember(fp, "", "host" + std::to_string(i) + ":1", int64_t(1000 + i));
    }
    Check(store.Size() == kMaxTrustedHosts, "the list fills to the cap");

    Fingerprint unseen = MakeFingerprint(1);
    unseen.bytes[0] ^= 0xFF;
    store.Remember(unseen, "", "newcomer:1", 9999);
    Check(store.Size() == kMaxTrustedHosts, "and never grows past it");
    Check(!store.FindByEndpoint("host0:1").has_value(), "the least recently seen machine is dropped");
    Check(store.Find(unseen).has_value(), "the newcomer is kept");
}

void TestSerializeRoundTrip() {
    std::printf("[trust] the known-hosts file survives a round trip...\n");
    TrustStore store;
    store.Remember(MakeFingerprint(5), "Workstation", "192.168.1.10:47777", 111);
    store.Remember(MakeFingerprint(6), "", "10.0.0.9:47777", 222);
    store.Remember(MakeFingerprint(7), "Laptop with spaces", "[fe80::1]:47777", 333);

    const std::string text = SerializeTrustStore(store);
    const TrustStore back = ParseTrustStore(text);
    Check(back.Size() == store.Size(), "every row comes back");
    Check(SerializeTrustStore(back) == text, "serialising is a fixpoint");

    const auto one = back.Find(MakeFingerprint(5));
    Check(one && one->label == "Workstation" && one->endpoint == "192.168.1.10:47777",
        "address, name and key all survive");
    Check(one && one->firstSeenUnix == 111 && one->lastSeenUnix == 111,
        "and so do both timestamps");
    const auto spaced = back.Find(MakeFingerprint(7));
    Check(spaced && spaced->label == "Laptop with spaces",
        "a name with spaces is kept whole because it is the last field");

    const std::string legacy = text + FormatFingerprint(MakeFingerprint(8)) + " 1 2 host:9 Old\tphone\n";
    const auto old = ParseTrustStore(legacy).Find(MakeFingerprint(8));
    Check(old && old->label == "Old", "a row from an older version with a client key column still reads");

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
    const auto host = ParseTrustStore(control).Find(MakeFingerprint(8));
    Check(host && host->label == "name", "control characters are stripped from a stored name");

    std::string over = FormatFingerprint(MakeFingerprint(9)) + " 1 2 host:3 ";
    over.append(kMaxTrustLabelBytes + 40, 'x');
    over += '\n';
    const auto capped = ParseTrustStore(over).Find(MakeFingerprint(9));
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

void TestStrictHostProfilesRejectDamage() {
    std::printf("[trust] a damaged host profile file grants no trust...\n");
    TrustStore store;
    store.Remember(MakeFingerprint(4), "Host", "host:1", 1);
    const std::string valid = SerializeTrustStore(store);
    const auto parsed = ParseTrustStoreStrict(valid);
    Check(parsed && parsed->Find(MakeFingerprint(4)) &&
              parsed->Find(MakeFingerprint(4))->label == "Host",
        "a valid profile is preserved");
    Check(!ParseTrustStoreStrict(valid + "damaged row\n"),
        "one malformed row invalidates every host pin");
    Check(!ParseTrustStoreStrict(valid + valid),
        "a duplicate key invalidates the file");
    Check(!ParseTrustStoreStrict(std::string(131073, 'x')),
        "an oversized profile file is rejected");
}

void TestTheKeyIsShownShortEnoughToRead() {
    std::printf("[trust] a short fingerprint is enough to tell machines apart...\n");
    const std::string shortForm = ShortFingerprint(MakeFingerprint(1));
    Check(shortForm.size() == kShortFingerprintChars, "it is trimmed to a column width");
    Check(shortForm.find("SHA256:") == std::string::npos, "the prefix every row shares is dropped");
    Check(FormatFingerprint(MakeFingerprint(1)).find(shortForm) != std::string::npos,
        "and what is shown really is the start of the full fingerprint");
    Check(ShortFingerprint(MakeFingerprint(1)) != ShortFingerprint(MakeFingerprint(2)),
        "two machines read differently");
    Check(ShortFingerprint(Fingerprint{}).empty(), "and a machine with no key shows nothing");
}

}

void RunTrustStoreTests() {
    TestFingerprintText();
    TestTheKeyIsShownShortEnoughToRead();
    TestTrustFollowsTheKey();
    TestRememberAndForget();
    TestCapEvictsOldest();
    TestSerializeRoundTrip();
    TestParseJunk();
    TestStrictHostProfilesRejectDamage();
}
