#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/FrameGate.h"

#include <cstdio>

using namespace deskhub;

namespace {

void TestUngated() {
    std::printf("[gate] fps 0 means every frame is admitted...\n");
    FrameGate gate;
    Check(gate.Admit(0, 1000), "first frame passes");
    Check(gate.Admit(0, 1001), "a frame 1us later still passes");
}

void TestFirstFrameAlwaysPasses() {
    std::printf("[gate] the first frame is never dropped...\n");
    FrameGate gate;
    Check(gate.Admit(30, 5'000'000), "no previous frame -> admitted");
    Check(!gate.Admit(30, 5'010'000), "and it becomes the reference");
}

void TestDropsTooEarly() {
    std::printf("[gate] a frame arriving early for the target fps is dropped...\n");
    FrameGate gate;
    Check(gate.Admit(30, 0), "reference frame");
    Check(!gate.Admit(30, 10'000), "10ms after at 30fps (33.3ms budget) -> dropped");
    Check(gate.Admit(30, 33'333), "a full period later -> admitted");
    Check(!gate.Admit(30, 43'333), "the reference advances");
}

void TestJitterTolerance() {
    std::printf("[gate] a frame a hair early still counts, so 60fps does not halve...\n");
    FrameGate gate;
    Check(gate.Admit(60, 0), "reference frame");
    Check(gate.Admit(60, 16'300), "16.3ms at 60fps (16.6ms budget) is within tolerance");

    FrameGate strict;
    Check(strict.Admit(60, 0), "reference frame");
    Check(!strict.Admit(60, 15'000), "15ms is more than the tolerance early -> dropped");
}

void TestNonMonotonicTimestamps() {
    std::printf("[gate] a timestamp that goes backwards is admitted, not swallowed...\n");
    FrameGate gate;
    Check(gate.Admit(30, 1'000'000), "reference frame");
    Check(gate.Admit(30, 900'000), "an older timestamp passes instead of wedging the gate");
    Check(!gate.Admit(30, 910'000), "and it resets the reference");
}

void TestReset() {
    std::printf("[gate] Reset drops the reference so the next frame passes...\n");
    FrameGate gate;
    Check(gate.Admit(30, 0), "reference frame");
    Check(!gate.Admit(30, 1'000), "next frame is too early");
    gate.Reset();
    Check(gate.Admit(30, 1'000), "after Reset the same frame is admitted");
}

void TestZeroTimestampIsAReference() {
    std::printf("[gate] a timestamp of exactly 0 still counts as a reference...\n");
    FrameGate gate;
    Check(gate.Admit(30, 0), "the frame at t=0 is admitted");
    Check(!gate.Admit(30, 1'000), "so the next frame 1ms later is dropped");
}

uint32_t AdmittedOverASecond(uint32_t targetFps, uint32_t captureFps) {
    FrameGate gate;
    const uint64_t stepUs = 1'000'000ull / captureFps;
    uint32_t admitted = 0;
    for (uint32_t i = 0; i < captureFps; ++i)
        if (gate.Admit(targetFps, uint64_t(i) * stepUs)) ++admitted;
    return admitted;
}

void TestAwkwardCaptureRateStillMeetsTheTarget() {
    std::printf("[gate] a capture rate that is not a multiple of the target still meets it...\n");
    Check(AdmittedOverASecond(30, 40) == 30, "40fps in, 30fps target -> 30 admitted, not 20");
    Check(AdmittedOverASecond(30, 45) == 30, "45fps in -> 30 admitted");
    Check(AdmittedOverASecond(60, 90) == 60, "90fps in, 60fps target -> 60 admitted");
    Check(AdmittedOverASecond(30, 60) == 30, "an exact multiple is unaffected");
    Check(AdmittedOverASecond(30, 30) == 30, "a capture already at the target passes whole");
}

void TestSlowCaptureIsNeverDecimated() {
    std::printf("[gate] a capture slower than the target loses nothing...\n");
    Check(AdmittedOverASecond(30, 10) == 10, "10fps in, 30fps target -> all 10 admitted");
    Check(AdmittedOverASecond(60, 25) == 25, "25fps in, 60fps target -> all 25 admitted");
}

void TestIdleTimeBanksNoBurst() {
    std::printf("[gate] a quiet spell does not buy a burst afterwards...\n");
    FrameGate gate;
    Check(gate.Admit(30, 0), "reference frame");
    Check(gate.Admit(30, 5'000'000), "a frame after five idle seconds is admitted");
    Check(!gate.Admit(30, 5'001'000), "but the one 1ms behind it is still too early");
    Check(gate.Admit(30, 5'033'333), "and the cadence resumes from there");
}

}

void RunFrameGateTests() {
    TestUngated();
    TestFirstFrameAlwaysPasses();
    TestDropsTooEarly();
    TestJitterTolerance();
    TestNonMonotonicTimestamps();
    TestReset();
    TestZeroTimestampIsAReference();
    TestAwkwardCaptureRateStillMeetsTheTarget();
    TestSlowCaptureIsNeverDecimated();
    TestIdleTimeBanksNoBurst();
}
