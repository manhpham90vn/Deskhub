#pragma once
#include <cstddef>
#include <cstdint>

namespace deskhub {

inline constexpr uint64_t kPacerMinSleepUs = 500;
inline constexpr uint64_t kPacerMaxBacklogUs = 100'000;
inline constexpr uint32_t kPacingRateMultiple = 2;

class Pacer {
public:
    void SetRateBps(uint64_t rateBps) {
        rateBps_ = rateBps;
        if (!rateBps_) nextUs_ = 0;
    }

    void Reset() {
        nextUs_ = 0;
    }

    uint64_t Gate(size_t bytes, uint64_t nowUs) {
        if (!rateBps_ || !bytes) return 0;
        if (nextUs_ < nowUs) nextUs_ = nowUs;
        if (nextUs_ - nowUs > kPacerMaxBacklogUs) nextUs_ = nowUs + kPacerMaxBacklogUs;
        const uint64_t waitUs = nextUs_ - nowUs;
        nextUs_ += uint64_t(bytes) * 8 * 1'000'000ull / rateBps_;
        return waitUs >= kPacerMinSleepUs ? waitUs : 0;
    }

private:
    uint64_t rateBps_ = 0;
    uint64_t nextUs_ = 0;
};

}
