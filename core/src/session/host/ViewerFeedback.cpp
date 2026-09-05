#include "deskhub/session/host/ViewerFeedback.h"

namespace deskhub {

FeedbackOutcome ApplyFeedback(SourcePipelineState& st, const Feedback& fb, uint64_t nowUs,
    const FeedbackHooks& hooks) {
    st.uiRttMs.store(fb.rttMs, std::memory_order_relaxed);
    st.uiLossPct.store(fb.lossPct, std::memory_order_relaxed);
    st.uiRecvKbps.store(fb.recvBitrateKbps, std::memory_order_relaxed);
    st.haveFeedback.store(true, std::memory_order_release);

    FeedbackOutcome out;
    const BitrateDecision d = st.rate->Update(fb, st.frameAgeMs.TakeReset(), nowUs);

    if (st.fecParityPin.load(std::memory_order_relaxed) == 0)
        st.wantFecParity.store(d.fecParityPerGroup, std::memory_order_relaxed);

    const bool fecPinned = st.fecArmedAlways.load(std::memory_order_relaxed) ||
                           st.fecArmedNever.load(std::memory_order_relaxed);
    if (d.fecToggled && !fecPinned) {
        st.wantFec.store(d.fecEnabled, std::memory_order_relaxed);
        out.fecToggled = true;
        out.fecEnabled = d.fecEnabled;
    }

    if (d.changeBitrate) {
        const uint32_t previous = st.rate->bitrateBps();
        if (hooks.setEncoderBitrate && hooks.setEncoderBitrate(d.bitrateBps)) {
            st.rate->CommitBitrate(d.bitrateBps);
            st.curBitrateBps.store(d.bitrateBps, std::memory_order_relaxed);
            out.bitrateChanged = true;
            out.previousBitrateBps = previous;
            out.bitrateBps = d.bitrateBps;
        }
    }

    if (!st.ladder) return out;
    if (!st.ladder->Update(st.rate->bitrateBps(), nowUs)) return out;

    out.previousStep = st.step;
    st.step = st.ladder->current();
    st.curFps.store(st.step.fps, std::memory_order_relaxed);
    out.step = st.step;
    out.qualityChanged = true;
    if (hooks.applyQualityStep) out.size = hooks.applyQualityStep(out.previousStep, st.step);
    st.qualityChanged.store(true, std::memory_order_release);
    return out;
}

void RespondToNack(SourcePipelineState& st, uint32_t frameId, std::span<const uint16_t> indices,
    const std::function<void(std::span<const uint8_t>)>& sendToRequester) {
    if (!ViewerCountOf(st) || !sendToRequester) return;

    std::lock_guard<std::mutex> lk(st.retxMutex);
    for (uint16_t idx : indices) {
        const auto d = st.retxCache.Find(frameId, idx);
        if (!d.empty()) sendToRequester(d);
    }
}

void ForgetViewers(SourcePipelineState& st) {
    std::lock_guard<std::mutex> lk(st.retxMutex);
    st.retxCache.Reset();
}

}
