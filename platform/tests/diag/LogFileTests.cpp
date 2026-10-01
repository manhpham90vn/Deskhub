#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/diag/LogFile.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using deskhubp::LocalTimeHms;
using deskhubp::LogFileName;

namespace {

bool AllDigits(const std::string& s, size_t from, size_t count) {
    if (from + count > s.size()) return false;
    for (size_t i = from; i < from + count; ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

int Number(const std::string& s, size_t from, size_t count) {
    return std::stoi(s.substr(from, count));
}

void TestTheTimestampIsReadableAtAGlance() {
    std::printf("[log] every log line is stamped with a wall-clock time...\n");
    const std::string hms = LocalTimeHms();
    Check(hms.size() == 8, "the stamp is exactly HH:MM:SS wide, so lines stay aligned");
    Check(hms[2] == ':' && hms[5] == ':', "with colons where they belong");
    Check(AllDigits(hms, 0, 2) && AllDigits(hms, 3, 2) && AllDigits(hms, 6, 2),
        "and digits everywhere else");

    Check(Number(hms, 0, 2) <= 23, "the hour is a real hour");
    Check(Number(hms, 3, 2) <= 59, "the minute is a real minute");
    Check(Number(hms, 6, 2) <= 60, "the second is a real second, leap second included");
}

void TestTheLogFileNameSortsByTime() {
    std::printf("[log] log file names sort by date, so the newest is the last one...\n");
    const std::string name = LogFileName();

    Check(name.rfind("deskhub-", 0) == 0, "every log is recognisably ours");
    Check(name.size() >= std::string("deskhub-YYYYMMDD-HHMMSS-0.log").size(),
        "the name carries a full date, a time and a pid");
    Check(name.substr(name.size() - 4) == ".log", "and ends in .log");

    Check(AllDigits(name, 8, 8), "YYYYMMDD is eight digits, which is what makes it sortable");
    Check(name[16] == '-', "then a separator");
    Check(AllDigits(name, 17, 6), "then HHMMSS");
    Check(name[23] == '-', "then a separator before the pid");

    const int year = Number(name, 8, 4);
    const int month = Number(name, 12, 2);
    const int day = Number(name, 14, 2);
    Check(year >= 2024 && year <= 2200, "the year comes from the real clock");
    Check(month >= 1 && month <= 12, "the month is a real month");
    Check(day >= 1 && day <= 31, "the day is a real day");

    Check(Number(name, 17, 2) <= 23 && Number(name, 19, 2) <= 59 && Number(name, 21, 2) <= 60,
        "and the time of day is a real time");
}

void TestTwoProcessesDoNotShareALogFile() {
    std::printf("[log] the name carries our pid, so two Deskhubs never overwrite each other...\n");
    const std::string name = LogFileName();
    const size_t pidStart = 24;
    const size_t pidEnd = name.size() - 4;
    Check(pidEnd > pidStart, "there is a pid between the time and the extension");
    Check(AllDigits(name, pidStart, pidEnd - pidStart), "and it is a plain number");
    Check(name.substr(pidStart, pidEnd - pidStart) != "0", "which is never the placeholder 0");
}

void TestTheNameIsStableWithinASecond() {
    std::printf("[log] asking twice in the same second gives the same file, not two...\n");
    Check(LogFileName().substr(0, 8) == LogFileName().substr(0, 8),
        "the prefix does not drift between calls");
}

void TestOldSessionLogsArePruned() {
    std::printf("[log] only the newest session logs stay in the folder...\n");
    const auto dir = UniqueTempDir("deskhub-logs-");
    if (dir.empty() || !std::filesystem::create_directory(dir)) {
        Check(false, "the log pruning test has a folder of its own");
        return;
    }
    const size_t extra = 3;
    for (size_t i = 0; i < deskhub::kKeptSessionLogs + extra; ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "deskhub-20260101-0000%02zu-1.log", i);
        std::ofstream(dir / name) << "line\n";
    }
    std::ofstream(dir / "host_key.pem") << "key\n";
    deskhubp::PruneOldSessionLogs(dir);

    size_t logs = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        if (deskhub::IsSessionLogName(entry.path().filename().string())) ++logs;
    Check(logs == deskhub::kKeptSessionLogs, "the folder keeps exactly the configured number");
    Check(!std::filesystem::exists(dir / "deskhub-20260101-000000-1.log") &&
              std::filesystem::exists(dir / "deskhub-20260101-000012-1.log"),
        "the oldest logs are the ones removed");
    Check(std::filesystem::exists(dir / "host_key.pem"), "other files are never touched");
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

}

void RunLogFileTests() {
    TestTheTimestampIsReadableAtAGlance();
    TestTheLogFileNameSortsByTime();
    TestTwoProcessesDoNotShareALogFile();
    TestTheNameIsStableWithinASecond();
    TestOldSessionLogsArePruned();
}
