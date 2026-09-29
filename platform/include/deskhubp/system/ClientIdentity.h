#pragma once

#include "deskhubp/system/HostIdentity.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

inline constexpr const char* kClientKeyFileName = "client_key.pem";

struct ClientIdentity {
    std::string keyPem{};
    std::vector<uint8_t> publicKey{};
    deskhub::Fingerprint fingerprint{};

    ClientIdentity() = default;
    ClientIdentity(const HostIdentity& legacy);

    bool Valid() const {
        return !keyPem.empty() && !publicKey.empty() && !deskhub::IsZero(fingerprint);
    }
};

struct ClientIdentityInfo {
    std::string name{};
    std::string publicKeyText{};
    deskhub::Fingerprint fingerprint{};
    bool valid = false;
};

ClientIdentity LoadClientIdentity();
ClientIdentity LoadClientIdentity(std::string_view name);
ClientIdentity LoadOrCreateClientIdentity();
ClientIdentity GenerateClientIdentity(std::string_view name);
bool ImportClientIdentity(std::string_view privateKeyPem, std::string_view passphrase);
bool ImportClientIdentity(std::string_view name, std::string_view privateKeyPem,
    std::string_view passphrase);
std::vector<ClientIdentityInfo> ListClientIdentities();
bool RemoveClientIdentity(std::string_view name);
std::string ClientPublicKeyText(const ClientIdentity& identity);
std::vector<uint8_t> SignWithClientIdentity(const ClientIdentity& identity,
    std::span<const uint8_t> data);

}
