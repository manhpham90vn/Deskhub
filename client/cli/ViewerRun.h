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
    std::string passcode{};
    std::string displayName{};
    std::vector<deskhub::SourceInfo> sources{};
    bool control = true;
    bool audio = true;
    std::string fecScheme{};
    bool nackGiven = false;
    bool nack = false;
    uint32_t holdFrames = 0;
    uint32_t audioDelayMs = 0;
    bool audioAdaptiveGiven = false;
    bool audioAdaptive = false;
    std::string videoPath{};
};

ExitCode RunViewers(const ViewRequest& request);

}
