#include "PerfHarness.h"

void RunQuicPerf();
void RunUdpPerf();

int main() {
    deskhub::perf::Begin("platform performance suite (loopback sockets, real QUIC)",
        "out/perf/platform-baseline.txt");

    RunUdpPerf();
    RunQuicPerf();

    return deskhub::perf::Summary();
}
