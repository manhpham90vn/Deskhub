#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

struct SavedIdentity {
    std::string key{};
    std::string trust{};

    SavedIdentity()
        : key(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName)),
          trust(deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName)) {
        ForgetHostIdentity();
    }

    ~SavedIdentity() {
        if (key.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
        if (trust.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, trust);
    }
};

void TestIdentityIsCreatedOnceAndKept() {
    std::printf("[identity] a machine makes one key and never changes it...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[identity] skipped: this build has no QUIC library\n");
        return;
    }
    const SavedIdentity guard;

    Check(!deskhubp::LoadHostIdentity().Valid(), "a machine that never ran has no identity");
    deskhubp::RemoveAppDataFile("host_cert.pem");

    const deskhubp::HostIdentity first = deskhubp::LoadOrCreateHostIdentity();
    Check(first.Valid(), "the first run creates one");
    Check(first.keyPem.find("BEGIN PRIVATE KEY") != std::string::npos,
        "the private key is written as PEM, which is what quiche loads");
    Check(!first.keyPath.empty(), "and has a path on disk to hand to the transport");
    Check(!first.publicKey.empty(), "the public half is ready to go on the wire");
    Check(deskhubp::ReadAppDataFile("host_cert.pem").empty(),
        "no certificate is written: the key alone is the identity");

    const std::string cert = deskhubp::TransportCertificatePem(first);
    Check(cert.find("BEGIN CERTIFICATE") != std::string::npos,
        "a certificate is made in memory whenever TLS needs one");
    Check(deskhubp::TransportCertificatePem(first) != cert,
        "and each one is fresh, so nothing about it is ever pinned");

    const deskhubp::HostIdentity again = deskhubp::LoadOrCreateHostIdentity();
    Check(again.fingerprint == first.fingerprint,
        "asking again returns the same key, which is what makes trust work");
    Check(deskhubp::LoadHostIdentity().fingerprint == first.fingerprint,
        "and a fresh load from disk agrees");

    Check(deskhub::FormatFingerprint(first.fingerprint).size() ==
              deskhub::kFingerprintPrefix.size() + deskhub::kFingerprintTextBytes,
        "the fingerprint is the fixed-width text a user can read out loud");

    Check(!deskhubp::FingerprintOfCertDer(std::span<const uint8_t>()).has_value(),
        "an empty certificate has no fingerprint");

    Check(ForgetHostIdentity(), "the identity can be thrown away");
    Check(!deskhubp::LoadHostIdentity().Valid(), "after which the machine has none again");

    const deskhubp::HostIdentity replacement = deskhubp::LoadOrCreateHostIdentity();
    Check(replacement.Valid() && replacement.fingerprint != first.fingerprint,
        "a new one is genuinely new - to other machines this is a different device");
}

void TestUnusableStoredIdentityDoesNotRotate() {
    std::printf("[identity] an unusable stored key never rotates silently...\n");
    if (!deskhubp::QuicAvailable()) return;
    const SavedIdentity guard;

    deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, "-----BEGIN PRIVATE KEY-----\n");
    Check(!deskhubp::LoadHostIdentity().Valid(), "a damaged key is refused");
    Check(!deskhubp::LoadOrCreateHostIdentity().Valid(),
        "asking for an identity refuses to replace the damaged key");
    Check(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName) == "-----BEGIN PRIVATE KEY-----\n",
        "the existing file is left for explicit recovery");

    const char* const kEd25519Key =
        "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VwBCIEIF1ax7f+e1fuIGCnrp1fvdSTe6wMi39Wo9kl2ENAvsWX\n"
        "-----END PRIVATE KEY-----\n";
    deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, kEd25519Key);
    Check(!deskhubp::LoadHostIdentity().Valid(),
        "a key TLS cannot use is refused rather than presented to a peer");
    Check(!deskhubp::LoadOrCreateHostIdentity().Valid(), "and is not rotated either");

#ifndef _WIN32
    ForgetHostIdentity();
    const auto keyPath = deskhubp::AppDataFilePath(deskhubp::kHostKeyFileName);
    std::error_code error;
    std::filesystem::create_symlink(keyPath.string() + ".missing", keyPath, error);
    Check(!error, "the test can place a dangling key symlink on disk");
    Check(!deskhubp::LoadOrCreateHostIdentity().Valid(),
        "a dangling private key symlink is treated as an existing identity");
