#include "deskhub/control/BitrateController.h"

namespace deskhub {

BitrateDecision BitrateController::Update(const Feedback& fb, uint64_t nowUs) {
    BitrateDecision d;

    const bool fecBefore = fec_;
    if (fb.lossPct >= 1) {
        cleanSeconds_ = 0;
        fec_ = true;
    } else if (++cleanSeconds_ >= kCleanSecondsBeforeDroppingFec) {
        fec_ = false;
    }
    d.fecEnabled = fec_;
    d.fecToggled = (fec_ != fecBefore);

    uint32_t next = cur_;
    if (fb.lossPct >= 5) {
        next = cur_ - cur_ / 4;
        lastDecreaseUs_ = nowUs;
    } else if (fb.lossPct >= 2) {
        next = cur_ - cur_ / 10;
        lastDecreaseUs_ = nowUs;
    } else if (fb.lossPct <= 1 && nowUs - lastDecreaseUs_ > 2'000'000) {
        next = cur_ + max_ / 20;
    }

    if (next > max_) next = max_;
    if (next < min_) next = min_;

    const uint32_t delta = next > cur_ ? next - cur_ : cur_ - next;
    d.changeBitrate = (next != cur_) && (delta >= cur_ / 50);
    d.bitrateBps = d.changeBitrate ? next : cur_;
    return d;
}

}
