#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/ffi/ClientKeyFfi.h"
#include "deskhubp/ffi/HostProfileFfi.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <cstdio>
#include <string>

namespace {

using deskhub::ui::HostProfileError;
using deskhub::ui::HostProfileMode;
using deskhub::ui::HostProfileRequest;

deskhub::Fingerprint KeyFor(uint8_t seed) {
    deskhub::Fingerprint fp;
    for (size_t i = 0; i < deskhub::kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(seed + i + 1);
    return fp;
}

void TestHostKeyTextAcceptsBothForms() {
    std::printf("[hostprofiles] a host key is pasted as a fingerprint or a public key...\n");
    const deskhub::Fingerprint fp = KeyFor(4);
    Check(deskhubp::ParseHostKeyText(deskhub::FormatFingerprint(fp)) == fp,
        "a SHA256 fingerprint is taken as it is");
    const auto identity = deskhubp::LoadOrCreateClientIdentity();
    const std::string publicKey = deskhubp::ClientPublicKeyText(identity);
    Check(deskhubp::ParseHostKeyText(publicKey) == identity.fingerprint,
        "a one-line public key becomes the fingerprint of its SPKI");
    Check(!deskhubp::ParseHostKeyText("not a key"), "anything else is refused");
}

void TestSavedHostsRoundTrip() {
    std::printf("[hostprofiles] a saved host is written, updated and removed on disk...\n");
    Check(deskhubp::ClearTrustedHosts(), "the test starts with no saved hosts");
    Check(deskhubp::LoadOrCreateClientIdentity().Valid(), "the default client key exists");

    HostProfileRequest add{"office", "127.0.0.1:47001", KeyFor(1), {}};
    Check(deskhubp::SaveHostProfile(HostProfileMode::Add, add) == HostProfileError::None,
        "a complete host is saved");
    const auto saved = deskhubp::LoadTrustStore().Find("127.0.0.1:47001");
    Check(saved && saved->label == "office" && saved->fingerprint == KeyFor(1),
        "the file holds the name, the address and the pinned key");
    Check(deskhubp::CheckTrustedHost("127.0.0.1:47001", KeyFor(1)) ==
              deskhub::TrustVerdict::Trusted,
        "and a connection to that address now trusts exactly that key");

    Check(deskhubp::SaveHostProfile(HostProfileMode::Add, add) == HostProfileError::AliasExists,
        "the same name cannot be added twice");
    Check(deskhubp::SaveHostProfile(HostProfileMode::Update,
              HostProfileRequest{"office", "", std::nullopt, "no-such-key"}) ==
              HostProfileError::UnknownIdentity,
        "a client key that does not exist is refused before anything is written");

    Check(deskhubp::SaveHostProfile(HostProfileMode::Update,
              HostProfileRequest{"office", "127.0.0.1:47002", std::nullopt, {}}) ==
              HostProfileError::None,
        "moving the host to a new address succeeds");
    Check(!deskhubp::LoadTrustStore().Find("127.0.0.1:47001") &&
              deskhubp::CheckTrustedHost("127.0.0.1:47002", KeyFor(1)) ==
                  deskhub::TrustVerdict::Trusted,
        "the old address is no longer trusted and the key moved with the host");

    Check(deskhubp::RemoveHostProfile("office") == HostProfileError::None,
        "removing the host succeeds");
    Check(deskhubp::RemoveHostProfile("office") == HostProfileError::AliasMissing,
        "a second removal reports there is nothing to remove");
    Check(deskhubp::LoadTrustStore().Size() == 0, "and the file is empty again");
}

void TestClientKeysAreCreatedAndListed() {
    std::printf("[hostprofiles] new client keys are made, refused and listed by name...\n");
    deskhubp::RemoveAppDataFile("client_key.work.pem");
    Check(dh_client_key_generate("work") == DHClientKeyOk, "a new key is generated");
    Check(dh_client_key_generate("work") == DHClientKeyNameInUse,
        "a second key cannot take the same name");
    Check(dh_client_key_generate("-bad") == DHClientKeyInvalidName, "a bad name is refused");
    Check(dh_client_key_import("other", "not a key", "") == DHClientKeyUnreadable,
        "an import that is not a private key is refused");
    Check(*dh_client_key_error_text(DHClientKeyUnreadable) != '\0', "with a sentence to show");

    DHClientKey keys[16]{};
    const int count = dh_client_keys(keys, 16);
    bool listed = false;
    for (int i = 0; i < count; ++i) listed = listed || std::string(keys[i].name) == "work";
    Check(listed, "the new key is listed");
    char publicKey[DH_PUBLIC_KEY_TEXT_CAP]{};
    Check(dh_client_public_key("work", publicKey, sizeof(publicKey)) > 0 &&
              deskhubp::ParseHostKeyText(publicKey) ==
                  deskhubp::LoadClientIdentity("work").fingerprint,
        "its public key text is the one a host will allow");
    Check(dh_client_public_key("missing", publicKey, sizeof(publicKey)) == 0,
        "a key that does not exist has no public key");
    deskhubp::RemoveAppDataFile("client_key.work.pem");
}

void TestUnreadableStoreIsNotOverwritten() {
    std::printf("[hostprofiles] a damaged saved-hosts file is never rewritten...\n");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, "damaged row\n"),
        "the saved-hosts file can be damaged for this test");
    Check(deskhubp::SaveHostProfile(HostProfileMode::Add,
              HostProfileRequest{"office", "127.0.0.1:47001", KeyFor(1), {}}) ==
              HostProfileError::StoreUnreadable,
        "a save reports the damage instead of replacing the file");
    Check(deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName) == "damaged row\n",
        "the damaged file is left for the user to fix");
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
}

