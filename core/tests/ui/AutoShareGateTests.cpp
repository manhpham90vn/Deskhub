#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/AutoShareGate.h"

#include <cstdio>

using namespace deskhub;

namespace {

void TestAReadyDesktopSharesOnTheFirstProbe() {
    std::printf("[autoshare] a desktop that is already up shares without any delay...\n");
    ui::AutoShareGate gate;
    Check(gate.Advance(true) == ui::AutoShareStep::ShareNow,
        "displays are there, so sharing starts straight away");
    Check(gate.WaitedMs() == 0, "and nothing was waited for");
    Check(gate.Decided(), "the gate is finished with");
}

void TestSharingWaitsUntilTheDisplaysAppear() {
    std::printf("[autoshare] a logon that beats the desktop keeps probing until it is up...\n");
    ui::AutoShareGate gate;
    Check(gate.Advance(false) == ui::AutoShareStep::KeepWaiting, "no display yet: keep waiting");
    Check(gate.Advance(false) == ui::AutoShareStep::KeepWaiting, "still nothing: still waiting");
    Check(gate.WaitedMs() == 2 * ui::kAutoShareProbeMs,
        "the wait counts one probe interval per attempt");
    Check(gate.Advance(true) == ui::AutoShareStep::ShareNow,
        "the moment a display shows up, sharing starts");
}

void TestAHeadlessMachineStopsWaitingInstead() {
    std::printf("[autoshare] a machine with no display ever gives up inside the deadline...\n");
    ui::AutoShareGate gate;
    ui::AutoShareStep step = ui::AutoShareStep::KeepWaiting;
    uint32_t probes = 0;
    while (step == ui::AutoShareStep::KeepWaiting) {
        step = gate.Advance(false);
        ++probes;
        Check(probes < 10000, "the wait terminates rather than spinning forever");
    }
    Check(step == ui::AutoShareStep::GiveUpWaiting, "waiting ends by giving up, not by sharing");
    Check(gate.WaitedMs() < ui::kAutoShareGiveUpMs,
        "and it gives up no later than the deadline it was given");
}

void TestTheDecisionIsMadeOnlyOnce() {
    std::printf("[autoshare] once decided, later probes cannot start a second share...\n");
    ui::AutoShareGate gate;
    Check(gate.Advance(true) == ui::AutoShareStep::ShareNow, "the first ready probe shares");
    Check(gate.Advance(true) == ui::AutoShareStep::ShareNow, "a repeat probe repeats the verdict");
    Check(gate.Advance(false) == ui::AutoShareStep::ShareNow,
        "and a display vanishing afterwards does not reopen the gate");

    ui::AutoShareGate quick(500, 0);
    Check(quick.Advance(false) == ui::AutoShareStep::GiveUpWaiting,
        "a zero deadline gives up on the first probe");
    Check(quick.Advance(true) == ui::AutoShareStep::GiveUpWaiting,
        "and stays given up even if displays arrive late");
}

void TestAZeroProbeIntervalStillTerminates() {
    std::printf("[autoshare] a caller passing a zero interval cannot hang the wait...\n");
    ui::AutoShareGate gate(0, 10);
    Check(gate.ProbeMs() >= 1, "the interval is clamped to something that advances the clock");
    ui::AutoShareStep step = ui::AutoShareStep::KeepWaiting;
    uint32_t probes = 0;
    while (step == ui::AutoShareStep::KeepWaiting && probes < 1000) {
        step = gate.Advance(false);
        ++probes;
    }
    Check(step == ui::AutoShareStep::GiveUpWaiting, "so the wait still reaches its deadline");
}

void TestTheRuleIsTheSameForCallersThatKeepTheirOwnClock() {
    std::printf("[autoshare] a caller counting its own wait gets the same verdicts...\n");
    const uint32_t probe = ui::kAutoShareProbeMs;
    const uint32_t giveUp = ui::kAutoShareGiveUpMs;
    Check(ui::NextAutoShareStep(true, 0, probe, giveUp) == ui::AutoShareStep::ShareNow,
        "displays already there: share");
    Check(ui::NextAutoShareStep(false, 0, probe, giveUp) == ui::AutoShareStep::KeepWaiting,
        "nothing yet, deadline far away: wait");
    Check(ui::NextAutoShareStep(false, giveUp, probe, giveUp) == ui::AutoShareStep::GiveUpWaiting,
        "past the deadline: give up");

    ui::AutoShareGate gate;
    uint32_t waitedMs = 0;
    for (int probes = 0; probes < 4; ++probes) {
        Check(gate.Advance(false) == ui::NextAutoShareStep(false, waitedMs, probe, giveUp),
            "the gate and the bare rule never disagree");
        waitedMs += probe;
        Check(gate.WaitedMs() == waitedMs, "and they count the same wait");
    }
}

}

void RunAutoShareGateTests() {
    TestTheRuleIsTheSameForCallersThatKeepTheirOwnClock();
    TestAReadyDesktopSharesOnTheFirstProbe();
    TestSharingWaitsUntilTheDisplaysAppear();
    TestAHeadlessMachineStopsWaitingInstead();
    TestTheDecisionIsMadeOnlyOnce();
    TestAZeroProbeIntervalStillTerminates();
}
