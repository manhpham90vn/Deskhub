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
    : prefill_(ClampPrefill(targetDelayMs, kMinPrefillFrames, kMaxPrefillFrames)) {}

bool AudioJitterBuffer::TooLate(uint32_t seq) const {
    return playing_ && int32_t(seq - nextSeq_) < 0;
}

void AudioJitterBuffer::Push(const AudioPacketView& pkt) {
    if (pkt.payload.empty()) return;
    ++stats_.framesReceived;

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
        const auto following = held_.find(nextSeq_);
        if (following != held_.end()) {
            concealed.recovery = following->second.payload;
            ++stats_.framesRecoverable;
        }
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
}

}
