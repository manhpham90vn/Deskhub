#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "deskhub/diag/WindowStat.h"
#include "deskhub/session/host/ScreenHostSession.h"

namespace deskhub::diag {

const char* StateName(ScreenHostSession::State s);

class SourceRate {
public:
    struct Window {
        double secs = 0.0;
        double captureFps = 0.0;
        double sendFps = 0.0;
        double sendKbps = 0.0;
    };

    Window Close(uint32_t captured, uint64_t framesSent, uint64_t bytesSent, uint64_t nowUs);

private:
    uint32_t lastCaptured_ = 0;
    uint64_t lastFrames_ = 0;
    uint64_t lastBytes_ = 0;
    uint64_t lastUs_ = 0;
};

struct ShareDiagCaps {
    bool capIdle = false;
    bool zerocopy = false;
    bool queueDrop = false;
};

class SourceDiag {
public:
    static constexpr size_t kSumBufBytes = 384;
    static constexpr size_t kStatusBufBytes = 384;
    static constexpr size_t kIdrBufBytes = 160;
    static constexpr size_t kKeyframeReqBufBytes = 320;

    explicit SourceDiag(ShareDiagCaps caps = {}) : caps_(caps) {}

    WindowStat encMs;
    WindowPercentile encUs;
    WindowStat encLatMs;
    WindowCount idr;
    WindowCount sendFail;
    WindowCount queueDrop;
    WindowMax burstMs;

    void LatchIdr(uint64_t bytes, uint32_t pkts, uint32_t burst);

    void CountKeyframeRequest(KeyframeReason reason);

    const char* FormatIdr(char* buf, size_t cap, const char* name);

    const char* FormatKeyframeRequests(char* buf, size_t cap, const char* name);

    const char* FormatSum(char* buf, size_t cap, const char* hms, const char* name,
        uint32_t capIdle, bool zerocopy);

    struct LinkView {
        bool have = false;
        uint32_t lossPct = 0;
        uint32_t rttMs = 0;
        uint32_t recvKbps = 0;
    };

    struct Window {
        SourceRate::Window rate;
        uint64_t inputApplied = 0;
        uint64_t inputLost = 0;
        uint64_t inputSkipped = 0;
    };

    static const char* FormatStatus(char* buf, size_t cap, const char* hms, const char* name,
        const char* state, const Window& w, const LinkView& link);

private:
    ShareDiagCaps caps_;
    std::atomic<uint64_t> idrBytes_{0};
    std::atomic<uint32_t> idrPkts_{0};
    std::atomic<uint32_t> idrBurstMs_{0};
    std::atomic<uint32_t> kfReq_[kKeyframeReasonCount] = {};
};

class ShareDiag {
public:
    static constexpr size_t kSumBufBytes = 160;

    WindowMax loopBusyMs;

    const char* FormatSum(char* buf, size_t cap, const char* hms, uint64_t datagramsSent,
        uint64_t datagramsRefused);

private:
    uint64_t lastDatagramsSent_ = 0;
    uint64_t lastDatagramsRefused_ = 0;
};

}
