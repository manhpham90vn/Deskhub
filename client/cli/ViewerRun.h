#pragma once
#include <string>
#include <vector>

#include "Commands.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/net/UdpSocket.h"

namespace deskhubcli {

struct ViewRequest {
    NetAddr server{};
    std::string hostLabel{};
    std::string displayName{};
    std::vector<deskhub::SourceInfo> sources{};
    bool control = true;
    bool audio = true;
};

ExitCode RunViewers(const ViewRequest& request);

}
