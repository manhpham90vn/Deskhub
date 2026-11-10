#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/MemoryFootprint.h"

#include <cstdio>
#include <vector>

namespace {

void TestARunningProcessHasAFootprint() {
    std::printf("[memory] a running process reports a positive footprint...\n");
    const int mb = deskhubp::MemoryFootprintMb();
    Check(mb > 0, "the footprint of this test process is at least one megabyte");
    Check(mb < 1'000'000, "and small enough to be megabytes rather than bytes");
}

void TestAllocationsShowUpInTheFootprint() {
    std::printf("[memory] a large allocation is visible in the reading...\n");
    const int before = deskhubp::MemoryFootprintMb();

    std::vector<char> ballast(64 * 1024 * 1024);
    volatile char* touched = ballast.data();
    for (std::size_t i = 0; i < ballast.size(); i += 4096) touched[i] = 1;
    const int during = deskhubp::MemoryFootprintMb();

    Check(during >= before + 32, "64 MB of touched memory raises the reading by at least 32");
    Check(touched[ballast.size() / 2] == 1, "the ballast is really there");
}

}

void RunMemoryFootprintTests() {
    TestARunningProcessHasAFootprint();
    TestAllocationsShowUpInTheFootprint();
}
