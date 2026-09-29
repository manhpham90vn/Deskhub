#pragma once

#include "deskhub/net/AuthorizedKeys.h"
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

inline constexpr const char* kAuthorizedKeysFileName = "authorized_keys";

struct AuthorizedClient {
    std::string label{};
    deskhub::Fingerprint fingerprint{};
};

enum class ClientKeyAuthorization { Authorized,
    Denied,
    ConfigError };

std::optional<deskhub::AuthorizedKeys> LoadAuthorizedKeys();
bool RememberAuthorizedKey(std::string_view publicKeyText);
bool ClearAuthorizedKeys();
bool IsClientKeyAuthorized(std::span<const uint8_t> publicKeySpki);
ClientKeyAuthorization CheckClientKeyAuthorization(std::span<const uint8_t> publicKeySpki);
std::optional<std::vector<AuthorizedClient>> ListAuthorizedClients();
bool ForgetAuthorizedClient(const deskhub::Fingerprint& fingerprint);
uint64_t AuthorizedKeysGeneration();

}