#endif
}

void TestTrustStoreOnDisk() {
    std::printf("[identity] the list of machines we have trusted survives a restart...\n");
    const SavedIdentity guard;
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);

    deskhub::Fingerprint fp;
    for (size_t i = 0; i < deskhub::kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(i + 1);

    Check(deskhubp::CheckTrustedHost(fp) == deskhub::TrustVerdict::Unknown,
        "a machine we have never met is unknown");
    Check(deskhubp::RememberTrustedHost(fp, "Desk", "10.1.2.3:47777", 1000),
        "trusting it writes the file");
    Check(deskhubp::CheckTrustedHost(fp) == deskhub::TrustVerdict::Trusted,
        "and a later launch reads it back");
    Check(deskhubp::TouchTrustedHost(fp, "10.1.2.9:47777", 2000) &&
              deskhubp::LoadTrustStore().Find(fp)->endpoint == "10.1.2.9:47777",
        "a host that answers from a new address is remembered there");
    const std::string valid = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName,
              valid + "damaged row\n"),
        "the isolated trust file can be corrupted for a regression check");
    Check(!deskhubp::TryLoadTrustStore(),
        "a damaged trust file is reported as invalid rather than an empty list");
    Check(deskhubp::CheckTrustedHost(fp) == deskhub::TrustVerdict::Unknown,
        "a damaged trust file does not trust even a valid stored pin");
    deskhub::Fingerprint fresh = fp;
    fresh.bytes[1] ^= 0xFF;
    Check(deskhubp::RememberTrustedHost(fresh, "Laptop", "10.9.9.9:47777", 2000),
        "trusting a machine replaces a damaged trust file with a fresh one");
    const auto restarted = deskhubp::TryLoadTrustStore();
    Check(restarted && restarted->Size() == 1 &&
              deskhubp::CheckTrustedHost(fresh) == deskhub::TrustVerdict::Trusted &&
              deskhubp::CheckTrustedHost(fp) == deskhub::TrustVerdict::Unknown,
        "the fresh file holds only the new pin, none of the damaged contents");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, valid + "damaged row\n"),
        "the trust file is damaged again");
    Check(deskhubp::ClearTrustedHosts(), "revoke-all replaces a damaged trust file");
    const auto emptied = deskhubp::TryLoadTrustStore();
    Check(emptied && emptied->Size() == 0, "with a readable empty one");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, valid + "damaged row\n"),
        "the trust file is damaged once more");
    Check(!deskhubp::ForgetTrustedHost(fp),
        "forgetting a pin the fresh file does not hold reports no change");
    const auto afterForget = deskhubp::TryLoadTrustStore();
    Check(afterForget && afterForget->Size() == 0,
        "but the damaged trust file is still replaced by an empty one");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, valid),
        "the valid trust file is restored");

    deskhub::Fingerprint other = fp;
    other.bytes[0] ^= 0xFF;
    Check(deskhubp::CheckTrustedHost(other) == deskhub::TrustVerdict::Unknown,
        "a different key is another machine, whichever address it answers from");

    Check(deskhubp::ForgetTrustedHost(fp), "the machine can be forgotten");
    Check(!deskhubp::ForgetTrustedHost(fp), "and forgetting it twice does nothing");
    Check(deskhubp::CheckTrustedHost(fp) == deskhub::TrustVerdict::Unknown,
        "after which it is a stranger again");

    Check(deskhubp::RememberTrustedHost(fp, "Desk", "10.1.2.3:47777", 3000),
        "the valid trust file can be populated again");
    Check(deskhubp::ClearTrustedHosts(), "revoke-all persists an empty trust list");
    const auto cleared = deskhubp::TryLoadTrustStore();
    Check(cleared && cleared->Size() == 0,
        "revoke-all leaves a readable empty trust file");

    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
}

}

void RunHostIdentityTests() {
    TestIdentityIsCreatedOnceAndKept();
    TestUnusableStoredIdentityDoesNotRotate();
    TestTrustStoreOnDisk();
}
