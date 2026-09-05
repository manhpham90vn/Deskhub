#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/PairedDevicesFile.h"

#include <cstdio>
#include <string>

namespace {

void TestAKeyProvesTheMachineItBelongsTo() {
    std::printf("[auth] a machine signs with the key its fingerprint is taken over...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[auth] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid(), "the machine has an identity");
    if (!identity.Valid()) return;

    const std::vector<uint8_t> pub = deskhubp::IdentityPublicKey(identity);
    Check(!pub.empty(), "its public key can be put on the wire");

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

    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity other = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    const std::vector<uint8_t> otherPub = deskhubp::IdentityPublicKey(other);
    Check(!deskhubp::VerifySignature(otherPub, message, signature),
        "and a different machine cannot pass off someone else's signature as its own");
}

void TestBothSidesAgreeOnlyWhenThePasscodeMatches() {
    std::printf("[auth] SPAKE2 agrees on a key only when both sides know the code...\n");
    if (!deskhubp::QuicAvailable()) return;

    const deskhubp::AuthSalt salt = deskhubp::NewAuthSalt();
    const deskhubp::PasscodeVerifier right = deskhubp::MakePasscodeVerifier(salt, "0417");
    const deskhubp::PasscodeVerifier wrong = deskhubp::MakePasscodeVerifier(salt, "1111");
    Check(right != wrong, "two different codes hash to two different verifiers");

    const deskhubp::AuthSalt otherSalt = deskhubp::NewAuthSalt();
    Check(deskhubp::MakePasscodeVerifier(otherSalt, "0417") != right,
        "and the same code on another machine stores differently, which is what the salt buys");

    {
        deskhubp::Spake2Session host;
        deskhubp::Spake2Session client;
        std::vector<uint8_t> hostMsg;
        std::vector<uint8_t> clientMsg;
        Check(host.Start(true, right, hostMsg) && client.Start(false, right, clientMsg),
            "both sides produce their SPAKE2 message");
        Check(!hostMsg.empty() && !clientMsg.empty(), "and neither message is empty");

        deskhubp::PasscodeVerifier hostKey{};
        deskhubp::PasscodeVerifier clientKey{};
        Check(host.Finish(clientMsg, hostKey) && client.Finish(hostMsg, clientKey),
            "each side completes the exchange");
        Check(hostKey == clientKey,
            "and they arrive at the same key - this is what proves the code without sending it");
    }

    {
        deskhubp::Spake2Session host;
        deskhubp::Spake2Session client;
        std::vector<uint8_t> hostMsg;
        std::vector<uint8_t> clientMsg;
        host.Start(true, right, hostMsg);
        client.Start(false, wrong, clientMsg);

        deskhubp::PasscodeVerifier hostKey{};
        deskhubp::PasscodeVerifier clientKey{};
        host.Finish(clientMsg, hostKey);
        client.Finish(hostMsg, clientKey);
        Check(hostKey != clientKey,
            "a wrong code still completes, but lands on a different key - so the MAC will not match");
    }

    deskhubp::Spake2Session lonely;
    deskhubp::PasscodeVerifier key{};
    Check(!lonely.Finish({}, key), "finishing without ever starting is refused");
}

void TestAProofCannotBeCarriedToADifferentHost() {
    std::printf("[auth] a proof is bound to the key the client was shown...\n");
    if (!deskhubp::QuicAvailable()) return;

    const deskhubp::AuthNonce nonce = deskhubp::NewAuthNonce();
    deskhub::Fingerprint real;
    deskhub::Fingerprint impostor;
    for (size_t i = 0; i < real.bytes.size(); ++i) {
        real.bytes[i] = uint8_t(i);
        impostor.bytes[i] = uint8_t(i + 1);
    }

    const deskhubp::PasscodeVerifier shared =
        deskhubp::MakePasscodeVerifier(deskhubp::NewAuthSalt(), "0417");

    const std::vector<uint8_t> toReal = deskhubp::AuthTranscript("client", nonce, real);
    const std::vector<uint8_t> toImpostor = deskhubp::AuthTranscript("client", nonce, impostor);
    Check(toReal != toImpostor, "the machine's own key is part of what gets proved");

    const deskhubp::AuthMac forReal = deskhubp::ComputeAuthMac(shared, toReal);
    const deskhubp::AuthMac forImpostor = deskhubp::ComputeAuthMac(shared, toImpostor);
    Check(!deskhubp::MacsMatch(forReal, forImpostor),
        "so a proof made for a machine in the middle is not one the real host accepts");
    Check(deskhubp::MacsMatch(forReal, deskhubp::ComputeAuthMac(shared, toReal)),
        "while the proof for the right machine checks out every time");

    Check(deskhubp::AuthTranscript("host", nonce, real) != toReal,
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

    Check(deskhubp::CheckPairedDevice(laptop) == deskhub::PairVerdict::Unknown,
        "nothing is paired to begin with");
    Check(deskhubp::RememberPairedDevice(laptop, "manh laptop", 1000), "pairing writes the file");
    Check(deskhubp::CheckPairedDevice(laptop) == deskhub::PairVerdict::Paired,
        "and reading it back lets that machine straight in - no passcode consulted");
    Check(deskhubp::CheckPairedDevice(stranger) == deskhub::PairVerdict::Unknown,
        "while a machine that never paired is still a stranger");

    Check(deskhubp::TouchPairedDevice(laptop, "manh laptop", 2000), "a visit is recorded");
    Check(!deskhubp::TouchPairedDevice(stranger, "ghost", 2000),
        "but a stranger's visit is not");

    Check(deskhubp::ForgetPairedDevice(laptop), "forgetting it reports that it did something");
    Check(deskhubp::CheckPairedDevice(laptop) == deskhub::PairVerdict::Unknown,
        "and that machine has to pair again - this is what revoking means");
    Check(!deskhubp::ForgetPairedDevice(laptop), "forgetting it twice changes nothing");

    deskhubp::RememberPairedDevice(laptop, "laptop", 3000);
    deskhubp::RememberPairedDevice(stranger, "phone", 3000);
    Check(deskhubp::LoadPairedDevices().Size() == 2, "two machines are on the list");
    deskhubp::ForgetAllPairedDevices();
    Check(deskhubp::LoadPairedDevices().Size() == 0, "and the big red button clears all of them");

    if (!saved.empty()) deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, saved);
}

}

void RunAuthProofTests() {
    TestAKeyProvesTheMachineItBelongsTo();
    TestBothSidesAgreeOnlyWhenThePasscodeMatches();
    TestAProofCannotBeCarriedToADifferentHost();
    TestThePairedListOutlivesTheProcess();
}
