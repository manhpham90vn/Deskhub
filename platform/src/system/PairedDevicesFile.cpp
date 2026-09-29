#include "deskhubp/system/PairedDevicesFile.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

std::atomic<uint64_t>& Generation() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

std::mutex& PairedDevicesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<bool>& InvalidConfigReported() {
    static std::atomic<bool> reported{false};
    return reported;
}

std::optional<deskhub::PairedDevices> InvalidConfig() {
    if (!InvalidConfigReported().exchange(true, std::memory_order_acq_rel))
        LOGE("authorized client keys: stored file cannot be read or parsed; denying admission");
    return std::nullopt;
}

void MarkPairedDevicesChanged() {
    Generation().fetch_add(1, std::memory_order_acq_rel);
}

std::optional<deskhub::PairedDevices> LoadPairedDevicesLocked() {
    const auto path = AppDataFilePath(kPairedDevicesFileName);
    if (path.empty()) return InvalidConfig();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error) && !error) {
            InvalidConfigReported().store(false, std::memory_order_release);
            return deskhub::PairedDevices{};
        }
        return InvalidConfig();
    }
    const auto size = file.tellg();
    if (size < 0 || size > 32768 || !file.seekg(0)) return InvalidConfig();
    std::string text(size_t(size), '\0');
    if (!text.empty() && !file.read(text.data(), size)) return InvalidConfig();
    auto devices = deskhub::ParsePairedDevicesStrict(text);
    if (!devices) return InvalidConfig();
    InvalidConfigReported().store(false, std::memory_order_release);
    return devices;
}

bool SavePairedDevicesLocked(const deskhub::PairedDevices& devices) {
    const bool saved = WriteAppDataFileAtomic(
        kPairedDevicesFileName, deskhub::SerializePairedDevices(devices));
    if (saved) MarkPairedDevicesChanged();
    return saved;
}

}

uint64_t PairedDevicesGeneration() {
    return Generation().load(std::memory_order_acquire);
}

deskhub::PairedDevices LoadPairedDevices() {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    return LoadPairedDevicesLocked().value_or(deskhub::PairedDevices{});
}

std::optional<deskhub::PairedDevices> TryLoadPairedDevices() {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    return LoadPairedDevicesLocked();
}

deskhub::PairVerdict CheckPairedDevice(const deskhub::Fingerprint& fingerprint) {
    return LoadPairedDevices().Check(fingerprint);
}

bool RememberPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    const ConfigFileLock fileLock(kPairedDevicesFileName);
    if (!fileLock.Valid()) return false;
    auto devices = LoadPairedDevicesLocked();
    if (!devices) return false;
    devices->Remember(fingerprint, name, nowUnix);
    return SavePairedDevicesLocked(*devices);
}

bool TouchPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    const ConfigFileLock fileLock(kPairedDevicesFileName);
    if (!fileLock.Valid()) return false;
    auto devices = LoadPairedDevicesLocked();
    if (!devices || !devices->Touch(fingerprint, name, nowUnix)) return false;
    return SavePairedDevicesLocked(*devices);
}

bool ForgetPairedDevice(const deskhub::Fingerprint& fingerprint) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    const ConfigFileLock fileLock(kPairedDevicesFileName);
    if (!fileLock.Valid()) return false;
    auto devices = LoadPairedDevicesLocked();
    if (!devices || !devices->Forget(fingerprint)) return false;
    return SavePairedDevicesLocked(*devices);
}

bool ForgetAllPairedDevices() {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    const ConfigFileLock fileLock(kPairedDevicesFileName);
    if (!fileLock.Valid()) return false;
    if (!LoadPairedDevicesLocked()) return false;
    return SavePairedDevicesLocked(deskhub::PairedDevices{});
}

}
