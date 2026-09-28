#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/Random.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

void TestARequestForNothingIsRefused() {
    std::printf("[random] a nonsensical request fails loudly instead of half-filling...\n");
    uint8_t byte = 0xAB;
    Check(!RandomBytes(nullptr, 8), "no buffer means no fill");
    Check(!RandomBytes(&byte, 0), "no length means no fill");
    Check(byte == 0xAB, "and the caller's memory is untouched");
}

void TestTheWholeBufferIsFilled() {
    std::printf("[random] every byte asked for is written, not just the first few...\n");
    for (size_t n : {size_t(1), size_t(4), size_t(15), size_t(16), size_t(64), size_t(4096)}) {
        std::vector<uint8_t> buf(n + 2, 0xCD);
        Check(RandomBytes(buf.data() + 1, n), "the fill succeeds");
        Check(buf.front() == 0xCD && buf.back() == 0xCD,
            "and stays inside the buffer it was given");

        bool allSame = true;
        for (size_t i = 1; i + 1 < buf.size(); ++i)
            if (buf[i] != buf[1]) allSame = false;
        Check(!(allSame && n >= 16),
            "a block of 16 or more bytes is not a constant, which a stub fill would be");
    }
}

void TestTheBytesAreSpreadAcrossTheRange() {
    std::printf("[random] the bytes cover the whole range, not one corner of it...\n");
    uint8_t buf[8192];
    Check(RandomBytes(buf, sizeof(buf)), "fill");

    int counts[256] = {};
    for (uint8_t b : buf) ++counts[b];

    int missing = 0;
    for (int c : counts)
        if (c == 0) ++missing;
    Check(missing < 32,
        "with 8 KiB of bytes nearly every value shows up at least once");
}

}

void RunRandomTests() {
    TestARequestForNothingIsRefused();
    TestTheWholeBufferIsFilled();
    TestTheBytesAreSpreadAcrossTheRange();
}
