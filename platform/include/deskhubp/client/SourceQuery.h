#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "deskhubp/net/UdpSocket.h"

#include "deskhub/protocol/Wire.h"

bool QuerySources(const NetAddr& server, std::vector<deskhub::SourceInfo>& out,
    const std::string& passcode = std::string(),
    deskhub::AuthResultCode* outCode = nullptr, deskhub::HostCaps* outCaps = nullptr,
    std::string_view clientIdentityName = {});
