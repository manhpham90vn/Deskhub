#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/net/PairingInvite.h"
#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/ffi/DevicesFfi.h"
#include "deskhubp/ffi/HostProfileFfi.h"
#include "deskhubp/ffi/PairingFfi.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
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
    const auto identity = deskhubp::LoadOrCreateHostIdentity();
    const std::string publicKey = deskhubp::IdentityPublicKeyText(identity);
    Check(deskhubp::ParseHostKeyText(publicKey) == identity.fingerprint,
        "a one-line public key becomes the fingerprint of its SPKI");
    Check(!deskhubp::ParseHostKeyText("not a key"), "anything else is refused");
}

void TestSavedHostsRoundTrip() {
    std::printf("[hostprofiles] a saved host is written, updated and removed on disk...\n");
    Check(deskhubp::ClearTrustedHosts(), "the test starts with no saved hosts");

    HostProfileRequest add{"office", "127.0.0.1:47001", KeyFor(1)};
    Check(deskhubp::SaveHostProfile(HostProfileMode::Add, add) == HostProfileError::None,
        "a complete host is saved");
    const auto saved = deskhubp::LoadTrustStore().Find(KeyFor(1));
    Check(saved && saved->label == "office" && saved->endpoint == "127.0.0.1:47001",
        "the file holds the name, the address and the pinned key");
    Check(deskhubp::CheckTrustedHost(KeyFor(1)) == deskhub::TrustVerdict::Trusted,
        "and a connection from that key is now trusted");

    Check(deskhubp::SaveHostProfile(HostProfileMode::Add, add) == HostProfileError::AliasExists,
        "the same name cannot be added twice");
    Check(deskhubp::SaveHostProfile(HostProfileMode::Add,
              HostProfileRequest{"office2", "127.0.0.9:47001", KeyFor(1)}) ==
              HostProfileError::KeyExists,
        "the same key cannot be saved under a second name");

    Check(deskhubp::SaveHostProfile(HostProfileMode::Update,
              HostProfileRequest{"office", "127.0.0.1:47002", std::nullopt}) ==
              HostProfileError::None,
        "moving the host to a new address succeeds");
    const auto moved = deskhubp::LoadTrustStore().Find(KeyFor(1));
    Check(moved && moved->endpoint == "127.0.0.1:47002" &&
              deskhubp::CheckTrustedHost(KeyFor(1)) == deskhub::TrustVerdict::Trusted,
        "the address moved with the host and the key stays trusted");

    Check(deskhubp::RemoveHostProfile("office") == HostProfileError::None,
        "removing the host succeeds");
    Check(deskhubp::RemoveHostProfile("office") == HostProfileError::AliasMissing,
        "a second removal reports there is nothing to remove");
    Check(deskhubp::LoadTrustStore().Size() == 0, "and the file is empty again");
}

void TestThisMachineHasOnePublicKey() {
    std::printf("[hostprofiles] the Devices page hands out this machine's one public key...\n");
    char publicKey[1024]{};
    Check(dh_host_public_key(publicKey, sizeof(publicKey)) > 0,
        "the bridge prints a public key line");
    const auto identity = deskhubp::LoadOrCreateHostIdentity();
    Check(deskhubp::ParseHostKeyText(publicKey) == identity.fingerprint,
        "and it is the line another host's owner pastes to allow this machine");
    char fingerprint[64]{};
    Check(dh_host_fingerprint(fingerprint, sizeof(fingerprint)) > 0 &&
              std::string(fingerprint) == deskhub::FormatFingerprint(identity.fingerprint),
        "the fingerprint shown beside it is the same key");
}

