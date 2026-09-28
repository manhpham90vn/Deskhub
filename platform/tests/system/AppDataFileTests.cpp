#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Random.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <system_error>

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

}

void RunAppDataFileTests() {
    TestRoundTrip();
    TestMissingFileReadsAsEmpty();
    TestLegacySettingsMigration();
}
