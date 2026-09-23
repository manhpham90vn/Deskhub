#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/PairedDevicesFile.h"

#include <cstdio>
#include <string>

namespace {

constexpr const char* kCode = "0417";

struct CleanSlate {
    std::string cert{};
    std::string key{};
    std::string paired{};

    CleanSlate() {
        cert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
        key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
        paired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
        deskhubp::ForgetAllPairedDevices();
    }

    ~CleanSlate() {
        if (!cert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, cert);
        if (!key.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
        if (paired.empty())
            deskhubp::ForgetAllPairedDevices();
        else
            deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, paired);
    }
};

struct TwoMachines {
    deskhubp::HostIdentity host{};
    deskhubp::HostIdentity client{};

    bool Make() {
        ForgetHostIdentity();
        client = deskhubp::LoadOrCreateHostIdentity("deskhub-client");
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity("deskhub-host");
        return client.Valid() && host.Valid() && !(client.fingerprint == host.fingerprint);
    }
};

deskhubp::HostAuthConfig HostConfig(const TwoMachines& machines, std::string passcode,
    bool allowNew = true) {
    deskhubp::HostAuthConfig config;
    config.identity = machines.host;
    config.SetPasscode(deskhubp::LoadOrCreateAuthSalt(), passcode);
    config.allowNewPairings = allowNew;
    return config;
}

deskhubp::ClientAuthConfig ClientConfig(const TwoMachines& machines, std::string passcode) {
    deskhubp::ClientAuthConfig config;
    config.identity = machines.client;
    config.passcode = std::move(passcode);
    config.hostFingerprint = machines.host.fingerprint;
    config.clientName = "manh laptop";
    return config;
}

deskhub::AuthResult RunHandshake(deskhubp::HostAuth& host, deskhubp::ClientAuth& client,
    bool& sawChallenge) {
    sawChallenge = false;
    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    if (!challenge) return deskhub::AuthResult{};
    sawChallenge = true;

    const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
    if (!response) {
        deskhub::AuthResult refused;
        refused.code = deskhub::AuthResultCode::NotPaired;
        return refused;
    }
    return host.Respond(*response, 1000);
}

void TestThePasscodePairsThemAndIsNeverSent() {
    std::printf("[authneg] the first meeting is settled by the code, which never travels...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[authneg] skipped: this build has no QUIC library\n");
        return;
    }

    const CleanSlate guard;
    TwoMachines machines;
    Check(machines.Make(), "two machines with two different keys");
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, kCode));

    const deskhub::AuthStart start = client.Begin();
    Check(!start.publicKey.empty(), "the client announces its key");
    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(start);
    Check(challenge && challenge->mode == deskhub::AuthMode::Passcode,
        "a machine it has never met, with a code set, is asked to prove the code");

    const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
    Check(response.has_value(), "the client can answer");
    Check(std::string(reinterpret_cast<const char*>(response->proof.data()),
              response->proof.size())
                  .find(kCode) == std::string::npos,
        "and nothing it sends contains the code itself");

    const deskhub::AuthResult result = host.Respond(*response, 1000);
    Check(result.code == deskhub::AuthResultCode::Accepted, "the host accepts it");
    Check(client.HostProvedThePasscode(result),
        "and the host proved the code back, so remembering its key is earned, not assumed");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Paired,
        "the client is now on the paired list");
}

void TestAPairedMachineNeedsNoCode() {
    std::printf("[authneg] once paired, the key alone gets in - no code anywhere...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;
    deskhubp::RememberPairedDevice(machines.client.fingerprint, "manh laptop", 500);

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, ""));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Signature,
        "a paired machine is asked to sign, not to type anything");

    const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
    Check(response.has_value(), "it signs with the key it paired under");
    Check(host.Respond(*response, 2000).code == deskhub::AuthResultCode::Accepted,
        "and is let in with no passcode in the exchange at all");
}

void TestATypedCodeIsAlwaysChecked() {
    std::printf("[authneg] a machine that types a code is held to it, paired or not...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;
    deskhubp::RememberPairedDevice(machines.client.fingerprint, "manh laptop", 500);

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, "9999"));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Passcode,
        "even a paired machine that offers a code is asked to prove it");
    const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
    Check(response &&
              host.Respond(*response, 600).code == deskhub::AuthResultCode::WrongPasscode,
        "and a wrong code is refused instead of falling back to the key");

    deskhubp::HostAuth again;
    deskhubp::ClientAuth right;
    again.Configure(HostConfig(machines, kCode));
    right.Configure(ClientConfig(machines, kCode));
    const std::optional<deskhub::AuthChallenge> retry = again.Begin(right.Begin());
    Check(retry && retry->mode == deskhub::AuthMode::Passcode, "the right code is also checked");
    const std::optional<deskhub::AuthResponse> proof = right.Answer(*retry);
    Check(proof && again.Respond(*proof, 700).code == deskhub::AuthResultCode::Accepted,
        "and lets the machine in as before");
}

void TestForgettingTurnsThatMachineAway() {
    std::printf("[authneg] forgetting a machine sends it back to proving itself...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;
    deskhubp::RememberPairedDevice(machines.client.fingerprint, "manh laptop", 500);
    Check(deskhubp::ForgetPairedDevice(machines.client.fingerprint), "the owner forgets it");

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, "9999"));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Passcode,
        "it is a stranger again and must prove the code");
    const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
    Check(response.has_value(), "all it can offer is the code it thinks it knows");
    Check(response &&
              host.Respond(*response, 600).code == deskhub::AuthResultCode::WrongPasscode,
        "and a code it no longer knows is refused");
}

