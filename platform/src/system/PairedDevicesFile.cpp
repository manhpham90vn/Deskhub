#include "deskhubp/system/PairedDevicesFile.h"

#include <atomic>
#include <cstring>

#include "deskhubp/system/AppDataFile.h"

namespace deskhubp {

namespace {

std::atomic<uint64_t>& Generation() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

void MarkPairedDevicesChanged() {
    Generation().fetch_add(1, std::memory_order_acq_rel);
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
    return deskhub::ParsePairedDevices(ReadAppDataFile(kPairedDevicesFileName));
}

bool SavePairedDevices(const deskhub::PairedDevices& devices) {
    const bool saved =
        WriteAppDataFile(kPairedDevicesFileName, deskhub::SerializePairedDevices(devices));
    MarkPairedDevicesChanged();
    return saved;
}

deskhub::PairVerdict CheckPairedDevice(const deskhub::Fingerprint& fingerprint) {
    return LoadPairedDevices().Check(fingerprint);
}

bool RememberPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    deskhub::PairedDevices devices = LoadPairedDevices();
    devices.Remember(fingerprint, name, nowUnix);
    return SavePairedDevices(devices);
}

bool TouchPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix) {
    deskhub::PairedDevices devices = LoadPairedDevices();
    if (!devices.Touch(fingerprint, name, nowUnix)) return false;
    return SavePairedDevices(devices);
}

bool ForgetPairedDevice(const deskhub::Fingerprint& fingerprint) {
    deskhub::PairedDevices devices = LoadPairedDevices();
    if (!devices.Forget(fingerprint)) return false;
    return SavePairedDevices(devices);
}

bool ForgetAllPairedDevices() {
    const bool had = LoadPairedDevices().Size() != 0;
    RemoveAppDataFile(kPairedDevicesFileName);
    MarkPairedDevicesChanged();
    return had;
}

}
