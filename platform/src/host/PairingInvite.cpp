#include "deskhubp/host/PairingInvite.h"

#include "deskhub/net/PairingInvite.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"

namespace deskhubp {

namespace {

std::vector<deskhub::PairingEndpoint> InviteEndpoints(uint16_t port, std::string_view bindIp) {
    std::vector<deskhub::PairingEndpoint> endpoints;
    if (!bindIp.empty()) {
        if (const auto chosen = deskhub::MakePairingEndpoint(bindIp, port))
            endpoints.push_back(*chosen);
        return endpoints;
    }
    for (const AdapterAddr& adapter : ListLocalIPv4()) {
        if (adapter.virtualAdapter) continue;
        if (endpoints.size() == deskhub::kMaxPairingEndpoints) break;
        if (const auto endpoint = deskhub::MakePairingEndpoint(adapter.ip, port))
            endpoints.push_back(*endpoint);
    }
    if (endpoints.empty())
        for (const AdapterAddr& adapter : ListLocalIPv4()) {
            if (endpoints.size() == deskhub::kMaxPairingEndpoints) break;
            if (const auto endpoint = deskhub::MakePairingEndpoint(adapter.ip, port))
                endpoints.push_back(*endpoint);
        }
    return endpoints;
}

}

std::string BuildPairingInvite(uint16_t port, std::string_view bindIp, std::string_view hostName) {
    const HostIdentity identity = LoadOrCreateHostIdentity();
    if (!identity.Valid()) {
        LOGE("[Pairing] This machine has no identity key, so it cannot make an invite.");
        return {};
    }
    deskhub::PairingInvite invite;
    invite.endpoints = InviteEndpoints(port, bindIp);
    if (invite.endpoints.empty()) {
        LOGW("[Pairing] No network address to put in the invite.");
        return {};
    }
    invite.hostKey = identity.fingerprint;
    const auto token = IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    if (!token) {
        LOGE("[Pairing] Could not issue a pairing token.");
        return {};
    }
    invite.token = *token;
    invite.hostName = std::string(hostName);
    std::string text = deskhub::FormatPairingInvite(invite);
    while (text.empty() && !invite.hostName.empty()) {
        invite.hostName.pop_back();
        text = deskhub::FormatPairingInvite(invite);
    }
    return text;
}

}
