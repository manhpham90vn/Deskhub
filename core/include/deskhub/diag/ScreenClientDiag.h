#pragma once
#include <cstddef>
#include <cstdint>

#include "deskhub/control/LinkStats.h"
#include "deskhub/diag/WindowStat.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/transport/Reassembler.h"

namespace deskhub::diag {

struct ScreenClientDiagCaps {
    bool presentMs = false;
    bool dispDrop = false;
};

class ScreenClientDiag {
public:
    static constexpr size_t kSumBufBytes = 704;
    static constexpr size_t kStatusBufBytes = 256;

    explicit ScreenClientDiag(ScreenClientDiagCaps caps = {}) : caps_(caps) {}

    WindowStat asmMs;
    WindowStat decMs;
    WindowStat presentMs;
    WindowCount dqDrop;
    WindowCount dispDrop;
    WindowMax loopBusyMs;
    RunningMin minRttUs;

    const char* FormatSum(char* buf, size_t cap, const char* hms, const LinkWindow& w,
        uint32_t gapMsMax, int64_t e2eUs, int64_t absoluteE2eUs = -1);

    static const char* FormatStatus(char* buf, size_t cap, const char* hms, const LinkWindow& w,
        uint32_t rttUs, int64_t e2eUs);

    static const char* FormatCompact(char* buf, size_t cap, const LinkWindow& w, uint32_t rttUs,
        int64_t e2eUs, const char* sep = "  ");

    static const char* FormatFrameDrop(char* buf, size_t cap,
        const Reassembler::FrameDropInfo& d);

    static constexpr size_t kFrameDropBufBytes = 192;
    static constexpr size_t kCompactBufBytes = 160;

private:
    ScreenClientDiagCaps caps_;
};

using KeyframeReason = deskhub::KeyframeReason;
using deskhub::kKeyframeReasonCount;

const char* KeyframeReasonName(KeyframeReason reason);

class KeyframeRequestLog {
public:
    static constexpr size_t kBufBytes = 96;
    static constexpr size_t kCountsBufBytes = 224;

    const char* Request(char* buf, size_t cap, uint64_t nowUs, KeyframeReason reason);

    const char* Arrived(char* buf, size_t cap, uint64_t nowUs, size_t idrBytes);

    const char* FormatCounts(char* buf, size_t cap);

    bool pending() const {
        return reqUs_ != 0;
    }

private:
    uint64_t reqUs_ = 0;
    uint32_t counts_[kKeyframeReasonCount] = {};
};

}
