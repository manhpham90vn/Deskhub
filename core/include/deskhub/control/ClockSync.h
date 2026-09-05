#pragma once
#include <cstdint>

namespace deskhub {

class ClockSync {
public:
    static constexpr uint64_t kWindowUs = 10'000'000;

    void AddSample(uint64_t sentUs, uint64_t hostUs, uint64_t receivedUs);

    bool ready() const {
        return have_;
    }

    int64_t offsetUs() const {
        return offsetUs_;
    }

    uint64_t bestRttUs() const {
        return bestRttUs_;
    }

    int64_t AbsoluteLatencyUs(uint64_t hostPtsUs, uint64_t localUs) const;

    void Reset();

private:
    void Rotate(uint64_t nowUs);

    bool have_ = false;
    bool windowEmpty_ = true;
    uint64_t bestRttUs_ = 0;
    int64_t offsetUs_ = 0;
    uint64_t curBestRttUs_ = 0;
    int64_t curOffsetUs_ = 0;
    uint64_t windowStartUs_ = 0;
};

}
