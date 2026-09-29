#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "deskhub/net/PublicKeyText.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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
    const auto root = UniqueTempDir("deskhub-config-dir-");
    if (root.empty()) {
        Check(false, "the separate config directory test has a unique location");
        return;
    }
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

void TestSharedContainerGetsItsOwnPrivateFolder() {
    std::printf("[appdata] a shared container holds the data in a folder of its own...\n");
    const auto container = UniqueTempDir("deskhub-container-");
    if (container.empty()) {
        Check(false, "the shared container test has a unique location");
        return;
    }
    std::filesystem::create_directory(container);
    const std::string oldAppDir = deskhubp::AppDataDirRef();
    const std::string oldConfigDir = deskhubp::ConfigDirRef();
    deskhubp::SetConfigDir("");
    deskhubp::SetAppDataDirInside(container.string());
    const auto expected = container / deskhubp::kAppDataFolderName;
    Check(deskhubp::AppDataDirRef() == expected.string(),
        "the data folder sits one level inside the container");
    Check(deskhubp::WriteAppDataFile(kTestFile, "inside") &&
              deskhubp::AppDataFilePath(kTestFile).parent_path() == expected,
        "config files land in the private folder, not the container root");
    deskhubp::SetAppDataDirInside("");
    Check(deskhubp::AppDataDirRef().empty(), "no container falls back to the default folder");

    deskhubp::SetConfigDir(oldConfigDir);
    deskhubp::SetAppDataDir(oldAppDir);
    std::error_code error;
    std::filesystem::remove_all(container, error);
}

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

constexpr size_t kWritersPerStore = 2;
constexpr size_t kEntriesPerWriter = 16;
constexpr uint64_t kLockHeldUs = 300'000;

void AppendSshString(std::vector<uint8_t>& out, std::string_view value) {
    const auto length = uint32_t(value.size());
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(uint8_t(length >> shift));
    out.insert(out.end(), value.begin(), value.end());
}

std::string SyntheticKeyText(size_t writer, size_t entry) {
    deskhub::PublicKeyText key;
    key.algorithm = deskhub::PublicKeyAlgorithm::Ed25519;
    std::string raw(32, 'k');
    raw[0] = char('a' + writer);
    raw[1] = char('a' + entry);
    AppendSshString(key.blob, "ssh-ed25519");
    AppendSshString(key.blob, raw);
    return deskhub::FormatPublicKeyText(key);
}

deskhub::Fingerprint SyntheticFingerprint(size_t writer, size_t entry) {
    deskhub::Fingerprint fingerprint;
    fingerprint.bytes.fill(0x5a);
    fingerprint.bytes[0] = uint8_t(writer + 1);
    fingerprint.bytes[1] = uint8_t(entry + 1);
    return fingerprint;
}

std::string SyntheticEndpoint(size_t writer, size_t entry) {
    return "10.0." + std::to_string(writer) + "." + std::to_string(entry + 1) + ":47777";
}

