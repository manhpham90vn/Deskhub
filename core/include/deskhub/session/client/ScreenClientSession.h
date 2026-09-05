#pragma once
#include "deskhub/input/InputSender.h"
#include "deskhub/control/ClockSync.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/ClipboardSync.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace deskhub {

inline constexpr uint64_t kHelloRetryUs = 500'000;
inline constexpr uint64_t kHelloGiveUpUs = 10'000'000;
inline constexpr uint64_t kPingIntervalUs = 1'000'000;
inline constexpr uint64_t kKeyframeRetryUs = 250'000;
inline constexpr uint64_t kFocusRetryUs = 50'000;
inline constexpr int kFocusRepeats = 3;

struct NegotiatedParams {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t fps = 60;
    uint32_t bitrateBps = 0;
    uint64_t timebaseUs = 0;
};

enum class ScreenSessionEnd : uint8_t { HostBye = 0,
    Timeout = 1,
    Rejected = 2,
    ConnectTimeout = 3 };

struct ScreenClientSessionCallbacks {
    std::function<void(std::span<const uint8_t>)> send;
    std::function<void(const NegotiatedParams&)> onReady;
    std::function<void(const NegotiatedParams&)> onReconfig;
    std::function<void(uint32_t rttUs)> onRtt;
    std::function<void(const char* reason, ScreenSessionEnd cause)> onDisconnect;
    std::function<void(std::string_view text)> onClipboardText;
};

class ScreenClientSession {
public:
    enum class State : uint8_t { Idle,
        Hello,
        Starting,
        Streaming,
        Dead };

    explicit ScreenClientSession(ScreenClientSessionCallbacks cb) : cb_(std::move(cb)) {}

    void Start(const Hello& hello, uint64_t nowUs);

    RejectReason rejectReason() const {
        return rejectReason_;
    }

    bool HandlePacket(std::span<const uint8_t> pkt, uint64_t nowUs);

    void NotifyVideoPacket(uint64_t nowUs);

    void Tick(uint64_t nowUs);

    void QueueInput(const InputEvent& e);

    void QueueClipboard(std::string_view text);

    void SetFocused(bool on);

    void RequestKeyframe(KeyframeReason reason) {
        if (!keyframeWanted_) keyframeReason_ = reason;
        keyframeWanted_ = true;
    }
    void CancelKeyframeRequest() {
        keyframeWanted_ = false;
    }

    void SendFeedback(const Feedback& fb);

    void SendNack(uint32_t frameId, std::span<const uint16_t> indices);

    void SendInvalidateRef(uint32_t frameId);

    void SendBye();

    State state() const {
        return state_;
    }
    uint32_t sessionId() const {
        return sessionId_;
    }
    const ClockSync& clockSync() const {
        return clockSync_;
    }

    uint32_t lastRttUs() const {
        return lastRttUs_;
    }
    const NegotiatedParams& params() const {
        return params_;
    }

private:
    void SendHello();
    void SendStart();
    void Die(const char* reason, ScreenSessionEnd cause);

    ScreenClientSessionCallbacks cb_;
    InputSender input_;
    ClipboardSync clip_;
    State state_ = State::Idle;
    uint32_t sessionId_ = 0;
    Hello hello_{};
    NegotiatedParams params_{};
    uint64_t startedUs_ = 0;
    uint64_t lastSentUs_ = 0;
    uint64_t lastRecvUs_ = 0;
    uint64_t lastPingUs_ = 0;
    uint64_t lastKeyframeReqUs_ = 0;
    uint64_t lastFocusUs_ = 0;
    int focusRepeatsLeft_ = 0;
    bool focusWanted_ = false;
    bool focusSent_ = false;
    uint32_t nextPingId_ = 1;
    uint32_t lastRttUs_ = 0;
    ClockSync clockSync_{};
    bool keyframeWanted_ = false;
    KeyframeReason keyframeReason_ = KeyframeReason::Unknown;
    RejectReason rejectReason_ = RejectReason::None;

    uint8_t buf_[kMaxDatagram] = {};
};

}
