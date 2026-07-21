#include "rgc/control/LinkStats.h"

namespace rgc {

LinkWindow LinkStats::Close(const Reassembler::Stats& cur, uint64_t videoBytes,
                            uint32_t renderedFrames, uint64_t nowUs) {
    LinkWindow w;
    // Dùng độ dài THẬT của cửa sổ chứ không phải windowUs_: vòng lặp client bị
    // recvfrom chặn tới 100ms nên cửa sổ hay dài hơn 1s một chút, chia theo hằng số
    // sẽ thổi phồng fps/kbps.
    const uint64_t elapsedUs = nowUs - lastUs_;
    w.secs = elapsedUs / 1e6;

    w.packetsReceived  = cur.packetsReceived  - prev_.packetsReceived;
    w.packetsLost      = cur.packetsLost      - prev_.packetsLost;
    w.packetsRecovered = cur.packetsRecovered - prev_.packetsRecovered;
    w.framesDropped    = cur.framesDropped    - prev_.framesDropped;

    for (size_t i = 0; i < 7; ++i) {
        w.lossRuns[i] = cur.lossRuns[i] - prev_.lossRuns[i];
        w.lossRunTotal += w.lossRuns[i];
    }
    w.lossRunMax = cur.lossRunMax;

    const uint64_t seen = w.packetsReceived + w.packetsLost;
    w.lossPct = seen ? 100.0 * double(w.packetsLost) / double(seen) : 0.0;

    if (w.secs > 0.0) {
        w.fps  = renderedFrames / w.secs;
        w.kbps = videoBytes * 8.0 / 1000.0 / w.secs;
    }

    prev_  = cur;
    lastUs_ = nowUs;
    return w;
}

Feedback MakeFeedback(const LinkWindow& w, uint32_t rttUs) {
    Feedback fb;
    fb.lostFrames      = uint16_t(w.framesDropped);
    fb.lossPct         = uint8_t(w.lossPct + 0.5); // làm tròn, kênh chỉ có 1 byte
    fb.rttMs           = uint16_t(rttUs / 1000);
    fb.recvBitrateKbps = uint32_t(w.kbps);
    return fb;
}

} // namespace rgc
