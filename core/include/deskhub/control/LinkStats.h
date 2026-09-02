#pragma once
#include <cstdint>

#include "deskhub/transport/Reassembler.h"
#include "deskhub/protocol/Wire.h"

namespace deskhub {

struct LinkWindow {
    double secs = 0.0;
    double fps = 0.0;
    double kbps = 0.0;
    double lossPct = 0.0;

    uint64_t packetsReceived = 0;
    uint64_t packetsLost = 0;
    uint64_t packetsRecovered = 0;
    uint64_t fecReceived = 0;
    uint64_t framesDropped = 0;

    uint64_t lossRuns[7] = {};
    uint64_t lossRunTotal = 0;
    uint64_t lossRunMax = 0;

    uint64_t latePackets = 0;
    double lateMsAvg = 0.0;
    uint64_t lateMsMax = 0;
};

class LinkStats {
public:
    explicit LinkStats(uint64_t startUs, uint64_t windowUs = 1'000'000)
        : lastUs_(startUs), windowUs_(windowUs) {}

    bool Due(uint64_t nowUs) const {
        return nowUs - lastUs_ >= windowUs_;
    }

    LinkWindow Close(const Reassembler::Stats& cur, uint64_t videoBytes,
        uint32_t renderedFrames, uint64_t nowUs);

private:
    Reassembler::Stats prev_{};
    uint64_t lastUs_;
    uint64_t windowUs_;
};

Feedback MakeFeedback(const LinkWindow& w, uint32_t rttUs);

}
