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
    std::string cert{};
    std::string key{};
    std::string trust{};

    SavedIdentity()
        : cert(deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName)),
          key(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName)),
          trust(deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName)) {
        ForgetHostIdentity();
    }

    ~SavedIdentity() {
        if (cert.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kHostCertFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, cert);
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
    std::printf("[identity] a host makes one key pair and never changes it...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[identity] skipped: this build has no QUIC library\n");
        return;
    }
    const SavedIdentity guard;

    Check(!deskhubp::LoadHostIdentity().Valid(), "a machine that never shared has no identity");

    const deskhubp::HostIdentity first = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(first.Valid(), "the first run creates one");
    Check(first.certPem.find("BEGIN CERTIFICATE") != std::string::npos,
        "the certificate is written as PEM, which is what quiche loads");
    Check(first.keyPem.find("BEGIN PRIVATE KEY") != std::string::npos,
        "and so is the private key");
    Check(!first.certPath.empty() && !first.keyPath.empty(),
        "both have a path on disk to hand to the transport");

    const deskhubp::HostIdentity again = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(again.fingerprint == first.fingerprint,
        "asking again returns the same key, which is what makes trust-on-first-use work");
    Check(deskhubp::LoadHostIdentity().fingerprint == first.fingerprint,
        "and a fresh load from disk agrees");

    Check(deskhub::FormatFingerprint(first.fingerprint).size() ==
              deskhub::kFingerprintPrefix.size() + deskhub::kFingerprintTextBytes,
        "the fingerprint is the fixed-width text a user can read out loud");

    Check(!deskhubp::FingerprintOfCertDer(std::span<const uint8_t>()).has_value(),
        "and neither does an empty one");

    Check(ForgetHostIdentity(), "the identity can be thrown away");
    Check(!deskhubp::LoadHostIdentity().Valid(), "after which the machine has none again");

    const deskhubp::HostIdentity replacement = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(replacement.Valid() && replacement.fingerprint != first.fingerprint,
        "a new one is genuinely new - this is the change every client must warn about");
}

const char* const kEd25519Cert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIHhMIGUoAMCAQICCCBRfTZ1RuD+MAUGAytlcDAXMRUwEwYDVQQDDAxkZXNraHVi\n"
    "LXRlc3QwHhcNMjYwODE0MDc0NzAwWhcNMzYwODExMDc0NzAwWjAXMRUwEwYDVQQD\n"
    "DAxkZXNraHViLXRlc3QwKjAFBgMrZXADIQBhTQ8gHVWnfLhVYNVYUFHCUlOZDMLE\n"
    "vmFAtNSKF3aJHzAFBgMrZXADQQBnBBBSFJ4a3wYEGVYKTIyBrZE4hRIWuNBhSD3P\n"
    "1lRxNSVAoWFAaTuUzL0Uy1QG8v04BqXvXPBLPFXfmKuGE7wJ\n"
    "-----END CERTIFICATE-----\n";

void TestUnusableStoredIdentityDoesNotRotate() {
    std::printf("[identity] an unusable stored host key never rotates silently...\n");
    if (!deskhubp::QuicAvailable()) return;
    const SavedIdentity guard;

    deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, kEd25519Cert);
    deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, "-----BEGIN PRIVATE KEY-----\n");
    Check(!deskhubp::LoadHostIdentity().Valid(),
        "but it is refused rather than presented to a peer that cannot use it");

    Check(!deskhubp::LoadOrCreateHostIdentity("deskhub-test").Valid(),
        "asking for an identity refuses the unusable stored pair");
    Check(deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName) == kEd25519Cert,
        "the existing certificate is left for explicit recovery");

    ForgetHostIdentity();
    const deskhubp::HostIdentity first = deskhubp::LoadOrCreateHostIdentity("first");
    Check(first.Valid(), "a fresh host identity can be generated");
    ForgetHostIdentity();
    const deskhubp::HostIdentity second = deskhubp::LoadOrCreateHostIdentity("second");
    Check(second.Valid(), "a second independent host identity can be generated");

    Check(deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, first.certPem),
        "the test can place a mismatched certificate on disk");
    Check(!deskhubp::LoadHostIdentity().Valid(),
        "a certificate and private key from different identities are invalid");
    Check(!deskhubp::LoadOrCreateHostIdentity("deskhub-test").Valid(),
        "a mismatched pair is not silently rotated");

    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    Check(!deskhubp::LoadOrCreateHostIdentity("deskhub-test").Valid(),
        "a missing private key beside an existing certificate is not silently rotated");

#ifndef _WIN32
    deskhubp::RemoveAppDataFile(deskhubp::kHostCertFileName);
    const auto keyPath = deskhubp::AppDataFilePath(deskhubp::kHostKeyFileName);
    std::error_code error;
    std::filesystem::create_symlink(keyPath.string() + ".missing", keyPath, error);
    Check(!error, "the test can place a dangling key symlink on disk");
    Check(!deskhubp::LoadOrCreateHostIdentity("deskhub-test").Valid(),
        "a dangling private key symlink is treated as an existing identity");
#endif
}

void TestTrustStoreOnDisk() {
    std::printf("[identity] the list of machines we have trusted survives a restart...\n");
    const SavedIdentity guard;
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);

    deskhub::Fingerprint fp;
    for (size_t i = 0; i < deskhub::kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(i + 1);

    Check(deskhubp::CheckTrustedHost("10.1.2.3:47777", fp) == deskhub::TrustVerdict::Unknown,
        "a machine we have never met is unknown");
    Check(deskhubp::RememberTrustedHost("10.1.2.3:47777", "Desk", fp, 1000),
        "trusting it writes the file");
    Check(deskhubp::CheckTrustedHost("10.1.2.3:47777", fp) == deskhub::TrustVerdict::Trusted,
        "and a later launch reads it back");
    const std::string valid = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName,
              valid + "damaged row\n"),
        "the isolated trust file can be corrupted for a regression check");
    Check(deskhubp::CheckTrustedHost("10.1.2.3:47777", fp) == deskhub::TrustVerdict::Unknown,
        "a damaged trust file does not trust even a valid stored pin");
    Check(!deskhubp::RememberTrustedHost("10.1.2.3:47777", "Desk", fp, 2000),
        "a normal update cannot overwrite a damaged trust file");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, valid),
        "the valid trust file is restored");

    deskhub::Fingerprint other = fp;
    other.bytes[0] ^= 0xFF;
    Check(deskhubp::CheckTrustedHost("10.1.2.3:47777", other) == deskhub::TrustVerdict::Changed,
        "a different key at the same address is reported as changed");

    Check(deskhubp::ForgetTrustedHost("10.1.2.3:47777"), "the machine can be forgotten");
    Check(!deskhubp::ForgetTrustedHost("10.1.2.3:47777"), "and forgetting it twice does nothing");
    Check(deskhubp::CheckTrustedHost("10.1.2.3:47777", fp) == deskhub::TrustVerdict::Unknown,
        "after which it is a stranger again");

    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
}

}

void RunHostIdentityTests() {
    TestIdentityIsCreatedOnceAndKept();
    TestUnusableStoredIdentityDoesNotRotate();
    TestTrustStoreOnDisk();
}