void TestUnreadableStoreIsNotOverwritten() {
    std::printf("[hostprofiles] a damaged saved-hosts file is never rewritten...\n");
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, "damaged row\n"),
        "the saved-hosts file can be damaged for this test");
    Check(deskhubp::SaveHostProfile(HostProfileMode::Add,
              HostProfileRequest{"office", "127.0.0.1:47001", KeyFor(1)}) ==
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
              std::string(rows[0].fingerprint) == key,
        "the row shows the name, the canonical address and the pinned key");
    Check(dh_host_profile_remove("127-0-0-1") == DHHostProfileOk, "the host is removed");
    Check(*dh_host_profile_error_text(DHHostProfileAliasMissing) != '\0',
        "every refusal comes with a sentence to show");

    Check(dh_host_trust_new("127.0.0.1:47020", "not a fingerprint") == DHHostProfileInvalidHostKey,
        "trusting a new host needs a real fingerprint");
    Check(dh_host_trust_new("127.0.0.1:47020", key.c_str()) == DHHostProfileOk,
        "a confirmed first-time host is saved");
    Check(dh_host_trust_new("127.0.0.1:47021", key.c_str()) == DHHostProfileOk,
        "trusting the same machine at another address keeps one row");
    Check(dh_host_profiles(rows, 4) == 1 && std::string(rows[0].alias) == "127-0-0-1-47020" &&
              std::string(rows[0].endpoint) == "127.0.0.1:47021",
        "the saved host keeps its name and remembers the latest address");
    char prompt[768]{};
    Check(dh_trust_new_host_prompt("127.0.0.1:47020", key.c_str(), prompt, sizeof(prompt)) > 0 &&
              std::string(prompt).find(key) != std::string::npos,
        "the confirmation text carries the fingerprint to compare");
    const std::string otherKey = deskhub::FormatFingerprint(KeyFor(9));
    Check(dh_trust_new_host_prompt("127.0.0.1:47021", otherKey.c_str(), prompt, sizeof(prompt)) >
                  0 &&
              std::string(prompt).find("127-0-0-1-47020") != std::string::npos &&
              std::string(prompt).find(key) != std::string::npos,
        "a different key at a known address warns which machine used to answer there");
    Check(dh_trust_new_host_prompt("127.0.0.1:47099", otherKey.c_str(), prompt, sizeof(prompt)) >
                  0 &&
              std::string(prompt).find("127-0-0-1-47020") == std::string::npos,
        "and says nothing extra at an address nobody has answered from");
    Check(dh_host_profile_remove("127-0-0-1-47020") == DHHostProfileOk, "and it can be removed");
}

void TestOutsideInviteNamesAnUntrustedKey() {
    std::printf("[hostprofiles] an invite from outside the app names a key still to confirm...\n");
    Check(deskhubp::ClearTrustedHosts(), "the test starts with no saved hosts");
    deskhub::PairingInvite invite;
    invite.endpoints.push_back(*deskhub::MakePairingEndpoint("127.0.0.1", 47030));
    invite.hostKey = KeyFor(11);
    invite.token.fill(0x5A);
    invite.hostName = "office";
    const std::string text = deskhub::FormatPairingInvite(invite);
    const std::string key = deskhub::FormatFingerprint(KeyFor(11));
    char out[DH_PAIRING_INVITE_CAP]{};
    Check(dh_pairing_invite_new_host_key(text.c_str(), out, sizeof(out)) > 0 &&
              std::string(out) == key,
        "an unknown host key is handed back for the new-host confirmation");
    Check(dh_host_trust_new("127.0.0.1:47030", key.c_str()) == DHHostProfileOk,
        "the user confirms the host");
    Check(dh_pairing_invite_new_host_key(text.c_str(), out, sizeof(out)) == 0 &&
              std::string(out).empty(),
        "a host already trusted needs no confirmation");
    Check(dh_pairing_invite_new_host_key("deskhub://pair/garbage", out, sizeof(out)) == 0,
        "a damaged invite names no key");
    Check(dh_pairing_invite_new_host_key(nullptr, out, sizeof(out)) == 0,
        "and neither does a missing one");
    Check(deskhubp::ClearTrustedHosts(), "the saved host is cleared again");
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
    TestOutsideInviteNamesAnUntrustedKey();
    TestThisMachineHasOnePublicKey();
    TestUnreadableStoreIsNotOverwritten();
}
