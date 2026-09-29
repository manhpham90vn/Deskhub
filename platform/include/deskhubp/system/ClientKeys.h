#pragma once
#include "deskhub/ui/ClientKeys.h"

#include "deskhubp/system/ClientIdentity.h"

#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

std::string ClientPublicKeyLine(const ClientIdentity& identity, std::string_view keyName);
std::vector<ClientIdentityInfo> LoadClientKeys();
deskhub::ui::ClientKeyError CreateClientKey(std::string_view name);
deskhub::ui::ClientKeyError DeleteClientKey(std::string_view name);
deskhub::ui::ClientKeyError ImportClientKey(std::string_view name, std::string_view privateKey,
    std::string_view passphrase);

}
