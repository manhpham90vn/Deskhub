#include "deskhubp/client/HostLink.h"

#include "deskhub/session/LinkRecovery.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/Random.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <chrono>
#include <utility>

namespace deskhubp {

namespace {

constexpr uint32_t kEstablishSliceMs = 50;
constexpr uint32_t kDecisionPollUs = 20'000;

}

void HostLinkChannel::Push(std::vector<uint8_t> bytes) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        inbox_.push_back(std::move(bytes));
    }
    cv_.notify_all();
}

std::optional<std::vector<uint8_t>> HostLinkChannel::Poll() {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (inbox_.empty()) return std::nullopt;
    std::vector<uint8_t> bytes = std::move(inbox_.front());
    inbox_.pop_front();
    return bytes;
}

bool HostLinkChannel::WaitWork(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this] { return !inbox_.empty() || kicked_; });
    const bool woke = !inbox_.empty() || kicked_;
    kicked_ = false;
    return woke;
}

void HostLinkChannel::Kick() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        kicked_ = true;
    }
    cv_.notify_all();
}

size_t HostLinkChannel::Pending() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return inbox_.size();
}

HostLink::~HostLink() {
    Stop();
}

bool HostLink::Start(const HostLinkConfig& config, HostLinkCallbacks callbacks) {
    if (running_.load(std::memory_order_acquire)) return false;
    if (thread_.joinable()) thread_.join();
    if (!QuicAvailable()) {
        SetState(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
        return false;
    }

    config_ = config;
    cb_ = std::move(callbacks);
    stop_.store(false, std::memory_order_release);
    peerGone_.store(false, std::memory_order_release);
    trustDecision_.store(int(TrustDecision::Pending), std::memory_order_release);
    authCode_.store(deskhub::AuthResultCode::NotPaired, std::memory_order_release);
    autoTrustPending_.store(false, std::memory_order_release);
    redial_.store(false, std::memory_order_release);
    pulse_.Reset();
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        fingerprint_ = deskhub::Fingerprint{};
        verdict_ = deskhub::TrustVerdict::Unknown;
        message_.clear();
        pulseView_ = deskhub::LinkPulseView{};
    }
    keepaliveIntervalUs_ = deskhub::KeepaliveIntervalUs(QuicSettings{}.idleTimeoutMs);
    linkLostAtUs_ = 0;
    redialAttempts_ = 0;

    sock_.SetRecvTimeout(config_.recvWaitMs);
    sock_.SetVideoPath(config_.videoPath);
    sock_.SetOnPeerGone([this](const NetAddr& peer) {
        if (peer == config_.host) peerGone_.store(true, std::memory_order_release);
    });
    sock_.SetOnStreamBroken([this](const NetAddr& peer, uint64_t streamId) {
        if (peer == config_.host && cb_.onStreamBroken) cb_.onStreamBroken(streamId);
    });

    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { Loop(); });
    return true;
}

void HostLink::Stop() {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    sock_.Close();
    running_.store(false, std::memory_order_release);
}

std::shared_ptr<HostLinkChannel> HostLink::Open(std::initializer_list<deskhub::Chan> chans) {
    auto channel = std::make_shared<HostLinkChannel>();
    const std::lock_guard<std::mutex> lock(routeMutex_);
    for (const deskhub::Chan chan : chans) routes_[size_t(chan)].push_back(channel);
    return channel;
}

void HostLink::AcceptFingerprint() {
    trustDecision_.store(int(TrustDecision::Accepted), std::memory_order_release);
}

void HostLink::RejectFingerprint() {
    trustDecision_.store(int(TrustDecision::Rejected), std::memory_order_release);
}

void HostLink::RequestRedial() {
    redial_.store(true, std::memory_order_release);
}

deskhub::LinkPulseView HostLink::Pulse() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return pulseView_;
}

bool HostLink::Send(std::span<const uint8_t> message) {
    NoteSent();
    return sock_.SendTo(config_.host, message.data(), message.size());
}

bool HostLink::SendRecordOn(uint64_t streamId, std::span<const uint8_t> message) {
    NoteSent();
    return sock_.SendRecordOn(config_.host, streamId, message);
}

bool HostLink::Settled() const {
    switch (State()) {
        case HostLinkState::Refused:
        case HostLinkState::Failed:
        case HostLinkState::Ended: return true;
        default: return false;
    }
}

std::string HostLink::Message() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return message_;
}

std::string HostLink::FingerprintText() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return deskhub::IsZero(fingerprint_) ? std::string() : FormatFingerprint(fingerprint_);
}

deskhub::TrustVerdict HostLink::Verdict() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return verdict_;
}

void HostLink::SetState(HostLinkState state, std::string_view message) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        message_.assign(message);
    }
    state_.store(state, std::memory_order_release);
    if (cb_.onState) cb_.onState(state, message);
}

void HostLink::Fail(HostLinkState state, std::string_view message) {
    SetState(state, message);
    stop_.store(true, std::memory_order_release);
}

void HostLink::NoteSent() {
    lastSendUs_.store(NowUs(), std::memory_order_relaxed);
}

