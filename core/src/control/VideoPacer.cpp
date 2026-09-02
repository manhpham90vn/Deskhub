#include "deskhub/control/VideoPacer.h"

namespace deskhub {

void VideoPacer::ObserveArrival(uint64_t ptsUs, uint64_t nowUs) {
    if (offset_->ready()) {
        const int64_t raw = int64_t(nowUs) - int64_t(ptsUs);
        const int64_t drift = raw - offset_->floorUs();
        if (drift > int64_t(kStreamJumpUs) || drift < -int64_t(kStreamJumpUs)) offset_->Reset();
    }
    offset_->AddSample(ptsUs, nowUs);

    const int64_t raw = int64_t(nowUs) - int64_t(ptsUs);
    if (haveRaw_) {
        const int64_t swing = raw - lastRawUs_;
        const uint64_t spread = uint64_t(swing < 0 ? -swing : swing);
        jitterUs_ = uint64_t(int64_t(jitterUs_) +
                             ((int64_t(spread) - int64_t(jitterUs_)) >> kJitterFilterShift));
    }
    lastRawUs_ = raw;
    haveRaw_ = true;

    if (!adaptive_) return;
    const uint64_t want = kMinLeadUs + jitterUs_ * kJitterLeadMultiple;
    leadUs_ = want < kMinLeadUs ? kMinLeadUs : (want > kMaxLeadUs ? kMaxLeadUs : want);
}

int64_t VideoPacer::DesiredTimebaseUs(uint64_t nowUs) const {
    return int64_t(nowUs) - offset_->floorUs() - int64_t(leadUs_);
}

uint64_t VideoPacer::DisplayTimeUs(uint64_t ptsUs, uint64_t nowUs) const {
    const int64_t at = int64_t(ptsUs) + offset_->floorUs() + int64_t(leadUs_);
    const uint64_t show = at > int64_t(nowUs) ? uint64_t(at) : nowUs;
    if (!displayIntervalUs_) return show;
    const uint64_t whole = (show + displayIntervalUs_ - 1) / displayIntervalUs_;
    return whole * displayIntervalUs_;
}

bool VideoPacer::NeedsResync(int64_t currentTimebaseUs, uint64_t nowUs) const {
    const int64_t diff = currentTimebaseUs - DesiredTimebaseUs(nowUs);
    return diff > int64_t(kResyncThresholdUs) || diff < -int64_t(kResyncThresholdUs);
}

}
