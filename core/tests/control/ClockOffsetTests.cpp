#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/ClockOffset.h"
#include "deskhub/control/ClockOffsetEstimator.h"
#include "deskhub/control/ClockSync.h"

#include <cstdio>
#include <memory>
#include <string_view>

using namespace deskhub;

namespace {

constexpr int64_t kSkewFresh = 0;
constexpr int64_t kSkewStale = -3ll * 24 * 3600 * 1'000'000;

void Feed(ClockOffset& co, int64_t skew, uint64_t hostUs, uint64_t delayUs) {
    co.AddSample(hostUs, uint64_t(int64_t(hostUs) + int64_t(delayUs) + skew));
}

void TestFloorIsSubtracted(int64_t skew) {
    ClockOffset co;
    Feed(co, skew, 1'000'000, 20'000);
    Check(co.LatencyUs() == 0, "the first frame defines the floor -> 0");
    Feed(co, skew, 1'016'000, 22'000);
    Check(co.LatencyUs() == 2'000, "a frame 2 ms above the floor");
    Feed(co, skew, 1'032'000, 70'000);
    Check(co.LatencyUs() == 50'000, "a 50 ms queueing spike shows up in full");
    Feed(co, skew, 1'048'000, 20'000);
    Check(co.LatencyUs() == 0, "back to the best level -> 0");
}

void TestNetFloorAddedBack(int64_t skew) {
    ClockOffset co;
    Feed(co, skew, 1'000'000, 20'000);
    Feed(co, skew, 1'016'000, 45'000);
    Check(co.LatencyUs() == 25'000, "floor not added: only the excess");
    Check(co.LatencyUs(4'000) == 29'000, "the measured network floor is added back (minRtt/2)");
    Check(co.LatencyUs(0) == 25'000, "a floor of 0 behaves like passing nothing");
}

void TestFloorDecays(int64_t skew) {
    ClockOffset co;
    uint64_t t = 1'000'000;
    Feed(co, skew, t, 20'000);

    for (int i = 0; i < 40; ++i) {
        t += ClockOffset::kWindowUs / 10;
        Feed(co, skew, t, 120'000);
    }
    Check(co.LatencyUs() == 0, "the floor relearned the new level -> a frame at that level is 0");

    t += ClockOffset::kWindowUs / 10;
    Feed(co, skew, t, 20'000);
    Check(co.LatencyUs() == 0, "faster than the current floor still clamps to 0, never negative");
}

void TestLongSilence(int64_t skew) {
    ClockOffset co;
    Feed(co, skew, 1'000'000, 20'000);
    const uint64_t t = 1'000'000 + ClockOffset::kWindowUs * 6;
    Feed(co, skew, t, 20'000);
    Check(co.LatencyUs() == 0, "after a long silence the first sample redefines the floor");
    Feed(co, skew, t + 16'000, 35'000);
    Check(co.LatencyUs() == 15'000, "and the next sample measures exactly the excess");
}

void TestNotReady() {
    ClockOffset co;
    Check(!co.ready(), "no samples yet");
    Check(co.LatencyUs() == -1, "not ready -> -1, not 0");
    co.AddSample(1'000'000, 1'020'000);
    Check(co.ready(), "one sample makes it ready");
    co.Reset();
    Check(!co.ready(), "Reset forgets everything (a new session carries a different C)");
}

void TestFloorIsExposed() {
    ClockOffset co;
    Check(co.floorUs() == 0, "no floor before any sample");
    co.AddSample(1'000'000, 1'020'000);
    Check(co.floorUs() == 20'000, "the floor is the best offset seen");
    co.AddSample(1'016'000, 1'038'000);
    Check(co.floorUs() == 20'000, "a slower frame cannot lower the floor");
    co.Reset();
    Check(co.floorUs() == 0, "Reset clears the floor");
}

void BothSkews(void (*fn)(int64_t), const char* name) {
    std::printf("[clock] %s...\n", name);
    fn(kSkewFresh);
    fn(kSkewStale);
}

void TestEstimatorsAgreeOnAQuietLink() {
    std::printf("[clock] every estimator reads a quiet link the same way...\n");

    for (std::string_view name : ClockOffsetEstimatorNames()) {
        const std::unique_ptr<ClockOffsetEstimator> est = MakeClockOffsetEstimator(name);
        Check(est != nullptr, "every listed estimator builds");
        if (!est) continue;
        Check(est->Name() == name, "and answers with the name it was asked for");
        Check(!est->ready(), "an estimator with no samples is not ready");
        Check(est->LatencyUs() < 0, "and reports no latency rather than a made-up zero");

        uint64_t hostUs = 1'000'000;
        for (int i = 0; i < 200; ++i, hostUs += 16'667)
            est->AddSample(hostUs, hostUs + 500'000 + 20'000);
        Check(est->ready(), "samples make it ready");
        Check(est->LatencyUs() >= 0 && est->LatencyUs() < 5'000,
            "a link with a constant one-way delay shows no excess latency");

        est->Reset();
        Check(!est->ready(), "Reset puts it back where it started");
    }

    Check(MakeClockOffsetEstimator("no-such-estimator") == nullptr,
        "an unknown estimator name builds nothing");
    Check(IsClockOffsetEstimatorName(kDefaultClockOffset),
        "the default is one of the registered names");
}

void TestOnlyTheTrendlineSeesSlowDrift() {
    std::printf("[clock] a drifting clock is invisible to a rolling minimum...\n");

    const std::unique_ptr<ClockOffsetEstimator> rolling =
        MakeClockOffsetEstimator("rolling-min");
    const std::unique_ptr<ClockOffsetEstimator> trend = MakeClockOffsetEstimator("trendline");
    Check(rolling && trend, "both estimators build");
    if (!rolling || !trend) return;

    uint64_t hostUs = 1'000'000;
    uint64_t localUs = hostUs + 500'000;
    for (int i = 0; i < 1200; ++i, hostUs += 16'667) {
        localUs += 16'667 + 500;
        rolling->AddSample(hostUs, localUs);
        trend->AddSample(hostUs, localUs);
    }

    const int64_t byMinimum = rolling->LatencyUs();
    const int64_t byTrend = trend->LatencyUs();
    if (byTrend * 4 >= byMinimum)
        std::printf("[clock]   rolling-min read %lld us, trendline read %lld us\n",
            (long long)byMinimum, (long long)byTrend);

    Check(byMinimum > 0,
        "a steadily drifting clock reads to a rolling minimum as latency piling up");
    Check(byTrend * 4 < byMinimum,
        "the trendline subtracts the drift and reports the jitter that is actually there, so "
        "the two disagree by enough to be worth choosing between");
}

void TestClockSyncSeparatesOffsetFromDelay() {
    std::printf("[clock] a two-way exchange separates clock offset from one-way delay...\n");

    ClockSync sync;
    Check(!sync.ready(), "with no exchange there is nothing to report");
    Check(sync.AbsoluteLatencyUs(1'000, 2'000) < 0,
        "and it says so rather than inventing a latency");

    constexpr int64_t kTrueOffsetUs = 7'000'000;
    constexpr uint64_t kOneWayUs = 20'000;

    uint64_t localUs = 1'000'000;
    for (int i = 0; i < 40; ++i, localUs += 1'000'000) {
        const uint64_t extra = uint64_t(i % 5) * 30'000;
        const uint64_t sent = localUs;
        const uint64_t hostSaw = uint64_t(int64_t(sent + kOneWayUs + extra) - kTrueOffsetUs);
        const uint64_t received = sent + 2 * (kOneWayUs + extra);
        sync.AddSample(sent, hostSaw, received);
    }

    Check(sync.ready(), "samples make it ready");
    const int64_t error = sync.offsetUs() - kTrueOffsetUs;
    Check(error > -3'000 && error < 3'000,
        "the sample with the least queueing recovers the clock offset to within a few ms, "
        "which no amount of one-way samples could ever have done");
    Check(sync.bestRttUs() <= 2 * kOneWayUs + 1'000,
        "and it keeps the cleanest round trip it saw, not the average");
}

void TestAbsoluteLatencyIsNotExcessOverAFloor() {
    std::printf(
        "[clock] the absolute number is the whole path, not the excess over a "
        "floor...\n");

    ClockSync sync;
    constexpr int64_t kTrueOffsetUs = -4'000'000;
    constexpr uint64_t kOneWayUs = 15'000;

    uint64_t localUs = 1'000'000;
    for (int i = 0; i < 20; ++i, localUs += 1'000'000) {
        const uint64_t sent = localUs;
        const uint64_t hostSaw = uint64_t(int64_t(sent + kOneWayUs) - kTrueOffsetUs);
        sync.AddSample(sent, hostSaw, sent + 2 * kOneWayUs);
    }

    const uint64_t hostPts = uint64_t(int64_t(localUs) - kTrueOffsetUs);
    const int64_t latency = sync.AbsoluteLatencyUs(hostPts, localUs + 90'000);
    Check(latency > 87'000 && latency < 93'000,
        "a frame stamped by the host and arriving 90 ms later reads as about 90 ms, whatever "
        "the two clocks disagree by");

    const std::unique_ptr<ClockOffsetEstimator> relative =
        MakeClockOffsetEstimator(kDefaultClockOffset);
    for (int i = 0; i < 20; ++i)
        relative->AddSample(hostPts + uint64_t(i) * 16'667,
            localUs + 90'000 + uint64_t(i) * 16'667);
    Check(relative->LatencyUs() < 5'000,
        "the rolling-min estimator reports almost nothing for the same 90 ms path, because a "
        "constant one-way delay is exactly what its floor absorbs - that is why e2e_ms alone "
        "could never be published next to another tool's number");
}

}

void RunClockOffsetTests() {
    BothSkews(TestFloorIsSubtracted, "the floor is subtracted, the queueing shows through");
    BothSkews(TestNetFloorAddedBack, "the measured network floor is added back, never counted twice");
    BothSkews(TestFloorDecays, "the floor relearns when the link degrades");
    BothSkews(TestLongSilence, "an idle source silent for several windows");
    std::printf("[clock] no samples / Reset...\n");
    TestNotReady();
    std::printf("[clock] the floor itself is readable for diagnostics...\n");
    TestFloorIsExposed();
    TestEstimatorsAgreeOnAQuietLink();
    TestOnlyTheTrendlineSeesSlowDrift();
    TestClockSyncSeparatesOffsetFromDelay();
    TestAbsoluteLatencyIsNotExcessOverAFloor();
}