void TestAWrongCodeIsRefused() {
    std::printf("[authneg] a wrong code is refused, and pairs nothing...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, "1111"));

    bool sawChallenge = false;
    const deskhub::AuthResult result = RunHandshake(host, client, sawChallenge);
    Check(sawChallenge, "the exchange got as far as a challenge");
    Check(result.code == deskhub::AuthResultCode::WrongPasscode, "the verdict names the cause");
    Check(!client.HostProvedThePasscode(result), "the host proved nothing to the client either");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Unknown,
        "and nothing was written to the paired list");
}

void TestAMachineInTheMiddleCannotRelayTheProof() {
    std::printf("[authneg] a proof made for one machine's key is refused by another...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhub::Fingerprint impostor = machines.host.fingerprint;
    impostor.bytes[0] = uint8_t(impostor.bytes[0] ^ 0xFF);

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    deskhubp::ClientAuthConfig fooled = ClientConfig(machines, kCode);
    fooled.hostFingerprint = impostor;
    client.Configure(fooled);

    bool sawChallenge = false;
    const deskhub::AuthResult result = RunHandshake(host, client, sawChallenge);
    Check(result.code == deskhub::AuthResultCode::WrongPasscode,
        "the real host refuses a proof bound to somebody else's key");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Unknown,
        "so relaying it pairs nothing");
}

void TestWithNoCodeSomeoneAtTheHostDecides() {
    std::printf("[authneg] with no code set, a stranger waits for the owner to say yes...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, ""));
    client.Configure(ClientConfig(machines, ""));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Approval,
        "with nothing to prove the machine by, it is put to the owner");
    Check(host.State() == deskhubp::HostAuthState::AwaitingApproval, "and the host waits");
    Check(!client.Answer(*challenge).has_value(), "the client has nothing to answer with");

    Check(host.Approve(false, 3000).code == deskhub::AuthResultCode::Refused,
        "saying no turns it away");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Unknown,
        "and pairs nothing");

    deskhubp::HostAuth second;
    second.Configure(HostConfig(machines, ""));
    second.Begin(client.Begin());
    Check(second.Approve(true, 3000).code == deskhub::AuthResultCode::Accepted,
        "saying yes lets it in");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Paired,
        "and pairs it, so it never has to ask again");
}

void TestACodelessMachineGoesToTheOwnerEvenWithACodeSet() {
    std::printf("[authneg] offering no code against a passcode host asks the owner...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode));
    client.Configure(ClientConfig(machines, ""));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Approval,
        "a machine that brings no code is put to the owner instead of being refused");
    Check(host.State() == deskhubp::HostAuthState::AwaitingApproval, "and the host waits");

    Check(host.Approve(true, 3000).code == deskhub::AuthResultCode::Accepted,
        "saying yes lets it in without a code");
    Check(deskhubp::CheckPairedDevice(machines.client.fingerprint) ==
              deskhub::PairVerdict::Paired,
        "and pairs it like any other approval");
}

void TestPairingCanBeClosedOff() {
    std::printf("[authneg] with new pairings off, even the right code is refused...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    deskhubp::ClientAuth client;
    host.Configure(HostConfig(machines, kCode, false));
    client.Configure(ClientConfig(machines, kCode));

    const std::optional<deskhub::AuthChallenge> challenge = host.Begin(client.Begin());
    Check(challenge && challenge->mode == deskhub::AuthMode::Denied,
        "a stranger is turned away before the code is even asked for");
    Check(!client.Answer(*challenge).has_value(), "so a leaked code buys nothing");

    deskhubp::RememberPairedDevice(machines.client.fingerprint, "manh laptop", 500);
    deskhubp::ClientAuth keysOnly;
    keysOnly.Configure(ClientConfig(machines, ""));
    deskhubp::HostAuth again;
    again.Configure(HostConfig(machines, kCode, false));
    const std::optional<deskhub::AuthChallenge> forPaired = again.Begin(keysOnly.Begin());
    Check(forPaired && forPaired->mode == deskhub::AuthMode::Signature,
        "while a machine already paired still gets in - closing the door keeps the keys");
}

void TestJunkNeverGetsAnywhere() {
    std::printf("[authneg] an unusable key is refused before anything else happens...\n");
    if (!deskhubp::QuicAvailable()) return;

    const CleanSlate guard;
    TwoMachines machines;
    if (!machines.Make()) return;

    deskhubp::HostAuth host;
    host.Configure(HostConfig(machines, kCode));

    deskhub::AuthStart junk;
    junk.publicKey.assign(40, 0x7E);
    junk.clientName = "not a key";
    Check(!host.Begin(junk).has_value(), "a blob that is not a public key is refused");

    deskhub::AuthResponse empty;
    Check(host.Respond(empty, 1000).code != deskhub::AuthResultCode::Accepted,
        "and answering a challenge that was never issued accepts nothing");
}

}

void RunAuthNegotiationTests() {
    TestThePasscodePairsThemAndIsNeverSent();
    TestAPairedMachineNeedsNoCode();
    TestForgettingTurnsThatMachineAway();
    TestAWrongCodeIsRefused();
    TestAMachineInTheMiddleCannotRelayTheProof();
    TestWithNoCodeSomeoneAtTheHostDecides();
    TestACodelessMachineGoesToTheOwnerEvenWithACodeSet();
    TestATypedCodeIsAlwaysChecked();
    TestPairingCanBeClosedOff();
    TestJunkNeverGetsAnywhere();
}
