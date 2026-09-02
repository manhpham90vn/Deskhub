#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/LinkPulse.h"
#include "deskhubp/net/SessionTransport.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace deskhubp {

enum class HostLinkState : uint8_t {
    Idle = 0,
    Connecting = 1,
    Deciding = 2,
    Authing = 3,
    Ready = 4,
    Recovering = 5,
    Refused = 6,
    Failed = 7,
    Ended = 8,
};

struct HostLinkConfig {
    NetAddr host{};
    std::string hostLabel{};
    std::string passcode{};
    std::string clientName{};
    bool recoverLink = false;
    bool trustGate = true;
    uint64_t recoverGraceUs = 0;
    uint32_t connectTimeoutMs = 10'000;
    uint32_t authTimeoutMs = 65'000;
    uint32_t recvWaitMs = 5;
    VideoPath videoPath = VideoPath::QuicDatagram;
};

struct HostLinkCallbacks {
    std::function<void(HostLinkState state, std::string_view message)> onState;
    std::function<void(deskhub::TrustVerdict, std::string_view fingerprint)> onTrustAsked;
    std::function<void(bool resumed)> onReady;
    std::function<void()> onLinkLost;
    std::function<void(uint64_t streamId)> onStreamBroken;
    std::function<void(const deskhub::LinkPulseView&)> onPulse;
};

class HostLinkChannel {
public:
    void Push(std::vector<uint8_t> bytes);
    std::optional<std::vector<uint8_t>> Poll();
    bool WaitWork(uint32_t timeoutMs);
    void Kick();
    size_t Pending() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> inbox_;
    bool kicked_ = false;
};

class HostLink {
public:
    HostLink() = default;
    ~HostLink();
    HostLink(const HostLink&) = delete;
    HostLink& operator=(const HostLink&) = delete;

    bool Start(const HostLinkConfig& config, HostLinkCallbacks callbacks);
    void Stop();

    std::shared_ptr<HostLinkChannel> Open(std::initializer_list<deskhub::Chan> chans);

    void AcceptFingerprint();
    void RejectFingerprint();

    void RequestRedial();
    deskhub::LinkPulseView Pulse() const;

    bool Send(std::span<const uint8_t> message);
    bool SendRecordOn(uint64_t streamId, std::span<const uint8_t> message);

    HostLinkState State() const {
        return state_.load(std::memory_order_acquire);
    }
    bool Running() const {
        return running_.load(std::memory_order_acquire);
    }
    bool Settled() const;
    std::string Message() const;
    std::string FingerprintText() const;
    deskhub::TrustVerdict Verdict() const;
    deskhub::AuthResultCode AuthCode() const {
        return authCode_.load(std::memory_order_acquire);
    }
    const HostLinkConfig& Config() const {
        return config_;
    }

private:
    enum class TrustDecision : int { Pending = 0,
        Accepted = 1,
        Rejected = 2 };

    void Loop();
    bool DialAndAdmit();
    bool AwaitEstablished();
    bool SettleTrust();
    bool RunAuth();
    void PumpReady();
    void Route(std::span<const uint8_t> message);
    void SetState(HostLinkState state, std::string_view message);
    void Fail(HostLinkState state, std::string_view message);
    void NoteSent();
    void SendLinkPing(uint64_t nowUs);
    void PublishPulse(uint64_t nowUs);

    HostLinkConfig config_{};
    HostLinkCallbacks cb_{};
    SessionTransport sock_{};
    deskhub::Fingerprint fingerprint_{};
    deskhub::TrustVerdict verdict_ = deskhub::TrustVerdict::Unknown;
    std::string message_{};

    std::array<std::vector<std::weak_ptr<HostLinkChannel>>, deskhub::kChanCount> routes_{};
    mutable std::mutex routeMutex_;

    mutable std::mutex mutex_;
    std::thread thread_{};
    std::atomic<HostLinkState> state_{HostLinkState::Idle};
    std::atomic<deskhub::AuthResultCode> authCode_{deskhub::AuthResultCode::NotPaired};
    std::atomic<int> trustDecision_{int(TrustDecision::Pending)};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> peerGone_{false};
    std::atomic<bool> autoTrustPending_{false};
    std::atomic<bool> redial_{false};
    std::atomic<uint64_t> lastSendUs_{0};
    uint64_t keepaliveIntervalUs_ = 0;
    uint64_t linkLostAtUs_ = 0;
    uint32_t redialAttempts_ = 0;
    deskhub::LinkPulse pulse_{};
    deskhub::LinkPulseView pulseView_{};
};

}
