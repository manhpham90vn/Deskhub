#pragma once

#include "deskhub/net/AuthorizedKeys.h"
#include "deskhub/net/PairedDevices.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace deskhubp {

inline constexpr const char* kAuthorizedKeysFileName = "authorized_keys";

struct AuthorizedKeysSnapshot {
    bool configured = false;
    deskhub::AuthorizedKeys keys{};
};

enum class ClientKeyAuthorization { Authorized,
    Denied,
    ConfigError };

std::optional<AuthorizedKeysSnapshot> LoadAuthorizedKeys();
bool RememberAuthorizedKey(std::string_view publicKeyText);
bool ForgetAuthorizedKey(std::span<const uint8_t> publicKeySpki);
bool ClearAuthorizedKeys();
bool IsClientKeyAuthorized(std::span<const uint8_t> publicKeySpki);
ClientKeyAuthorization CheckClientKeyAuthorization(std::span<const uint8_t> publicKeySpki);
std::optional<deskhub::PairedDevices> LoadEffectiveAuthorizedDevices();
bool ForgetEffectiveAuthorizedDevice(const deskhub::Fingerprint& fingerprint);
uint64_t AuthorizedKeysGeneration();

}
