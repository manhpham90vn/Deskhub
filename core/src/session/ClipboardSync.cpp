#include "deskhub/session/ClipboardSync.h"

#include <algorithm>

namespace deskhub {

uint64_t ClipboardSync::HashText(std::string_view text) {
    uint64_t h = 14695981039346656037ull;
    for (char c : text) {
        h ^= uint8_t(c);
        h *= 1099511628211ull;
    }
    return h;
}

bool ClipboardSync::OfferLocal(std::string_view text) {
    const std::string bounded = TruncateClipboardText(text);
    if (bounded.empty()) return false;
    const uint64_t hash = HashText(bounded);
    if (haveSentHash_ && hash == lastSentHash_) return false;
    if (haveAppliedHash_ && hash == lastAppliedHash_) return false;
    outgoing_ = bounded;
    ++revision_;
    sendsLeft_ = 1 + kClipboardResendCount;
    lastSentHash_ = hash;
    haveSentHash_ = true;
    return true;
}

size_t ClipboardSync::SendAllChunks(const SendFn& send) {
    const size_t chunkCount =
        (outgoing_.size() + kMaxClipboardChunkPayload - 1) / kMaxClipboardChunkPayload;
    size_t sent = 0;
    for (size_t i = 0; i < chunkCount; ++i) {
        const size_t offset = i * kMaxClipboardChunkPayload;
        const size_t len = std::min(kMaxClipboardChunkPayload, outgoing_.size() - offset);
        ClipboardChunkView chunk;
        chunk.revision = revision_;
        chunk.chunkIndex = uint16_t(i);
        chunk.chunkCount = uint16_t(chunkCount);
        chunk.payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(outgoing_.data()) + offset, len);
        const size_t bytes = BuildClipboardChunk(buf_, sessionId_, chunk);
        if (!bytes) return sent;
        send(std::span<const uint8_t>(buf_, bytes));
        ++sent;
    }
    return sent;
}

size_t ClipboardSync::Flush(uint64_t nowUs, const SendFn& send) {
    if (!sendsLeft_ || outgoing_.empty() || !send) return 0;
    if (sendsLeft_ <= kClipboardResendCount &&
        nowUs - lastSendUs_ < kClipboardResendIntervalUs)
        return 0;
    --sendsLeft_;
    lastSendUs_ = nowUs;
    return SendAllChunks(send);
}

bool ClipboardSync::Accept(const ClipboardChunkView& chunk) {
    if (haveTakenRevision_ && chunk.revision <= lastTakenRevision_) return false;
    if (chunk.revision != inRevision_ || chunk.chunkCount != inChunks_.size()) {
        inRevision_ = chunk.revision;
        inChunks_.assign(chunk.chunkCount, {});
        inReceived_ = 0;
    }
    std::vector<uint8_t>& slot = inChunks_[chunk.chunkIndex];
    if (!slot.empty()) return false;
    if (chunk.payload.empty()) return false;
    slot.assign(chunk.payload.begin(), chunk.payload.end());
    ++inReceived_;
    if (inReceived_ < inChunks_.size()) return false;

    std::string text;
    for (const std::vector<uint8_t>& part : inChunks_)
        text.append(reinterpret_cast<const char*>(part.data()), part.size());
    text = TruncateClipboardText(text);
    if (text.empty()) return false;

    lastTakenRevision_ = inRevision_;
    haveTakenRevision_ = true;
    lastAppliedHash_ = HashText(text);
    haveAppliedHash_ = true;
    completed_ = std::move(text);
    inChunks_.clear();
    inReceived_ = 0;
    return true;
}

std::optional<std::string> ClipboardSync::TakeCompleted() {
    std::optional<std::string> out = std::move(completed_);
    completed_.reset();
    return out;
}

void ClipboardSync::Reset() {
    outgoing_.clear();
    revision_ = 0;
    sendsLeft_ = 0;
    lastSendUs_ = 0;
    haveSentHash_ = false;
    haveAppliedHash_ = false;
    ForgetReceived();
}

void ClipboardSync::ForgetReceived() {
    inRevision_ = 0;
    haveTakenRevision_ = false;
    inChunks_.clear();
    inReceived_ = 0;
    completed_.reset();
}

}
