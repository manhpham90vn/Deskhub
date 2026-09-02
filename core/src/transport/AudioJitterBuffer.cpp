#include "deskhub/transport/AudioJitterBuffer.h"

#include <algorithm>

namespace deskhub {

namespace {

uint32_t ClampPrefill(uint32_t targetDelayMs, uint32_t minFrames, uint32_t maxFrames) {
    const uint32_t frames = targetDelayMs / kAudioFrameMs;
    return std::clamp(frames, minFrames, maxFrames);
}

}

AudioJitterBuffer::AudioJitterBuffer(uint32_t targetDelayMs)
    : prefill_(ClampPrefill(targetDelayMs, kMinPrefillFrames, kMaxPrefillFrames)),
      basePrefill_(prefill_) {}

void AudioJitterBuffer::NoteArrival(const AudioPacketView& pkt, uint64_t arrivedUs) {
    if (!arrivedUs) return;
    if (lastArrivedUs_ && lastTimestampUs_ && pkt.hdr.timestampUs > lastTimestampUs_) {
        const int64_t sent = int64_t(pkt.hdr.timestampUs) - int64_t(lastTimestampUs_);
        const int64_t arrived = int64_t(arrivedUs) - int64_t(lastArrivedUs_);
        const int64_t drift = arrived - sent;
        const uint64_t spread = uint64_t(drift < 0 ? -drift : drift);
        jitterUs_ = uint64_t(int64_t(jitterUs_) +
                             ((int64_t(spread) - int64_t(jitterUs_)) >> kJitterFilterShift));
    }
    lastArrivedUs_ = arrivedUs;
    lastTimestampUs_ = pkt.hdr.timestampUs;

    if (!adaptive_) return;
    const uint32_t wantMs = uint32_t(
        std::min<uint64_t>(kAudioFrameMs + jitterUs_ * kJitterDelayMultiple / 1000,
            kMaxAdaptiveDelayMs));
    const uint32_t want = ClampPrefill(wantMs, kMinPrefillFrames, kMaxPrefillFrames);
    if (want <= prefill_ || !playing_ || held_.size() >= want) prefill_ = want;
}

bool AudioJitterBuffer::TooLate(uint32_t seq) const {
    return playing_ && int32_t(seq - nextSeq_) < 0;
}

void AudioJitterBuffer::Push(const AudioPacketView& pkt, uint64_t arrivedUs) {
    if (pkt.payload.empty()) return;
    ++stats_.framesReceived;
    NoteArrival(pkt, arrivedUs);

    const int32_t ahead = int32_t(pkt.hdr.seq - nextSeq_);
    if (playing_ && (ahead > int32_t(kResyncGapFrames) || ahead < -int32_t(kResyncGapFrames))) {
        ++stats_.resyncs;
        Reset();
    }
    if (TooLate(pkt.hdr.seq)) {
        ++stats_.framesLate;
        return;
    }
    if (held_.find(pkt.hdr.seq) != held_.end()) {
        ++stats_.framesDuplicate;
        return;
    }

    Frame f;
    f.seq = pkt.hdr.seq;
    f.timestampUs = pkt.hdr.timestampUs;
    f.payload.assign(pkt.payload.begin(), pkt.payload.end());
    held_.emplace(f.seq, std::move(f));

    while (held_.size() > prefill_ + kBurstDropThreshold) {
        held_.erase(held_.begin());
        ++stats_.framesDropped;
        if (playing_) nextSeq_ = held_.begin()->first;
    }
}

std::optional<AudioJitterBuffer::Frame> AudioJitterBuffer::Pop() {
    if (!playing_) {
        if (held_.size() < prefill_) return std::nullopt;
        playing_ = true;
        nextSeq_ = held_.begin()->first;
    }
    if (held_.empty()) {
        ++stats_.underruns;
        playing_ = false;
        return std::nullopt;
    }

    const auto it = held_.find(nextSeq_);
    if (it == held_.end()) {
        Frame concealed;
        concealed.seq = nextSeq_;
        concealed.timestampUs = held_.begin()->second.timestampUs;
        concealed.concealed = true;
        ++nextSeq_;
        ++stats_.framesConcealed;
        return concealed;
    }

    Frame out = std::move(it->second);
    held_.erase(it);
    ++nextSeq_;
    ++stats_.framesPlayed;
    return out;
}

void AudioJitterBuffer::Reset() {
    held_.clear();
    playing_ = false;
    nextSeq_ = 0;
    jitterUs_ = 0;
    lastArrivedUs_ = 0;
    lastTimestampUs_ = 0;
    prefill_ = basePrefill_;
}

}
