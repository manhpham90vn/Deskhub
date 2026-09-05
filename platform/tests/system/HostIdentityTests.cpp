#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <cstdio>
#include <string>

namespace {

void TestIdentityIsCreatedOnceAndKept() {
    std::printf("[identity] a host makes one key pair and never changes it...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[identity] skipped: this build has no QUIC library\n");
        return;
    }
    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();

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

    const auto fromPem = deskhubp::FingerprintOfCertPem(first.certPem);
    Check(fromPem && *fromPem == first.fingerprint,
        "the same fingerprint comes back out of the certificate alone");
    Check(!deskhubp::FingerprintOfCertPem("not a certificate").has_value(),
        "junk in place of a certificate has no fingerprint");
    Check(!deskhubp::FingerprintOfCertDer(std::span<const uint8_t>()).has_value(),
        "and neither does an empty one");

    Check(deskhubp::ForgetHostIdentity(), "the identity can be thrown away");
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

void TestUnusableStoredKeyIsReplaced() {
    std::printf("[identity] a stored key this build cannot sign with is thrown away...\n");
    if (!deskhubp::QuicAvailable()) return;
    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();

    deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, kEd25519Cert);
    deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, "-----BEGIN PRIVATE KEY-----\n");
    Check(deskhubp::FingerprintOfCertPem(kEd25519Cert).has_value(),
        "the old certificate still parses, so this is not a parse failure");
    Check(!deskhubp::LoadHostIdentity().Valid(),
        "but it is refused rather than presented to a peer that cannot use it");

    const deskhubp::HostIdentity fresh = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(fresh.Valid(), "asking for an identity replaces it with one that works");
    Check(fresh.certPem.find("BEGIN CERTIFICATE") != std::string::npos,
        "and writes the replacement to disk");
    Check(deskhubp::LoadHostIdentity().fingerprint == fresh.fingerprint,
        "which is what the next launch reads back");
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
    TestUnusableStoredKeyIsReplaced();
    TestTrustStoreOnDisk();
}
