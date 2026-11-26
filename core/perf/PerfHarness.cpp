#include "PerfHarness.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

uint64_t g_allocationCalls = 0;

void* CountedAllocate(std::size_t size) {
    ++g_allocationCalls;
    void* block = std::malloc(size != 0 ? size : 1);
    if (block == nullptr) throw std::bad_alloc();
    return block;
}

void* CountedAllocateAligned(std::size_t size, std::size_t alignment) {
    ++g_allocationCalls;
    const std::size_t wanted = size != 0 ? size : 1;
#if defined(_MSC_VER)
    void* block = _aligned_malloc(wanted, alignment);
#else
    const std::size_t rounded = ((wanted + alignment - 1) / alignment) * alignment;
    void* block = std::aligned_alloc(alignment, rounded);
#endif
    if (block == nullptr) throw std::bad_alloc();
    return block;
}

void ReleaseAligned(void* block) noexcept {
#if defined(_MSC_VER)
    _aligned_free(block);
#else
    std::free(block);
#endif
}

}

void* operator new(std::size_t size) {
    return CountedAllocate(size);
}

void* operator new[](std::size_t size) {
    return CountedAllocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return CountedAllocateAligned(size, std::size_t(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return CountedAllocateAligned(size, std::size_t(alignment));
}

void operator delete(void* block) noexcept {
    std::free(block);
}

void operator delete[](void* block) noexcept {
    std::free(block);
}

void operator delete(void* block, std::size_t) noexcept {
    std::free(block);
}

void operator delete[](void* block, std::size_t) noexcept {
    std::free(block);
}

void operator delete(void* block, std::align_val_t) noexcept {
    ReleaseAligned(block);
}

void operator delete[](void* block, std::align_val_t) noexcept {
    ReleaseAligned(block);
}

void operator delete(void* block, std::size_t, std::align_val_t) noexcept {
    ReleaseAligned(block);
}

void operator delete[](void* block, std::size_t, std::align_val_t) noexcept {
    ReleaseAligned(block);
}

namespace deskhub::perf {

namespace {

using Clock = std::chrono::steady_clock;

constexpr double kMinimumBatchNanos = 5'000'000.0;
constexpr double kWarmUpNanos = 200'000'000.0;
constexpr uint64_t kMaxBatchRuns = 1u << 22;
constexpr int kDefaultRepeats = 7;
constexpr int kScalingAttempts = 3;
constexpr double kDefaultTolerance = 0.25;

int g_failures = 0;
const char* g_defaultBaselinePath = "out/perf/baseline.txt";

struct Row {
    std::string name;
    double nanosPerUnit = 0.0;
    double allocationsPerUnit = 0.0;
};

std::vector<Row>& Rows() {
    static std::vector<Row> rows;
    return rows;
}

const char* EnvOr(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : fallback;
}

int Repeats() {
    static const int repeats = [] {
        const int parsed = std::atoi(EnvOr("DESKHUB_PERF_REPEATS", ""));
        return parsed > 0 ? parsed : kDefaultRepeats;
    }();
    return repeats;
}

double Tolerance() {
    static const double tolerance = [] {
        const double parsed = std::atof(EnvOr("DESKHUB_PERF_TOLERANCE", ""));
        return parsed > 0.0 ? parsed : kDefaultTolerance;
    }();
    return tolerance;
}

std::map<std::string, double> LoadBaseline() {
    std::map<std::string, double> baseline;
    const char* path = EnvOr("DESKHUB_PERF_BASELINE", g_defaultBaselinePath);
    std::FILE* file = std::fopen(path, "r");
    if (file == nullptr) {
        std::printf("no baseline at %s - record one with 'make perf-baseline' on an idle machine, then this run reports every change against it\n",
            path);
        return baseline;
    }
    char name[256] = {};
    double nanos = 0.0;
    double allocations = 0.0;
    while (std::fscanf(file, "%255s %lf %lf", name, &nanos, &allocations) == 3)
        baseline[name] = nanos;
    std::fclose(file);
    std::printf("baseline: %s (%zu measurements, tolerance %.0f%%)\n", path, baseline.size(),
        Tolerance() * 100.0);
    return baseline;
}

const std::map<std::string, double>& Baseline() {
    static const std::map<std::string, double> baseline = LoadBaseline();
    return baseline;
}

std::string Decimals(double value, int places) {
    char text[64] = {};
    std::snprintf(text, sizeof(text), "%.*f", places, value);
    return text;
}

std::string FormatCost(double nanos) {
    if (nanos < 10.0) return Decimals(nanos, 3);
    if (nanos < 1000.0) return Decimals(nanos, 1);
    return Decimals(nanos, 0);
}

void Fail(const char* name, const std::string& why) {
    ++g_failures;
    std::printf("  FAIL: %s %s\n", name, why.c_str());
}

double NanosSince(Clock::time_point start) {
    return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

struct Timing {
    double nanosPerRun = 0.0;
    uint64_t runsPerBatch = 1;
};

double TimeBatch(const std::function<void()>& run, uint64_t runsPerBatch) {
    const Clock::time_point start = Clock::now();
    for (uint64_t i = 0; i < runsPerBatch; ++i) run();
    return NanosSince(start);
}

Timing TimeRun(const std::function<void()>& run) {
    uint64_t runsPerBatch = 1;
    double nanos = TimeBatch(run, runsPerBatch);
    while (nanos < kMinimumBatchNanos && runsPerBatch < kMaxBatchRuns) {
        runsPerBatch *= 4;
        nanos = TimeBatch(run, runsPerBatch);
    }
    double best = nanos;
    for (int repeat = 1; repeat < Repeats(); ++repeat)
        best = std::min(best, TimeBatch(run, runsPerBatch));
    return Timing{best / double(runsPerBatch), runsPerBatch};
}

void PrintBaselineDelta(const char* name, double nanosPerUnit) {
    const auto recorded = Baseline().find(name);
    if (recorded == Baseline().end() || recorded->second <= 0.0) {
        std::printf("        -\n");
        return;
    }
    const double change = nanosPerUnit / recorded->second - 1.0;
    std::printf(" %+7.1f%%\n", change * 100.0);
    if (change > Tolerance())
        Fail(name,
            "is " + Decimals(change * 100.0, 0) + "% slower than the recorded baseline (" +
                Decimals(Tolerance() * 100.0, 0) +
                "% allowed) - profile the change, re-run on an idle machine, or re-record with "
                "'make perf-baseline' if the slowdown is intended");
}

void Record(const char* name, double nanosPerUnit, double allocationsPerUnit) {
    Rows().push_back(Row{name, nanosPerUnit, allocationsPerUnit});
}

void PrintRow(const char* name, const char* unit, double nanosPerUnit, uint64_t bytesPerRun,
    double nanosPerRun, double allocationsPerUnit) {
    std::printf("%-38s %11s ns/%-8s", name, FormatCost(nanosPerUnit).c_str(), unit);
    if (bytesPerRun != 0 && nanosPerRun > 0.0)
        std::printf(" %9.1f MB/s", double(bytesPerRun) * 1000.0 / nanosPerRun);
    else
        std::printf(" %14s", "");
    std::printf(" %7.2f alloc/%-8s", allocationsPerUnit, unit);
}

void WriteResults() {
    const char* path = std::getenv("DESKHUB_PERF_WRITE");
    if (path == nullptr || path[0] == '\0') return;
    std::FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        std::printf("could not write %s - the run itself is unaffected, only the recording is lost\n",
            path);
        ++g_failures;
        return;
    }
    for (const Row& row : Rows())
        std::fprintf(file, "%s %.4f %.4f\n", row.name.c_str(), row.nanosPerUnit,
            row.allocationsPerUnit);
    std::fclose(file);
    std::printf("recorded %zu measurements to %s\n", Rows().size(), path);
}

uint32_t g_random = 0x1234ABCD;
volatile uint64_t g_consumed = 0;

void WarmUpProcessor() {
    const Clock::time_point start = Clock::now();
    uint64_t spun = 0;
    while (NanosSince(start) < kWarmUpNanos)
        for (int step = 0; step < 100'000; ++step) spun += uint64_t(step) * 3;
    g_consumed = g_consumed + spun;
}

}

uint32_t NextRandom() {
    g_random ^= g_random << 13;
    g_random ^= g_random >> 17;
    g_random ^= g_random << 5;
    return g_random;
}

void FillRandom(std::span<uint8_t> out) {
    for (uint8_t& byte : out) byte = uint8_t(NextRandom());
}

void Consume(uint64_t value) {
    g_consumed = g_consumed + value;
}

void Begin(const char* title, const char* defaultBaselinePath) {
    g_defaultBaselinePath = defaultBaselinePath;
    std::printf("=== %s ===\n", title);
    WarmUpProcessor();
    std::printf("release build only - debug and sanitizer numbers say nothing about production speed\n");
    Baseline();
    std::printf("\n%-38s %-23s %-14s %-22s %s\n", "workload", "cost per unit", "throughput",
        "allocations", "vs baseline");
}

void BeginGroup(const char* title) {
    std::printf("\n--- %s ---\n", title);
}

void Measure(const Workload& workload) {
    if (!workload.run || workload.unitsPerRun == 0) {
        Fail(workload.name, "needs a body and at least one unit of work per run");
        return;
    }

    workload.run();

    const uint64_t callsBefore = g_allocationCalls;
    workload.run();
    const double allocationsPerUnit =
        double(g_allocationCalls - callsBefore) / double(workload.unitsPerRun);

    const Timing timing = TimeRun(workload.run);
    const double nanosPerUnit = timing.nanosPerRun / double(workload.unitsPerRun);

    PrintRow(workload.name, workload.unit, nanosPerUnit, workload.bytesPerRun,
        timing.nanosPerRun, allocationsPerUnit);
    PrintBaselineDelta(workload.name, nanosPerUnit);
    Record(workload.name, nanosPerUnit, allocationsPerUnit);

    if (workload.maxAllocationsPerUnit >= 0.0 &&
        allocationsPerUnit > workload.maxAllocationsPerUnit + 1e-9)
        Fail(workload.name,
            "allocates " + Decimals(allocationsPerUnit, 2) + " times per " +
                std::string(workload.unit) + ", budget " +
                Decimals(workload.maxAllocationsPerUnit, 2) +
                " - this path runs per frame or per packet, so a fresh allocation costs more "
                "than the work it does; reuse the buffer instead");
}

void MeasureScaling(const ScalingWorkload& workload) {
    if (!workload.run || workload.smallUnits == 0 || workload.largeUnits <= workload.smallUnits ||
        workload.maxTimeRatio <= 0.0) {
        Fail(workload.name, "needs a body, largeUnits above smallUnits and a time ratio ceiling");
        return;
    }

    const std::function<void()> small = [&workload] { workload.run(workload.smallUnits); };
    const std::function<void()> large = [&workload] { workload.run(workload.largeUnits); };
    small();
    large();

    double timeRatio = 0.0;
    double nanosPerUnit = 0.0;
    for (int attempt = 0; attempt < kScalingAttempts; ++attempt) {
        const Timing smallTiming = TimeRun(small);
        const Timing largeTiming = TimeRun(large);
        if (smallTiming.nanosPerRun <= 0.0) {
            Fail(workload.name, "measured no time at all for the small case");
            return;
        }
        const double ratio = largeTiming.nanosPerRun / smallTiming.nanosPerRun;
        if (attempt == 0 || ratio < timeRatio) {
            timeRatio = ratio;
            nanosPerUnit = largeTiming.nanosPerRun / double(workload.largeUnits);
        }
        if (timeRatio <= workload.maxTimeRatio) break;
    }

    const double workRatio = double(workload.largeUnits) / double(workload.smallUnits);

    std::printf("%-38s %11s ns/%-8s %5.1fx work -> %5.2fx time", workload.name,
        FormatCost(nanosPerUnit).c_str(), workload.unit, workRatio, timeRatio);
    PrintBaselineDelta(workload.name, nanosPerUnit);
    Record(workload.name, nanosPerUnit, 0.0);

    if (timeRatio > workload.maxTimeRatio)
        Fail(workload.name,
            "costs " + Decimals(timeRatio, 2) + "x the time for " + Decimals(workRatio, 1) +
                "x the work (ceiling " + Decimals(workload.maxTimeRatio, 1) +
                "x) - the path stopped scaling linearly, which is what turns a busy session into "
                "a stall");
}

int Summary() {
    std::printf("\n");
    WriteResults();
    if (g_failures == 0) {
        std::printf("=== PASS: %zu measurements within budget ===\n", Rows().size());
        return 0;
    }
    std::printf("=== FAIL: %d of %zu measurements outside budget ===\n", g_failures, Rows().size());
    return 1;
}

}
