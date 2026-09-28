#include "deskhubp/client/SourceQuery.h"

#include <cinttypes>

#include "deskhubp/diag/Log.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {
constexpr uint32_t kHandshakeTimeoutMs = 5'000;
constexpr uint32_t kAuthTimeoutMs = 65'000;
constexpr uint64_t kListTimeoutUs = 5'000'000;
constexpr uint32_t kPollWaitMs = 2;
}

bool QuerySources(const NetAddr& server, std::vector<deskhub::SourceInfo>& out,
    const std::string& passcode, deskhub::AuthResultCode* outCode,
    deskhub::HostCaps* outCaps, std::string_view clientIdentityName) {
    out.clear();
    if (outCode) *outCode = deskhub::AuthResultCode::NotPaired;
    if (outCaps) *outCaps = deskhub::HostCaps{};

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
    config.passcode = passcode;
    config.clientName = deskhubp::SessionDeviceName();
    config.clientIdentityName = clientIdentityName;
    config.connectTimeoutMs = kHandshakeTimeoutMs;
    config.authTimeoutMs = kAuthTimeoutMs;
    config.recvWaitMs = kPollWaitMs;
    if (!link.Start(config, deskhubp::HostLinkCallbacks{})) {
        LOGE("[Sources] Could not open a connection to %s.", server.ToString().c_str());
        return false;
    }

    while (!link.Settled() && link.State() != deskhubp::HostLinkState::Ready)
        control->WaitWork(kPollWaitMs);
    if (outCode) *outCode = link.AuthCode();
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
        for (size_t i = 0; i < cnt; ++i) out.push_back(std::move(tmp[i]));
        if (outCaps) *outCaps = deskhub::HostCapsOfFlags(h->flags);
        LOGI("[Sources] Host is sharing %zu source(s).", out.size());
        return true;
    }

    LOGW("[Sources] No SOURCE_LIST from %s after %" PRIu64 " ms.", server.ToString().c_str(),
        kListTimeoutUs / 1000);
    return false;
}
