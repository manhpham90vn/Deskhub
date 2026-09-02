#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/RecoveryPolicy.h"

#include <cstdio>

using namespace deskhub::media;

namespace {

void EncodeRun(RecoveryPolicy& policy, uint32_t from, uint32_t to) {
    for (uint32_t frameId = from; frameId <= to; ++frameId)
        policy.NoteEncoded(frameId, frameId == 0, policy.ShouldMarkLongTerm(frameId));
}

void TestAnEncoderWithNoHelpAsksForAKeyframe() {
    std::printf("[recovery] an encoder that can do nothing clever still asks for an IDR...\n");

    RecoveryPolicy policy;
    Check(!policy.caps().longTermReference && !policy.caps().intraRefresh,
        "the default caps claim nothing, which is what every backend reports today");

    EncodeRun(policy, 0, 90);
    const RecoveryDecision d = policy.OnReferenceLost(91);
    Check(d.action == RecoveryAction::Keyframe,
        "with no long-term reference and no intra refresh there is nothing else to ask for");
    Check(!policy.haveLongTerm(),
        "and a keyframe throws away whatever reference state was being tracked");
}

void TestLongTermReferenceIsPreferredOverAKeyframe() {
    std::printf("[recovery] a backend with long-term references falls back instead of "
                "re-sending everything...\n");

    RecoveryPolicy policy(EncoderRecoveryCaps{true, false});
    EncodeRun(policy, 0, 90);
    Check(policy.haveLongTerm(), "marked frames become the reference to fall back to");
    Check(policy.newestLongTerm() == 90, "and the newest one wins");

    const RecoveryDecision d = policy.OnReferenceLost(95);
    Check(d.action == RecoveryAction::InvalidateReference,
        "losing a frame after the reference costs an invalidation, not a whole picture");
    Check(d.referenceFrameId == 90, "and it names the reference the encoder should fall back to");
}

void TestAReferenceOlderThanTheLossIsTheOnlySafeOne() {
    std::printf("[recovery] a reference newer than the loss is not usable...\n");

    RecoveryPolicy policy(EncoderRecoveryCaps{true, false});
    EncodeRun(policy, 0, 90);

    const RecoveryDecision d = policy.OnReferenceLost(30);
    Check(d.action == RecoveryAction::Keyframe,
        "the newest reference is newer than the frame that was lost, so falling back to it "
        "would encode against something the viewer may never have decoded");
}

void TestASecondLossWhileRecoveringEscalates() {
    std::printf("[recovery] a second report before the first recovery lands escalates...\n");

    RecoveryPolicy policy(EncoderRecoveryCaps{true, false});
    EncodeRun(policy, 0, 90);

    Check(policy.OnReferenceLost(95).action == RecoveryAction::InvalidateReference,
        "the first report is answered cheaply");
    Check(policy.OnReferenceLost(96).action == RecoveryAction::Keyframe,
        "a second report arriving before any frame has been encoded means the cheap answer "
        "did not work, so it escalates rather than looping on it");
}

void TestIntraRefreshIsUsedWhenThereIsNoReference() {
    std::printf("[recovery] intra refresh covers a backend with no long-term reference...\n");

    RecoveryPolicy policy(EncoderRecoveryCaps{false, true});
    EncodeRun(policy, 0, 90);

    const RecoveryDecision d = policy.OnReferenceLost(95);
    Check(d.action == RecoveryAction::IntraRefresh,
        "a rolling refresh is still cheaper than a full picture");
    Check(policy.refreshing(), "and the policy remembers a refresh is under way");

    policy.NoteEncoded(96, false, false);
    Check(policy.OnReferenceLost(97).action == RecoveryAction::None,
        "another loss during the refresh asks for nothing - the refresh already covers it");

    for (uint32_t frameId = 97; frameId < 97 + RecoveryPolicy::kIntraRefreshFrames; ++frameId)
        policy.NoteEncoded(frameId, false, false);
    Check(!policy.refreshing(), "the refresh finishes after its own length");
}

void TestMarkingOnlyHappensWhenTheBackendCanUseIt() {
    std::printf("[recovery] frames are only marked when the backend can act on the mark...\n");

    RecoveryPolicy plain;
    Check(!plain.ShouldMarkLongTerm(0) && !plain.ShouldMarkLongTerm(30),
        "a backend without long-term references is never asked to mark one");

    RecoveryPolicy capable(EncoderRecoveryCaps{true, false});
    Check(capable.ShouldMarkLongTerm(0) && capable.ShouldMarkLongTerm(30),
        "a capable backend marks on the interval");
    Check(!capable.ShouldMarkLongTerm(31), "and not in between");

    capable.SetCaps(EncoderRecoveryCaps{});
    Check(!capable.haveLongTerm(),
        "changing what the backend can do throws away state gathered under the old answer");
}

}

void RunRecoveryPolicyTests() {
    TestAnEncoderWithNoHelpAsksForAKeyframe();
    TestLongTermReferenceIsPreferredOverAKeyframe();
    TestAReferenceOlderThanTheLossIsTheOnlySafeOne();
    TestASecondLossWhileRecoveringEscalates();
    TestIntraRefreshIsUsedWhenThereIsNoReference();
    TestMarkingOnlyHappensWhenTheBackendCanUseIt();
}
