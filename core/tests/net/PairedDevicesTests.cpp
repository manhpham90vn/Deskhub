#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/PairedDevices.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

Fingerprint Key(uint8_t seed) {
    Fingerprint fp;
    for (size_t i = 0; i < fp.bytes.size(); ++i) fp.bytes[i] = uint8_t(seed + i);
    return fp;
}

void TestAPairedKeyIsTheWholeCredential() {
    std::printf("[paired] a machine on the list is let in by its key alone...\n");
    PairedDevices devices;
    Check(devices.Check(Key(1)) == PairVerdict::Unknown,
        "a machine that never paired is not on the list");

    devices.Remember(Key(1), "manh-laptop", 1000);
    Check(devices.Check(Key(1)) == PairVerdict::Paired,
        "once paired, the key is enough - no passcode is consulted");
    Check(devices.Check(Key(2)) == PairVerdict::Unknown,
        "and pairing one machine does not let a different one in");

    const std::optional<PairedDevice> found = devices.Find(Key(1));
    Check(found && found->name == "manh-laptop" && found->pairedUnix == 1000,
        "the list remembers who it was and when");
}

void TestForgettingIsWhatRevokes() {
    std::printf("[paired] forgetting a machine is what actually turns it away...\n");
    PairedDevices devices;
    devices.Remember(Key(1), "laptop", 1000);
    devices.Remember(Key(2), "phone", 1000);
    Check(devices.Size() == 2, "two machines are paired");

    Check(devices.Forget(Key(1)), "forgetting one reports that it did something");
    Check(devices.Check(Key(1)) == PairVerdict::Unknown, "that machine has to pair again");
    Check(devices.Check(Key(2)) == PairVerdict::Paired, "the other is untouched");
    Check(!devices.Forget(Key(1)), "forgetting it twice changes nothing");

    devices.Clear();
    Check(devices.Size() == 0 && devices.Check(Key(2)) == PairVerdict::Unknown,
        "and clearing the list turns everyone away at once");
}

void TestPairingTwiceKeepsOneRow() {
    std::printf("[paired] a machine that comes back is the same row, not a second one...\n");
    PairedDevices devices;
    devices.Remember(Key(1), "laptop", 1000);
    devices.Remember(Key(1), "laptop-renamed", 2000);
    Check(devices.Size() == 1, "the same key never appears twice");

    const std::optional<PairedDevice> found = devices.Find(Key(1));
    Check(found && found->pairedUnix == 1000, "the day it first paired is kept");
    Check(found && found->lastSeenUnix == 2000, "the last visit is moved forward");
    Check(found && found->name == "laptop-renamed", "and a machine that renamed itself is followed");

    Check(devices.Touch(Key(1), "", 3000), "an empty name is not a rename");
    Check(devices.Find(Key(1))->name == "laptop-renamed", "so the name it had stands");
    Check(!devices.Touch(Key(9), "ghost", 3000), "touching a machine that never paired does nothing");
}

void TestTheListCannotGrowForever() {
    std::printf("[paired] the list is bounded, dropping whoever has been away longest...\n");
    PairedDevices devices;
    for (size_t i = 0; i < kMaxPairedDevices + 10; ++i)
        devices.Remember(Key(uint8_t(i)), "machine-" + std::to_string(i), int64_t(i + 1));
    Check(devices.Size() == kMaxPairedDevices, "it stops at the cap");
    Check(devices.Check(Key(0)) == PairVerdict::Unknown, "the longest-unseen machine was dropped");
    Check(devices.Check(Key(uint8_t(kMaxPairedDevices + 9))) == PairVerdict::Paired,
        "and the most recent one is still there");
}

void TestJunkNeverBecomesAPairing() {
    std::printf("[paired] an empty key is never written down...\n");
    PairedDevices devices;
    devices.Remember(Fingerprint{}, "nobody", 1000);
    Check(devices.Size() == 0, "a zero fingerprint is not a machine");
    Check(devices.Check(Fingerprint{}) == PairVerdict::Unknown,
        "and asking about one is answered as a stranger, not as paired");
}

void TestTheListSurvivesARestart() {
    std::printf("[paired] the list written to disk reads back the same...\n");
    PairedDevices devices;
    devices.Remember(Key(1), "manh laptop", 1000);
    devices.Remember(Key(2), "phone", 1500);
    devices.Touch(Key(1), "manh laptop", 2000);

    const std::string text = SerializePairedDevices(devices);
    const auto back = ParsePairedDevicesStrict(text);
    Check(back && back->Size() == 2, "both machines come back");
    if (!back) return;
    Check(back->Check(Key(1)) == PairVerdict::Paired && back->Check(Key(2)) == PairVerdict::Paired,
        "and both are still let in");

    const std::optional<PairedDevice> one = back->Find(Key(1));
    Check(one && one->name == "manh laptop", "a name with a space survives the round trip");
    Check(one && one->pairedUnix == 1000 && one->lastSeenUnix == 2000, "so do both timestamps");

    Check(SerializePairedDevices(*back) == text, "writing what was read gives the same file");
    Check(ParsePairedDevicesStrict("")->Size() == 0, "an empty file is an empty list");
    Check(!ParsePairedDevicesStrict("# just a comment\nnonsense\nSHA256:short 1 2\n"),
        "and junk lines invalidate the entire admission list");
}

void TestStrictAdmissionListRejectsDamage() {
    std::printf("[paired] admission fails closed for damaged or duplicate rows...\n");
    PairedDevices devices;
    devices.Remember(Key(1), "laptop", 1000);
    const std::string row = SerializePairedDevices(devices);
    const auto valid = ParsePairedDevicesStrict(row);
    Check(valid && valid->Check(Key(1)) == PairVerdict::Paired,
        "a valid stored key remains authorized");
    Check(!ParsePairedDevicesStrict(row + "nonsense\n"),
        "a damaged row invalidates the whole admission list");
    Check(!ParsePairedDevicesStrict(row + row),
        "a duplicate key invalidates the admission list");
    std::string control = row;
    control.insert(control.size() - 1, 1, '\x01');
    Check(!ParsePairedDevicesStrict(control),
        "a control byte in a key label invalidates the admission list");
    Check(!ParsePairedDevicesStrict(std::string(32769, 'x')),
        "an oversized admission list is rejected before parsing");
    Check(ParsePairedDevicesStrict("")->Size() == 0,
        "an intentionally empty admission list authorizes nobody");
}

void TestTheKeyIsShownShortEnoughToRead() {
    std::printf("[paired] the list shows enough of a key to tell machines apart...\n");
    const std::string shortForm = ShortFingerprint(Key(1));
    Check(shortForm.size() == kShortFingerprintChars, "it is trimmed to a column width");
    Check(shortForm.find("SHA256:") == std::string::npos, "the prefix every row shares is dropped");
    Check(FormatFingerprint(Key(1)).find(shortForm) != std::string::npos,
        "and what is shown really is the start of the full fingerprint");
    Check(ShortFingerprint(Key(1)) != ShortFingerprint(Key(2)), "two machines read differently");
    Check(ShortFingerprint(Fingerprint{}).empty(), "and a machine with no key shows nothing");
}

}

void RunPairedDevicesTests() {
    TestTheKeyIsShownShortEnoughToRead();
    TestAPairedKeyIsTheWholeCredential();
    TestForgettingIsWhatRevokes();
    TestPairingTwiceKeepsOneRow();
    TestTheListCannotGrowForever();
    TestJunkNeverBecomesAPairing();
    TestTheListSurvivesARestart();
    TestStrictAdmissionListRejectsDamage();
}
