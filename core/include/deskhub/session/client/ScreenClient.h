#pragma once
#include "deskhub/control/LinkStats.h"
#include "deskhub/diag/ScreenClientDiag.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/client/ScreenClientSession.h"
#include "deskhub/transport/Reassembler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace deskhub {

inline constexpr uint8_t kDefaultClientFps = 60;
inline constexpr uint32_t kLoopStallWarnMs = 50;
inline constexpr size_t kNackBatchMax = 64;

struct ScreenClientConfig {
    uint32_t clientId = 0;
    uint16_t maxWidth = 0;
    uint16_t maxHeight = 0;
    uint8_t sourceId = 0;
    uint8_t desiredFps = kDefaultClientFps;
    bool sendNacks = false;
    uint32_t overtakenLimit = 0;
    uint32_t stallTimeoutFrames = 0;
    bool wantsAudio = false;
    bool logLossRuns = false;
    const char* statusSeparator = "  ";
    std::string passcode{};
    std::string displayName{};
    std::string fecScheme{};
};

struct ScreenClientCallbacks {
    std::function<void(std::span<const uint8_t>)> send;
    std::function<void(Reassembler::Frame&&)> onFrame;
    std::function<void(const AudioPacketView&)> onAudioPacket;
    std::function<void(std::string_view text)> onClipboardText;
    std::function<void(const NegotiatedParams&, bool reconfigured)> onParams;
    std::function<void(const char* reason, ScreenSessionEnd cause)> onEnded;
    std::function<uint32_t()> takeRenderedCount;
    std::function<int64_t()> latencyUs;
    std::function<void(const char* compactStatus)> onStatus;
    std::function<std::string()> localTime;
    std::function<void(bool warn, const char* line)> log;
};

class ScreenClient {
public:
    ScreenClient(ScreenClientCallbacks cb, diag::ScreenClientDiag& diag);

    void Start(const ScreenClientConfig& cfg, uint64_t nowUs);

    void OnDatagram(std::span<const uint8_t> pkt, uint64_t nowUs);

    void PollFrames(uint64_t nowUs);

    void RequestKeyframe(diag::KeyframeReason reason, uint64_t nowUs);

    void PlanNacks(uint64_t nowUs);

    void QueueInput(const InputEvent& e) {
        session_.QueueInput(e);
    }
    void QueueClipboard(std::string_view text) {
        session_.QueueClipboard(text);
    }
    void SetFocused(bool on) {
        session_.SetFocused(on);
    }

    bool Tick(uint64_t nowUs);

    void CountLoopBusy(uint64_t startedUs, uint64_t nowUs);

    void SendBye() {
        session_.SendBye();
    }

    bool streaming() const {
        return session_.state() == ScreenClientSession::State::Streaming;
    }
    uint32_t lastRttUs() const {
        return session_.lastRttUs();
    }

private:
    ScreenClientSessionCallbacks MakeSessionCallbacks();
    void EnsureReassembler();
    void Report(uint64_t nowUs);
    void Log(const char* line) const {
        if (cb_.log) cb_.log(false, line);
    }
    void LogWarn(const char* line) const {
        if (cb_.log) cb_.log(true, line);
    }

    ScreenClientCallbacks cb_;
    diag::ScreenClientDiag& diag_;
    ScreenClientConfig cfg_{};
    ScreenClientSession session_;
    std::unique_ptr<Reassembler> reasm_;
    int64_t lastAbsoluteE2eUs_ = -1;
    LinkStats linkStats_;
    diag::KeyframeRequestLog kfLog_;
    uint64_t windowBytes_ = 0;
    char line_[diag::ScreenClientDiag::kSumBufBytes] = {};
};

}
