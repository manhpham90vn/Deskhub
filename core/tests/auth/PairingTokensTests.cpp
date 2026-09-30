#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/auth/PairingTokens.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

PairingToken TokenFor(uint8_t seed) {
    PairingToken token{};
    for (size_t i = 0; i < token.size(); ++i) token[i] = uint8_t(seed + i * 3 + 1);
    return token;
}

void TestSingleUse() {
    std::printf("[tokens] a pairing token works once, before it expires...\n");
    PairingTokens tokens;
    Check(tokens.Issue(TokenFor(1), 1000 + kPairingTokenTtlSeconds, 1000), "a token is issued");
    Check(!tokens.Issue(TokenFor(1), 1000 + kPairingTokenTtlSeconds, 1000),
        "the same token cannot be issued twice");
    Check(!tokens.Issue(PairingToken{}, 2000, 1000), "an all-zero token is never issued");
    Check(!tokens.Issue(TokenFor(2), 1000, 1000), "a token that has already expired is refused");
    Check(!tokens.Consume(TokenFor(9), 1001), "an unknown token is refused");
    Check(tokens.Consume(TokenFor(1), 1001), "the issued token is accepted");
    Check(!tokens.Consume(TokenFor(1), 1002), "and only once");
    Check(tokens.Issue(TokenFor(3), 1100, 1000), "another token is issued");
    Check(!tokens.Consume(TokenFor(3), 1100), "a token is dead the second it expires");
    Check(tokens.Tokens().empty(), "expired tokens are dropped from the list");
    Check(tokens.Issue(TokenFor(4), 2000, 1000), "issue");
    const uint8_t shortToken[16] = {};
    Check(!tokens.Consume(shortToken, 1001), "a token of the wrong length never matches");
}

void TestCapKeepsTheFreshest() {
    std::printf("[tokens] only a few tokens are live at once...\n");
    PairingTokens tokens;
    for (uint8_t i = 0; i < kMaxPairingTokens; ++i)
        Check(tokens.Issue(TokenFor(i), 2000 + i, 1000), "the list fills to the cap");
    Check(tokens.Issue(TokenFor(50), 3000, 1000), "one more still issues");
    Check(tokens.Tokens().size() == kMaxPairingTokens, "but the list never grows past the cap");
    Check(!tokens.Consume(TokenFor(0), 1001), "the token expiring soonest made room");
    Check(tokens.Consume(TokenFor(50), 1001), "and the newcomer is live");
    tokens.Clear();
    Check(tokens.Tokens().empty(), "the list can be emptied when the QR code is hidden");
}

void TestFileRoundTrip() {
    std::printf("[tokens] the token file survives a round trip and drops stale lines...\n");
    PairingTokens tokens;
    tokens.Issue(TokenFor(1), 5000, 1000);
    tokens.Issue(TokenFor(2), 6000, 1000);
    const std::string text = SerializePairingTokens(tokens);
    const auto back = ParsePairingTokens(text, 1000);
    Check(back && back->Tokens() == tokens.Tokens(), "every token and expiry comes back");
    const auto later = ParsePairingTokens(text, 5500);
    Check(later && later->Tokens().size() == 1, "a token past its expiry is not read back");
    Check(ParsePairingTokens("", 0) && ParsePairingTokens("", 0)->Tokens().empty(),
        "an empty file holds no tokens");
    Check(!ParsePairingTokens("garbage\n", 0).has_value(), "a damaged line invalidates the file");
    Check(!ParsePairingTokens("AAAA 12\n", 0).has_value(), "a short token invalidates the file");
    Check(!ParsePairingTokens(text + text, 1000).has_value(), "a duplicate line invalidates the file");
    Check(!ParsePairingTokens(std::string(kMaxPairingTokensFileBytes + 1, 'x'), 0).has_value(),
        "an oversized file is refused");
}

}

void RunPairingTokensTests() {
    TestSingleUse();
    TestCapKeepsTheFreshest();
    TestFileRoundTrip();
}
