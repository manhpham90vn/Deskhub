#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"

#include <cstdio>
#include <string>

namespace {

struct SavedIdentity {
    std::string key{};

    SavedIdentity() {
        key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    }

    ~SavedIdentity() {
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
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    Check(identity.Valid(), "the machine has an identity");
    if (!identity.Valid()) return;

    const std::vector<uint8_t> pub = identity.publicKey;
    Check(!pub.empty(), "its public key can be put on the wire");

    const std::string publicText = deskhubp::IdentityPublicKeyText(identity);
    Check(!publicText.empty(), "the key can be copied as OpenSSH text");
    const std::string labelled = deskhubp::IdentityPublicKeyLine(identity, "Study PC");
    Check(labelled.ends_with(" Study PC") && labelled.starts_with(publicText),
        "the copied line can carry the device name as its label");
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
    const deskhubp::HostIdentity other = deskhubp::LoadOrCreateHostIdentity();
    const std::vector<uint8_t> otherPub = other.publicKey;
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

}

void RunAuthProofTests() {
    TestAKeyProvesTheMachineItBelongsTo();
    TestAProofCannotBeCarriedToADifferentHost();
}
