#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/AccessRequests.h"
#include "deskhub/net/Base64.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

PublicKeyText KeyFor(uint8_t seed, std::string label) {
    std::string blob("\x00\x00\x00\x0bssh-ed25519\x00\x00\x00\x20", 19);
    for (int i = 0; i < 32; ++i) blob.push_back(char(seed + i));
    PublicKeyText key;
    key.algorithm = PublicKeyAlgorithm::Ed25519;
    key.blob.assign(blob.begin(), blob.end());
    key.label = std::move(label);
    return key;
}

void TestAddDedupesAndExpires() {
    std::printf("[requests] a device asking twice is one row, and old rows fade...\n");
    AccessRequests requests;
    Check(requests.Add(AccessRequest{KeyFor(1, "phone"), "10.0.0.2:5000", 0}, 100),
        "a request is recorded");
    Check(requests.Add(AccessRequest{KeyFor(1, "phone renamed"), "10.0.0.3:5001", 0}, 200),
        "asking again is accepted");
    Check(requests.Requests().size() == 1, "but it stays one row");
    const auto found = requests.Find(KeyFor(1, ""));
    Check(found && found->address == "10.0.0.3:5001" && found->requestedUnix == 200 &&
              found->key.label == "phone renamed",
        "the row carries the latest address, time and name");
    Check(!requests.Add(AccessRequest{KeyFor(2, "x"), "", 0}, 200), "no address, no request");
    Check(!requests.Add(AccessRequest{KeyFor(2, "x"), "bad address", 0}, 200),
        "an address with spaces is refused");
    Check(!requests.Add(AccessRequest{PublicKeyText{}, "10.0.0.4:1", 0}, 200),
        "an invalid key is refused");
    Check(requests.Expire(200 + kAccessRequestTtlSeconds - 1) == 0, "a fresh request stays");
    Check(requests.Expire(200 + kAccessRequestTtlSeconds) == 1, "and fades after ten minutes");
    Check(requests.Requests().empty(), "leaving the list empty");
}

void TestCapEvictsOldest() {
    std::printf("[requests] the list is capped so a flood cannot fill the disk...\n");
    AccessRequests requests;
    for (uint8_t i = 0; i < kMaxAccessRequests; ++i)
        requests.Add(AccessRequest{KeyFor(i, "d"), "10.0.1." + std::to_string(i) + ":1", 0}, 1000 + i);
    Check(requests.Requests().size() == kMaxAccessRequests, "the list fills to the cap");
    requests.Add(AccessRequest{KeyFor(99, "late"), "10.0.0.9:1", 0}, 5000);
    Check(requests.Requests().size() == kMaxAccessRequests, "and never grows past it");
    Check(!requests.Find(KeyFor(0, "")).has_value(), "the oldest request made room");
    Check(requests.Find(KeyFor(99, "")).has_value(), "for the newest");
    Check(requests.Remove(KeyFor(99, "")), "a request can be removed when it is approved or denied");
    Check(!requests.Remove(KeyFor(99, "")), "removing it again reports nothing happened");
}

void TestOneRequestPerAddress() {
    std::printf("[requests] each address holds at most one pending request...\n");
    AccessRequests requests;
    Check(requests.Add(AccessRequest{KeyFor(1, "real"), "10.0.0.7:5000", 0}, 100),
        "a request from one address is recorded");
    Check(requests.Add(AccessRequest{KeyFor(2, "other"), "10.0.0.70:5000", 0}, 110),
        "a request from a different address is recorded");
    for (uint8_t i = 0; i < 100; ++i)
        requests.Add(AccessRequest{KeyFor(uint8_t(100 + i), "flood"), "10.0.0.66:" + std::to_string(4000 + i), 0},
            200 + i);
    Check(requests.Requests().size() == 3, "a flood of fresh keys from one address is one row");
    Check(requests.Find(KeyFor(199, "")).has_value(), "holding only its newest key");
    Check(requests.Find(KeyFor(1, "")) && requests.Find(KeyFor(2, "")),
        "and every other address keeps its request");

    Check(requests.Add(AccessRequest{KeyFor(3, "same host"), "10.0.0.7:6000", 0}, 400),
        "another key from the same address on another port is accepted");
    Check(!requests.Find(KeyFor(1, "")).has_value() && requests.Find(KeyFor(3, "")).has_value(),
        "and replaces that address's earlier request");

    Check(requests.Add(AccessRequest{KeyFor(2, "moved"), "10.0.0.7:7000", 0}, 500),
        "a known key arriving from an address another key holds is accepted");
    Check(requests.Requests().size() == 2 && !requests.Find(KeyFor(3, "")).has_value(),
        "and leaves one row for the key and none behind for either source");
    const auto moved = requests.Find(KeyFor(2, ""));
    Check(moved && moved->address == "10.0.0.7:7000", "the row carries the new address");
}

void TestAddressHostForms() {
    std::printf("[requests] the address comparison ignores only the port...\n");
    AccessRequests requests;
    requests.Add(AccessRequest{KeyFor(1, ""), "10.0.0.5", 0}, 100);
    requests.Add(AccessRequest{KeyFor(2, ""), "10.0.0.5:9", 0}, 110);
    Check(requests.Requests().size() == 1, "an address with and without a port is one source");
    requests.Add(AccessRequest{KeyFor(3, ""), "[fe80::1]:5", 0}, 120);
    requests.Add(AccessRequest{KeyFor(4, ""), "[fe80::1]:6", 0}, 130);
    Check(requests.Requests().size() == 2 && requests.Find(KeyFor(4, "")),
        "a bracketed address drops its port");
    requests.Add(AccessRequest{KeyFor(5, ""), "fe80::2", 0}, 140);
    requests.Add(AccessRequest{KeyFor(6, ""), "fe80::3", 0}, 150);
    Check(requests.Requests().size() == 4, "bare addresses with several colons compare whole");
    requests.Add(AccessRequest{KeyFor(7, ""), "[fe80::4", 0}, 160);
    requests.Add(AccessRequest{KeyFor(8, ""), "[fe80::4", 0}, 170);
    Check(requests.Requests().size() == 5 && requests.Find(KeyFor(8, "")),
        "an unclosed bracket compares whole");
}

