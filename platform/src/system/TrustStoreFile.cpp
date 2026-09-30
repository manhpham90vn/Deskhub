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

struct WritableTrustStore {
    deskhub::TrustStore store{};
    bool discarded = false;
};

WritableTrustStore LoadForWriteLocked() {
    auto store = LoadTrustStoreLocked();
    if (store) return WritableTrustStore{std::move(*store), false};
    LOGW("known_hosts: discarding the unreadable file and writing a fresh one");
    return WritableTrustStore{deskhub::TrustStore{}, true};
}

template <typename Change>
bool ChangeTrustStore(Change change) {
    const std::lock_guard<std::mutex> lock(TrustStoreMutex());
    const ConfigFileLock fileLock(kTrustStoreFileName);
    if (!fileLock.Valid()) return false;
    WritableTrustStore writable = LoadForWriteLocked();
    if (change(writable.store)) return SaveTrustStoreLocked(writable.store);
    if (writable.discarded) SaveTrustStoreLocked(deskhub::TrustStore{});
    return false;
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
    return ChangeTrustStore([](deskhub::TrustStore& store) {
        store.Clear();
        return true;
    });
}

deskhub::TrustVerdict CheckTrustedHost(const deskhub::Fingerprint& fingerprint) {
    return LoadTrustStore().Check(fingerprint);
}

bool RememberTrustedHost(const deskhub::Fingerprint& fingerprint, std::string_view label,
    std::string_view endpoint, int64_t nowUnix) {
    if (deskhub::IsZero(fingerprint)) return false;
    return ChangeTrustStore([&](deskhub::TrustStore& store) {
        store.Remember(fingerprint, label, endpoint, nowUnix);
        return true;
    });
}

bool TouchTrustedHost(const deskhub::Fingerprint& fingerprint, std::string_view endpoint,
    int64_t nowUnix) {
    return ChangeTrustStore([&](deskhub::TrustStore& store) {
        return store.Touch(fingerprint, endpoint, nowUnix);
    });
}

bool CreateTrustedHostProfile(const deskhub::TrustedHost& profile) {
    return ChangeTrustStore([&](deskhub::TrustStore& store) {
        if (store.Find(profile.fingerprint) || store.Size() >= deskhub::kMaxTrustedHosts)
            return false;
        for (const auto& host : store.Hosts())
            if (host.label == profile.label) return false;
        store.Insert(profile);
        const auto saved = store.Find(profile.fingerprint);
        return saved && saved->label == profile.label && saved->endpoint == profile.endpoint;
    });
}

bool UpdateTrustedHostProfile(const deskhub::TrustedHost& expected,
    const deskhub::TrustedHost& profile) {
    return ChangeTrustStore([&](deskhub::TrustStore& store) {
        const auto previous = store.Find(expected.fingerprint);
        if (!previous || *previous != expected) return false;
        if (profile.fingerprint != expected.fingerprint && store.Find(profile.fingerprint))
            return false;
        if (!store.Forget(expected.fingerprint)) return false;
        store.Insert(profile);
        const auto saved = store.Find(profile.fingerprint);
        return saved && saved->label == profile.label && saved->endpoint == profile.endpoint;
    });
}

bool ForgetTrustedHost(const deskhub::Fingerprint& fingerprint) {
    return ChangeTrustStore(
        [&](deskhub::TrustStore& store) { return store.Forget(fingerprint); });
}

}