template <typename Write>
size_t RunWritersTogether(Write write) {
    std::atomic<size_t> succeeded{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> writers;
    for (size_t writer = 0; writer < kWritersPerStore; ++writer)
        writers.emplace_back([&, writer] {
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (size_t entry = 0; entry < kEntriesPerWriter; ++entry)
                if (write(writer, entry)) succeeded.fetch_add(1, std::memory_order_relaxed);
        });
    go.store(true, std::memory_order_release);
    for (std::thread& writer : writers) writer.join();
    return succeeded.load();
}

void TestConcurrentEditsNeverLoseAnEntry() {
    std::printf("[appdata] two writers editing the key and host lists at once lose nothing...\n");
    const IsolatedAppData isolated("deskhub-concurrent-");
    if (isolated.dir.empty()) {
        Check(false, "the concurrent edit test has a unique directory");
        return;
    }
    const size_t expected = kWritersPerStore * kEntriesPerWriter;

    const size_t keysAdded = RunWritersTogether([](size_t writer, size_t entry) {
        return deskhubp::RememberAuthorizedKey(SyntheticKeyText(writer, entry));
    });
    Check(keysAdded == expected, "every concurrent authorized key write reports success");
    const auto keys = deskhubp::LoadAuthorizedKeys();
    Check(keys && keys->Keys().size() == expected,
        "and every key is in the file afterwards, none overwritten by the other writer");

    const size_t hostsAdded = RunWritersTogether([](size_t writer, size_t entry) {
        return deskhubp::RememberTrustedHost(SyntheticEndpoint(writer, entry), "host",
            SyntheticFingerprint(writer, entry), 1);
    });
    Check(hostsAdded == expected, "every concurrent host pin write reports success");
    const auto hosts = deskhubp::TryLoadTrustStore();
    Check(hosts && hosts->Size() == expected, "and every pin is in known_hosts afterwards");
    bool allPinned = hosts.has_value();
    for (size_t writer = 0; writer < kWritersPerStore && hosts; ++writer)
        for (size_t entry = 0; entry < kEntriesPerWriter; ++entry)
            allPinned = allPinned &&
                        hosts->Check(SyntheticEndpoint(writer, entry),
                            SyntheticFingerprint(writer, entry)) ==
                            deskhub::TrustVerdict::Trusted;
    Check(allPinned, "each endpoint keeps the key its own writer pinned");
}

void TestAnotherProcessHoldingTheFileLockIsWaitedFor() {
    std::printf("[appdata] an edit waits while another process holds the config lock...\n");
    const IsolatedAppData isolated("deskhub-config-lock-");
    if (isolated.dir.empty()) {
        Check(false, "the config lock test has a unique directory");
        return;
    }
    std::atomic<bool> finished{false};
    std::atomic<bool> saved{false};
    std::thread writer;
    {
        const deskhubp::ConfigFileLock otherProcess(deskhubp::kAuthorizedKeysFileName);
        Check(otherProcess.Valid(), "the lock another process would take is held");
        writer = std::thread([&] {
            saved.store(deskhubp::RememberAuthorizedKey(SyntheticKeyText(0, 0)),
                std::memory_order_release);
            finished.store(true, std::memory_order_release);
        });
        SleepUs(kLockHeldUs);
        Check(!finished.load(std::memory_order_acquire),
            "the edit does not proceed while the lock is held");
        Check(deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName).empty(),
            "and nothing is written underneath the lock holder");
    }
    writer.join();
    Check(saved.load(std::memory_order_acquire), "once the lock is released the edit completes");
    const auto keys = deskhubp::LoadAuthorizedKeys();
    Check(keys && keys->Keys().size() == 1, "and the key is saved exactly once");
}

void TestKnownHostsFailuresNeverGrantTrust() {
    std::printf("[appdata] a missing, damaged or unwritable known_hosts grants no trust...\n");
    const IsolatedAppData isolated("deskhub-known-hosts-");
    if (isolated.dir.empty()) {
        Check(false, "the known_hosts test has a unique directory");
        return;
    }
    const std::string endpoint = SyntheticEndpoint(0, 0);
    const deskhub::Fingerprint pinned = SyntheticFingerprint(0, 0);

    const auto missing = deskhubp::TryLoadTrustStore();
    Check(missing && missing->Size() == 0, "a missing known_hosts reads as an empty list");
    Check(deskhubp::CheckTrustedHost(endpoint, pinned) == deskhub::TrustVerdict::Unknown,
        "and trusts nobody");

    Check(deskhubp::RememberTrustedHost(endpoint, "host", pinned, 1), "a host is pinned");
    const std::string valid = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    Check(deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, valid + "damaged row\n"),
        "known_hosts is damaged behind the app's back");
    Check(!deskhubp::TryLoadTrustStore(), "a reload reports the damage instead of a partial list");
    Check(deskhubp::CheckTrustedHost(endpoint, pinned) == deskhub::TrustVerdict::Unknown,
        "and even the valid row no longer vouches for the host");

    const auto target = deskhubp::AppDataFilePath(deskhubp::kTrustStoreFileName);
    std::error_code error;
    std::filesystem::remove(target, error);
    std::filesystem::create_directory(target, error);
    Check(!error, "a directory blocks replacement of known_hosts");
    Check(!deskhubp::RememberTrustedHost(endpoint, "host", pinned, 1),
        "a failed known_hosts write reports failure");
    Check(!deskhubp::TryLoadTrustStore() &&
              deskhubp::CheckTrustedHost(endpoint, pinned) == deskhub::TrustVerdict::Unknown,
        "and an unreadable store trusts nobody");

    std::filesystem::remove(target, error);
    Check(deskhubp::RememberTrustedHost(endpoint, "host", pinned, 1) &&
              deskhubp::CheckTrustedHost(endpoint, pinned) == deskhub::TrustVerdict::Trusted,
        "once the path is writable again the pin is saved and read back");
}

}

void RunAppDataFileTests() {
    TestRoundTrip();
    TestMissingFileReadsAsEmpty();
    TestConfigDirectoryCanBeSeparateFromLogs();
    TestSharedContainerGetsItsOwnPrivateFolder();
    TestUnknownSettingsKeysAreIgnored();
    TestADamagedAuthorizedKeysFileDeniesThenStartsFresh();
    TestAuthorizedClientsAreListedAndForgotten();
    TestConcurrentEditsNeverLoseAnEntry();
    TestAnotherProcessHoldingTheFileLockIsWaitedFor();
    TestKnownHostsFailuresNeverGrantTrust();
}
