#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/client/ConnectFlow.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

SourceInfo Info(uint8_t id) {
    SourceInfo s;
    s.sourceId = id;
    return s;
}

void TestDecision() {
    std::printf("[connect] the source-query result decides picker vs auto-start...\n");

    const std::vector<SourceInfo> none;
    const ConnectDecision empty = DecideAfterSourceQuery(none);
    Check(!empty.showPicker && empty.sourceId == 0,
        "no sources (or a failed query) -> start source 0 and let the host answer");

    const std::vector<SourceInfo> one{Info(3)};
    const ConnectDecision single = DecideAfterSourceQuery(one);
    Check(!single.showPicker && single.sourceId == 3,
        "exactly one source -> auto-start it without a picker");

    const std::vector<SourceInfo> many{Info(1), Info(2)};
    Check(DecideAfterSourceQuery(many).showPicker, "more than one source -> ask the user");
}

}

void RunConnectFlowTests() {
    TestDecision();
}
