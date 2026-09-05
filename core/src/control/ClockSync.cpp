#include "deskhub/control/ClockSync.h"

namespace deskhub {

void ClockSync::Reset() {
    have_ = false;
    windowEmpty_ = true;
    bestRttUs_ = 0;
    offsetUs_ = 0;
    curBestRttUs_ = 0;
    curOffsetUs_ = 0;
    windowStartUs_ = 0;
}

void ClockSync::Rotate(uint64_t nowUs) {
    while (nowUs - windowStartUs_ >= kWindowUs) {
        if (!windowEmpty_) {
            bestRttUs_ = curBestRttUs_;
            offsetUs_ = curOffsetUs_;
        }
        windowStartUs_ += kWindowUs;
        windowEmpty_ = true;
    }
}

void ClockSync::AddSample(uint64_t sentUs, uint64_t hostUs, uint64_t receivedUs) {
    if (!hostUs || receivedUs < sentUs) return;
    const uint64_t rttUs = receivedUs - sentUs;

    const int64_t midpoint = int64_t(sentUs) + int64_t(rttUs / 2);
    const int64_t offset = midpoint - int64_t(hostUs);

    if (!have_) {
        have_ = true;
        windowStartUs_ = receivedUs;
        bestRttUs_ = curBestRttUs_ = rttUs;
        offsetUs_ = curOffsetUs_ = offset;
        windowEmpty_ = false;
        return;
    }

    Rotate(receivedUs);

    if (windowEmpty_ || rttUs < curBestRttUs_) {
        curBestRttUs_ = rttUs;
        curOffsetUs_ = offset;
        windowEmpty_ = false;
    }
    if (rttUs < bestRttUs_) {
        bestRttUs_ = rttUs;
        offsetUs_ = offset;
    }
}

int64_t ClockSync::AbsoluteLatencyUs(uint64_t hostPtsUs, uint64_t localUs) const {
    if (!have_) return -1;
    const int64_t latency = int64_t(localUs) - offsetUs_ - int64_t(hostPtsUs);
    return latency < 0 ? 0 : latency;
}

}
