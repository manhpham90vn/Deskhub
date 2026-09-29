#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/system/AppDataFile.h"
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

    bool Make() {
        ForgetHostIdentity();
        client = deskhubp::LoadOrCreateHostIdentity("client-test");
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity("host-test");
        return client.Valid() && host.Valid() && client.fingerprint != host.fingerprint;
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

}

void RunAuthNegotiationTests() {
    if (!deskhubp::QuicAvailable()) return;
    TestUnknownKeyNeverRequestsApproval();
    TestAuthorizedKeyMustSignForThisHost();
    TestRevocationDuringHandshakeIsEnforced();
    TestAProofCannotMoveToAnotherTlsSession();
    TestMalformedKeyIsRejected();
}
