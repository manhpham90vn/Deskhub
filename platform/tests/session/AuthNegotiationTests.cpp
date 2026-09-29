#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"

#include <cstdio>
#include <string>

namespace {

struct CleanSlate {
    std::string cert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    std::string key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    std::string authorizedKeys = deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);

    CleanSlate() {
        RevokeAllClientKeys();
    }

    ~CleanSlate() {
        if (!cert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, cert);
        if (!key.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
        if (authorizedKeys.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kAuthorizedKeysFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kAuthorizedKeysFileName, authorizedKeys);
    }
};

struct Machines {
    deskhubp::HostIdentity host{};
    deskhubp::HostIdentity client{};
    deskhubp::HostIdentity other{};

    bool Make() {
        ForgetHostIdentity();
        client = deskhubp::LoadOrCreateHostIdentity("client-test");
        ForgetHostIdentity();
        other = deskhubp::LoadOrCreateHostIdentity("other-client-test");
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity("host-test");
        return client.Valid() && other.Valid() && host.Valid() &&
               client.fingerprint != host.fingerprint && other.fingerprint != client.fingerprint;
    }

    deskhubp::HostAuthConfig HostConfig() const {
        deskhubp::HostAuthConfig config;
        config.identity = host;
        config.sessionId.fill(0x41);
        return config;
    }

