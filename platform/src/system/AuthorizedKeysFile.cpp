#include "deskhubp/system/AuthorizedKeysFile.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

constexpr const char* kAuthorizedKeysActiveFileName = "authorized_keys_active";

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

std::optional<AuthorizedKeysSnapshot> InvalidConfig() {
    if (!InvalidReported().exchange(true, std::memory_order_acq_rel))
        LOGE("authorized_keys: configuration cannot be read or parsed; denying admission");
    return std::nullopt;
}

std::optional<AuthorizedKeysSnapshot> LoadLocked() {
    const auto path = AppDataFilePath(kAuthorizedKeysFileName);
    if (path.empty()) return InvalidConfig();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error) && !error) {
            const auto marker = AppDataFilePath(kAuthorizedKeysActiveFileName);
            if (marker.empty()) return InvalidConfig();
            if (!std::filesystem::exists(marker, error) && !error) {
                InvalidReported().store(false, std::memory_order_release);
                return AuthorizedKeysSnapshot{};
            }
        }
        return InvalidConfig();
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
    return AuthorizedKeysSnapshot{true, std::move(*keys)};
}

bool SaveLocked(const deskhub::AuthorizedKeys& keys) {
    const auto marker = AppDataFilePath(kAuthorizedKeysActiveFileName);
    if (marker.empty()) return false;
    std::error_code error;
    if (!std::filesystem::exists(marker, error)) {
        if (error || !WriteAppDataFileAtomic(kAuthorizedKeysActiveFileName, "v1\n"))
            return false;
    }
    if (!WriteAppDataFileAtomic(kAuthorizedKeysFileName, deskhub::SerializeAuthorizedKeys(keys)))
        return false;
    Generation().fetch_add(1, std::memory_order_acq_rel);
    return true;
}

}

std::optional<AuthorizedKeysSnapshot> LoadAuthorizedKeys() {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    return LoadLocked();
}

bool RememberAuthorizedKey(std::string_view publicKeyText) {
    const auto parsed = deskhub::ParsePublicKeyText(publicKeyText);
    if (!parsed) return false;
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kAuthorizedKeysFileName);
    if (!fileLock.Valid()) return false;
    auto snapshot = LoadLocked();
    if (!snapshot || !snapshot->keys.Add(*parsed)) return false;
    return SaveLocked(snapshot->keys);
}

bool ForgetAuthorizedKey(std::span<const uint8_t> publicKeySpki) {
    const auto parsed = deskhub::ParsePublicKeyText(PublicKeyTextFromSpki(publicKeySpki));
    if (!parsed) return false;
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kAuthorizedKeysFileName);
    if (!fileLock.Valid()) return false;
    auto snapshot = LoadLocked();
    if (!snapshot || !snapshot->configured || !snapshot->keys.Remove(*parsed)) return false;
    return SaveLocked(snapshot->keys);
}

bool ClearAuthorizedKeys() {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kAuthorizedKeysFileName);
    if (!fileLock.Valid()) return false;
    return SaveLocked(deskhub::AuthorizedKeys{});
}

bool IsClientKeyAuthorized(std::span<const uint8_t> publicKeySpki) {
    return CheckClientKeyAuthorization(publicKeySpki) == ClientKeyAuthorization::Authorized;
}

ClientKeyAuthorization CheckClientKeyAuthorization(std::span<const uint8_t> publicKeySpki) {
    const auto snapshot = LoadAuthorizedKeys();
    if (!snapshot) return ClientKeyAuthorization::ConfigError;
    if (!snapshot->configured) {
        const auto fingerprint = FingerprintOfPublicKey(publicKeySpki);
        return fingerprint && CheckPairedDevice(*fingerprint) == deskhub::PairVerdict::Paired
                   ? ClientKeyAuthorization::Authorized
                   : ClientKeyAuthorization::Denied;
    }
    const auto parsed = deskhub::ParsePublicKeyText(PublicKeyTextFromSpki(publicKeySpki));
    return parsed && snapshot->keys.Contains(*parsed) ? ClientKeyAuthorization::Authorized
                                                      : ClientKeyAuthorization::Denied;
}

std::optional<deskhub::PairedDevices> LoadEffectiveAuthorizedDevices() {
    const auto snapshot = LoadAuthorizedKeys();
    if (!snapshot) return std::nullopt;
    if (!snapshot->configured) return LoadPairedDevices();
    deskhub::PairedDevices devices;
    for (const auto& key : snapshot->keys.Keys()) {
        const auto spki = PublicKeySpkiFromText(deskhub::FormatPublicKeyText(key));
        const auto fingerprint = FingerprintOfPublicKey(spki);
        if (!fingerprint) return std::nullopt;
        devices.Insert(deskhub::PairedDevice{*fingerprint, key.label, 0, 0});
    }
    return devices;
}

bool ForgetEffectiveAuthorizedDevice(const deskhub::Fingerprint& fingerprint) {
    const auto snapshot = LoadAuthorizedKeys();
    if (!snapshot) return false;
    if (!snapshot->configured) return ForgetPairedDevice(fingerprint);
    for (const auto& key : snapshot->keys.Keys()) {
        const auto spki = PublicKeySpkiFromText(deskhub::FormatPublicKeyText(key));
        if (FingerprintOfPublicKey(spki) == fingerprint) return ForgetAuthorizedKey(spki);
    }
    return false;
}

uint64_t AuthorizedKeysGeneration() {
    return Generation().load(std::memory_order_acquire);
}

}
