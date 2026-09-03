#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/SourcePipelineState.h"
#include "deskhubp/host/EncoderRecovery.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using deskhub::SourcePipelineState;
using deskhub::media::EncoderRecoveryCaps;
using deskhub::media::RecoveryPolicy;
using deskhubp::PrepareRecovery;

namespace {

constexpr uint32_t kStartBps = 20'000'000;
constexpr uint32_t kMinBps = 1'000'000;

struct CapableEncoder {
    std::vector<uint32_t> marked;
    std::vector<uint32_t> invalidated;
    std::vector<uint32_t> refreshed;
    bool acceptInvalidate = true;
    bool acceptRefresh = true;

    bool MarkLongTermReference(uint32_t frameId) {
        marked.push_back(frameId);
        return true;
    }
    bool InvalidateReference(uint32_t firstInvalidFrameId) {
        if (!acceptInvalidate) return false;
        invalidated.push_back(firstInvalidFrameId);
        return true;
    }
    bool BeginIntraRefresh(uint32_t frames) {
        if (!acceptRefresh) return false;
        refreshed.push_back(frames);
        return true;
    }
};

struct PlainEncoder {};

static_assert(deskhub::media::ReferenceInvalidatingEncoder<CapableEncoder>,
    "the test double has to satisfy the same concept a real encoder does");
static_assert(deskhub::media::IntraRefreshEncoder<CapableEncoder>,
    "the test double has to satisfy the same concept a real encoder does");
static_assert(!deskhub::media::ReferenceInvalidatingEncoder<PlainEncoder>,
    "an encoder without the methods must not satisfy the concept");

void TestAnEncoderThatCanInvalidateNeedsNoIdr() {
    std::printf("[recovery] a capable encoder patches loss without spending an IDR...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{true, false});
    CapableEncoder encoder;

    st.invalidateBeforeFrame.store(91);
    const bool idr = PrepareRecovery(st, encoder, 120, false);

    Check(!idr, "the frame stays a P-frame");
    Check(encoder.invalidated.size() == 1 && encoder.invalidated[0] == 91,
        "the encoder is told which frame is the first one it may no longer reference");
    Check(st.invalidateBeforeFrame.load() == 0, "and the request is consumed, not repeated");
}

void TestAnEncoderThatRefusesFallsBackToIdr() {
    std::printf("[recovery] when the encoder cannot honour it, recovery becomes an IDR...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{true, false});
    CapableEncoder encoder;
    encoder.acceptInvalidate = false;

    st.invalidateBeforeFrame.store(91);
    Check(PrepareRecovery(st, encoder, 120, false),
        "a refused invalidate must not leave the loss unpatched");
}

void TestAnEncoderWithoutTheMethodsFallsBackToIdr() {
    std::printf("[recovery] a backend with no long-term references still recovers...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    PlainEncoder encoder;

    st.invalidateBeforeFrame.store(91);
    Check(PrepareRecovery(st, encoder, 120, false),
        "the caller falls back to an IDR rather than dropping the request on the floor");

    st.wantIntraRefresh.store(true);
    Check(PrepareRecovery(st, encoder, 121, false), "the same holds for an intra refresh");
}

void TestIntraRefreshIsAskedForWithAPeriod() {
    std::printf("[recovery] an intra refresh carries the length the policy chose...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{false, true});
    CapableEncoder encoder;

    st.wantIntraRefresh.store(true);
    const bool idr = PrepareRecovery(st, encoder, 120, false);

    Check(!idr, "a refresh that the encoder accepts needs no IDR");
    Check(encoder.refreshed.size() == 1 &&
              encoder.refreshed[0] == RecoveryPolicy::kIntraRefreshFrames,
        "the encoder gets the same frame count the policy counts down");
    Check(!st.wantIntraRefresh.load(), "and the request is consumed");
}

void TestLongTermReferencesAreMarkedOnTheCadenceThePolicyExpects() {
    std::printf("[recovery] the frames the policy will later name are the ones marked...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{true, false});
    CapableEncoder encoder;

    for (uint32_t frameId = 0; frameId < 3 * RecoveryPolicy::kLongTermIntervalFrames; ++frameId)
        PrepareRecovery(st, encoder, frameId, false);

    Check(encoder.marked.size() == 3, "one mark per long-term interval, no more");
    Check(encoder.marked[0] == 0 && encoder.marked[1] == RecoveryPolicy::kLongTermIntervalFrames &&
              encoder.marked[2] == 2 * RecoveryPolicy::kLongTermIntervalFrames,
        "and they land on exactly the frames RecoveryPolicy::ShouldMarkLongTerm names");
}

void TestAnIdrIsAlwaysMarked() {
    std::printf("[recovery] an IDR becomes a long-term reference the host can fall back to...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{true, false});
    CapableEncoder encoder;

    PrepareRecovery(st, encoder, 7, true);

    Check(encoder.marked.size() == 1 && encoder.marked[0] == 7,
        "otherwise core records a long-term frame the encoder never kept");
}

void TestNothingPendingTouchesNothing() {
    std::printf("[recovery] a quiet frame costs no encoder calls...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    st.recovery.SetCaps(EncoderRecoveryCaps{true, true});
    CapableEncoder encoder;

    Check(!PrepareRecovery(st, encoder, 7, false), "a P-frame stays a P-frame");
    Check(encoder.marked.empty() && encoder.invalidated.empty() && encoder.refreshed.empty(),
        "and no recovery call is made at all");
}

void TestCapsOffMeansNoMarking() {
    std::printf("[recovery] with caps cleared the encoder is left alone...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    CapableEncoder encoder;

    for (uint32_t frameId = 0; frameId <= RecoveryPolicy::kLongTermIntervalFrames; ++frameId)
        PrepareRecovery(st, encoder, frameId, false);

    Check(encoder.marked.empty(),
        "a source whose encoder never declared long-term support marks nothing");
}

}

void RunEncoderRecoveryTests() {
    std::printf("--- host: turning a RecoveryPolicy decision into encoder calls ---\n");
    TestAnEncoderThatCanInvalidateNeedsNoIdr();
    TestAnEncoderThatRefusesFallsBackToIdr();
    TestAnEncoderWithoutTheMethodsFallsBackToIdr();
    TestIntraRefreshIsAskedForWithAPeriod();
    TestLongTermReferencesAreMarkedOnTheCadenceThePolicyExpects();
    TestAnIdrIsAlwaysMarked();
    TestNothingPendingTouchesNothing();
    TestCapsOffMeansNoMarking();
}