    deskhubp::ClientAuthConfig ClientConfig() const {
        deskhubp::ClientAuthConfig config;
        config.identity = client;
        config.hostFingerprint = host.fingerprint;
        config.sessionId.fill(0x41);
        config.clientName = "client";
        return config;
    }
};

void TestUnknownKeyNeverRequestsApproval() {
    std::printf("[authneg] an unknown key is denied without passcode or approval...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    const deskhub::AuthStart start = client.Begin();
    Check(!start.publicKey.empty(), "the client offers its public key");
    const auto challenge = host.Begin(start);
    Check(challenge && challenge->mode == deskhub::AuthMode::Denied,
        "an unlisted key is denied");
    Check(host.State() == deskhubp::HostAuthState::Settled,
        "the host does not wait for approval");
    Check(challenge && !client.Answer(*challenge), "the client cannot answer a denial");
    Check(!deskhubp::IsClientKeyAuthorized(deskhubp::ClientIdentity(machines.client).publicKey),
        "the unlisted key is never added automatically");
}

void TestAuthorizedKeyMustSignForThisHost() {
    std::printf("[authneg] an authorized key must sign for the host it reaches...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client),
        "the owner grants the client key locally");
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    const auto challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "the listed client receives a signature challenge");
    if (!challenge) return;
    const auto response = client.Answer(*challenge);
    Check(response && !response->proof.empty(), "the client signs the challenge");
    if (!response) return;
    Check(host.Respond(*response).code == deskhub::AuthResultCode::Accepted,
        "the signature admits the client");
    Check(host.Respond(*response).code != deskhub::AuthResultCode::Accepted,
        "the same signed response is not accepted twice");
    Check(!host.Begin(client.Begin()), "a settled handshake cannot issue another challenge");

    deskhubp::HostAuth wrongHost;
    deskhubp::ClientAuth fooled;
    wrongHost.Configure(machines.HostConfig());
    auto wrongConfig = machines.ClientConfig();
    wrongConfig.hostFingerprint.bytes[0] ^= 0xff;
    fooled.Configure(wrongConfig);
    const auto wrongChallenge = wrongHost.Begin(fooled.Begin());
    const auto wrongResponse = wrongChallenge ? fooled.Answer(*wrongChallenge) : std::nullopt;
    Check(wrongResponse &&
              wrongHost.Respond(*wrongResponse).code ==
                  deskhub::AuthResultCode::BadSignature,
        "a signature for another host key has a distinct error code");
}

void TestRevocationDuringHandshakeIsEnforced() {
    std::printf("[authneg] revocation during the challenge blocks admission...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client),
        "the client is authorized first");
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    const auto challenge = host.Begin(client.Begin());
    const auto response = challenge ? client.Answer(*challenge) : std::nullopt;
    Check(response.has_value(), "the client signs before revocation");
    Check(deskhubp::ForgetAuthorizedClient(machines.client.fingerprint),
        "the owner revokes the key while auth is pending");
    if (response)
        Check(host.Respond(*response).code != deskhub::AuthResultCode::Accepted,
            "the signed response cannot finish after revocation");
}

void TestAProofCannotMoveToAnotherTlsSession() {
    std::printf("[authneg] a signed request cannot be replayed on another TLS session...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client),
        "the client key is authorized");
    deskhubp::HostAuth original;
    deskhubp::HostAuth other;
    deskhubp::ClientAuth client;
    original.Configure(machines.HostConfig());
    auto otherConfig = machines.HostConfig();
    otherConfig.sessionId[0] ^= 1;
    other.Configure(otherConfig);
    client.Configure(machines.ClientConfig());
    const auto challenge = original.Begin(client.Begin());
    const auto response = challenge ? client.Answer(*challenge) : std::nullopt;
    Check(response.has_value(), "the client signs for its own TLS session");
    if (!response) return;
    Check(other.Begin(client.Begin()).has_value(), "another connection requests authentication");
    Check(other.Respond(*response).code != deskhub::AuthResultCode::Accepted,
        "that connection rejects the captured signature");
}

void TestMalformedKeyIsRejected() {
    std::printf("[authneg] malformed public keys cannot start auth...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    deskhubp::HostAuth host;
    host.Configure(machines.HostConfig());
    deskhub::AuthStart invalid;
    invalid.publicKey.assign(40, 0x7e);
    Check(!host.Begin(invalid), "a malformed key is rejected before a challenge");
}

deskhub::AuthResultCode AnswerWith(const Machines& machines, std::vector<uint8_t> proof) {
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    const auto challenge = host.Begin(client.Begin());
    if (!challenge || challenge->mode != deskhub::AuthMode::Signature)
        return deskhub::AuthResultCode::Refused;
    deskhub::AuthResponse response;
    response.proof = std::move(proof);
    return host.Respond(response).code;
}

std::vector<uint8_t> SignedByClient(const Machines& machines, std::span<const uint8_t> data) {
    return deskhubp::SignWithClientIdentity(deskhubp::ClientIdentity(machines.client), data);
}

void TestAPublicKeyAloneCannotImpersonateAClient() {
    std::printf("[authneg] knowing a client's public key is not enough to sign in as it...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client), "the victim's key is authorized");
    deskhubp::HostAuth host;
    host.Configure(machines.HostConfig());

    deskhubp::ClientIdentity forged(machines.other);
    forged.publicKey = deskhubp::ClientIdentity(machines.client).publicKey;
    deskhubp::ClientAuth impostor;
    auto config = machines.ClientConfig();
    config.identity = forged;
    impostor.Configure(config);

    const auto challenge = host.Begin(impostor.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "presenting the authorized public key earns a challenge");
    const auto response = challenge ? impostor.Answer(*challenge) : std::nullopt;
    Check(response.has_value(),
        "the impostor signs the right transcript with the wrong private key");
    if (!response) return;
    Check(host.Respond(*response).code == deskhub::AuthResultCode::BadSignature,
        "the host rejects it, even with session and host key exactly right");
}

void TestTheKeyCannotChangeMidHandshake() {
    std::printf("[authneg] the key that asked for a challenge is the key that must answer...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client) && GrantClientKey(machines.other),
        "two client keys are authorized");
    deskhubp::HostAuth host;
    host.Configure(machines.HostConfig());
    deskhubp::ClientAuth first;
    deskhubp::ClientAuth second;
    first.Configure(machines.ClientConfig());
    auto secondConfig = machines.ClientConfig();
    secondConfig.identity = machines.other;
    second.Configure(secondConfig);

    const auto challenge = host.Begin(first.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "the first key receives a challenge");
    Check(!host.Begin(second.Begin()), "a second key cannot restart the pending handshake");
    const auto swapped = challenge ? second.Answer(*challenge) : std::nullopt;
    Check(swapped.has_value(), "the second key signs its own transcript");
    if (!swapped) return;
    Check(host.Respond(*swapped).code == deskhub::AuthResultCode::BadSignature,
        "a proof from a different key than the one presented is refused");
    Check(host.PeerFingerprint() == machines.client.fingerprint,
        "and the host never records the swapped key as the peer");
}

void TestOnlyTheExactTranscriptIsAccepted() {
    std::printf("[authneg] a signature over anything but this handshake is refused...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client), "the client key is authorized");
    const auto config = machines.HostConfig();
    const auto clientKey = deskhubp::ClientIdentity(machines.client).publicKey;
    const auto otherKey = deskhubp::ClientIdentity(machines.other).publicKey;
    const auto transcript = [&](deskhub::AuthRole role, std::span<const uint8_t> key,
                                const deskhub::Fingerprint& hostKey) {
        return deskhub::AuthTranscript(role, config.sessionId, key, hostKey);
    };

    const auto right = transcript(deskhub::AuthRole::Client, clientKey, machines.host.fingerprint);
    Check(AnswerWith(machines, SignedByClient(machines, right)) ==
              deskhub::AuthResultCode::Accepted,
        "the exact transcript signed by the client key is accepted");
    Check(AnswerWith(machines,
              SignedByClient(machines, transcript(deskhub::AuthRole::Host, clientKey,
                                           machines.host.fingerprint))) ==
              deskhub::AuthResultCode::BadSignature,
        "a signature over the host-role transcript is refused");
    Check(AnswerWith(machines,
              SignedByClient(machines, transcript(deskhub::AuthRole::Client, otherKey,
                                           machines.host.fingerprint))) ==
              deskhub::AuthResultCode::BadSignature,
        "a signature naming another client key is refused");
    Check(AnswerWith(machines, SignedByClient(machines, std::vector<uint8_t>(64, 0x41))) ==
              deskhub::AuthResultCode::BadSignature,
        "a signature over unrelated bytes is refused");
    auto truncated = SignedByClient(machines, right);
    truncated.resize(truncated.size() / 2);
    Check(AnswerWith(machines, truncated) == deskhub::AuthResultCode::BadSignature,
        "a truncated signature is refused");
    Check(AnswerWith(machines, {}) == deskhub::AuthResultCode::BadSignature,
        "an empty proof is refused");
}

void TestAnAnswerBeforeAChallengeIsRefused() {
    std::printf("[authneg] an answer that arrives before any challenge is refused...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client), "the client key is authorized");
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    deskhub::AuthChallenge invented;
    invented.mode = deskhub::AuthMode::Signature;
    const auto early = client.Answer(invented);
    Check(early.has_value(), "the client can produce a valid proof without being asked");
    if (!early) return;
    Check(host.Respond(*early).code != deskhub::AuthResultCode::Accepted,
        "the host refuses a response it never challenged for");
    Check(host.State() == deskhubp::HostAuthState::Settled && !host.Begin(client.Begin()),
        "and the out-of-order message ends the handshake");
}

}

void RunAuthNegotiationTests() {
    if (!deskhubp::QuicAvailable()) return;
    TestUnknownKeyNeverRequestsApproval();
    TestAuthorizedKeyMustSignForThisHost();
    TestRevocationDuringHandshakeIsEnforced();
    TestAProofCannotMoveToAnotherTlsSession();
    TestMalformedKeyIsRejected();
    TestAPublicKeyAloneCannotImpersonateAClient();
    TestTheKeyCannotChangeMidHandshake();
    TestOnlyTheExactTranscriptIsAccepted();
    TestAnAnswerBeforeAChallengeIsRefused();
}
