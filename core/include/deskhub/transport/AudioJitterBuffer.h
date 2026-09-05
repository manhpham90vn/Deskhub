#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace deskhub {

inline constexpr uint32_t kAudioFrameMs = 20;
inline constexpr uint64_t kAudioFrameUs = kAudioFrameMs * 1000ull;
inline constexpr uint32_t kDefaultAudioDelayMs = 60;
inline constexpr uint32_t kMaxAudioDelayMs = 500;

class AudioJitterBuffer {
public:
    struct Frame {
        uint32_t seq = 0;
        uint64_t timestampUs = 0;
        bool concealed = false;
        std::vector<uint8_t> payload;
    };

    struct Stats {
        uint64_t framesReceived = 0;
        uint64_t framesPlayed = 0;
        uint64_t framesConcealed = 0;
        uint64_t framesLate = 0;
        uint64_t framesDuplicate = 0;
        uint64_t framesDropped = 0;
        uint64_t underruns = 0;
        uint64_t resyncs = 0;
    };

    explicit AudioJitterBuffer(uint32_t targetDelayMs = kDefaultAudioDelayMs);

    void Push(const AudioPacketView& pkt, uint64_t arrivedUs = 0);

    void SetAdaptiveTarget(bool on) {
        adaptive_ = on;
    }

    bool adaptiveTarget() const {
        return adaptive_;
    }

    uint32_t jitterMs() const {
        return uint32_t(jitterUs_ / 1000);
    }

    std::optional<Frame> Pop();

    void Reset();

    uint32_t targetDelayMs() const {
        return prefill_ * kAudioFrameMs;
    }

    size_t buffered() const {
        return held_.size();
    }

    bool playing() const {
        return playing_;
    }

    const Stats& stats() const {
        return stats_;
    }

private:
    static constexpr uint32_t kMinPrefillFrames = 1;
    static constexpr uint32_t kJitterFilterShift = 4;
    static constexpr uint32_t kJitterDelayMultiple = 3;
    static constexpr uint32_t kMaxAdaptiveDelayMs = 200;
    static constexpr uint32_t kMaxPrefillFrames = 25;
    static constexpr size_t kBurstDropThreshold = 3;
    static constexpr uint32_t kResyncGapFrames = 100;

    bool TooLate(uint32_t seq) const;
    void NoteArrival(const AudioPacketView& pkt, uint64_t arrivedUs);

    std::map<uint32_t, Frame> held_;
    uint32_t prefill_;
    uint32_t basePrefill_;
    bool adaptive_ = false;
    uint64_t jitterUs_ = 0;
    uint64_t lastArrivedUs_ = 0;
    uint64_t lastTimestampUs_ = 0;
    uint32_t nextSeq_ = 0;
    bool playing_ = false;
    Stats stats_;
};

}
