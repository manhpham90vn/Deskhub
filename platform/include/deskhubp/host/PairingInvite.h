#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace deskhubp {

std::string BuildPairingInvite(uint16_t port, std::string_view bindIp, std::string_view hostName);

}
