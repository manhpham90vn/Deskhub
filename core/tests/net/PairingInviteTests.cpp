#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/PairingInvite.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

Fingerprint HostKey() {
    Fingerprint fp;
    for (size_t i = 0; i < kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(0x40 + i);
    return fp;
}

PairingToken Token() {
    PairingToken token{};
    for (size_t i = 0; i < token.size(); ++i) token[i] = uint8_t(0xA0 + i);
    return token;
}

PairingInvite Sample() {
    PairingInvite invite;
    invite.endpoints.push_back(*MakePairingEndpoint("192.168.1.10", 47777));
    invite.endpoints.push_back(*MakePairingEndpoint("10.0.0.5", 47777));
    invite.hostKey = HostKey();
    invite.token = Token();
    invite.hostName = "Study PC";
    return invite;
}

void TestRoundTrip() {
    std::printf("[invite] a QR payload carries addresses, host key, token and name...\n");
    const PairingInvite invite = Sample();
    const std::string text = FormatPairingInvite(invite);
    Check(text.starts_with(kPairingInvitePrefix), "the text is a deskhub:// link");
    Check(IsPairingInvite(text) && IsPairingInvite("  " + text + "\n"),
        "the link is recognised, even with stray whitespace");
    Check(!IsPairingInvite("192.168.1.10:47777"), "an address is not an invite");
    const auto back = ParsePairingInvite(text);
    Check(back.has_value() && *back == invite, "every field survives the trip");
    Check(back && FormatPairingEndpoint(back->endpoints[0]) == "192.168.1.10:47777",
        "an endpoint prints as the address a client can dial");
    Check(ParsePairingInvite("  " + text + "  ").has_value(), "surrounding whitespace is ignored");
}

void TestSizeBudget() {
    std::printf("[invite] the largest payload still fits a small QR code...\n");
    PairingInvite invite = Sample();
    invite.endpoints.clear();
    for (uint32_t i = 0; i < kMaxPairingEndpoints; ++i)
        invite.endpoints.push_back(PairingEndpoint{0xC0A80001u + i, 65535});
    invite.hostName = std::string(kMaxPairingHostNameBytes, 'W');
    const std::string text = FormatPairingInvite(invite);
    Check(!text.empty() && text.size() <= kMaxPairingInviteChars,
        "the longest invite stays under the QR budget");
    Check(ParsePairingInvite(text).has_value() && *ParsePairingInvite(text) == invite,
        "and still parses");
    invite.endpoints.push_back(PairingEndpoint{1, 1});
    Check(FormatPairingInvite(invite).empty(), "a fifth address is refused");
}

void TestRejectsBadFields() {
    std::printf("[invite] a payload with a missing or damaged field is refused...\n");
    PairingInvite invite = Sample();
    invite.hostKey = Fingerprint{};
    Check(FormatPairingInvite(invite).empty(), "no host key means no invite");
    invite = Sample();
    invite.token = PairingToken{};
    Check(FormatPairingInvite(invite).empty(), "no token means no invite");
    invite = Sample();
    invite.endpoints.clear();
    Check(FormatPairingInvite(invite).empty(), "no address means no invite");
    invite = Sample();
    invite.hostName = "bad\x01name";
    Check(FormatPairingInvite(invite).empty(), "control characters in the name are refused");
    invite = Sample();
    invite.hostName = std::string(kMaxPairingHostNameBytes + 1, 'x');
    Check(FormatPairingInvite(invite).empty(), "an over-long name is refused");
    Check(!MakePairingEndpoint("0.0.0.0", 47777).has_value(), "the unspecified address is refused");
    Check(!MakePairingEndpoint("192.168.1.10", 0).has_value(), "port zero is refused");
    Check(!MakePairingEndpoint("host.local", 47777).has_value(), "a host name is not an address");

    const std::string text = FormatPairingInvite(Sample());
    Check(!ParsePairingInvite(text.substr(0, text.size() - 1)).has_value(),
        "a truncated link is refused");
    Check(!ParsePairingInvite(text + "A").has_value(), "a link with trailing bytes is refused");
    Check(!ParsePairingInvite("deskhub://pair/").has_value(), "an empty payload is refused");
    Check(!ParsePairingInvite("deskhub://connect/" + text.substr(kPairingInvitePrefix.size()))
              .has_value(),
        "another path under the scheme is not an invite");
    std::string flipped = text;
    flipped[kPairingInvitePrefix.size()] = flipped[kPairingInvitePrefix.size()] == 'A' ? 'B' : 'A';
    Check(!ParsePairingInvite(flipped).has_value(), "a different version byte is refused");
}

}

void RunPairingInviteTests() {
    TestRoundTrip();
    TestSizeBudget();
    TestRejectsBadFields();
}
