#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/PairedDevicesFile.h"
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

void TestLegacySettingsMigration() {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "a migration test directory has a unique name");
        return;
    }
    std::string name = "deskhub-migration-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    const std::string previous = deskhubp::AppDataDirRef();
    deskhubp::SetAppDataDir(dir.string());

    const std::string legacy = "name=workstation\nport=4200\npasscode=0417\nallow_new_pairings=1\n";
    Check(deskhubp::WriteAppDataFile(deskhubp::kUiSettingsFileName, legacy),
        "legacy settings are stored in an isolated directory");
    const auto loaded = deskhubp::LoadUiSettings();
    Check(loaded.deviceName == "workstation" && loaded.port == 4200,
        "settings migration keeps supported values");
    const std::string migrated = deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName);
    Check(migrated.find("passcode") == std::string::npos &&
              migrated.find("allow_new_pairings") == std::string::npos,
        "loading legacy settings removes discarded secrets from disk");
    Check(migrated == deskhub::ui::SerializeUiSettings(loaded),
        "loading legacy settings writes the sanitized form");
    Check(!deskhubp::WriteAppDataFileAtomic("missing/settings.txt", "new"),
        "an atomic write failure reports failure without changing the settings file");
    Check(deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName) == migrated,
        "a failed atomic write leaves the previous settings intact");

    deskhubp::SetAppDataDir(previous);
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

void TestAuthorizedKeyWritesOnlyPublishSuccessfulChanges() {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "an authorized key test directory has a unique name");
        return;
    }
    std::string name = "deskhub-authorized-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    const std::string previous = deskhubp::AppDataDirRef();
    deskhubp::SetAppDataDir(dir.string());

    deskhub::Fingerprint key;
    key.bytes[0] = 1;
    Check(deskhubp::RememberPairedDevice(key, "laptop", 1000),
        "an authorized key is saved in the isolated directory");
    const std::string valid = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    const std::string damaged = valid + "damaged row\n";
    Check(deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, damaged),
        "the isolated admission list can be corrupted for a regression test");
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Check(key) == deskhub::PairVerdict::Unknown,
        "a damaged admission list authorizes no key, even one on a valid row");
    Check(!deskhubp::RememberPairedDevice(key, "changed", 2000),
        "a normal update cannot overwrite a damaged admission list");
    const uint64_t generationBeforeDamagedRevoke = deskhubp::PairedDevicesGeneration();
    Check(!deskhubp::ForgetAllPairedDevices(),
        "revoke-all cannot overwrite a damaged admission list");
    Check(deskhubp::PairedDevicesGeneration() == generationBeforeDamagedRevoke,
        "a failed revoke-all does not publish a policy change");
    Check(deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName) == damaged,
        "the damaged list remains available for repair");
    Check(deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, valid),
        "the valid admission list is restored");
    const uint64_t beforeFailure = deskhubp::PairedDevicesGeneration();
    const auto target = deskhubp::AppDataFilePath(deskhubp::kPairedDevicesFileName);
    std::error_code error;
    std::filesystem::remove(target, error);
    std::filesystem::create_directory(target, error);
    Check(!error, "a directory blocks replacement of the authorized key file");
    Check(!deskhubp::RememberPairedDevice(key, "changed", 2000),
        "a failed authorized key write reports failure");
    Check(!deskhubp::ForgetAllPairedDevices(), "failed revoke-all reports failure");
    Check(deskhubp::PairedDevicesGeneration() == beforeFailure,
        "failed writes do not announce a policy change");

    std::filesystem::remove(target, error);
    Check(deskhubp::RememberPairedDevice(key, "laptop", 3000),
        "the authorized key file can be written again");
    const uint64_t beforeRevoke = deskhubp::PairedDevicesGeneration();
    Check(deskhubp::ForgetAllPairedDevices(), "revoke-all persists an empty allowlist");
    Check(deskhubp::TryLoadPairedDevices().value_or(deskhub::PairedDevices{}).Size() == 0 &&
              deskhubp::PairedDevicesGeneration() == beforeRevoke + 1,
        "successful revoke-all publishes one policy change");

    deskhubp::SetAppDataDir(previous);
    std::filesystem::remove_all(dir, error);
}