void TestFileKeepsNewestPerAddress() {
    std::printf("[requests] a file holding two requests from one address keeps the newer...\n");
    AccessRequests older;
    older.Add(AccessRequest{KeyFor(1, ""), "10.0.0.8:1", 0}, 1000);
    AccessRequests newer;
    newer.Add(AccessRequest{KeyFor(2, ""), "10.0.0.8:2", 0}, 1100);
    const std::string olderLine = SerializeAccessRequests(older);
    const std::string newerLine = SerializeAccessRequests(newer);
    for (const std::string& text : {olderLine + newerLine, newerLine + olderLine}) {
        const auto back = ParseAccessRequests(text, 1200);
        Check(back && back->Requests().size() == 1 && back->Find(KeyFor(2, "")),
            "whichever line comes first, the newer request survives");
    }
}

void TestFileRoundTrip() {
    std::printf("[requests] the request file survives a round trip and drops stale lines...\n");
    AccessRequests requests;
    requests.Add(AccessRequest{KeyFor(1, "Manh laptop"), "192.168.1.20:47777", 0}, 1000);
    requests.Add(AccessRequest{KeyFor(2, ""), "192.168.1.21:47777", 0}, 1100);
    const std::string text = SerializeAccessRequests(requests);
    const auto back = ParseAccessRequests(text, 1200);
    Check(back && back->Requests().size() == 2, "every row comes back");
    const auto one = back ? back->Find(KeyFor(1, "")) : std::nullopt;
    Check(one && one->key.label == "Manh laptop" && one->address == "192.168.1.20:47777" &&
              one->requestedUnix == 1000,
        "a name with a space, the address and the time all survive");
    Check(back && SerializeAccessRequests(*back) == text, "serialising is a fixpoint");
    const auto later = ParseAccessRequests(text, 1000 + kAccessRequestTtlSeconds);
    Check(later && later->Requests().size() == 1, "a row past its ten minutes is not read back");
    Check(ParseAccessRequests("", 0) && ParseAccessRequests("", 0)->Requests().empty(),
        "an empty file holds no requests");
    Check(!ParseAccessRequests("junk\n", 0).has_value(), "a damaged line invalidates the file");
    Check(!ParseAccessRequests("100 10.0.0.1:1 not-a-key\n", 0).has_value(),
        "a line without a key invalidates the file");
    Check(!ParseAccessRequests(text + text, 1200).has_value(), "a duplicate key invalidates the file");
    Check(!ParseAccessRequests(std::string(kMaxAccessRequestsFileBytes + 1, 'x'), 0).has_value(),
        "an oversized file is refused");
}

void TestDenialHoldsUntilItFades() {
    std::printf("[requests] a denied device stays turned away until the row fades...\n");
    AccessRequests requests;
    requests.Add(AccessRequest{KeyFor(1, "pest"), "10.0.0.9:1", 0}, 100);
    Check(!requests.Deny(KeyFor(2, ""), 110), "denying a key that never asked does nothing");
    Check(requests.Deny(KeyFor(1, ""), 120), "a pending request can be denied");
    Check(requests.IsDenied(KeyFor(1, "")) && !requests.IsDenied(KeyFor(2, "")),
        "only that key is marked");
    Check(!requests.Add(AccessRequest{KeyFor(1, "pest"), "10.0.0.9:2", 0}, 130),
        "asking again is refused instead of filing a fresh request");
    const auto held = requests.Find(KeyFor(1, ""));
    Check(held && held->denied && held->requestedUnix == 120 && held->address == "10.0.0.9:1",
        "and leaves the denial as it was");
    Check(requests.Add(AccessRequest{KeyFor(3, "neighbour"), "10.0.0.9:3", 0}, 140),
        "another key from the same address can still ask");
    Check(requests.IsDenied(KeyFor(1, "")) && requests.Find(KeyFor(3, "")),
        "without lifting the denial");

    const std::string text = SerializeAccessRequests(requests);
    const auto back = ParseAccessRequests(text, 150);
    Check(back && back->IsDenied(KeyFor(1, "")) && !back->IsDenied(KeyFor(3, "")) &&
              back->Requests().size() == 2,
        "the denial survives the request file");
    Check(back && SerializeAccessRequests(*back) == text, "serialising a denial is a fixpoint");

    Check(requests.Expire(120 + kAccessRequestTtlSeconds) == 1, "the denial fades with its row");
    Check(requests.Add(AccessRequest{KeyFor(1, "pest"), "10.0.0.9:4", 0}, 130 + kAccessRequestTtlSeconds),
        "after which the device may ask again");
}

}

void RunAccessRequestsTests() {
    TestAddDedupesAndExpires();
    TestCapEvictsOldest();
    TestOneRequestPerAddress();
    TestAddressHostForms();
    TestFileKeepsNewestPerAddress();
    TestFileRoundTrip();
    TestDenialHoldsUntilItFades();
}
