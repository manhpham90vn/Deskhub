#include "deskhubp/client/SourceQuery.h"

#include <cinttypes>

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {
constexpr uint32_t kHandshakeTimeoutMs = 5'000;
constexpr uint64_t kListTimeoutUs = 5'000'000;
constexpr uint32_t kPollWaitMs = 2;

SourceQueryFailure FailureKindOf(const deskhubp::HostLink& link) {
    if (link.State() == deskhubp::HostLinkState::Refused)
        return link.Message() ==
                       deskhub::ui::AuthRefusalText(deskhub::AuthResultCode::AwaitingApproval)
                   ? SourceQueryFailure::AwaitingApproval
                   : SourceQueryFailure::Refused;
    if (link.Message() == deskhub::ui::kInviteHostMismatch) return SourceQueryFailure::InviteMismatch;
    if (link.FingerprintText().empty()) return SourceQueryFailure::Unreachable;
    if (link.Verdict() == deskhub::TrustVerdict::Unknown) return SourceQueryFailure::UntrustedHost;
    return SourceQueryFailure::LocalError;
}

bool Cancelled(const SourceQueryRequest& request) {
    return request.cancel != nullptr && request.cancel->load(std::memory_order_acquire);
}

}

std::vector<NetAddr> PairingInviteEndpoints(std::string_view invite) {
    std::vector<NetAddr> endpoints;
    const auto parsed = deskhub::ParsePairingInvite(invite);
    if (!parsed) return endpoints;
    for (const deskhub::PairingEndpoint& endpoint : parsed->endpoints)
        endpoints.push_back(NetAddr{endpoint.ip, endpoint.port});
    return endpoints;
}

bool QuerySources(const NetAddr& server, SourceQueryReply& reply,
    const SourceQueryRequest& request) {
    reply = SourceQueryReply{};
    reply.failure = deskhub::ui::SourceQueryFailed(server.ToString());
    reply.failureKind = SourceQueryFailure::Unreachable;
    reply.answeredAddress = server.ToString();

    if (!deskhubp::QuicAvailable()) {
        LOGE("[Sources] This build has no QUIC library.");
        return false;
    }

    deskhubp::HostLink link;
    const std::shared_ptr<deskhubp::HostLinkChannel> control =
        link.Open({deskhub::Chan::Control});

    deskhubp::HostLinkConfig config;
    config.host = server;
    config.hostLabel = server.ToString();
    config.clientName = deskhubp::SessionDeviceName();
    config.acceptNewHostKey = request.acceptNewHostKey;
    config.connectTimeoutMs = kHandshakeTimeoutMs;
    config.approvalWaitUs = uint64_t(request.approvalWaitMs) * 1000;
    config.recvWaitMs = kPollWaitMs;
    if (const auto invite = deskhub::ParsePairingInvite(request.pairingInvite)) {
        config.expectedHostKey = invite->hostKey;
        config.pairingToken.assign(invite->token.begin(), invite->token.end());
        config.hostLabel = invite->hostName.empty() ? server.ToString() : invite->hostName;
    }
    deskhubp::HostLinkCallbacks hooks;
    if (request.onProgress)
        hooks.onState = [&request](deskhubp::HostLinkState state, std::string_view message) {
            if (state == deskhubp::HostLinkState::AwaitingApproval) request.onProgress(message);
        };
    if (!link.Start(config, std::move(hooks))) {
        LOGE("[Sources] Could not open a connection to %s.", server.ToString().c_str());
        return false;
    }

    while (!link.Settled() && link.State() != deskhubp::HostLinkState::Ready) {
        if (Cancelled(request)) {
            link.Stop();
            reply.failure = deskhub::ui::kAuthAwaitingApproval;
            reply.failureKind = SourceQueryFailure::AwaitingApproval;
            return false;
        }
        control->WaitWork(kPollWaitMs);
    }
    if (link.State() == deskhubp::HostLinkState::Refused ||
        link.State() == deskhubp::HostLinkState::Failed) {
        const std::string reason = link.Message();
        if (!reason.empty()) reply.failure = reason;
        if (link.Verdict() == deskhub::TrustVerdict::Unknown)
            reply.unknownHostKey = deskhub::ParseFingerprint(link.FingerprintText());
        reply.failureKind = FailureKindOf(link);
    }
    if (link.State() == deskhubp::HostLinkState::Refused) {
        LOGW("[Sources] %s did not let this machine in.", server.ToString().c_str());
        return false;
    }
    if (link.State() != deskhubp::HostLinkState::Ready) {
        LOGW("[Sources] %s did not answer the handshake.", server.ToString().c_str());
        return false;
    }

    uint8_t query[deskhub::kMaxDatagram];
    const size_t qn = deskhub::BuildListSources(query);
    if (!qn) return false;
    link.Send(std::span<const uint8_t>(query, qn));

    const uint64_t deadline = NowUs() + kListTimeoutUs;
    while (NowUs() < deadline) {
        const auto message = control->Poll();
        if (!message) {
            control->WaitWork(kPollWaitMs);
            continue;
        }

        const auto span = std::span<const uint8_t>(message->data(), message->size());
        const auto h = deskhub::ParseCommonHeader(span);
        if (!h || h->type != deskhub::MsgType::SourceList) continue;

        deskhub::SourceInfo tmp[deskhub::kMaxSources];
        const size_t cnt = deskhub::ParseSourceList(deskhub::PayloadOf(span), tmp);
        for (size_t i = 0; i < cnt; ++i) reply.sources.push_back(std::move(tmp[i]));
        reply.caps = deskhub::HostCapsOfFlags(h->flags);
        reply.hostName = deskhub::ParseSourceListHostName(deskhub::PayloadOf(span));
        reply.failure.clear();
        reply.failureKind = SourceQueryFailure::None;
        LOGI("[Sources] Host is sharing %zu source(s).", reply.sources.size());
        return true;
    }

    LOGW("[Sources] No SOURCE_LIST from %s after %" PRIu64 " ms.", server.ToString().c_str(),
        kListTimeoutUs / 1000);
    return false;
}

bool QuerySourcesByInvite(std::string_view invite, SourceQueryReply& reply,
    const SourceQueryRequest& request) {
    reply = SourceQueryReply{};
    reply.failure = deskhub::ui::kInviteInvalid;
    reply.failureKind = SourceQueryFailure::LocalError;
    const std::vector<NetAddr> endpoints = PairingInviteEndpoints(invite);
    if (endpoints.empty()) return false;
    SourceQueryRequest withInvite = request;
    withInvite.pairingInvite = std::string(invite);
    for (const NetAddr& endpoint : endpoints) {
        if (Cancelled(request)) return false;
        if (QuerySources(endpoint, reply, withInvite)) return true;
        if (reply.failureKind != SourceQueryFailure::Unreachable) return false;
    }
    return false;
}
