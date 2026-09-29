#include "deskhubp/system/TrustStoreFile.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/diag/Log.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

namespace deskhubp {

namespace {

std::mutex& TrustStoreMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<bool>& InvalidReported() {
    static std::atomic<bool> reported{false};
    return reported;
}

std::optional<deskhub::TrustStore> InvalidConfig() {
    if (!InvalidReported().exchange(true, std::memory_order_acq_rel))
        LOGE("known_hosts: configuration cannot be read or parsed; rejecting host trust");
    return std::nullopt;
}

std::optional<deskhub::TrustStore> LoadTrustStoreLocked() {
    const auto path = AppDataFilePath(kTrustStoreFileName);
    if (path.empty()) return InvalidConfig();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error) && !error) {
            InvalidReported().store(false, std::memory_order_release);
            return deskhub::TrustStore{};
        }
        return InvalidConfig();
    }
    const auto size = file.tellg();
    if (size < 0 || size > 131072 || !file.seekg(0)) return InvalidConfig();
    std::string text(size_t(size), '\0');
    if (!text.empty() && !file.read(text.data(), size)) return InvalidConfig();
    auto store = deskhub::ParseTrustStoreStrict(text);
    if (!store) return InvalidConfig();
    InvalidReported().store(false, std::memory_order_release);
    return store;
}

bool SaveTrustStoreLocked(const deskhub::TrustStore& store) {
    return WriteAppDataFileAtomic(kTrustStoreFileName, deskhub::SerializeTrustStore(store));
}

}

deskhub::TrustStore LoadTrustStore() {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    return LoadTrustStoreLocked().value_or(deskhub::TrustStore{});
}

std::optional<deskhub::TrustStore> TryLoadTrustStore() {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    return LoadTrustStoreLocked();
}

bool ClearTrustedHosts() {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    const ConfigFileLock fileLock(kTrustStoreFileName);
    if (!fileLock.Valid()) return false;
    auto store = LoadTrustStoreLocked();
    if (!store) return false;
    store->Clear();
    return SaveTrustStoreLocked(*store);
}

deskhub::TrustVerdict CheckTrustedHost(std::string_view endpoint,
    const deskhub::Fingerprint& fingerprint) {
    return LoadTrustStore().Check(endpoint, fingerprint);
}

bool RememberTrustedHost(std::string_view endpoint, std::string_view label,
    const deskhub::Fingerprint& fingerprint, int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    const ConfigFileLock fileLock(kTrustStoreFileName);
    if (!fileLock.Valid()) return false;
    auto store = LoadTrustStoreLocked();
    if (!store) return false;
    store->Remember(endpoint, label, fingerprint, nowUnix);
    return SaveTrustStoreLocked(*store);
}

bool RememberTrustedHostProfile(std::string_view endpoint, std::string_view label,
    const deskhub::Fingerprint& fingerprint, std::string_view identityName,
    int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    const ConfigFileLock fileLock(kTrustStoreFileName);
    if (!fileLock.Valid()) return false;
    auto store = LoadTrustStoreLocked();
    if (!store) return false;
    store->Remember(endpoint, label, fingerprint, nowUnix);
    if (!store->SetProfile(endpoint, label, identityName)) return false;
    return SaveTrustStoreLocked(*store);
}

bool ForgetTrustedHost(std::string_view endpoint) {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    const ConfigFileLock fileLock(kTrustStoreFileName);
    if (!fileLock.Valid()) return false;
    auto store = LoadTrustStoreLocked();
    if (!store || !store->Forget(endpoint)) return false;
    return SaveTrustStoreLocked(*store);
}

}
