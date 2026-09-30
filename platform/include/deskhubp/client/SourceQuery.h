#pragma once
#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "deskhubp/net/UdpSocket.h"

#include "deskhub/net/PairingInvite.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"

struct SourceQueryRequest {
    bool acceptNewHostKey = false;
    std::string pairingInvite{};
    uint32_t approvalWaitMs = 120'000;
    std::function<void(std::string_view)> onProgress{};
    const std::atomic<bool>* cancel = nullptr;
};

enum class SourceQueryFailure {
    None,
    Unreachable,
    UntrustedHost,
    Refused,
    AwaitingApproval,
    InviteMismatch,
    LocalError,
};

struct SourceQueryReply {
    std::vector<deskhub::SourceInfo> sources{};
    deskhub::HostCaps caps{};
    std::string hostName{};
    std::string failure{};
    std::optional<deskhub::Fingerprint> unknownHostKey{};
    SourceQueryFailure failureKind = SourceQueryFailure::None;
    std::string answeredAddress{};
};

bool QuerySources(const NetAddr& server, SourceQueryReply& reply,
    const SourceQueryRequest& request = {});
std::vector<NetAddr> PairingInviteEndpoints(std::string_view invite);
bool QuerySourcesByInvite(std::string_view invite, SourceQueryReply& reply,
    const SourceQueryRequest& request = {});
