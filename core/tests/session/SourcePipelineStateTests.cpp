#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/SourcePipelineState.h"

#include <cstdio>
#include <type_traits>

using namespace deskhub;

namespace {

constexpr uint32_t kStartBps = 20'000'000;
constexpr uint32_t kMinBps = 1'000'000;

void TestAFreshSourceIsReadyToStream() {
    std::printf("[pipeline] a source starts in the state that lets frames flow...\n");
    SourcePipelineState st(kStartBps, kMinBps);

    Check(!st.paused.load(), "a new source is not paused, or nothing would ever be sent");
    Check(!st.failed.load(), "a new source is not failed");
    Check(!st.shutdownDone, "a new source has not been shut down");
    Check(!st.netReady.load(), "but it is not on the wire until the capture side says so");
    Check(st.wantFec.load(), "FEC is armed up front, before any loss has been measured");
    Check(!st.forceIdr.load(), "no keyframe is owed before a client has asked for one");
    Check(!st.haveFeedback.load(), "no link numbers are claimed before the client reports any");
    Check(st.replyPacked.load() == 0, "there is no reply address until a datagram arrives");
    Check(ViewerCountOf(st) == 0, "and nobody is watching yet");
    Check(!st.session, "the session object is attached by the owner, not invented here");
    Check(!st.ladder, "the quality ladder is built once the client size is known");
}

void TestTheStartingBitrateIsTheOneThatWasAskedFor() {
    std::printf("[pipeline] the encoder and the controller start on the same number...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Check(st.curBitrateBps.load() == kStartBps,
        "the bitrate the UI shows is the bitrate the encoder was configured with");
    Check(st.rate->bitrateBps() == kStartBps,
        "the controller starts from the same number, so the first feedback is not a jump");
    Check(st.rate->fecEnabled(), "and it agrees with wantFec that FEC is armed");
}

void TestCountersStartAtZero() {
    std::printf("[pipeline] the per-source counters start empty, never with stale values...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Check(st.bytesSent.load() == 0 && st.framesSent.load() == 0 && st.captured.load() == 0,
        "nothing has been captured or sent yet");
    Check(st.nextFrameId.load() == 0, "frame ids start at 0, as the receiver expects");
    Check(st.lastFrameUs.load() == 0 && st.lastKeepaliveUs == 0,
        "no send timestamps are claimed before the first frame");
    Check(st.uiRttMs.load() == 0 && st.uiLossPct.load() == 0 && st.uiRecvKbps.load() == 0,
        "the UI shows nothing rather than a made-up link quality");
    Check(st.srcW.load() == 0 && st.srcH.load() == 0 && st.nativeW.load() == 0 &&
              st.nativeH.load() == 0 && st.wantW.load() == 0 && st.wantH.load() == 0,
        "no size is assumed before the capture reports one");
    Check(st.cliW == 0 && st.cliH == 0, "and none is assumed for the client either");
}

void TestDiagCapabilitiesAreCarriedThrough() {
    std::printf("[pipeline] what the backend can do is recorded once, at construction...\n");
    SourcePipelineState plain(kStartBps, kMinBps);
    char buf[diag::SourceDiag::kSumBufBytes];
    const char* line = plain.diag.FormatSum(buf, sizeof(buf), "00:00:00", "src", 0, false);
    Check(line && *line, "a source with no special capability still produces a diag line");

    SourcePipelineState fancy(kStartBps, kMinBps, diag::ShareDiagCaps{true, true});
    line = fancy.diag.FormatSum(buf, sizeof(buf), "00:00:00", "src", 3, true);
    Check(line && *line, "so does one that reports idle frames and zero-copy");
}

void TestASourceCannotBeCopiedByAccident() {
    std::printf(
        "[pipeline] the state holds a mutex and threads — copying it must not "
        "compile...\n");
    Check(!std::is_copy_constructible_v<SourcePipelineState>, "no copy construction");
    Check(!std::is_copy_assignable_v<SourcePipelineState>, "no copy assignment");
    Check(std::has_virtual_destructor_v<SourcePipelineState>,
        "clients derive from it, so deleting through the base must run their destructor");
}

}

void RunSourcePipelineStateTests() {
    TestAFreshSourceIsReadyToStream();
    TestTheStartingBitrateIsTheOneThatWasAskedFor();
    TestCountersStartAtZero();
    TestDiagCapabilitiesAreCarriedThrough();
    TestASourceCannotBeCopiedByAccident();
}
