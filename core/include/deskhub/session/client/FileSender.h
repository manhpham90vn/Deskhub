#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/FileTransfer.h"
#include "deskhub/transfer/Crc32.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace deskhub {

enum class FileSenderState : uint8_t {
    Idle = 0,
    Offering = 1,
    Sending = 2,
    Done = 3,
    Refused = 4,
    Failed = 5,
};

struct FileSenderCallbacks {
    std::function<bool(std::span<const uint8_t> message)> send;
    std::function<size_t(uint16_t fileIndex, uint64_t offset, std::span<uint8_t> out)> read;
    std::function<void(const TransferProgress&)> onProgress;
    std::function<void(FileSenderState, TransferReason)> onFinished;
};

class FileSender {
public:
    explicit FileSender(FileSenderCallbacks cb) : cb_(std::move(cb)) {}

    bool Offer(uint32_t batchId, std::vector<TransferFile> files);
    void HandleMessage(std::span<const uint8_t> message);
    size_t Pump(size_t maxChunks);
    void Cancel();
    void LinkLost();

    FileSenderState State() const {
        return state_;
    }
    TransferReason Reason() const {
        return reason_;
    }
    bool Busy() const {
        return state_ == FileSenderState::Offering || state_ == FileSenderState::Sending;
    }
    TransferProgress Progress() const;
    const std::vector<TransferFile>& Files() const {
        return files_;
    }

private:
    bool SendOneChunk();
    bool FinishFile();
    void Finish(FileSenderState state, TransferReason reason);
    void Abort(TransferReason reason);
    bool Emit(size_t written);
    void Report();

    FileSenderCallbacks cb_;
    FileSenderState state_ = FileSenderState::Idle;
    TransferReason reason_ = TransferReason::Accepted;
    uint32_t batchId_ = 0;
    std::vector<TransferFile> files_{};
    uint16_t index_ = 0;
    size_t acked_ = 0;
    uint64_t sent_ = 0;
    uint64_t batchSent_ = 0;
    uint64_t batchSize_ = 0;
    Crc32 crc_{};
    std::vector<uint8_t> buf_ = std::vector<uint8_t>(kMaxRecordSize);
    std::vector<uint8_t> chunk_ = std::vector<uint8_t>(kMaxFileChunkBytes);
};

}
