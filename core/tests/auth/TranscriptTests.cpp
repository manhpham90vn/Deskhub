#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/auth/Transcript.h"
#include "deskhub/protocol/ByteOrder.h"

#include <array>
#include <cstdio>
#include <string_view>
#include <vector>

void RunAuthTranscriptTests() {
    std::printf("[auth] signature transcript binds its domain, version, role, session and keys...\n");
    deskhub::AuthSessionId sessionId{};
    sessionId.fill(0x10);
    std::vector<uint8_t> clientKey(44, 0x20);
    deskhub::Fingerprint host{};
    host.bytes.fill(0x30);

    const auto original =
        deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, clientKey, host);
    constexpr std::string_view domain = "Deskhub/auth/signature";
    Check(!original.empty(), "a valid transcript can be encoded");
    if (original.empty()) return;
    Check(deskhub::GetU32(original.data()) == domain.size(),
        "the domain has an unambiguous length");
    Check(std::string_view(reinterpret_cast<const char*>(original.data() + 4), domain.size()) ==
              domain,
        "the domain separates this signature from other uses of the key");
    const size_t metadata = 4 + domain.size();
    Check(deskhub::GetU32(original.data() + metadata) == 2 &&
              original[metadata + 4] == deskhub::kAuthVersion &&
              original[metadata + 5] == uint8_t(deskhub::AuthRole::Client),
        "auth version and signer role are explicit fields");

    auto differentSession = sessionId;
    differentSession[0] ^= 1;
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, differentSession, clientKey, host) !=
              original,
        "a different TLS session changes the signed bytes");
    auto differentKey = clientKey;
    differentKey[0] ^= 1;
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, differentKey, host) !=
              original,
        "a different client public key changes the signed bytes");
    auto differentHost = host;
    differentHost.bytes[0] ^= 1;
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, clientKey, differentHost) !=
              original,
        "a different TLS host key changes the signed bytes");
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Host, sessionId, clientKey, host) != original,
        "a host proof cannot be mistaken for a client proof");
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, {}, host).empty(),
        "an absent client key cannot produce a proof");
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, clientKey,
              deskhub::Fingerprint{})
              .empty(),
        "an absent host key cannot produce a proof");
    Check(deskhub::AuthTranscript(deskhub::AuthRole::Client, {}, clientKey, host).empty(),
        "an absent TLS session identifier cannot produce a proof");
}
