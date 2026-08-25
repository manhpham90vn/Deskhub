#include "deskhub/transport/Reassembler.h"

#include <iterator>

namespace deskhub {

Reassembler::Pending* Reassembler::Slot(uint32_t id, uint16_t pktCount,
    uint64_t timestampUs, uint64_t nowUs) {
    if (haveBarrier_ && id <= barrierId_) return nullptr;

    auto it = pending_.find(id);
    if (it == pending_.end()) {
        while (pending_.size() >= kMaxPendingFrames)
            Drop(pending_.begin(), DropReason::Evicted, nowUs);
        it = pending_.emplace(id, Pending{}).first;
        Pending& f = it->second;
        f.pktCount = pktCount;
        f.pieces.resize(pktCount);
        f.timestampUs = timestampUs;
        f.firstSeenUs = nowUs;
    }
    if (it->second.pktCount != pktCount) return nullptr;
    it->second.lastPushUs = nowUs;
    return &it->second;
}

void Reassembler::Push(const VideoPacketView& pkt, uint64_t nowUs) {
    ++stats_.packetsReceived;

    if (lastPushUs_) {
        const uint32_t gapMs = uint32_t((nowUs - lastPushUs_) / 1000);
        if (gapMs > maxGapMs_) maxGapMs_ = gapMs;
    }
    lastPushUs_ = nowUs;

    if (pkt.payload.empty()) return;

    if (haveBarrier_ && pkt.hdr.frameId <= barrierId_)
        NoteLatePacket(pkt.hdr.frameId, nowUs);

    Pending* fp = Slot(pkt.hdr.frameId, pkt.hdr.pktCount, pkt.hdr.timestampUs, nowUs);
    if (!fp) return;
    Pending& f = *fp;
    if (pkt.hdr.pktIndex >= f.pktCount) return;
    if (pkt.hdr.pktIndex > f.maxIndexSeen) f.maxIndexSeen = pkt.hdr.pktIndex;
    auto& slot = f.pieces[pkt.hdr.pktIndex];
    if (!slot.empty()) return;
    slot.assign(pkt.payload.begin(), pkt.payload.end());
    f.bytes += slot.size();
    f.idr = f.idr || pkt.idr;
    ++f.received;

    const size_t numGroups = (size_t(f.pktCount) + kFecGroupSize - 1) / kFecGroupSize;
    if (numGroups) TryRecover(f, uint8_t(pkt.hdr.pktIndex % numGroups));
}

void Reassembler::PushFec(const FecPacketView& pkt, uint64_t nowUs) {
    ++stats_.fecReceived;
    if (pkt.parity.size() < kFecLenPrefix) return;

    Pending* fp = Slot(pkt.hdr.frameId, pkt.hdr.pktCount, pkt.hdr.timestampUs, nowUs);
    if (!fp) return;
    Pending& f = *fp;
    f.idr = f.idr || pkt.idr;

    auto& slot = f.parity[pkt.hdr.groupIndex];
    if (!slot.empty()) return;
    slot.assign(pkt.parity.begin(), pkt.parity.end());
    TryRecover(f, pkt.hdr.groupIndex);
}

bool Reassembler::TryRecover(Pending& f, uint8_t group) {
    auto pit = f.parity.find(group);
    if (pit == f.parity.end()) return false;
    const std::vector<uint8_t>& par = pit->second;

    const size_t numGroups = (size_t(f.pktCount) + kFecGroupSize - 1) / kFecGroupSize;
    if (numGroups == 0 || group >= numGroups) return false;

    size_t missing = 0, missingIdx = 0;
    for (size_t i = group; i < f.pktCount; i += numGroups)
        if (f.pieces[i].empty()) {
            ++missing;
            missingIdx = i;
        }
    if (missing != 1) return false;

    std::vector<uint8_t> rec(par);
    for (size_t i = group; i < f.pktCount; i += numGroups) {
        if (i == missingIdx) continue;
        const auto& p = f.pieces[i];
        if (kFecLenPrefix + p.size() > rec.size()) return false;
        rec[0] ^= uint8_t(p.size() >> 8);
        rec[1] ^= uint8_t(p.size() & 0xFF);
        for (size_t b = 0; b < p.size(); ++b) rec[kFecLenPrefix + b] ^= p[b];
    }

    const size_t len = (size_t(rec[0]) << 8) | rec[1];
    if (len == 0 || len > kMaxVideoPayload || kFecLenPrefix + len > rec.size()) return false;

    f.pieces[missingIdx].assign(rec.begin() + kFecLenPrefix,
        rec.begin() + kFecLenPrefix + len);
    f.bytes += len;
    ++f.received;
    ++stats_.packetsRecovered;
    return true;
}

std::optional<Reassembler::Frame> Reassembler::PopReady(uint64_t nowUs) {
    while (!pending_.empty()) {
        auto head = pending_.begin();
        Pending& f = head->second;

        if (f.Complete()) {
            if (waitingForIdr_ && !f.idr) {
                Drop(head, DropReason::PreIdr, nowUs);
                continue;
            }
            Frame out;
            out.frameId = head->first;
            out.timestampUs = f.timestampUs;
            out.idr = f.idr;
            out.firstSeenUs = f.firstSeenUs;
            out.nal.reserve(f.bytes);
            for (const auto& p : f.pieces)
                out.nal.insert(out.nal.end(), p.begin(), p.end());
            haveBarrier_ = true;
            barrierId_ = head->first;
            waitingForIdr_ = false;
            ++stats_.framesCompleted;
            pending_.erase(head);
            return out;
        }

        const uint64_t activityUs =
            f.lastNackUs > f.lastPushUs ? f.lastNackUs : f.lastPushUs;
        if (nowUs - f.firstSeenUs > HardTimeoutUs() || nowUs - activityUs > StallTimeoutUs()) {
            Drop(head, DropReason::Timeout, nowUs);
            continue;
        }
        if (!f.idr) {
            size_t newerComplete = 0;
            for (auto n = std::next(head); n != pending_.end(); ++n)
                if (n->second.Complete()) ++newerComplete;
            if (newerComplete >= 2) {
                Drop(head, DropReason::Overtaken, nowUs);
                continue;
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

size_t Reassembler::PlanNack(uint64_t nowUs, uint64_t rttUs, uint32_t& frameId,
    std::span<uint16_t> out) {
    if (out.empty()) return 0;
    for (auto& [id, f] : pending_) {
        if (f.Complete()) continue;
        if (nowUs - f.firstSeenUs >= HardTimeoutUs()) continue;
        if (nowUs - f.firstSeenUs < kNackHoldUs) return 0;
        const uint64_t interval = rttUs > kNackMinIntervalUs ? rttUs : kNackMinIntervalUs;
        if (f.lastNackUs && nowUs - f.lastNackUs < interval) return 0;

        const bool tailStalled = nowUs - f.lastPushUs >= frameIntervalUs_;
        const uint16_t scanLimit = tailStalled ? f.pktCount : f.maxIndexSeen;
        size_t n = 0;
        for (uint16_t i = 0; i < scanLimit && n < out.size(); ++i)
            if (f.pieces[i].empty()) out[n++] = i;
        if (n == 0) return 0;
        f.lastNackUs = nowUs;
        frameId = id;
        return n;
    }
    return 0;
}

bool Reassembler::TakeLossEvent() {
    const bool e = lossEvent_;
    lossEvent_ = false;
    return e;
}

void Reassembler::Drop(PendingMap::iterator it, DropReason reason, uint64_t nowUs) {
    const Pending& f = it->second;
    const bool loss = reason != DropReason::PreIdr;

    FrameDropInfo info;
    info.frameId = it->first;
    info.reason = reason;
    info.total = f.pktCount;
    info.idr = f.idr;
    info.waitedMs = uint32_t((nowUs - f.firstSeenUs) / 1000);
    info.bytesGot = uint32_t(f.bytes);

    if (loss) {
        ++stats_.framesDropped;
        stats_.packetsLost += uint64_t(it->second.pktCount - it->second.received);

        size_t run = 0;
        bool anyMissing = false;
        for (size_t i = 0; i <= f.pktCount; ++i) {
            const bool gone = i < f.pktCount && f.pieces[i].empty();
            if (gone) {
                ++info.missing;
                if (!anyMissing) {
                    anyMissing = true;
                    info.firstMissing = uint16_t(i);
                }
                info.lastMissing = uint16_t(i);
                ++run;
            } else if (run) {
                size_t b = 0;
                if (run <= 3)
                    b = run - 1;
                else if (run < 8)
                    b = 3;
                else if (run < 16)
                    b = 4;
                else if (run < 32)
                    b = 5;
                else
                    b = 6;
                ++stats_.lossRuns[b];
                if (run > stats_.lossRunMax) stats_.lossRunMax = run;
                run = 0;
            }
        }

        graveyard_[graveNext_] = Grave{it->first, nowUs};
        graveNext_ = (graveNext_ + 1) % kGraveyardSize;

        lossEvent_ = true;
        ++stats_.lossEvents;
    } else {
        ++stats_.framesSkipped;
    }
    if (!haveBarrier_ || it->first > barrierId_) {
        haveBarrier_ = true;
        barrierId_ = it->first;
    }
    const uint32_t droppedId = it->first;
    pending_.erase(it);

    if (onFrameDrop) onFrameDrop(info);
    if (loss && onReferenceLost) onReferenceLost(droppedId);
}

void Reassembler::NoteLatePacket(uint32_t id, uint64_t nowUs) {
    for (const Grave& g : graveyard_) {
        if (g.dropUs == 0 || g.frameId != id) continue;
        ++stats_.latePackets;
        const uint64_t lateMs = (nowUs - g.dropUs) / 1000;
        stats_.lateMsSum += lateMs;
        if (lateMs > stats_.lateMsMax) stats_.lateMsMax = lateMs;
        return;
    }
}

}
