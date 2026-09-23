#pragma once
#include <cstdint>

namespace deskhub {

struct QualityStep {
    uint16_t width = 0, height = 0;
    uint8_t fps = 0;
    uint8_t scalePct = 100;

    friend bool operator==(const QualityStep& a, const QualityStep& b) {
        return a.width == b.width && a.height == b.height && a.fps == b.fps &&
               a.scalePct == b.scalePct;
    }
};

class QualityLadder {
public:
    static constexpr uint32_t kBppNum = 8, kBppDen = 100;

    static constexpr uint32_t kUpHeadroomPct = 120;
    static constexpr uint64_t kUpDwellUs = 5'000'000;
    static constexpr uint64_t kUpDwellResizeUs = 15'000'000;

    QualityLadder(uint16_t maxW, uint16_t maxH, uint8_t maxFps);

    bool Update(uint32_t bitrateBps, uint64_t nowUs);

    QualityStep current() const {
        return step_;
    }

    int rungCount() const {
        return count_;
    }
    int rung() const {
        return rung_;
    }

private:
    QualityStep StepAt(int rung) const;
    uint32_t RequiredBps(int rung) const;
    int BestRungFor(uint32_t bitrateBps) const;

    uint16_t maxW_, maxH_;
    uint8_t maxFps_;
    int count_ = 0;
    int rung_ = 0;
    QualityStep step_{};
    uint64_t lastChangeUs_ = 0;
    uint64_t upSinceUs_ = 0;
};

}