void TestPublicKeyAllowlistOverridesLegacyFingerprints() {
    if (!deskhubp::QuicAvailable()) return;
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "the public key allowlist test has a unique directory");
        return;
    }
    std::string name = "deskhub-authorized-text-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const auto dir = std::filesystem::temp_directory_path() / name;
    const std::string previous = deskhubp::AppDataDirRef();
    deskhubp::SetAppDataDir(dir.string());

    const auto first = deskhubp::LoadOrCreateClientIdentity();
    const auto second = deskhubp::GenerateClientIdentity("second");
    Check(first.Valid() && second.Valid(), "two client keys are created in the isolated store");
    if (first.Valid() && second.Valid()) {
        Check(deskhubp::RememberPairedDevice(first.fingerprint, "legacy", 1),
            "a fingerprint-only legacy entry exists");
        Check(deskhubp::IsClientKeyAuthorized(first.publicKey),
            "a legacy key remains authorized until the new store is configured");
        const std::string validLegacy =
            deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
        Check(deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName,
                  validLegacy + "invalid row\n"),
            "the legacy allowlist can be damaged for a reload check");
        Check(deskhubp::CheckClientKeyAuthorization(first.publicKey) ==
                      deskhubp::ClientKeyAuthorization::ConfigError &&
                  !deskhubp::LoadEffectiveAuthorizedDevices(),
            "a damaged legacy allowlist reports a configuration error instead of an empty list");
        Check(deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, validLegacy),
            "the valid legacy allowlist is restored");
        Check(deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(second)),
            "a full public key is saved in authorized_keys");
        Check(!deskhubp::IsClientKeyAuthorized(first.publicKey) &&
                  deskhubp::IsClientKeyAuthorized(second.publicKey),
            "the new public key list becomes authoritative when configured");
        Check(!deskhubp::RememberAuthorizedKey(deskhubp::ClientPublicKeyText(second)),
            "a duplicate public key is not stored twice");
        const uint64_t generationBeforeDamage = deskhubp::AuthorizedKeysGeneration();
        const std::string validKeys =
            deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);
        deskhubp::RemoveAppDataFile(deskhubp::kAuthorizedKeysFileName);
        Check(!deskhubp::IsClientKeyAuthorized(first.publicKey),
            "deleting the new list cannot reactivate a legacy fingerprint entry");
        Check(deskhubp::CheckClientKeyAuthorization(first.publicKey) ==
                  deskhubp::ClientKeyAuthorization::ConfigError,
            "an unreadable active allowlist has a distinct configuration error");
        Check(!deskhubp::ClearAuthorizedKeys(),
            "revoke-all cannot mask a missing active allowlist");
        Check(deskhubp::WriteAppDataFile(deskhubp::kAuthorizedKeysFileName,
                  validKeys + "invalid key\n"),
            "the active allowlist can be damaged for a regression check");
        Check(!deskhubp::ClearAuthorizedKeys(),
            "revoke-all cannot overwrite a damaged allowlist");
        Check(deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName) ==
                  validKeys + "invalid key\n",
            "a failed revoke-all preserves the allowlist for repair");
        Check(deskhubp::AuthorizedKeysGeneration() == generationBeforeDamage,
            "failed revoke-all does not publish a policy change");
        Check(deskhubp::WriteAppDataFile(deskhubp::kAuthorizedKeysFileName, validKeys),
            "the valid allowlist is restored");
        Check(deskhubp::ClearAuthorizedKeys() &&
                  !deskhubp::IsClientKeyAuthorized(second.publicKey),
            "an intentionally empty authorized_keys file denies every client");
    }

    deskhubp::SetAppDataDir(previous);
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

}

void RunAppDataFileTests() {
    TestRoundTrip();
    TestMissingFileReadsAsEmpty();
    TestConfigDirectoryCanBeSeparateFromLogs();
    TestLegacySettingsMigration();
    TestAuthorizedKeyWritesOnlyPublishSuccessfulChanges();
    TestPublicKeyAllowlistOverridesLegacyFingerprints();
}
