#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/auth/AuthDeadlines.h"

#include <cstdio>

namespace {

constexpr size_t kCapacity = 2;
constexpr uint64_t kStartUs = 1'000'000;
constexpr uint64_t kWindowUs = 10'000'000;
constexpr uint64_t kLingerUs = 2'000'000;

void TestTheCapBoundsWaitingConnections() {
    deskhub::AuthDeadlines deadlines(kCapacity);
    Check(deadlines.Admit(1, kStartUs + kWindowUs), "the first connection is admitted");
    Check(deadlines.Admit(2, kStartUs + kWindowUs), "the second connection is admitted");
    Check(!deadlines.Admit(3, kStartUs + kWindowUs), "a third waits outside the cap");
    Check(deadlines.Admit(2, kStartUs + 1), "a key already waiting stays admitted");
    Check(!deadlines.Due(2, kStartUs + 1), "re-admitting does not move its deadline");
    deadlines.Forget(1);
    Check(deadlines.Admit(3, kStartUs + kWindowUs), "a settled connection frees its slot");
    deadlines.Clear();
    Check(deadlines.Admit(4, kStartUs) && deadlines.Admit(5, kStartUs),
        "clearing frees every slot");
}

void TestDeadlinesExpireOnTime() {
    deskhub::AuthDeadlines deadlines(kCapacity);
    deadlines.Admit(1, kStartUs + kWindowUs);
    deadlines.Admit(2, kStartUs + kWindowUs + 1);
    Check(!deadlines.Due(1, kStartUs + kWindowUs - 1), "nothing is due before the deadline");
    Check(deadlines.Due(1, kStartUs + kWindowUs), "the deadline itself is due");
    Check(!deadlines.Due(9, kStartUs + kWindowUs), "an unknown key is never due");
    Check(deadlines.TakeDue(kStartUs).empty(), "nothing is taken early");
    const std::vector<uint64_t> due = deadlines.TakeDue(kStartUs + kWindowUs);
    Check(due.size() == 1 && due[0] == 1, "only the expired key is taken");
    Check(!deadlines.Due(1, kStartUs + kWindowUs), "a taken key is forgotten");
    Check(deadlines.Due(2, kStartUs + kWindowUs + 1), "a later key keeps waiting");
}

void TestARefusalOnlyShortensTheWait() {
    deskhub::AuthDeadlines deadlines(kCapacity);
    deadlines.Admit(1, kStartUs + kWindowUs);
    deadlines.Hasten(1, kStartUs + kLingerUs);
    Check(deadlines.Due(1, kStartUs + kLingerUs), "a refusal pulls the deadline in");
    deadlines.Hasten(1, kStartUs + kWindowUs);
    Check(deadlines.Due(1, kStartUs + kLingerUs), "a later deadline never extends it");
    deadlines.Hasten(7, kStartUs + kLingerUs);
    Check(deadlines.Due(7, kStartUs + kLingerUs), "a refused key not yet tracked gets one");
}

}

void RunAuthDeadlinesTests() {
    std::printf("[auth] unauthenticated connections are capped and expire...\n");
    TestTheCapBoundsWaitingConnections();
    TestDeadlinesExpireOnTime();
    TestARefusalOnlyShortensTheWait();
}
