#include "deskhubp/system/ClientIdentity.h"

namespace deskhubp {

ClientIdentity::ClientIdentity(const HostIdentity&) {
}

ClientIdentity LoadClientIdentity() {
    return {};
}

ClientIdentity LoadClientIdentity(std::string_view) {
    return {};
}

ClientIdentity LoadOrCreateClientIdentity() {
    return {};
}

ClientIdentity GenerateClientIdentity(std::string_view) {
    return {};
}

bool ImportClientIdentity(std::string_view, std::string_view) {
    return false;
}

bool ImportClientIdentity(std::string_view, std::string_view, std::string_view) {
    return false;
}

std::vector<ClientIdentityInfo> ListClientIdentities() {
    return {};
}

bool RemoveClientIdentity(std::string_view) {
    return false;
}

std::string ClientPublicKeyText(const ClientIdentity&) {
    return {};
}

std::vector<uint8_t> SignWithClientIdentity(const ClientIdentity&, std::span<const uint8_t>) {
    return {};
}

}
