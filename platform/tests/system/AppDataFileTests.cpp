#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/Random.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <system_error>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {

constexpr const char* kTestFile = "platform-test-appdata.txt";

void TestRoundTrip() {
    std::printf("[appdata] a small config file writes and reads back intact...\n");
    Check(deskhubp::WriteAppDataFile(kTestFile, "alpha\nbeta\n"), "the write succeeds");
    Check(deskhubp::ReadAppDataFile(kTestFile) == "alpha\nbeta\n",
        "the read returns exactly what was written");

    Check(deskhubp::WriteAppDataFile(kTestFile, "v2"), "a second write succeeds");
    Check(deskhubp::ReadAppDataFile(kTestFile) == "v2", "a rewrite replaces, never appends");

    std::error_code ec;
    std::filesystem::remove(deskhubp::AppDataFilePath(kTestFile), ec);
}

void TestMissingFileReadsAsEmpty() {
    std::printf("[appdata] a missing file is an empty string, not an error...\n");
    Check(deskhubp::ReadAppDataFile("platform-test-never-written.txt").empty(),
        "first launch with no saved state is fine");
}

void TestConfigDirectoryCanBeSeparateFromLogs() {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "the separate config directory test has a unique location");
        return;
    }
    std::string name = "deskhub-config-dir-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directory(root);
    const std::string oldAppDir = deskhubp::AppDataDirRef();
    const std::string oldConfigDir = deskhubp::ConfigDirRef();
    deskhubp::SetAppDataDir((root / "logs").string());
    deskhubp::SetConfigDir((root / "config").string());
    Check(deskhubp::WriteAppDataFile(kTestFile, "separate"),
        "the service can write configuration outside the log directory");
    Check(deskhubp::AppDataFilePath(kTestFile).parent_path() == root / "config" &&
              deskhubp::LogDir() == (root / "logs").string(),
        "the config and log paths remain independent");

#ifndef _WIN32
    const auto config = root / "config";
    struct stat mode{};
    Check(::stat(config.c_str(), &mode) == 0 && (mode.st_mode & 0777) == 0700,
        "the service config directory belongs to its user alone");
    const auto stored = config / kTestFile;
    Check(::stat(stored.c_str(), &mode) == 0 && (mode.st_mode & 0777) == 0600,
        "a general app data write creates an owner-only file");
    Check(::chmod(config.c_str(), 0777) == 0 && deskhubp::ConfigDir() == config.string() &&
              ::stat(config.c_str(), &mode) == 0 && (mode.st_mode & 0777) == 0700,
        "an existing permissive config directory is restricted before use");
    const auto link = root / "config-link";
    std::error_code linkError;
    std::filesystem::create_directory_symlink(config, link, linkError);
    Check(!linkError, "the test creates a symlink to the config directory");
    if (!linkError) {
        deskhubp::SetConfigDir(link.string());
        Check(deskhubp::ConfigDir().empty() &&
                  !deskhubp::WriteAppDataFileAtomic(kTestFile, "via-link"),
            "a symlink cannot stand in for the private config directory");
    }
#endif

    deskhubp::SetConfigDir(oldConfigDir);
    deskhubp::SetAppDataDir(oldAppDir);
    std::error_code error;
    std::filesystem::remove_all(root, error);
}

std::filesystem::path UniqueTempDir(const std::string& prefix) {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) return {};
    std::string name = prefix;
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    return std::filesystem::temp_directory_path() / name;
}

struct IsolatedAppData {
    std::filesystem::path dir{};
    std::string previous = deskhubp::AppDataDirRef();

    explicit IsolatedAppData(const std::string& prefix) : dir(UniqueTempDir(prefix)) {
        if (!dir.empty()) deskhubp::SetAppDataDir(dir.string());
    }

    ~IsolatedAppData() {
        deskhubp::SetAppDataDir(previous);
        std::error_code error;
        if (!dir.empty()) std::filesystem::remove_all(dir, error);
    }

    IsolatedAppData(const IsolatedAppData&) = delete;
    IsolatedAppData& operator=(const IsolatedAppData&) = delete;
};

