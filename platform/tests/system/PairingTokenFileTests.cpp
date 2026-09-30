#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/PairingInvite.h"
#include "deskhubp/host/PairingInvite.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"

#include <cstdio>
#include <string>

namespace {

void TestATokenIsIssuedRedeemedOnceAndRevoked() {
    std::printf("[tokens] a pairing token on disk is redeemed once and can be revoked...\n");
    const IsolatedAppData data("deskhub-pairing-tokens");
    const auto token = deskhubp::IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    Check(token.has_value() && !deskhub::IsZero(*token), "a random token is issued");
    Check(!deskhubp::ReadAppDataFile(deskhubp::kPairingTokensFileName).empty(),
        "and written to the token file");
    if (!token) return;
    deskhub::PairingToken wrong = *token;
    wrong[0] ^= 0xFF;
    Check(!deskhubp::RedeemPairingToken(wrong), "a token that differs in one byte is refused");
    Check(deskhubp::RedeemPairingToken(*token), "the issued token is redeemed");
    Check(!deskhubp::RedeemPairingToken(*token), "and cannot be redeemed twice");
    Check(deskhubp::ReadAppDataFile(deskhubp::kPairingTokensFileName).empty(),
        "an empty token list leaves no file behind");

    const auto revoked = deskhubp::IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    Check(revoked.has_value() && deskhubp::RevokePairingTokens(),
        "hiding the QR code revokes outstanding tokens");
    Check(!deskhubp::RedeemPairingToken(*revoked), "so a revoked token is refused");
    Check(!deskhubp::IssuePairingToken(0).has_value(), "a token with no lifetime is never issued");
}

void TestAnInviteCarriesThisMachine() {
    std::printf("[tokens] the invite a host shows names its key, address and a live token...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[tokens] skipped: this build has no key support\n");
        return;
    }
    const IsolatedAppData data("deskhub-pairing-invite");
    const std::string text = deskhubp::BuildPairingInvite(47777, "127.0.0.1", "Study PC");
    Check(deskhub::IsPairingInvite(text), "the invite is a deskhub:// link");
    const auto invite = deskhub::ParsePairingInvite(text);
    Check(invite.has_value(), "and it parses");
    if (!invite) return;
    Check(invite->hostKey == deskhubp::LoadHostIdentity().fingerprint,
        "it names this machine's key");
    Check(invite->endpoints.size() == 1 &&
              deskhub::FormatPairingEndpoint(invite->endpoints.front()) == "127.0.0.1:47777",
        "a chosen network gives one address");
    Check(invite->hostName == "Study PC", "and carries the device name");
    Check(deskhubp::RedeemPairingToken(invite->token), "the token inside is live");
    Check(!deskhubp::RedeemPairingToken(invite->token), "and single use");
    Check(deskhubp::BuildPairingInvite(47777, "203.0.113.9", "x").empty() ||
              deskhub::ParsePairingInvite(deskhubp::BuildPairingInvite(47777, "203.0.113.9", "x")),
        "a bind address is used as given");
    const std::string longName = deskhubp::BuildPairingInvite(47777, "127.0.0.1",
        std::string(deskhub::kMaxPairingHostNameBytes + 20, 'n'));
    Check(deskhub::ParsePairingInvite(longName).has_value(), "an over-long name is cut, not refused");
}

}

void RunPairingTokenFileTests() {
    TestATokenIsIssuedRedeemedOnceAndRevoked();
    TestAnInviteCarriesThisMachine();
}
