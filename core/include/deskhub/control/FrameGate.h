#pragma once
#include <atomic>
#include <cstdint>

namespace deskhub {

class FrameGate {
public:
    bool Admit(uint32_t fps, uint64_t timestampUs);

    void Reset() {
        lastUs_.store(0, std::memory_order_relaxed);
        nextDueUs_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr uint64_t kJitterToleranceUs = 500;

    std::atomic<uint64_t> lastUs_{0};
    std::atomic<uint64_t> nextDueUs_{0};
};

}
