#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/Pacer.h"

#include <cstdio>

using namespace deskhub;

namespace {

constexpr uint64_t kOneMbps = 1'000'000;
constexpr uint64_t kStartUs = 1'000'000'000;

void TestDisabledByDefault() {
    std::printf("[pacer] with no rate set nothing is ever gated...\n");
    Pacer p;
    Check(p.Gate(1500, kStartUs) == 0, "a fresh pacer lets the first send through");
    Check(p.Gate(1500, kStartUs) == 0, "and every send after it, however fast they come");
}

void TestEmptySendIsFree() {
    std::printf("[pacer] a zero-byte send costs nothing and consumes no budget...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);
    Check(p.Gate(0, kStartUs) == 0, "zero bytes never waits");
    Check(p.Gate(1000, kStartUs) == 0, "the real send that follows is still unblocked");
}

void TestRateArithmetic() {
    std::printf("[pacer] the wait equals the airtime of the bytes already queued...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);

    Check(p.Gate(1000, kStartUs) == 0, "the first send goes out immediately");
    Check(p.Gate(1000, kStartUs) == 8000,
        "1000 bytes at 1 Mbps is 8000 us of airtime the next send must wait for");
    Check(p.Gate(1000, kStartUs) == 16000, "a third back-to-back send waits for both");

    Pacer q;
    q.SetRateBps(10 * kOneMbps);
    Check(q.Gate(12'500, kStartUs) == 0, "first send free");
    Check(q.Gate(1, kStartUs) == 10000, "12500 bytes at 10 Mbps is 10 ms");
}

void TestTimePassingRepaysTheDebt() {
    std::printf("[pacer] time spent elsewhere counts against the debt...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);
    p.Gate(1000, kStartUs);

    Check(p.Gate(1000, kStartUs + 8000) == 0,
        "waiting out the full airtime clears the wait entirely");
    Check(p.Gate(1000, kStartUs + 10'000) == 6000,
        "arriving 2 ms into an 8 ms slot leaves 6 ms to wait");
}

void TestNoBurstCredit() {
    std::printf("[pacer] a long idle period does not bank a burst allowance...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);
    p.Gate(1000, kStartUs);

    Check(p.Gate(1000, kStartUs + 10'000'000) == 0, "the send after a 10 s idle is free");
    Check(p.Gate(1000, kStartUs + 10'000'000) == 8000,
        "but only one — the idle bought no extra credit");
}

void TestSubThresholdWaitsAreNotLost() {
    std::printf("[pacer] waits under the sleep threshold report zero yet still accrue...\n");
    Pacer p;
    p.SetRateBps(80 * kOneMbps);

    Check(p.Gate(1000, kStartUs) == 0, "first send free");
    for (int i = 1; i <= 4; ++i)
        Check(p.Gate(1000, kStartUs) == 0,
            "a 100 us slice is below kPacerMinSleepUs, so no sleep is asked for");
    Check(p.Gate(1000, kStartUs) == 5 * 100,
        "the skipped slices are still owed and surface once they add up past the threshold");
}

void TestResetAndRateChange() {
    std::printf("[pacer] Reset() and dropping the rate both clear the backlog...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);
    p.Gate(10'000, kStartUs);
    Check(p.Gate(1, kStartUs) == 80'000, "a big frame leaves a big debt");

    p.Reset();
    Check(p.Gate(1, kStartUs) == 0, "Reset() forgets it");

    p.Gate(10'000, kStartUs);
    p.SetRateBps(0);
    Check(p.Gate(1, kStartUs) == 0, "turning pacing off forgets it too");

    p.SetRateBps(kOneMbps);
    Check(p.Gate(1000, kStartUs) == 0, "and turning it back on starts from a clean slate");
}

void TestTheBacklogCannotGrowWithoutBound() {
    std::printf("[pacer] a runaway backlog is capped, so a sender never sleeps forever...\n");
    Pacer p;
    p.SetRateBps(kOneMbps);

    for (int i = 0; i < 200; ++i) p.Gate(10'000, kStartUs);
    Check(p.Gate(1, kStartUs) == kPacerMaxBacklogUs,
        "however far behind the sender falls, the next wait stays inside the cap");

    p.SetRateBps(1000);
    p.Gate(1'000'000, kStartUs);
    Check(p.Gate(1, kStartUs) <= kPacerMaxBacklogUs,
        "a huge frame at a tiny rate cannot wedge the thread either");
}

}

void RunPacerTests() {
    TestDisabledByDefault();
    TestEmptySendIsFree();
    TestRateArithmetic();
    TestTimePassingRepaysTheDebt();
    TestNoBurstCredit();
    TestSubThresholdWaitsAreNotLost();
    TestResetAndRateChange();
    TestTheBacklogCannotGrowWithoutBound();
}
