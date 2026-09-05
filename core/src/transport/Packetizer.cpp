#include "deskhub/transport/Packetizer.h"

namespace deskhub {

Packetizer::Packetizer() : scheme_(MakeFecScheme(kDefaultFecScheme)) {}

bool Packetizer::SetFecScheme(std::string_view name) {
    std::unique_ptr<FecScheme> scheme = MakeFecScheme(name);
    if (!scheme) return false;
    scheme_ = std::move(scheme);
    return true;
}

size_t Packetizer::SendFrame(std::span<const uint8_t> nal, uint32_t frameId,
    uint64_t timestampUs, bool idr, const SendFn& send) {
    if (nal.empty() || !send) return 0;
    const size_t count = (nal.size() + kMaxVideoPayload - 1) / kMaxVideoPayload;
    if (count > 0xFFFF) return 0;
    const size_t numGroups = FecGroupCount(uint16_t(count), uint8_t(groups_));
    const bool fec = fec_ && numGroups && numGroups <= kMaxFecGroups;

    VideoHeader vh;
    vh.frameId = frameId;
    vh.timestampUs = timestampUs;
    vh.pktCount = uint16_t(count);

    for (size_t i = 0; i < count; ++i) {
        const size_t off = i * kMaxVideoPayload;
        const size_t len = (nal.size() - off < kMaxVideoPayload) ? nal.size() - off
                                                                 : kMaxVideoPayload;
        vh.pktIndex = uint16_t(i);
        const bool frameEnd = (i + 1 == count);
        const size_t n = BuildVideoPacket(buf_, sessionId_, vh, idr, frameEnd,
            nal.subspan(off, len));
        if (!n) return 0;
        send(std::span<const uint8_t>(buf_, n));
    }

    if (!fec) return count;

    FecHeader fh;
    fh.frameId = frameId;
    fh.timestampUs = timestampUs;
    fh.pktCount = uint16_t(count);
    fh.groups = uint8_t(groups_);
    for (size_t g = 0; g < numGroups; ++g) {
        members_.clear();
        for (size_t i = g; i < count; i += numGroups) {
            const size_t off = i * kMaxVideoPayload;
            const size_t len = (nal.size() - off < kMaxVideoPayload) ? nal.size() - off
                                                                     : kMaxVideoPayload;
            members_.push_back(nal.subspan(off, len));
        }
        parityOut_.assign(scheme_->ParityPerGroup(), std::span<const uint8_t>{});
        const size_t produced = scheme_->Encode(members_, parityOut_);
        fh.groupIndex = uint8_t(g);
        for (size_t p = 0; p < produced; ++p) {
            fh.parityIndex = uint8_t(p);
            const size_t n = BuildFecPacket(buf_, sessionId_, fh, idr, parityOut_[p]);
            if (!n) return 0;
            send(std::span<const uint8_t>(buf_, n));
        }
    }
    return count;
}

}
