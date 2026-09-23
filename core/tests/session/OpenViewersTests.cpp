#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/client/OpenViewers.h"

#include <cstdio>

using namespace deskhub;

namespace {

void TestTheLastCloseIsTheOneThatReports() {
    std::printf("[viewers] only the close that empties the app reports it...\n");
    OpenViewerCount v;
    v.Opened();
    v.Opened();
    Check(!v.Closed(), "closing the first of two leaves one behind");
    Check(v.Closed(), "closing the second empties the app");
    Check(v.Closed(), "and it stays empty");
}

void TestASingleViewerRoundTrips() {
    std::printf("[viewers] the common case: one viewer opens and closes...\n");
    OpenViewerCount v;
    v.Opened();
    Check(v.Closed(), "closing it is the last close");
}

void TestAnUnbalancedCloseCannotGoNegative() {
    std::printf("[viewers] a close with nothing open cannot strand the count below zero...\n");
    OpenViewerCount v;
    Check(v.Closed(), "closing nothing still reads as empty");
    Check(v.Closed(), "and closing again does not push the count below zero");

    v.Opened();
    Check(v.Closed(), "so the next open is the only one, and closing it empties the app");
}

}

void RunOpenViewersTests() {
    TestTheLastCloseIsTheOneThatReports();
    TestASingleViewerRoundTrips();
    TestAnUnbalancedCloseCannotGoNegative();
}