void HostLink::SendLinkPing(uint64_t nowUs) {
    uint8_t buf[deskhub::kMaxDatagram];
    const size_t n = deskhub::BuildPing(buf, 0, pulse_.MakePing(nowUs));
    if (n) Send(std::span<const uint8_t>(buf, n));
    PublishPulse(nowUs);
}

void HostLink::PublishPulse(uint64_t nowUs) {
    const deskhub::LinkPulseView view = pulse_.View(nowUs);
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        pulseView_ = view;
    }
    if (cb_.onPulse) cb_.onPulse(view);
}

void HostLink::Loop() {
    bool resumed = false;
    while (!stop_.load(std::memory_order_acquire)) {
        if (!DialAndAdmit()) break;

        SetState(HostLinkState::Ready,
            resumed ? deskhub::ui::kTerminalReattached : std::string_view{});
        linkLostAtUs_ = 0;
        redialAttempts_ = 0;
        redial_.store(false, std::memory_order_release);
        pulse_.Reset();
        PublishPulse(NowUs());
        if (cb_.onReady) cb_.onReady(resumed);

        PumpReady();
        if (stop_.load(std::memory_order_acquire)) break;

        if (!config_.recoverLink) {
            Fail(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
            break;
        }
        LOGW("link: connection to %s dropped \xE2\x80\x94 reconnecting",
            config_.host.ToString().c_str());
        linkLostAtUs_ = NowUs();
        redialAttempts_ = 0;
        resumed = true;
        SetState(HostLinkState::Recovering, deskhub::ui::kTerminalReattaching);
        if (cb_.onLinkLost) cb_.onLinkLost();
    }
    running_.store(false, std::memory_order_release);
}

bool HostLink::DialAndAdmit() {
    while (!stop_.load(std::memory_order_acquire)) {
        const bool recovering = linkLostAtUs_ != 0;
        if (recovering) {
            if (!deskhub::ReconnectStillWorthTrying(NowUs() - linkLostAtUs_,
                    config_.recoverGraceUs)) {
                LOGW("link: gave up reconnecting to %s after %u attempts",
                    config_.host.ToString().c_str(), redialAttempts_);
                Fail(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
                return false;
            }
            const uint64_t waitUntil = NowUs() + deskhub::ReconnectDelayUs(redialAttempts_, RandomU32());
            while (!stop_.load(std::memory_order_acquire) && NowUs() < waitUntil)
                SleepUs(kDecisionPollUs);
            if (stop_.load(std::memory_order_acquire)) return false;
            ++redialAttempts_;
        }

        peerGone_.store(false, std::memory_order_release);
        sock_.Close();
        if (!recovering) SetState(HostLinkState::Connecting, deskhub::ui::kTransferConnecting);
        if (!sock_.Connect(QuicSettings{}, config_.host, config_.hostLabel)) {
            Fail(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
            return false;
        }
        if (!AwaitEstablished()) {
            if (recovering) continue;
            Fail(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
            return false;
        }
        if (!SettleTrust()) return false;
        if (RunAuth()) return true;
        if (Settled() || stop_.load(std::memory_order_acquire)) return false;
    }
    return false;
}

bool HostLink::AwaitEstablished() {
    const uint64_t deadline = NowUs() + uint64_t(config_.connectTimeoutMs) * 1000;
    while (!stop_.load(std::memory_order_acquire) && NowUs() < deadline) {
        if (sock_.WaitEstablished(config_.host, kEstablishSliceMs)) return true;
    }
    return sock_.Established(config_.host);
}

bool HostLink::SettleTrust() {
    const std::optional<deskhub::Fingerprint> peer = sock_.PeerFingerprint(config_.host);
    if (!peer) {
        Fail(HostLinkState::Failed, deskhub::ui::kTerminalUnreachable);
        return false;
    }

    const std::string endpoint = config_.host.ToString();
    const deskhub::TrustVerdict verdict = CheckTrustedHost(endpoint, *peer);
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        fingerprint_ = *peer;
        verdict_ = verdict;
    }

    if (verdict != deskhub::TrustVerdict::Changed) {
        autoTrustPending_.store(verdict == deskhub::TrustVerdict::Unknown,
            std::memory_order_release);
        return true;
    }
    if (!config_.trustGate) {
        autoTrustPending_.store(false, std::memory_order_release);
        return true;
    }

    trustDecision_.store(int(TrustDecision::Pending), std::memory_order_release);
    SetState(HostLinkState::Deciding, deskhub::ui::kTrustChangedBody);
    LOGW(
        "link: %s answered with %s, which is not the key on record, so this link is parked "
        "until someone accepts or rejects it - it sends nothing while it waits, and the peer "
        "reads that as a silent link rather than as a question",
        endpoint.c_str(), FormatFingerprint(*peer).c_str());
    if (cb_.onTrustAsked) cb_.onTrustAsked(verdict, FormatFingerprint(*peer));

    uint8_t buf[deskhub::kMaxRecordSize];
    uint64_t lastKeepaliveUs = NowUs();
    while (!stop_.load(std::memory_order_acquire)) {
        const auto decision = TrustDecision(trustDecision_.load(std::memory_order_acquire));
        if (decision == TrustDecision::Rejected) {
            Fail(HostLinkState::Ended, deskhub::ui::kTrustReject);
            return false;
        }
        if (decision == TrustDecision::Accepted) {
            RememberTrustedHost(endpoint, config_.hostLabel, *peer, NowUnixSeconds());
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                verdict_ = deskhub::TrustVerdict::Trusted;
            }
            return true;
        }
        NetAddr from;
        if (sock_.RecvFrom(buf, sizeof(buf), from) < 0) SleepUs(kDecisionPollUs);
        const uint64_t nowUs = NowUs();
        if (deskhub::KeepaliveDue(nowUs, lastKeepaliveUs, keepaliveIntervalUs_)) {
            sock_.SendKeepalive(config_.host);
            lastKeepaliveUs = nowUs;
        }
    }
    return false;
}

bool HostLink::RunAuth() {
    const bool recovering = linkLostAtUs_ != 0;
    if (!recovering) SetState(HostLinkState::Authing, deskhub::ui::kTransferConnecting);

    deskhub::Fingerprint peer;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        peer = fingerprint_;
    }

    ClientAuthConfig auth;
    auth.identity = LoadOrCreateHostIdentity(config_.clientName);
    auth.passcode = config_.passcode;
    auth.hostFingerprint = peer;
    auth.clientName = config_.clientName;

    deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
    bool hostProved = false;
    const bool allowed = sock_.RunClientAuth(config_.host, std::move(auth), config_.authTimeoutMs,
        code, hostProved, &stop_);
    authCode_.store(code, std::memory_order_release);
    if (!allowed) {
        if (stop_.load(std::memory_order_acquire)) return false;
        if (recovering && code == deskhub::AuthResultCode::TimedOut) return false;
        Fail(HostLinkState::Refused, deskhub::ui::AuthRefusalText(code));
        return false;
    }

    if (hostProved && autoTrustPending_.exchange(false, std::memory_order_acq_rel) &&
        !deskhub::IsZero(peer)) {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            verdict_ = deskhub::TrustVerdict::Trusted;
        }
        RememberTrustedHost(config_.host.ToString(), config_.hostLabel, peer, NowUnixSeconds());
        LOGI("link: passcode accepted \xE2\x80\x94 remembering %s as %s",
            config_.host.ToString().c_str(), FormatFingerprint(peer).c_str());
    }
    return true;
}

