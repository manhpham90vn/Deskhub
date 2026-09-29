#include "deskhubp/system/ClientKeys.h"

#include "deskhubp/system/ClientIdentity.h"

#include <algorithm>

#include "deskhub/ui/HostProfiles.h"
#include "deskhubp/system/TrustStoreFile.h"

#include "deskhub/net/PublicKeyText.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace deskhubp {

namespace {

using deskhub::ui::ClientKeyError;

ClientKeyError CheckNewName(std::string_view name) {
    if (!deskhub::ui::IsValidClientKeyName(name)) return ClientKeyError::InvalidName;
    for (const ClientIdentityInfo& existing : ListClientIdentities())
        if (existing.name == name) return ClientKeyError::NameInUse;
    return ClientKeyError::None;
}

}

std::string ClientPublicKeyLine(const ClientIdentity& identity, std::string_view keyName) {
    auto parsed = deskhub::ParsePublicKeyText(ClientPublicKeyText(identity));
    if (!parsed) return {};
    parsed->label = deskhub::ui::ClientKeyLabel(SessionDeviceName(), keyName);
    return deskhub::FormatPublicKeyText(*parsed);
}

std::vector<ClientIdentityInfo> LoadClientKeys() {
    LoadOrCreateClientIdentity();
    std::vector<ClientIdentityInfo> keys;
    for (ClientIdentityInfo& key : ListClientIdentities()) {
        if (!key.valid) continue;
        key.publicKeyText = ClientPublicKeyLine(LoadClientIdentity(key.name), key.name);
        keys.push_back(std::move(key));
    }
    return keys;
}

ClientKeyError CreateClientKey(std::string_view name) {
    const ClientKeyError nameError = CheckNewName(name);
    if (nameError != ClientKeyError::None) return nameError;
    return GenerateClientIdentity(name).Valid() ? ClientKeyError::None
                                                : ClientKeyError::WriteFailed;
}

ClientKeyError DeleteClientKey(std::string_view name) {
    if (name == deskhub::ui::kDefaultIdentityName) return ClientKeyError::DefaultKey;
    const auto keys = ListClientIdentities();
    const bool exists = std::any_of(keys.begin(), keys.end(),
        [&](const ClientIdentityInfo& key) { return key.name == name; });
    if (!exists) return ClientKeyError::KeyMissing;
    const auto store = TryLoadTrustStore();
    if (store && std::any_of(store->Hosts().begin(), store->Hosts().end(),
                     [&](const deskhub::TrustedHost& host) {
                         return deskhub::ui::ProfileIdentityName(host) == name;
                     }))
        return ClientKeyError::KeyInUse;
    return RemoveClientIdentity(name) ? ClientKeyError::None : ClientKeyError::WriteFailed;
}

ClientKeyError ImportClientKey(std::string_view name, std::string_view privateKey,
    std::string_view passphrase) {
    const ClientKeyError nameError = CheckNewName(name);
    if (nameError != ClientKeyError::None) return nameError;
    return ImportClientIdentity(name, privateKey, passphrase) ? ClientKeyError::None
                                                              : ClientKeyError::UnreadableKey;
}

}
