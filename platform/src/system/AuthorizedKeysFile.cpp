#include "deskhubp/system/AuthorizedKeysFile.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

constexpr const char* kRetiredFileNames[] = {"paired_devices", "authorized_keys_active"};

struct WritableKeys {
    deskhub::AuthorizedKeys keys{};
    bool discarded = false;
};

std::mutex& StoreMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<uint64_t>& Generation() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

std::atomic<bool>& InvalidReported() {
    static std::atomic<bool> reported{false};
    return reported;
}

std::optional<deskhub::AuthorizedKeys> InvalidConfig() {
    if (!InvalidReported().exchange(true, std::memory_order_acq_rel))
        LOGE("authorized_keys: configuration cannot be read or parsed; denying admission");
    return std::nullopt;
}

void RemoveRetiredFilesOncePerDir(const std::filesystem::path& keysPath) {
    static std::filesystem::path cleanedDir;
    const std::filesystem::path dir = keysPath.parent_path();
    if (dir == cleanedDir) return;
    cleanedDir = dir;
    for (const char* name : kRetiredFileNames) {
        std::error_code error;
        std::filesystem::remove(dir / name, error);
    }
}

std::optional<deskhub::AuthorizedKeys> LoadLocked() {
    const auto path = AppDataFilePath(kAuthorizedKeysFileName);
    if (path.empty()) return InvalidConfig();
    RemoveRetiredFilesOncePerDir(path);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::error_code error;
        if (std::filesystem::exists(path, error) || error) return InvalidConfig();
        InvalidReported().store(false, std::memory_order_release);
        return deskhub::AuthorizedKeys{};
    }
    const auto size = file.tellg();
    if (size < 0 || size > std::streamoff(deskhub::kMaxAuthorizedKeysFileBytes) ||
        !file.seekg(0))
        return InvalidConfig();
    std::string text(size_t(size), '\0');
    if (!text.empty() && !file.read(text.data(), size)) return InvalidConfig();
    auto keys = deskhub::ParseAuthorizedKeys(text);
    if (!keys) return InvalidConfig();
    InvalidReported().store(false, std::memory_order_release);
    return keys;
}

WritableKeys LoadForWriteLocked() {
    auto keys = LoadLocked();
    if (keys) return WritableKeys{std::move(*keys), false};
    LOGW("authorized_keys: discarding the unreadable file and writing a fresh one");
    return WritableKeys{deskhub::AuthorizedKeys{}, true};
}

bool SaveLocked(const deskhub::AuthorizedKeys& keys) {
    if (!WriteAppDataFileAtomic(kAuthorizedKeysFileName, deskhub::SerializeAuthorizedKeys(keys)))
        return false;
    Generation().fetch_add(1, std::memory_order_acq_rel);
    return true;
}

std::optional<deskhub::Fingerprint> FingerprintOfKey(const deskhub::PublicKeyText& key) {
    return FingerprintOfPublicKey(PublicKeySpkiFromText(deskhub::FormatPublicKeyText(key)));
}

template <typename Change>
bool ChangeKeys(Change change) {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kAuthorizedKeysFileName);
    if (!fileLock.Valid()) return false;
    WritableKeys writable = LoadForWriteLocked();
    if (change(writable.keys)) return SaveLocked(writable.keys);
    if (writable.discarded) SaveLocked(deskhub::AuthorizedKeys{});
    return false;
}

}

std::optional<deskhub::AuthorizedKeys> LoadAuthorizedKeys() {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    return LoadLocked();
}

bool RememberAuthorizedKey(std::string_view publicKeyText) {
    const auto parsed = deskhub::ParsePublicKeyText(publicKeyText);
    if (!parsed) return false;
    return ChangeKeys([&](deskhub::AuthorizedKeys& keys) { return keys.Add(*parsed); });
}

bool ClearAuthorizedKeys() {
    return ChangeKeys([](deskhub::AuthorizedKeys& keys) {
        keys = deskhub::AuthorizedKeys{};
        return true;
    });
}

bool IsClientKeyAuthorized(std::span<const uint8_t> publicKeySpki) {
    return CheckClientKeyAuthorization(publicKeySpki) == ClientKeyAuthorization::Authorized;
}

ClientKeyAuthorization CheckClientKeyAuthorization(std::span<const uint8_t> publicKeySpki) {
    const auto keys = LoadAuthorizedKeys();
    if (!keys) return ClientKeyAuthorization::ConfigError;
    const auto parsed = deskhub::ParsePublicKeyText(PublicKeyTextFromSpki(publicKeySpki));
    return parsed && keys->Contains(*parsed) ? ClientKeyAuthorization::Authorized
                                             : ClientKeyAuthorization::Denied;
}

std::optional<std::vector<AuthorizedClient>> ListAuthorizedClients() {
    const auto keys = LoadAuthorizedKeys();
    if (!keys) return std::nullopt;
    std::vector<AuthorizedClient> clients;
    for (const auto& key : keys->Keys()) {
        const auto fingerprint = FingerprintOfKey(key);
        if (!fingerprint) return std::nullopt;
        clients.push_back(AuthorizedClient{key.label, *fingerprint});
    }
    return clients;
}

bool ForgetAuthorizedClient(const deskhub::Fingerprint& fingerprint) {
    return ChangeKeys([&](deskhub::AuthorizedKeys& keys) {
        const auto& stored = keys.Keys();
        const auto match = std::find_if(stored.begin(), stored.end(),
            [&](const deskhub::PublicKeyText& key) { return FingerprintOfKey(key) == fingerprint; });
        if (match == stored.end()) return false;
        const auto target = deskhub::ParsePublicKeyText(deskhub::FormatPublicKeyText(*match));
        return target && keys.Remove(*target);
    });
}

uint64_t AuthorizedKeysGeneration() {
    return Generation().load(std::memory_order_acquire);
}

}
