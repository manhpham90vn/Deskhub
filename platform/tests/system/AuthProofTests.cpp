#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/PairedDevicesFile.h"

#include <cstdio>
#include <string>

namespace {

struct SavedIdentity {
    std::string cert{};
    std::string key{};

    SavedIdentity() {
        cert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
        key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    }

    ~SavedIdentity() {
        if (!cert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, cert);
        if (!key.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
    }
};

void TestAKeyProvesTheMachineItBelongsTo() {
    std::printf("[auth] a machine signs with the key its fingerprint is taken over...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[auth] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid(), "the machine has an identity");
    if (!identity.Valid()) return;

    const std::vector<uint8_t> pub = deskhubp::IdentityPublicKey(identity);
    Check(!pub.empty(), "its public key can be put on the wire");

    const std::string publicText = deskhubp::IdentityPublicKeyText(identity);
    Check(!publicText.empty(), "the key can be copied as OpenSSH text");
    Check(deskhubp::PublicKeySpkiFromText(publicText) == pub,
        "copying the text back gives the same public key used by authentication");
    Check(deskhubp::PublicKeySpkiFromText("ssh-ed25519 AAAA").empty(),
        "malformed imported key text is refused");

    const std::optional<deskhub::Fingerprint> derived = deskhubp::FingerprintOfPublicKey(pub);
    Check(derived && *derived == identity.fingerprint,
        "hashing that key gives exactly the fingerprint the far side looks up");

    const std::vector<uint8_t> message = {'d', 'e', 's', 'k', 'h', 'u', 'b'};
    const std::vector<uint8_t> signature = deskhubp::SignWithIdentity(identity, message);
    Check(!signature.empty(), "it can sign a challenge");
    Check(deskhubp::VerifySignature(pub, message, signature),
        "and the signature checks out against the key it published");

    std::vector<uint8_t> tampered = message;
    tampered[0] = 'D';
    Check(!deskhubp::VerifySignature(pub, tampered, signature),
        "a challenge that was altered does not verify");

    std::vector<uint8_t> badSignature = signature;
    badSignature[badSignature.size() / 2] ^= 0xFF;
    Check(!deskhubp::VerifySignature(pub, message, badSignature),
        "nor does a signature that was altered");
    Check(!deskhubp::VerifySignature({}, message, signature), "an empty key verifies nothing");

    ForgetHostIdentity();
    const deskhubp::HostIdentity other = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    const std::vector<uint8_t> otherPub = deskhubp::IdentityPublicKey(other);
    Check(!deskhubp::VerifySignature(otherPub, message, signature),
        "and a different machine cannot pass off someone else's signature as its own");
}

void TestAProofCannotBeCarriedToADifferentHost() {
    std::printf("[auth] a proof is bound to the key the client was shown...\n");
    if (!deskhubp::QuicAvailable()) return;

    deskhub::AuthSessionId sessionId{};
    sessionId.fill(0x41);
    deskhub::Fingerprint real;
    deskhub::Fingerprint impostor;
    for (size_t i = 0; i < real.bytes.size(); ++i) {
        real.bytes[i] = uint8_t(i);
        impostor.bytes[i] = uint8_t(i + 1);
    }
    const std::vector<uint8_t> clientKey(44, 0x21);

    const std::vector<uint8_t> toReal =
        deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, clientKey, real);
    const std::vector<uint8_t> toImpostor =
        deskhub::AuthTranscript(deskhub::AuthRole::Client, sessionId, clientKey, impostor);
    Check(toReal != toImpostor, "the machine's own key is part of what gets proved");

    Check(deskhub::AuthTranscript(deskhub::AuthRole::Host, sessionId, clientKey, real) != toReal,
        "and the two directions are not interchangeable, so neither can be replayed at the other");
}

void TestThePairedListOutlivesTheProcess() {
    std::printf("[auth] a machine paired once is still paired after a restart...\n");
    const std::string saved = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();

    deskhub::Fingerprint laptop;
    deskhub::Fingerprint stranger;
    for (size_t i = 0; i < laptop.bytes.size(); ++i) {
        laptop.bytes[i] = uint8_t(i + 3);
        stranger.bytes[i] = uint8_t(i + 200);
    }

    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Check(laptop) == deskhub::PairVerdict::Unknown,
        "nothing is paired to begin with");
    Check(deskhubp::RememberPairedDevice(laptop, "manh laptop", 1000), "pairing writes the file");
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Check(laptop) == deskhub::PairVerdict::Paired,
        "and reading it back lets that machine straight in");
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Check(stranger) == deskhub::PairVerdict::Unknown,
        "while a machine that never paired is still a stranger");

    Check(deskhubp::TouchPairedDevice(laptop, "manh laptop", 2000), "a visit is recorded");
    Check(!deskhubp::TouchPairedDevice(stranger, "ghost", 2000),
        "but a stranger's visit is not");

    Check(deskhubp::ForgetPairedDevice(laptop), "forgetting it reports that it did something");
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Check(laptop) == deskhub::PairVerdict::Unknown,
        "and that machine has to pair again - this is what revoking means");
    Check(!deskhubp::ForgetPairedDevice(laptop), "forgetting it twice changes nothing");

    deskhubp::RememberPairedDevice(laptop, "laptop", 3000);
    deskhubp::RememberPairedDevice(stranger, "phone", 3000);
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Size() == 2, "two machines are on the list");
    deskhubp::ForgetAllPairedDevices();
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Size() == 0, "and the big red button clears all of them");

    if (!saved.empty()) deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, saved);
}

}

void RunAuthProofTests() {
    TestAKeyProvesTheMachineItBelongsTo();
    TestAProofCannotBeCarriedToADifferentHost();
    TestThePairedListOutlivesTheProcess();
}