void TestUnknownSettingsKeysAreIgnored() {
    std::printf("[appdata] settings keys this version does not know are ignored...\n");
    const IsolatedAppData isolated("deskhub-settings-");
    if (isolated.dir.empty()) {
        Check(false, "a settings test directory has a unique name");
        return;
    }

    const std::string old = "name=workstation\nport=4200\npasscode=0417\nallow_new_pairings=1\n";
    Check(deskhubp::WriteAppDataFile(deskhubp::kUiSettingsFileName, old),
        "an old settings file is stored in an isolated directory");
    const auto loaded = deskhubp::LoadUiSettings();
    Check(loaded.deviceName == "workstation" && loaded.port == 4200,
        "the keys this version knows are still read");
    Check(deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName) == old,
        "reading settings never rewrites the file");
    deskhubp::SaveUiSettings(loaded);
    const std::string saved = deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName);
    Check(saved == deskhub::ui::SerializeUiSettings(loaded) &&
              saved.find("passcode") == std::string::npos,
        "the next save writes only the keys this version knows");
    Check(!deskhubp::WriteAppDataFileAtomic("missing/settings.txt", "new"),
        "an atomic write failure reports failure without changing the settings file");
    Check(deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName) == saved,
        "a failed atomic write leaves the previous settings intact");
}

bool IsAuthorized(const deskhubp::ClientIdentity& identity) {
    return deskhubp::IsClientKeyAuthorized(identity.publicKey);
}

bool DamageAuthorizedKeys(const std::string& valid) {
    return deskhubp::WriteAppDataFile(deskhubp::kAuthorizedKeysFileName,
        valid + "damaged row\n");
}

void TestADamagedAuthorizedKeysFileDeniesThenStartsFresh() {
    std::printf("[appdata] a damaged authorized_keys denies everyone until the next change...\n");
    if (!deskhubp::QuicAvailable()) return;
    const IsolatedAppData isolated("deskhub-authorized-");
    if (isolated.dir.empty()) {
        Check(false, "an authorized key test directory has a unique name");
        return;
    }

    const auto laptop = deskhubp::GenerateClientIdentity("laptop");
    const auto phone = deskhubp::GenerateClientIdentity("phone");
    Check(laptop.Valid() && phone.Valid(), "two client keys are created in the isolated store");
    if (!laptop.Valid() || !phone.Valid()) return;

    Check(GrantClientKey(laptop), "an authorized key is saved in the isolated directory");
    const std::string valid = deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);
    Check(DamageAuthorizedKeys(valid), "the admission list can be corrupted for a regression test");
    Check(deskhubp::CheckClientKeyAuthorization(laptop.publicKey) ==
                  deskhubp::ClientKeyAuthorization::ConfigError &&
              !IsAuthorized(laptop),
        "a damaged admission list authorizes no key, even one on a valid row");
    Check(!deskhubp::ListAuthorizedClients(),
        "a damaged admission list reads as an error, not as an empty list");

    const uint64_t beforeAdd = deskhubp::AuthorizedKeysGeneration();
    Check(GrantClientKey(phone), "adding a key to a damaged list succeeds");
    Check(IsAuthorized(phone) && !IsAuthorized(laptop),
        "the damaged contents are discarded and the fresh file holds only the new key");
    Check(deskhubp::AuthorizedKeysGeneration() == beforeAdd + 1,
        "starting fresh publishes one policy change");

    Check(DamageAuthorizedKeys(valid), "the list is damaged again");
    Check(RevokeAllClientKeys(), "revoke-all replaces a damaged list");
    const auto afterClear = deskhubp::ListAuthorizedClients();
    Check(afterClear && afterClear->empty(), "with an empty, readable one");

    Check(DamageAuthorizedKeys(valid), "the list is damaged once more");
    Check(!deskhubp::ForgetAuthorizedClient(laptop.fingerprint),
        "forgetting a key the fresh file does not hold reports no change");
    const auto afterForget = deskhubp::ListAuthorizedClients();
    Check(afterForget && afterForget->empty(),
        "but the damaged file is still replaced by an empty one");

    const uint64_t beforeFailure = deskhubp::AuthorizedKeysGeneration();
    const auto target = deskhubp::AppDataFilePath(deskhubp::kAuthorizedKeysFileName);
    std::error_code error;
    std::filesystem::remove(target, error);
    std::filesystem::create_directory(target, error);
    Check(!error, "a directory blocks replacement of the authorized key file");
    Check(!GrantClientKey(laptop), "a failed authorized key write reports failure");
    Check(!RevokeAllClientKeys(), "failed revoke-all reports failure");
    Check(deskhubp::AuthorizedKeysGeneration() == beforeFailure,
        "failed writes do not announce a policy change");

    std::filesystem::remove(target, error);
    Check(GrantClientKey(laptop), "the authorized key file can be written again");
    const uint64_t beforeRevoke = deskhubp::AuthorizedKeysGeneration();
    Check(RevokeAllClientKeys(), "revoke-all persists an empty allowlist");
    Check(!IsAuthorized(laptop) && deskhubp::AuthorizedKeysGeneration() == beforeRevoke + 1,
        "successful revoke-all publishes one policy change");
}

