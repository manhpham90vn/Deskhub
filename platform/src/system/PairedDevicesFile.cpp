#include "deskhubp/system/PairedDevicesFile.h"

#include <atomic>
#include <cstring>
#include <mutex>

#include "deskhubp/system/AppDataFile.h"

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

void MarkPairedDevicesChanged() {
    Generation().fetch_add(1, std::memory_order_acq_rel);
}

deskhub::PairedDevices LoadPairedDevicesLocked() {
    return deskhub::ParsePairedDevices(ReadAppDataFile(kPairedDevicesFileName));
}

bool SavePairedDevicesLocked(const deskhub::PairedDevices& devices) {
    const bool saved =
        WriteAppDataFile(kPairedDevicesFileName, deskhub::SerializePairedDevices(devices));
    MarkPairedDevicesChanged();
    return saved;
}

}

uint64_t PairedDevicesGeneration() {
    return Generation().load(std::memory_order_acquire);
}

AuthSalt LoadOrCreateAuthSalt() {
    const std::string stored = ReadAppDataFile(kAuthSaltFileName);
    AuthSalt salt{};
    if (stored.size() == salt.size()) {
        std::memcpy(salt.data(), stored.data(), salt.size());
        return salt;
    }
    salt = NewAuthSalt();
    WriteAppDataFile(kAuthSaltFileName,
        std::string(reinterpret_cast<const char*>(salt.data()), salt.size()));
    return salt;
}

deskhub::PairedDevices LoadPairedDevices() {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    return LoadPairedDevicesLocked();
}

deskhub::PairVerdict CheckPairedDevice(const deskhub::Fingerprint& fingerprint) {
    return LoadPairedDevices().Check(fingerprint);
}

bool RememberPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    deskhub::PairedDevices devices = LoadPairedDevicesLocked();
    devices.Remember(fingerprint, name, nowUnix);
    return SavePairedDevicesLocked(devices);
}

bool TouchPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    deskhub::PairedDevices devices = LoadPairedDevicesLocked();
    if (!devices.Touch(fingerprint, name, nowUnix)) return false;
    return SavePairedDevicesLocked(devices);
}

bool ForgetPairedDevice(const deskhub::Fingerprint& fingerprint) {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    deskhub::PairedDevices devices = LoadPairedDevicesLocked();
    if (!devices.Forget(fingerprint)) return false;
    return SavePairedDevicesLocked(devices);
}

bool ForgetAllPairedDevices() {
    const std::lock_guard<std::mutex> lock(PairedDevicesMutex());
    const bool had = LoadPairedDevicesLocked().Size() != 0;
    RemoveAppDataFile(kPairedDevicesFileName);
    MarkPairedDevicesChanged();
    return had;
}

}
