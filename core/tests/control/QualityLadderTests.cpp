#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/QualityLadder.h"

#include <cstdio>

using namespace deskhub;

namespace {

constexpr uint16_t kW = 1920, kH = 1246;
constexpr uint8_t kFps = 60;

uint64_t Hold(QualityLadder& q, uint32_t bps, int seconds, uint64_t t0) {
    uint64_t t = t0;
    for (int i = 0; i < seconds; ++i) {
        t += 1'000'000;
        q.Update(bps, t);
    }
    return t;
}

uint32_t RequiredBps(const QualityLadder& q) {
    uint32_t low = 0;
    uint32_t high = 500'000'000;
    while (low + 1 < high) {
        const uint32_t mid = low + (high - low) / 2;
        QualityLadder probe = q;
        probe.Update(mid, 0);
        if (probe.rung() > q.rung())
            low = mid;
        else
            high = mid;
    }
    return high;
}

void TestTopRungOnAHealthyLink() {
    std::printf("[quality] good link -> exactly the ceiling the user set...\n");
    QualityLadder q(kW, kH, kFps);
    Check(q.current() == QualityStep{kW, kH, kFps}, "starts at the ceiling rung");
    const uint32_t need = RequiredBps(q);
    Check(need < 20'000'000u, "the ceiling rung needs under 20 Mbps (the host default)");
    Hold(q, 20'000'000, 30, 0);
    Check(q.current() == QualityStep{kW, kH, kFps}, "20 Mbps: nothing changes");
}

void TestFpsGoesFirst() {
    std::printf("[quality] drop fps FIRST, keep the pixels...\n");
    QualityLadder q(kW, kH, kFps);
    q.Update(RequiredBps(q) - 1, 1'000'000);
    const QualityStep s = q.current();
    Check(s.width == kW && s.height == kH, "resolution unchanged");
    Check(s.fps < kFps, "fps went down");
}

void TestResolutionOnlyAfterFpsFloor() {
    std::printf("[quality] only shrink pixels AFTER fps hits the floor...\n");
    QualityLadder q(kW, kH, kFps);
    uint8_t fpsWhenShrunk = 0;
    for (uint32_t bps = 12'000'000; bps >= 500'000; bps -= 250'000) {
        q.Update(bps, 1'000'000);
        if (q.current().width < kW) {
            fpsWhenShrunk = q.current().fps;
            break;
        }
    }
    Check(fpsWhenShrunk != 0, "a pixel-shrinking rung exists");
    Check(fpsWhenShrunk <= 20, "by the time pixels shrink, fps is already at the floor of 20");
}

void TestNeverExceedsCeiling() {
    std::printf("[quality] never exceeds the ceiling, even with bandwidth to spare...\n");
    QualityLadder q(kW, kH, kFps);
    Hold(q, 500'000'000, 120, 0);
    Check(q.current() == QualityStep{kW, kH, kFps}, "still exactly the ceiling, never scales past it");
}

void TestDownIsImmediate() {
    std::printf("[quality] a drop is immediate, in a single Update...\n");
    QualityLadder q(kW, kH, kFps);
    Check(q.Update(1'500'000, 1'000'000), "a single Update already drops");
    Check(q.rung() > 0, "left the ceiling rung");
    Check(RequiredBps(q) <= 1'500'000, "the new rung fits what the link can carry");

    const int mid = q.rung();
    Check(q.Update(400'000, 2'000'000), "the next drop is immediate too");
    Check(q.rung() > mid, "moves to an even lower rung");
}

void TestUpNeedsHeadroom() {
    std::printf("[quality] stepping up needs 20%% headroom, exactly enough does not...\n");
    QualityLadder q(kW, kH, kFps);
    q.Update(1'500'000, 1'000'000);
    const int low = q.rung();

    Hold(q, RequiredBps(q), 60, 1'000'000);
    Check(q.rung() == low, "exactly enough, no headroom -> stays on the rung");
}

void TestFpsOnlyStepUpIsQuick() {
    std::printf("[quality] fps-only step up: 5 second dwell...\n");
    QualityLadder q(kW, kH, kFps);
    q.Update(4'000'000, 1'000'000);
    const QualityStep low = q.current();
    Check(low.width == kW, "still full pixels, only fps lowered");

    uint64_t t = Hold(q, 500'000'000, 4, 1'000'000);
    Check(q.current() == low, "4 seconds: dwell not met");
    t = Hold(q, 500'000'000, 3, t);
    Check(q.current().fps > low.fps, "past 5 seconds: fps goes back up");
    Check(q.current().width == kW, "and the pixels stay untouched");
}

void TestResizeStepUpWaitsLonger() {
    std::printf("[quality] resolution-changing step up: 15 second dwell...\n");
    QualityLadder q(kW, kH, kFps);
    q.Update(1'500'000, 1'000'000);
    const QualityStep low = q.current();
    Check(low.width < kW, "sitting on a pixel-shrunk rung");

    uint64_t t = Hold(q, 500'000'000, 10, 1'000'000);
    Check(q.current() == low, "10 seconds: not enough — the next rung changes resolution");
    t = Hold(q, 500'000'000, 8, t);
    Check(q.current().width > low.width, "past 15 seconds: resolution goes back up");
}

void TestUpOneRungAtATime() {
    std::printf("[quality] step up one rung at a time, never skipping...\n");
    QualityLadder q(kW, kH, kFps);
    q.Update(400'000, 1'000'000);
    int prev = q.rung();
    uint64_t t = 1'000'000;
    for (int i = 0; i < 120; ++i) {
        t += 1'000'000;
        if (q.Update(500'000'000, t)) {
            Check(q.rung() == prev - 1, "each step up moves exactly one rung");
            prev = q.rung();
        }
    }
    Check(q.rung() == 0, "eventually gets back to the ceiling rung");
}

void TestLowUserFpsCollapsesDuplicateRungs() {
    std::printf("[quality] a low fps ceiling collapses duplicate rungs...\n");
    QualityLadder q60(kW, kH, 60);
    QualityLadder q30(kW, kH, 30);
    Check(q30.rungCount() < q60.rungCount(), "30fps ceiling -> a shorter ladder");
    Check(q30.current() == QualityStep{kW, kH, 30}, "the ceiling rung respects the user ceiling");
    for (uint32_t bps = 20'000'000; bps >= 300'000; bps -= 500'000) {
        q30.Update(bps, 1'000'000);
        Check(q30.current().fps <= 30, "no rung exceeds the fps ceiling");
    }
}

void TestTinySourceStopsTheLadder() {
    std::printf("[quality] a tiny source never shrinks below what the encoder accepts...\n");
    QualityLadder q(320, 200, 60);
    for (uint32_t bps = 20'000'000; bps >= 100'000; bps -= 200'000) {
        q.Update(bps, 1'000'000);
        Check(q.current().width >= 160 && q.current().height >= 64,
            "every rung stays encodable");
    }
}

}

void RunQualityLadderTests() {
    TestTopRungOnAHealthyLink();
    TestFpsGoesFirst();
    TestResolutionOnlyAfterFpsFloor();
    TestNeverExceedsCeiling();
    TestDownIsImmediate();
    TestUpNeedsHeadroom();
    TestFpsOnlyStepUpIsQuick();
    TestResizeStepUpWaitsLonger();
    TestUpOneRungAtATime();
    TestLowUserFpsCollapsesDuplicateRungs();
    TestTinySourceStopsTheLadder();
}
