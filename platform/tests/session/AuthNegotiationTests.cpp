#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/PairingTokenFile.h"

#include <cstdio>
#include <string>

namespace {

struct CleanSlate {
    std::string key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    std::string authorizedKeys = deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);

    CleanSlate() {
        RevokeAllClientKeys();
    }

    ~CleanSlate() {
        RevokeAllClientKeys();
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
        client = deskhubp::LoadOrCreateHostIdentity();
        ForgetHostIdentity();
        other = deskhubp::LoadOrCreateHostIdentity();
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity();
        return client.Valid() && other.Valid() && host.Valid() &&
               client.fingerprint != host.fingerprint && other.fingerprint != client.fingerprint;
    }

    deskhubp::HostAuthConfig HostConfig() const {
        deskhubp::HostAuthConfig config;
        config.identity = host;
        config.sessionId.fill(0x41);
        config.peerAddress = "10.0.0.7:47777";
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

void TestUnknownKeyWaitsForApproval() {
    std::printf("[authneg] an unknown key proves itself, then waits and leaves a request behind...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    client.Configure(machines.ClientConfig());
    const deskhub::AuthStart start = client.Begin();
    Check(!start.publicKey.empty(), "the client offers its public key");
    Check(start.pairingToken.empty(), "and no token, since it was not invited");
    const auto challenge = host.Begin(start);
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "an unlisted key is first asked to prove it holds the private key");
    const auto early = deskhubp::ListAccessRequests();
    Check(early && early->empty(), "and nothing is written before that proof");
    const auto response = challenge ? client.Answer(*challenge) : std::nullopt;
    Check(response.has_value(), "the client signs the challenge");
    if (!response) return;
    Check(host.Respond(*response).code == deskhub::AuthResultCode::AwaitingApproval,
        "the proven key is told to wait for approval");
    Check(host.State() == deskhubp::HostAuthState::Settled,
        "the handshake itself is over; the client has to come back");
    Check(!deskhubp::IsClientKeyAuthorized(machines.client.publicKey),
        "the unlisted key is never added automatically");
    const auto requests = deskhubp::ListAccessRequests();
    Check(requests && requests->size() == 1 &&
              requests->front().fingerprint == machines.client.fingerprint &&
              requests->front().label == "client" &&
              requests->front().address == "10.0.0.7:47777",
        "the owner sees who asked, by name, key and address");

    Check(deskhubp::ApproveAccessRequest(machines.client.fingerprint), "the owner approves");
    deskhubp::HostAuth again;
    again.Configure(machines.HostConfig());
    const auto approved = again.Begin(client.Begin());
    const auto proof = approved ? client.Answer(*approved) : std::nullopt;
    Check(proof && again.Respond(*proof).code == deskhub::AuthResultCode::Accepted,
        "the next attempt from that key is admitted");
    const auto allowed = deskhubp::ListAuthorizedClients();
    Check(allowed && allowed->size() == 1 && allowed->front().label == "client",
        "and the key is listed under the name the device gave");
}

void TestAnUnsignedClaimLeavesNothingBehind() {
    std::printf("[authneg] a public key without its private key cannot ask for access...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    const auto token = deskhubp::IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    Check(token.has_value(), "the host issued a token for its QR code");
    if (!token) return;

    deskhubp::HostIdentity forged = machines.other;
    forged.publicKey = machines.client.publicKey;
    auto config = machines.ClientConfig();
    config.identity = forged;
    config.clientName = "impostor";
    config.pairingToken.assign(token->begin(), token->end());
    deskhubp::ClientAuth impostor;
    impostor.Configure(config);
    deskhubp::HostAuth host;
    host.Configure(machines.HostConfig());
    const auto challenge = host.Begin(impostor.Begin());
    const auto response = challenge ? impostor.Answer(*challenge) : std::nullopt;
    Check(response && host.Respond(*response).code == deskhub::AuthResultCode::BadSignature,
        "a claim signed with the wrong private key is refused");
    const auto requests = deskhubp::ListAccessRequests();
    Check(requests && requests->empty(), "and no request carries the claimed key or name");
    Check(!deskhubp::IsClientKeyAuthorized(machines.client.publicKey),
        "the claimed key is not added to the allowed list");

    deskhubp::HostAuth genuine;
    deskhubp::ClientAuth client;
    genuine.Configure(machines.HostConfig());
    auto genuineConfig = machines.ClientConfig();
    genuineConfig.pairingToken.assign(token->begin(), token->end());
    client.Configure(genuineConfig);
    const auto genuineChallenge = genuine.Begin(client.Begin());
    const auto proof = genuineChallenge ? client.Answer(*genuineChallenge) : std::nullopt;
    Check(proof && genuine.Respond(*proof).code == deskhub::AuthResultCode::Accepted,
        "the token was not spent, so the real device still pairs with it");
}

void TestAPairingTokenAdmitsWithoutApproval() {
    std::printf("[authneg] a pairing token from the QR code lets an unknown key straight in...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    const auto token = deskhubp::IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    Check(token.has_value(), "the host issued a token for its QR code");
    if (!token) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(machines.HostConfig());
    auto config = machines.ClientConfig();
    config.pairingToken.assign(token->begin(), token->end());
    client.Configure(config);
    const deskhub::AuthStart start = client.Begin();
    Check(start.pairingToken.size() == deskhub::kPairingTokenBytes, "the client sends the token");
    const auto challenge = host.Begin(start);
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "the token holder is asked for a signature");
    Check(!deskhubp::IsClientKeyAuthorized(machines.client.publicKey),
        "the key is not saved before the signature arrives");
    const auto response = challenge ? client.Answer(*challenge) : std::nullopt;
    Check(response && host.Respond(*response).code == deskhub::AuthResultCode::Accepted,
        "and the signature admits the client at once");
    Check(!host.PairingTokenRejected(), "the token is not counted as a guess");
    Check(deskhubp::IsClientKeyAuthorized(machines.client.publicKey),
        "the key is now on the allowed list");
    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->empty(),
        "no request is left for the owner to click");

    deskhubp::HostAuth replay;
    replay.Configure(machines.HostConfig());
    auto otherConfig = machines.ClientConfig();
    otherConfig.identity = machines.other;
    otherConfig.pairingToken.assign(token->begin(), token->end());
    deskhubp::ClientAuth other;
    other.Configure(otherConfig);
    const auto replayed = replay.Begin(other.Begin());
    const auto replayProof = replayed ? other.Answer(*replayed) : std::nullopt;
    Check(replayProof &&
              replay.Respond(*replayProof).code == deskhub::AuthResultCode::AwaitingApproval,
        "a second machine replaying the same token is only asked to wait");
    Check(replay.PairingTokenRejected(), "and the replay counts as a failed guess");
    Check(!deskhubp::IsClientKeyAuthorized(machines.other.publicKey),
        "the second machine is not let in");
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
    return deskhubp::SignWithIdentity(machines.client, data);
}

void TestAPublicKeyAloneCannotImpersonateAClient() {
    std::printf("[authneg] knowing a client's public key is not enough to sign in as it...\n");
    const CleanSlate guard;
    Machines machines;
    if (!machines.Make()) return;
    Check(GrantClientKey(machines.client), "the victim's key is authorized");
    deskhubp::HostAuth host;
    host.Configure(machines.HostConfig());

    deskhubp::HostIdentity forged = machines.other;
    forged.publicKey = machines.client.publicKey;
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
    const auto clientKey = machines.client.publicKey;
    const auto otherKey = machines.other.publicKey;
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
    TestUnknownKeyWaitsForApproval();
    TestAnUnsignedClaimLeavesNothingBehind();
    TestAPairingTokenAdmitsWithoutApproval();
    TestAuthorizedKeyMustSignForThisHost();
    TestRevocationDuringHandshakeIsEnforced();
    TestAProofCannotMoveToAnotherTlsSession();
    TestMalformedKeyIsRejected();
    TestAPublicKeyAloneCannotImpersonateAClient();
    TestTheKeyCannotChangeMidHandshake();
    TestOnlyTheExactTranscriptIsAccepted();
    TestAnAnswerBeforeAChallengeIsRefused();
}