void HostLink::PumpReady() {
    uint8_t buf[deskhub::kMaxRecordSize];
    uint64_t lastKeepaliveUs = NowUs();
    while (!stop_.load(std::memory_order_acquire)) {
        NetAddr from;
        const int n = sock_.RecvFrom(buf, sizeof(buf), from);
        if (n < 0) return;
        if (n > 0 && from == config_.host) Route(std::span<const uint8_t>(buf, size_t(n)));
        if (peerGone_.load(std::memory_order_acquire)) return;
        if (redial_.exchange(false, std::memory_order_acq_rel)) {
            LOGW("link: a redial of %s was asked for \xE2\x80\x94 dropping this connection",
                config_.host.ToString().c_str());
            return;
        }

        const uint64_t nowUs = NowUs();
        if (pulse_.PingDue(nowUs)) SendLinkPing(nowUs);
        if (config_.recoverLink && pulse_.Stalled(nowUs)) {
            LOGW("link: no pong from %s for %llu ms \xE2\x80\x94 treating the link as lost",
                config_.host.ToString().c_str(),
                (unsigned long long)(deskhub::kLinkStallAfterUs / 1000));
            return;
        }

        const uint64_t lastSendUs = lastSendUs_.load(std::memory_order_relaxed);
        const uint64_t idleSinceUs = lastSendUs > lastKeepaliveUs ? lastSendUs : lastKeepaliveUs;
        if (deskhub::KeepaliveDue(nowUs, idleSinceUs, keepaliveIntervalUs_)) {
            sock_.SendKeepalive(config_.host);
            lastKeepaliveUs = nowUs;
        }
    }
}

void HostLink::Route(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header || size_t(header->chan) >= deskhub::kChanCount) return;
    if (header->type == deskhub::MsgType::Pong && header->sessionId == 0) {
        const auto pong = deskhub::ParsePingPong(deskhub::PayloadOf(message));
        if (pong && pulse_.OnPong(*pong, NowUs())) PublishPulse(NowUs());
        return;
    }
    const std::lock_guard<std::mutex> lock(routeMutex_);
    std::vector<std::weak_ptr<HostLinkChannel>>& lane = routes_[size_t(header->chan)];
    for (auto at = lane.begin(); at != lane.end();) {
        if (const std::shared_ptr<HostLinkChannel> channel = at->lock()) {
            channel->Push(std::vector<uint8_t>(message.begin(), message.end()));
            ++at;
        } else {
            at = lane.erase(at);
        }
    }
}

}