void TestFfiSavesTheSameHosts() {
    std::printf("[hostprofiles] the app bridge saves and lists the same hosts...\n");
    Check(deskhubp::ClearTrustedHosts(), "the test starts with no saved hosts");
    const std::string key = deskhub::FormatFingerprint(KeyFor(7));
    Check(dh_host_trust_new("127.0.0.1", key.c_str()) == DHHostProfileOk,
        "a confirmed first-time host is saved through the bridge");

    DHHostProfile rows[4]{};
    Check(dh_host_profiles(nullptr, 0) == 1, "the count is available without a buffer");
    Check(dh_host_profiles(rows, 4) == 1 && std::string(rows[0].alias) == "127-0-0-1" &&
              std::string(rows[0].endpoint) == "127.0.0.1:47777" &&
              std::string(rows[0].identity) == "default" && std::string(rows[0].fingerprint) == key,
        "the row shows the name, the canonical address, the client key and the pinned key");
    Check(dh_host_profile_remove("127-0-0-1") == DHHostProfileOk, "the host is removed");
    Check(*dh_host_profile_error_text(DHHostProfileAliasMissing) != '\0',
        "every refusal comes with a sentence to show");

    Check(dh_host_trust_new("127.0.0.1:47020", "not a fingerprint") == DHHostProfileInvalidHostKey,
        "trusting a new host needs a real fingerprint");
    Check(dh_host_trust_new("127.0.0.1:47020", key.c_str()) == DHHostProfileOk,
        "a confirmed first-time host is saved");
    Check(dh_host_trust_new("127.0.0.1:47020", key.c_str()) == DHHostProfileAddressInUse,
        "and trusting it again never overwrites the saved key");
    Check(dh_host_profiles(rows, 4) == 1 && std::string(rows[0].alias) == "127-0-0-1-47020",
        "the saved host gets a name from its address");
    char prompt[512]{};
    Check(dh_trust_new_host_prompt("127.0.0.1:47020", key.c_str(), prompt, sizeof(prompt)) > 0 &&
              std::string(prompt).find(key) != std::string::npos,
        "the confirmation text carries the fingerprint to compare");
    Check(dh_host_profile_remove("127-0-0-1-47020") == DHHostProfileOk, "and it can be removed");
}

}

void RunHostProfilesTests() {
    if (!deskhubp::QuicAvailable()) {
        std::printf("[hostprofiles] skipped: this build has no key support\n");
        return;
    }
    TestHostKeyTextAcceptsBothForms();
    TestSavedHostsRoundTrip();
    TestFfiSavesTheSameHosts();
    TestClientKeysAreCreatedAndListed();
    TestUnreadableStoreIsNotOverwritten();
}
