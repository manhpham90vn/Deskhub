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
        requests.Add(AccessRequest{KeyFor(i, "d"), "10.0.0.1:1", 0}, 1000 + i);
    Check(requests.Requests().size() == kMaxAccessRequests, "the list fills to the cap");
    requests.Add(AccessRequest{KeyFor(99, "late"), "10.0.0.9:1", 0}, 5000);
    Check(requests.Requests().size() == kMaxAccessRequests, "and never grows past it");
    Check(!requests.Find(KeyFor(0, "")).has_value(), "the oldest request made room");
    Check(requests.Find(KeyFor(99, "")).has_value(), "for the newest");
    Check(requests.Remove(KeyFor(99, "")), "a request can be removed when it is approved or denied");
    Check(!requests.Remove(KeyFor(99, "")), "removing it again reports nothing happened");
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

}

void RunAccessRequestsTests() {
    TestAddDedupesAndExpires();
    TestCapEvictsOldest();
    TestFileRoundTrip();
}
