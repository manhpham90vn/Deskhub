#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/transport/FecScheme.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace deskhub {

class Reassembler {
public:
    struct Frame {
        uint32_t frameId = 0;
        uint64_t timestampUs = 0;
        bool idr = false;
        uint64_t firstSeenUs = 0;
        std::vector<uint8_t> nal;
    };

    enum class DropReason : uint8_t {
        Timeout,
        Overtaken,
        Evicted,
        PreIdr,
    };

    struct FrameDropInfo {
        uint32_t frameId = 0;
        DropReason reason = DropReason::Timeout;
        uint16_t missing = 0, total = 0;
        uint16_t firstMissing = 0, lastMissing = 0;
        bool idr = false;
        uint32_t waitedMs = 0;
        uint32_t bytesGot = 0;
    };

    std::function<void(const FrameDropInfo&)> onFrameDrop;

    std::function<void(uint32_t frameId)> onReferenceLost;

    struct Stats {
        uint64_t packetsReceived = 0;
        uint64_t fecReceived = 0;
        uint64_t framesCompleted = 0;
        uint64_t framesDropped = 0;
        uint64_t framesSkipped = 0;
        uint64_t packetsLost = 0;
        uint64_t lossEvents = 0;
        uint64_t packetsRecovered = 0;

        uint64_t lossRuns[7] = {};
        uint64_t lossRunMax = 0;

        uint64_t packetsEverAbsent = 0;
        uint64_t packetsNeverArrived = 0;
        uint64_t packetsRepairedByFec = 0;
        uint64_t packetsRepairedAfterNack = 0;
        uint64_t packetsReordered = 0;
        uint64_t absentRuns[7] = {};
        uint64_t absentRunMax = 0;

        uint64_t latePackets = 0;
        uint64_t lateMsSum = 0;
        uint64_t lateMsMax = 0;
    };

    explicit Reassembler(uint64_t frameIntervalUs = 16'667)
        : scheme_(MakeFecScheme(kDefaultFecScheme)),
          frameIntervalUs_(frameIntervalUs ? frameIntervalUs : 16'667) {}

    bool SetFecScheme(std::string_view name);

    bool SetFecParityPerGroup(size_t count) {
        return scheme_->SetParityPerGroup(count);
    }

    std::string_view fecScheme() const {
        return scheme_->Name();
    }

    void SetFps(uint32_t fps) {
        if (fps) frameIntervalUs_ = 1'000'000ull / fps;
    }

    void SetRttUs(uint64_t rttUs) {
        rttUs_ = rttUs;
    }

    void SetNackEnabled(bool on) {
        nackEnabled_ = on;
    }

    void SetOvertakenLimit(size_t frames) {
        overtakenLimit_ = frames < kMinOvertakenLimit ? kMinOvertakenLimit : frames;
    }

    void SetStallTimeoutMultiple(uint64_t frames) {
        stallMultiple_ = frames ? frames : 1;
    }

    size_t overtakenLimit() const {
        return OvertakenLimit();
    }

    void Push(const VideoPacketView& pkt, uint64_t nowUs);

    void PushFec(const FecPacketView& pkt, uint64_t nowUs);

    std::optional<Frame> PopReady(uint64_t nowUs);

    size_t PlanNack(uint64_t nowUs, uint64_t rttUs, uint32_t& frameId, std::span<uint16_t> out);

    bool TakeLossEvent();

    bool WaitingForIdr() const {
        return waitingForIdr_;
    }

    const Stats& stats() const {
        return stats_;
    }

    uint32_t TakeMaxGapMs() {
        const uint32_t g = maxGapMs_;
        maxGapMs_ = 0;
        return g;
    }

private:
    struct Pending {
        std::vector<std::vector<uint8_t>> pieces;
        std::map<uint16_t, std::vector<uint8_t>> parity;
        std::vector<bool> absent;
        std::vector<bool> repairedByFec;
        std::vector<bool> nacked;
        uint16_t pktCount = 0;
        uint16_t received = 0;
        uint16_t fecGroups = 0;
        uint16_t maxIndexSeen = 0;
        bool anyIndexSeen = false;
        uint64_t timestampUs = 0;
        bool idr = false;
        uint64_t firstSeenUs = 0;
        uint64_t lastPushUs = 0;
        uint64_t lastNackUs = 0;
        size_t bytes = 0;
        bool Complete() const {
            return pktCount != 0 && received == pktCount;
        }
    };
    using PendingMap = std::map<uint32_t, Pending>;

    void Drop(PendingMap::iterator it, DropReason reason, uint64_t nowUs);
    void NoteWireLoss(const Pending& f);
    Pending* Slot(uint32_t id, uint16_t pktCount, uint64_t timestampUs, uint64_t nowUs);
    bool TryRecover(Pending& f, uint8_t group);
    void NoteLatePacket(uint32_t id, uint64_t nowUs);

    static constexpr size_t kMaxPendingFrames = 8;

    static constexpr uint64_t kHardTimeoutMultiple = 30;
    static constexpr uint64_t kRetransmitRttMultiple = 3;
    static constexpr uint64_t kRetransmitRttDivisor = 2;

    static constexpr uint64_t kNackHoldUs = 2'000;
    static constexpr uint64_t kNackMinIntervalUs = 10'000;
    static constexpr size_t kMinOvertakenLimit = 2;

    uint64_t RepairWindowUs() const {
        return kNackHoldUs + rttUs_ * kRetransmitRttMultiple / kRetransmitRttDivisor;
    }

    size_t OvertakenLimit() const {
        if (!nackEnabled_ || overtakenLimit_ <= kMinOvertakenLimit) return kMinOvertakenLimit;
        const uint64_t frames = (RepairWindowUs() + frameIntervalUs_ - 1) / frameIntervalUs_;
        const size_t want = size_t(frames) + 1;
        if (want < kMinOvertakenLimit) return kMinOvertakenLimit;
        return want < overtakenLimit_ ? want : overtakenLimit_;
    }

    uint64_t StallTimeoutUs() const {
        const uint64_t paced = stallMultiple_ * frameIntervalUs_;
        const uint64_t retransmit = RepairWindowUs();
        const uint64_t wait = paced > retransmit ? paced : retransmit;
        const uint64_t hard = HardTimeoutUs();
        return wait < hard ? wait : hard;
    }
    uint64_t HardTimeoutUs() const {
        return kHardTimeoutMultiple * frameIntervalUs_;
    }

    PendingMap pending_;
    std::unique_ptr<FecScheme> scheme_;
    std::vector<FecSlot> slots_;
    std::vector<std::span<const uint8_t>> parityView_;
    uint64_t frameIntervalUs_;
    uint64_t rttUs_ = 0;
    bool nackEnabled_ = false;
    size_t overtakenLimit_ = kMinOvertakenLimit;
    uint64_t stallMultiple_ = 2;
    bool waitingForIdr_ = true;
    bool lossEvent_ = false;
    bool haveBarrier_ = false;
    uint32_t barrierId_ = 0;
    Stats stats_;

    struct Grave {
        uint32_t frameId = 0;
        uint64_t dropUs = 0;
    };
    static constexpr size_t kGraveyardSize = 16;
    Grave graveyard_[kGraveyardSize];
    size_t graveNext_ = 0;

    uint64_t lastPushUs_ = 0;
    uint32_t maxGapMs_ = 0;
};

}
