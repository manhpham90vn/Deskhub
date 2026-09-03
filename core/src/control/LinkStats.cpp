#include "deskhub/control/LinkStats.h"

#include <cmath>

namespace deskhub {

LinkWindow LinkStats::Close(const Reassembler::Stats& cur, uint64_t videoBytes,
    uint32_t renderedFrames, uint64_t nowUs) {
    LinkWindow w;
    const uint64_t elapsedUs = nowUs - lastUs_;
    w.secs = elapsedUs / 1e6;

    w.packetsReceived = cur.packetsReceived - prev_.packetsReceived;
    w.packetsLost = cur.packetsLost - prev_.packetsLost;
    w.packetsRecovered = cur.packetsRecovered - prev_.packetsRecovered;
    w.fecReceived = cur.fecReceived - prev_.fecReceived;
    w.framesDropped = cur.framesDropped - prev_.framesDropped;

    for (size_t i = 0; i < 7; ++i) {
        w.lossRuns[i] = cur.lossRuns[i] - prev_.lossRuns[i];
        w.lossRunTotal += w.lossRuns[i];
    }
    w.lossRunMax = cur.lossRunMax;

    w.packetsEverAbsent = cur.packetsEverAbsent - prev_.packetsEverAbsent;
    w.packetsNeverArrived = cur.packetsNeverArrived - prev_.packetsNeverArrived;
    w.packetsRepairedByFec = cur.packetsRepairedByFec - prev_.packetsRepairedByFec;
    w.packetsRepairedAfterNack = cur.packetsRepairedAfterNack - prev_.packetsRepairedAfterNack;
    w.packetsReordered = cur.packetsReordered - prev_.packetsReordered;
    for (size_t i = 0; i < 7; ++i) {
        w.absentRuns[i] = cur.absentRuns[i] - prev_.absentRuns[i];
        w.absentRunTotal += w.absentRuns[i];
    }
    w.absentRunMax = cur.absentRunMax;

    w.latePackets = cur.latePackets - prev_.latePackets;
    const uint64_t lateMsInWin = cur.lateMsSum - prev_.lateMsSum;
    w.lateMsAvg = w.latePackets ? double(lateMsInWin) / double(w.latePackets) : 0.0;
    w.lateMsMax = cur.lateMsMax;

    const uint64_t seen = w.packetsReceived + w.packetsLost;
    w.lossPct = seen ? 100.0 * double(w.packetsLost) / double(seen) : 0.0;

    const uint64_t onWire = w.packetsReceived + w.packetsNeverArrived;
    const uint64_t lostOnWire = w.packetsEverAbsent - w.packetsReordered;
    w.wireLossPct = onWire ? 100.0 * double(lostOnWire) / double(onWire) : 0.0;

    if (w.secs > 0.0) {
        w.fps = renderedFrames / w.secs;
        w.kbps = videoBytes * 8.0 / 1000.0 / w.secs;
    }

    prev_ = cur;
    lastUs_ = nowUs;
    return w;
}

Feedback MakeFeedback(const LinkWindow& w, uint32_t rttUs) {
    Feedback fb;
    fb.lostFrames = uint16_t(w.framesDropped);
    fb.lossPct = uint8_t(std::lround(w.lossPct));
    fb.rttMs = uint16_t((rttUs + 500) / 1000);
    fb.recvBitrateKbps = uint32_t(w.kbps);
    return fb;
}

}
