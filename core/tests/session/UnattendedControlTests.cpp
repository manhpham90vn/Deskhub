#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/ByteOrder.h"
#include "deskhub/session/host/UnattendedControl.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

UnattendedConfig SampleConfig() {
    UnattendedConfig config;
    config.enabled = true;
    config.hostKeyPem = "-----BEGIN PRIVATE KEY-----\nabc\n-----END PRIVATE KEY-----\n";
    config.authorizedKeys = "ecdsa-sha2-nistp256 AAAA laptop\n";
    config.settings = "fps=60\nunattended=1\n";
    return config;
}

std::vector<uint8_t> ConfigureBytes(const UnattendedConfig& config) {
    std::vector<uint8_t> bytes{uint8_t(SecureDesktopRequest::Configure)};
    const std::vector<uint8_t> payload = EncodeUnattendedConfig(config);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

UnattendedAction FeedOneByOne(UnattendedControlServer& server, const std::vector<uint8_t>& bytes) {
    UnattendedAction last = UnattendedAction::NeedMore;
    for (const uint8_t byte : bytes) {
        last = server.Feed(std::span<const uint8_t>(&byte, 1));
        if (last != UnattendedAction::NeedMore) return last;
    }
    return last;
}

void TestSingleByteRequests() {
    std::printf("[unattended] one byte names the request and that is the whole message...\n");
    const uint8_t agent = uint8_t(SecureDesktopRequest::Agent);
    const uint8_t takeover = uint8_t(SecureDesktopRequest::Takeover);
    const uint8_t status = uint8_t(SecureDesktopRequest::Status);
    UnattendedControlServer a;
    Check(a.Feed(std::span<const uint8_t>(&agent, 1)) == UnattendedAction::Agent,
        "an agent request completes on its first byte");
    UnattendedControlServer t;
    Check(t.Feed(std::span<const uint8_t>(&takeover, 1)) == UnattendedAction::Takeover,
        "so does a takeover");
    UnattendedControlServer s;
    Check(s.Feed(std::span<const uint8_t>(&status, 1)) == UnattendedAction::Status,
        "and a status query");
    Check(t.Feed(std::span<const uint8_t>(&takeover, 1)) == UnattendedAction::Refuse,
        "a second request on the same connection is refused");
    UnattendedControlServer empty;
    Check(empty.Feed({}) == UnattendedAction::NeedMore, "no bytes yet means keep reading");
}

void TestUnknownRequest() {
    std::printf("[unattended] an unknown first byte is refused before any work...\n");
    const uint8_t zero = 0;
    const uint8_t beyond = uint8_t(SecureDesktopRequest::Status) + 1;
    UnattendedControlServer z;
    Check(z.Feed(std::span<const uint8_t>(&zero, 1)) == UnattendedAction::Refuse, "0 is refused");
    UnattendedControlServer b;
    Check(b.Feed(std::span<const uint8_t>(&beyond, 1)) == UnattendedAction::Refuse,
        "a byte past the last request is refused");
    Check(b.Feed(std::span<const uint8_t>(&zero, 1)) == UnattendedAction::Refuse,
        "a refused connection stays refused");
}

void TestConfigureArrivesInPieces() {
    std::printf("[unattended] a configuration is assembled however the bytes arrive...\n");
    const UnattendedConfig config = SampleConfig();
    const std::vector<uint8_t> bytes = ConfigureBytes(config);

    UnattendedControlServer whole;
    Check(whole.Feed(bytes) == UnattendedAction::Configure, "one write delivers it");
    Check(whole.config() == config, "and every block is read back");

    UnattendedControlServer dribble;
    Check(FeedOneByOne(dribble, bytes) == UnattendedAction::Configure,
        "byte-by-byte delivery reaches the same action");
    Check(dribble.config() == config, "with the same configuration");

    UnattendedControlServer halves;
    const size_t cut = bytes.size() / 2;
    Check(halves.Feed(std::span<const uint8_t>(bytes).first(cut)) == UnattendedAction::NeedMore,
        "half a configuration asks for more");
    Check(halves.Feed(std::span<const uint8_t>(bytes).subspan(cut)) == UnattendedAction::Configure,
        "the second half completes it");

    UnattendedConfig off;
    UnattendedControlServer disable;
    Check(disable.Feed(ConfigureBytes(off)) == UnattendedAction::Configure,
        "switching the feature off is a configuration too");
    Check(!disable.config().enabled && disable.config().hostKeyPem.empty(),
        "and carries no key");
}

void TestConfigureRejectsBadBodies() {
    std::printf("[unattended] a configuration that breaks the wire limits is refused...\n");
    std::vector<uint8_t> oversized{uint8_t(SecureDesktopRequest::Configure), 0, 0, 0, 0};
    PutU32(oversized.data() + 1, uint32_t(kUnattendedConfigMaxBytes + 1));
    UnattendedControlServer big;
    Check(big.Feed(oversized) == UnattendedAction::Refuse,
        "a length past the maximum is refused before the body is read");

    std::vector<uint8_t> zeroLength{uint8_t(SecureDesktopRequest::Configure), 0, 0, 0, 0};
    UnattendedControlServer empty;
    Check(empty.Feed(zeroLength) == UnattendedAction::Refuse, "a zero length is refused");

    std::vector<uint8_t> bytes = ConfigureBytes(SampleConfig());
    bytes[1 + kUnattendedConfigLengthSize] = 1;
    PutU32(bytes.data() + 1 + kUnattendedConfigLengthSize + 1, 0);
    UnattendedControlServer forged;
    Check(forged.Feed(bytes) == UnattendedAction::Refuse,
        "a body that says enabled without a key is refused");

    std::vector<uint8_t> trailing = ConfigureBytes(SampleConfig());
    trailing.push_back(uint8_t(SecureDesktopRequest::Takeover));
    UnattendedControlServer extra;
    Check(extra.Feed(trailing) == UnattendedAction::Refuse,
        "bytes after a complete request are refused");
}

void TestStatusRoundTrip() {
    std::printf("[unattended] the host's permission state travels as two bytes...\n");
    UnattendedStatus status;
    status.screen = UnattendedPermission::Granted;
    status.input = UnattendedPermission::Denied;
    const auto bytes = EncodeUnattendedStatus(status);
    Check(DecodeUnattendedStatus(bytes) == status, "both permissions round-trip");
    const uint8_t bad[] = {3, 0};
    Check(!DecodeUnattendedStatus(bad), "a value past Granted is refused");
    const uint8_t one[] = {2};
    Check(!DecodeUnattendedStatus(one), "a short reply is refused");

    Check(ParseUnattendedStatus(SerializeUnattendedStatus(status)) == status,
        "the file the host writes reads back the same");
    Check(ParseUnattendedStatus("") == UnattendedStatus{},
        "no file means nothing is known yet");
    Check(ParseUnattendedStatus("screen=granted\r\ninput=what\n").screen ==
                  UnattendedPermission::Granted &&
              ParseUnattendedStatus("screen=granted\r\ninput=what\n").input ==
                  UnattendedPermission::Unknown,
        "Windows line endings are fine and junk reads as unknown");
}

}

void RunUnattendedControlTests() {
    TestSingleByteRequests();
    TestUnknownRequest();
    TestConfigureArrivesInPieces();
    TestConfigureRejectsBadBodies();
    TestStatusRoundTrip();
}