void TestAuthorizedClientsAreListedAndForgotten() {
    std::printf("[appdata] authorized_keys is the only list of clients let in...\n");
    if (!deskhubp::QuicAvailable()) return;
    const IsolatedAppData isolated("deskhub-authorized-list-");
    if (isolated.dir.empty()) {
        Check(false, "the authorized client list test has a unique directory");
        return;
    }

    const auto laptop = deskhubp::GenerateClientIdentity("laptop");
    const auto phone = deskhubp::GenerateClientIdentity("phone");
    Check(laptop.Valid() && phone.Valid(), "two client keys are created in the isolated store");
    if (!laptop.Valid() || !phone.Valid()) return;

    const std::string retiredList = "paired_devices";
    const std::string retiredMarker = "authorized_keys_active";
    Check(deskhubp::WriteAppDataFile(retiredList,
              deskhub::FormatFingerprint(laptop.fingerprint) + " 1 1 laptop\n") &&
              deskhubp::WriteAppDataFile(retiredMarker, "v1\n"),
        "files from an older version are present");
    Check(!IsAuthorized(laptop), "a fingerprint in the retired list admits nobody");
    Check(!std::filesystem::exists(deskhubp::AppDataFilePath(retiredList)) &&
              !std::filesystem::exists(deskhubp::AppDataFilePath(retiredMarker)),
        "and the retired files are deleted on first load");

    const auto empty = deskhubp::ListAuthorizedClients();
    Check(empty && empty->empty(), "a missing authorized_keys is an empty list");

    Check(deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(laptop) + " work laptop"),
        "a labelled public key is saved");
    Check(GrantClientKey(phone), "a second key is saved");
    Check(!deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(phone)),
        "a duplicate public key is not stored twice");
    const auto clients = deskhubp::ListAuthorizedClients();
    Check(clients && clients->size() == 2, "both clients are listed");
    if (clients && clients->size() == 2) {
        Check((*clients)[0].fingerprint == laptop.fingerprint &&
                  (*clients)[0].label == "work laptop",
            "a listed client carries its label and fingerprint");
        Check((*clients)[1].fingerprint == phone.fingerprint, "in the order they were added");
    }

    Check(deskhubp::ForgetAuthorizedClient(laptop.fingerprint), "a client is forgotten");
    Check(!IsAuthorized(laptop) && IsAuthorized(phone),
        "only the forgotten client loses access");
    Check(!deskhubp::ForgetAuthorizedClient(laptop.fingerprint),
        "forgetting it twice changes nothing");

    Check(RevokeAllClientKeys() && !IsAuthorized(phone),
        "an intentionally empty authorized_keys file denies every client");
}

}

void RunAppDataFileTests() {
    TestRoundTrip();
    TestMissingFileReadsAsEmpty();
    TestConfigDirectoryCanBeSeparateFromLogs();
    TestUnknownSettingsKeysAreIgnored();
    TestADamagedAuthorizedKeysFileDeniesThenStartsFresh();
    TestAuthorizedClientsAreListedAndForgotten();
}
