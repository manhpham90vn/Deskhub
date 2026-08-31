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

enum class FileReceiverState : uint8_t {
    Idle = 0,
    Receiving = 1,
    Done = 2,
    Failed = 3,
};

struct FileReceiverLimits {
    size_t maxFiles = kMaxTransferFiles;
    uint64_t maxFileBytes = kMaxTransferFileBytes;
    uint64_t maxBatchBytes = kMaxTransferBatchBytes;
};

struct FileReceiverCallbacks {
    std::function<void(std::span<const uint8_t> message)> send;
    std::function<std::string(uint16_t fileIndex, const std::string& safeName, uint64_t size)>
        open;
    std::function<bool(uint16_t fileIndex, std::span<const uint8_t> data)> write;
    std::function<bool(uint16_t fileIndex, bool keep)> close;
    std::function<void(const TransferProgress&)> onProgress;
    std::function<void(std::string_view auditLine)> onAudit;
};

class FileReceiver {
public:
    explicit FileReceiver(FileReceiverCallbacks cb) : cb_(std::move(cb)) {}

    void SetAccepting(bool on);
    void SetLimits(const FileReceiverLimits& limits) {
        limits_ = limits;
    }
    void SetPeer(TransferPeer peer) {
        peer_ = std::move(peer);
    }

    void HandleMessage(std::span<const uint8_t> message);
    void LinkLost();

    bool Accepting() const {
        return accepting_;
    }
    FileReceiverState State() const {
        return state_;
    }
    TransferReason Reason() const {
        return reason_;
    }
    uint32_t BatchId() const {
        return batchId_;
    }
    bool Busy() const {
        return state_ == FileReceiverState::Receiving;
    }
    TransferProgress Progress() const;
    const std::vector<std::string>& Stored() const {
        return stored_;
    }

private:
    void OnOffer(std::span<const uint8_t> payload);
    void OnChunk(std::span<const uint8_t> payload);
    void OnDone(std::span<const uint8_t> payload);
    void OnCancel(std::span<const uint8_t> payload);

    TransferReason Admit(const FileOffer& offer, std::vector<std::string>& safeNames) const;
    bool OpenCurrent();
    bool CloseCurrent(bool keep);
    void Refuse(uint32_t batchId, TransferReason reason);
    void Abort(TransferReason reason, bool tellPeer);
    void Ack(TransferReason reason);
    void Emit(size_t written);
    void Audit(std::string_view what, std::string_view detail = {});
    void Report();

    FileReceiverCallbacks cb_;
    FileReceiverLimits limits_{};
    TransferPeer peer_{};
    bool accepting_ = false;
    FileReceiverState state_ = FileReceiverState::Idle;
    TransferReason reason_ = TransferReason::Accepted;
    uint32_t batchId_ = 0;
    std::vector<TransferFile> files_{};
    std::vector<std::string> names_{};
    std::vector<std::string> stored_{};
    uint16_t index_ = 0;
    uint64_t received_ = 0;
    uint64_t batchReceived_ = 0;
    uint64_t batchSize_ = 0;
    bool fileOpen_ = false;
    Crc32 crc_{};
    std::vector<uint8_t> buf_ = std::vector<uint8_t>(kMaxRecordSize);
};

}
