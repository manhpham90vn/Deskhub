#pragma once
#include "deskhub/net/PairedDevices.h"

#include <cstdint>
#include <optional>

namespace deskhubp {

inline constexpr const char* kPairedDevicesFileName = "paired_devices";
deskhub::PairedDevices LoadPairedDevices();
std::optional<deskhub::PairedDevices> TryLoadPairedDevices();

deskhub::PairVerdict CheckPairedDevice(const deskhub::Fingerprint& fingerprint);
bool RememberPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix);
bool TouchPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix);
bool ForgetPairedDevice(const deskhub::Fingerprint& fingerprint);
bool ForgetAllPairedDevices();
uint64_t PairedDevicesGeneration();

}
