#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/ClockOffsetEstimator.h"
#include "deskhub/control/VideoPacer.h"

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <set>
#include <string_view>

using deskhub::ClockOffsetEstimatorNames;
using deskhub::kDefaultClockOffset;
using deskhub::VideoPacer;

namespace {

constexpr uint64_t kLeadUs = VideoPacer::kDefaultLeadUs;
constexpr uint64_t kDisplayIntervalUs = 6'944;
constexpr uint64_t kFrameUs = 16'667;
constexpr uint64_t kTransitUs = 100'000;

VideoPacer SteadyPacer(uint64_t frames, uint64_t& lastPts, uint64_t& lastNow) {
    VideoPacer pacer;
    for (uint64_t i = 0; i < frames; ++i) {
        const uint64_t pts = 1'000'000 + i * kFrameUs;
        const uint64_t jitterUs = (i % 4) * 3'000;
        lastPts = pts;
        lastNow = pts + kTransitUs + jitterUs;
        pacer.ObserveArrival(lastPts, lastNow);
    }
    return pacer;
}

void TestSteadyStreamGetsTheLead() {
    std::printf("[pacer] the fastest frame is scheduled one lead ahead of arrival...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(64, pts, now);
    Check(pacer.ready(), "the pacer is primed after a steady run");

    const uint64_t fastestPts = pts + kFrameUs;
    const uint64_t fastestNow = fastestPts + kTransitUs;
    pacer.ObserveArrival(fastestPts, fastestNow);
    Check(pacer.DisplayTimeUs(fastestPts, fastestNow) == fastestNow + kLeadUs,
        "a zero-jitter frame displays exactly one lead after it arrives");

    const uint64_t jitteredPts = fastestPts + kFrameUs;
    const uint64_t jitteredNow = jitteredPts + kTransitUs + 10'000;
    pacer.ObserveArrival(jitteredPts, jitteredNow);
    Check(pacer.DisplayTimeUs(jitteredPts, jitteredNow) == jitteredNow + kLeadUs - 10'000,
        "a jittered frame spends its slack and keeps the same display cadence");
}

void TestJitterRemovedFromCadence() {
    std::printf("[pacer] display times follow the pts grid, not the arrival wobble...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(64, pts, now);

    uint64_t prevDisplay = 0;
    for (uint64_t i = 1; i <= 4; ++i) {
        const uint64_t p = pts + i * kFrameUs;
        const uint64_t n = p + kTransitUs + (i % 2) * 9'000;
        pacer.ObserveArrival(p, n);
        const uint64_t display = pacer.DisplayTimeUs(p, n);
        if (prevDisplay)
            Check(display - prevDisplay == kFrameUs,
                "consecutive display times are one frame interval apart");
        prevDisplay = display;
    }
}

void TestLateFrameDisplaysImmediately() {
    std::printf("[pacer] a frame later than the lead is shown at once, not queued...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(64, pts, now);

    const uint64_t latePts = pts + kFrameUs;
    const uint64_t lateNow = latePts + kTransitUs + kLeadUs + 20'000;
    pacer.ObserveArrival(latePts, lateNow);
    Check(pacer.DisplayTimeUs(latePts, lateNow) == lateNow,
        "the display time clamps to now instead of the past");
}

void TestResyncThreshold() {
    std::printf("[pacer] the timebase is nudged only past the resync threshold...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(64, pts, now);

    const int64_t desired = pacer.DesiredTimebaseUs(now);
    Check(!pacer.NeedsResync(desired, now), "an exact timebase needs no resync");
    Check(!pacer.NeedsResync(desired + 100'000, now), "a small drift is left to glide");
    Check(pacer.NeedsResync(desired + 300'000, now), "a large forward drift forces a jump");
    Check(pacer.NeedsResync(desired - 300'000, now), "a large backward drift forces a jump");
}

void TestStreamRestartResets() {
    std::printf("[pacer] a pts jump is read as a new stream, not as huge jitter...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(64, pts, now);

    const uint64_t restartPts = 1'000;
    const uint64_t restartNow = now + kFrameUs;
    pacer.ObserveArrival(restartPts, restartNow);
    Check(pacer.ready(), "the pacer reprimes on the first frame of the new stream");
    Check(pacer.DisplayTimeUs(restartPts, restartNow) == restartNow + kLeadUs,
        "the new stream starts on a fresh mapping instead of freezing");
}

void TestResetClears() {
    std::printf("[pacer] reset forgets the mapping...\n");
    uint64_t pts = 0, now = 0;
    VideoPacer pacer = SteadyPacer(8, pts, now);
    pacer.Reset();
    Check(!pacer.ready(), "a reset pacer is unprimed");
}

void TestAdaptiveLeadGivesDelayBackOnASteadyLink() {
    std::printf("[pacer] an adaptive lead hands delay back when the link stops wobbling...\n");

    VideoPacer steady;
    steady.SetAdaptiveLead(true);
    uint64_t ptsUs = 1'000'000;
    for (int frame = 0; frame < 300; ++frame, ptsUs += 16'667)
        steady.ObserveArrival(ptsUs, ptsUs + 40'000);
    Check(steady.leadUs() < VideoPacer::kDefaultLeadUs,
        "a link that never wobbles does not need a third of a frame of slack");

    VideoPacer rough;
    rough.SetAdaptiveLead(true);
    uint32_t state = 3;
    ptsUs = 1'000'000;
    for (int frame = 0; frame < 300; ++frame, ptsUs += 16'667) {
        state = state * 1664525u + 1013904223u;
        rough.ObserveArrival(ptsUs, ptsUs + 40'000 + (state >> 16) % 30'000);
    }
    Check(rough.leadUs() > steady.leadUs(),
        "while a wobbly one is given more, which is the whole trade the option offers");
    Check(rough.leadUs() <= VideoPacer::kMaxLeadUs, "and the lead is still bounded");

    rough.SetAdaptiveLead(false);
    Check(rough.leadUs() == VideoPacer::kDefaultLeadUs,
        "turning adaptation off puts the fixed lead straight back");
}

size_t PhasesSeen(uint64_t vsyncUs) {
    VideoPacer pacer;
    pacer.SetDisplayIntervalUs(vsyncUs);

    uint64_t ptsUs = 1'000'000;
    for (int warm = 0; warm < 10; ++warm, ptsUs += 16'667)
        pacer.ObserveArrival(ptsUs, ptsUs + 40'000);

    std::set<uint64_t> phases;
    for (int frame = 0; frame < 200; ++frame, ptsUs += 16'667) {
        const uint64_t nowUs = ptsUs + 40'000;
        pacer.ObserveArrival(ptsUs, nowUs);
        const uint64_t show = pacer.DisplayTimeUs(ptsUs, nowUs);
        phases.insert(show % kDisplayIntervalUs);
    }
    return phases.size();
}

void TestVsyncSnappingFixesThePhaseAgainstTheDisplay() {
    std::printf("[pacer] snapping puts every frame on the display's own grid...\n");

    const size_t freeRunning = PhasesSeen(0);
    const size_t snapped = PhasesSeen(kDisplayIntervalUs);

    if (snapped >= freeRunning)
        std::printf("[pacer]   free-running landed on %zu phases, snapped on %zu\n", freeRunning,
            snapped);
    Check(freeRunning > 1,
        "a 60 fps stream against a 144 Hz panel lands on a different point inside the refresh "
        "interval almost every frame, and each one waits a different slice for the scan-out");
    Check(snapped == 1,
        "snapping puts every frame on the same phase, so the wait for scan-out stops varying "
        "- that variation is the judder the option is meant to remove");
}

uint64_t PhaseSpreadUs(uint64_t vsyncUs, uint64_t leadUs, uint64_t wobbleUs) {
    VideoPacer pacer(leadUs);
    pacer.SetDisplayIntervalUs(vsyncUs);

    uint32_t state = 5;
    uint64_t ptsUs = 1'000'000;
    for (int warm = 0; warm < 20; ++warm, ptsUs += kFrameUs)
        pacer.ObserveArrival(ptsUs, ptsUs + kTransitUs);

    uint64_t lowest = ~uint64_t(0);
    uint64_t highest = 0;
    for (int frame = 0; frame < 300; ++frame, ptsUs += kFrameUs) {
        state = state * 1664525u + 1013904223u;
        const uint64_t wobble = wobbleUs ? (state >> 16) % wobbleUs : 0;
        const uint64_t nowUs = ptsUs + kTransitUs + wobble;
        pacer.ObserveArrival(ptsUs, nowUs);
        const uint64_t phase = pacer.DisplayTimeUs(ptsUs, nowUs) % kDisplayIntervalUs;
        lowest = std::min(lowest, phase);
        highest = std::max(highest, phase);
    }
    return highest - lowest;
}

struct ClockRun {
    uint64_t phaseSpreadUs = 0;
    uint64_t leadUs = 0;
    uint64_t worstLateUs = 0;
};

ClockRun RunClock(std::string_view name, uint64_t wobbleUs, bool driftStep) {
    VideoPacer pacer(VideoPacer::kDefaultLeadUs);
    Check(pacer.SetClockOffset(name), "every registered clock estimator drives the pacer");

    uint32_t state = 9;
    uint64_t ptsUs = 1'000'000;
    for (int warm = 0; warm < 20; ++warm, ptsUs += kFrameUs)
        pacer.ObserveArrival(ptsUs, ptsUs + kTransitUs);

    uint64_t transitUs = kTransitUs;
    uint64_t lowest = ~uint64_t(0);
    uint64_t highest = 0;
    ClockRun run;
    for (int frame = 0; frame < 300; ++frame, ptsUs += kFrameUs) {
        if (driftStep && frame == 150) transitUs += 30'000;
        state = state * 1664525u + 1013904223u;
        const uint64_t wobble = wobbleUs ? (state >> 16) % wobbleUs : 0;
        const uint64_t nowUs = ptsUs + transitUs + wobble;
        pacer.ObserveArrival(ptsUs, nowUs);

        const uint64_t showUs = pacer.DisplayTimeUs(ptsUs, nowUs);
        const uint64_t phase = showUs % kDisplayIntervalUs;
        lowest = std::min(lowest, phase);
        highest = std::max(highest, phase);
        run.worstLateUs = std::max(run.worstLateUs, showUs - nowUs);
    }
    run.phaseSpreadUs = highest - lowest;
    run.leadUs = pacer.leadUs();
    return run;
}

void TestClockEstimatorsDriveThePacer() {
    std::printf("[pacer] the clock sweep: which offset estimator the pacer runs on...\n");
    std::printf("[csv] estimator,wobble_ms,drift_step,phase_spread_us,lead_us,worst_wait_us\n");

    const uint64_t wobbles[] = {0, 5'000, 20'000};

    uint64_t steadySpread = 0;
    bool sawEstimator = false;
    for (std::string_view name : ClockOffsetEstimatorNames()) {
        for (uint64_t wobbleUs : wobbles)
            for (bool driftStep : {false, true}) {
                const ClockRun run = RunClock(name, wobbleUs, driftStep);
                std::printf("[csv] %.*s,%llu,%d,%llu,%llu,%llu\n", int(name.size()), name.data(),
                    (unsigned long long)(wobbleUs / 1000), driftStep ? 1 : 0,
                    (unsigned long long)run.phaseSpreadUs, (unsigned long long)run.leadUs,
                    (unsigned long long)run.worstLateUs);
                if (name == kDefaultClockOffset && wobbleUs == 0 && !driftStep)
                    steadySpread = run.phaseSpreadUs;
            }
        sawEstimator = true;
    }

    Check(sawEstimator, "the sweep ran at least one estimator");
    Check(RunClock(kDefaultClockOffset, 0, false).phaseSpreadUs == steadySpread,
        "the sweep is deterministic, so two runs of the same point agree");
}

void TestJudderAgainstAddedDelay() {
    std::printf("[pacer] judder against the delay bought to remove it...\n");
    std::printf("[csv] lead_us,vsync_us,wobble_ms,phase_spread_us\n");

    const uint64_t leads[] = {8'000, 16'000, 33'000, 66'000};
    const uint64_t wobbles[] = {0, 5'000, 20'000};

    for (uint64_t wobbleUs : wobbles)
        for (uint64_t leadUs : leads) {
            const uint64_t loose = PhaseSpreadUs(0, leadUs, wobbleUs);
            const uint64_t snapped = PhaseSpreadUs(kDisplayIntervalUs, leadUs, wobbleUs);
            std::printf("[csv] %llu,0,%llu,%llu\n", (unsigned long long)leadUs,
                (unsigned long long)(wobbleUs / 1000), (unsigned long long)loose);
            std::printf("[csv] %llu,%llu,%llu,%llu\n", (unsigned long long)leadUs,
                (unsigned long long)kDisplayIntervalUs, (unsigned long long)(wobbleUs / 1000),
                (unsigned long long)snapped);
            Check(snapped == 0,
                "snapping holds the phase at zero whatever lead was bought, so vsync matching "
                "is not a thing you trade latency for");
        }

    Check(PhaseSpreadUs(0, 8'000, 0) > 0,
        "without snapping even a perfectly steady link lands on a spread of phases, because "
        "60 fps content and a 144 Hz panel never line up");
    const uint64_t nearlyTheWholeInterval = kDisplayIntervalUs * 8 / 10;
    Check(PhaseSpreadUs(0, 8'000, 0) > nearlyTheWholeInterval &&
              PhaseSpreadUs(0, 66'000, 0) > nearlyTheWholeInterval,
        "and buying eight times the lead leaves the spread covering almost the whole refresh "
        "interval either way - judder from phase is not something more delay can fix, which "
        "is why the curve above has no trade in it: snapping costs nothing and fixes it");
}

}

void RunVideoPacerTests() {
    TestSteadyStreamGetsTheLead();
    TestJitterRemovedFromCadence();
    TestLateFrameDisplaysImmediately();
    TestResyncThreshold();
    TestStreamRestartResets();
    TestResetClears();
    TestAdaptiveLeadGivesDelayBackOnASteadyLink();
    TestVsyncSnappingFixesThePhaseAgainstTheDisplay();
    TestJudderAgainstAddedDelay();
    TestClockEstimatorsDriveThePacer();
}
