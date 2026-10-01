#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/diag/LogRetention.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

void TestOnlySessionLogsAreRecognised() {
    std::printf("[logs] only files named like a session log are ever pruned...\n");
    Check(IsSessionLogName("deskhub-20261001-093000-4242.log"), "a session log is recognised");
    Check(!IsSessionLogName("deskhub-latest.log"), "the latest-log link is not a session log");
    Check(!IsSessionLogName("deskhub-20261001-093000-.log"), "a log needs a pid");
    Check(!IsSessionLogName("deskhub-2026100-093000-1.log"), "the date is eight digits");
    Check(!IsSessionLogName("deskhub-20261001_093000-1.log"), "separated by a dash");
    Check(!IsSessionLogName("deskhub-20261001-0930x0-1.log"), "the time is six digits");
    Check(!IsSessionLogName("deskhub-20261001-093000_1.log"), "and a dash before the pid");
    Check(!IsSessionLogName("deskhub-20261001-093000-1x.log"), "the pid is a number");
    Check(!IsSessionLogName("deskhub-20261001-093000-1.txt"), "only .log files");
    Check(!IsSessionLogName("host_key.pem"), "no other file in the folder");
    Check(!IsSessionLogName("deskhub-.log"), "and a bare prefix is not a log");
}

void TestTheNewestLogsAreKept() {
    std::printf("[logs] the oldest session logs go, the newest ones stay...\n");
    const std::vector<std::string> names = {
        "deskhub-20261001-093000-7.log",
        "known_hosts",
        "deskhub-20250101-000000-1.log",
        "deskhub-latest.log",
        "deskhub-20260615-120000-3.log",
    };
    const auto pruned = SessionLogsToPrune(names, 1);
    Check(pruned.size() == 2 && pruned[0] == "deskhub-20250101-000000-1.log" &&
              pruned[1] == "deskhub-20260615-120000-3.log",
        "everything but the newest log goes, oldest first");
    Check(SessionLogsToPrune(names, 3).empty(), "nothing goes while the count is within the limit");
    Check(SessionLogsToPrune(names, 0).size() == 3, "a limit of zero prunes every session log");
    Check(SessionLogsToPrune({}, kKeptSessionLogs).empty(), "an empty folder prunes nothing");
}

}

void RunLogRetentionTests() {
    TestOnlySessionLogsAreRecognised();
    TestTheNewestLogsAreKept();
}
