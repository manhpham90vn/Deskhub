#pragma once
#include "deskhub/net/PairedDevices.h"
#include "deskhubp/system/AuthProof.h"

#include <cstdint>

namespace deskhubp {

inline constexpr const char* kPairedDevicesFileName = "paired_devices";
inline constexpr const char* kAuthSaltFileName = "auth_salt";

AuthSalt LoadOrCreateAuthSalt();

deskhub::PairedDevices LoadPairedDevices();

deskhub::PairVerdict CheckPairedDevice(const deskhub::Fingerprint& fingerprint);
bool RememberPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix);
bool TouchPairedDevice(const deskhub::Fingerprint& fingerprint, std::string_view name,
    int64_t nowUnix);
bool ForgetPairedDevice(const deskhub::Fingerprint& fingerprint);
bool ForgetAllPairedDevices();
uint64_t PairedDevicesGeneration();

}
