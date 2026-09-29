#pragma once
#include <optional>
#include <string>
#include <vector>

#include "deskhubp/net/UdpSocket.h"

#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"

struct SourceQueryRequest {
    std::string clientIdentityName{};
    bool acceptNewHostKey = false;
};

struct SourceQueryReply {
    std::vector<deskhub::SourceInfo> sources{};
    deskhub::HostCaps caps{};
    std::string hostName{};
    std::string failure{};
    std::optional<deskhub::Fingerprint> unknownHostKey{};
};

bool QuerySources(const NetAddr& server, SourceQueryReply& reply,
    const SourceQueryRequest& request = {});
