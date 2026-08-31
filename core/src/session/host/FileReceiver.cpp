#include "deskhub/session/host/FileReceiver.h"

#include "deskhub/transfer/SafeName.h"

#include <utility>

namespace deskhub {

void FileReceiver::SetAccepting(bool on) {
    accepting_ = on;
    if (!on && Busy()) Abort(TransferReason::NotAccepting, true);
}

void FileReceiver::HandleMessage(std::span<const uint8_t> message) {
    const auto header = ParseCommonHeader(message);
    if (!header || header->chan != Chan::File) return;
    const std::span<const uint8_t> payload = PayloadOf(message);

    switch (header->type) {
        case MsgType::FileOffer: OnOffer(payload); return;
        case MsgType::FileChunk: OnChunk(payload); return;
        case MsgType::FileDone: OnDone(payload); return;
        case MsgType::FileCancel: OnCancel(payload); return;
        default: return;
    }
}

void FileReceiver::OnOffer(std::span<const uint8_t> payload) {
    const auto offer = ParseFileOffer(payload);
    if (!offer) return;

    std::vector<std::string> safeNames;
    const TransferReason verdict = Admit(*offer, safeNames);
    if (verdict != TransferReason::Accepted) {
        Refuse(offer->batchId, verdict);
        return;
    }

    batchId_ = offer->batchId;
    files_ = offer->files;
    names_ = std::move(safeNames);
    stored_.assign(files_.size(), std::string{});
    index_ = 0;
    received_ = 0;
    batchReceived_ = 0;
    batchSize_ = 0;
    for (const TransferFile& f : files_) batchSize_ += f.size;
    fileOpen_ = false;
    crc_.Reset();
    state_ = FileReceiverState::Receiving;
    reason_ = TransferReason::Accepted;

    FileAccept accept;
    accept.batchId = batchId_;
    accept.reason = TransferReason::Accepted;
    Emit(BuildFileAccept(buf_, accept));
    Audit("offered", std::to_string(files_.size()) + " files, " + std::to_string(batchSize_) +
                         " bytes");
    Report();
}

TransferReason FileReceiver::Admit(const FileOffer& offer,
    std::vector<std::string>& safeNames) const {
    if (!accepting_) return TransferReason::NotAccepting;
    if (state_ == FileReceiverState::Receiving) return TransferReason::Busy;
    if (offer.files.empty()) return TransferReason::Corrupt;
    if (offer.files.size() > limits_.maxFiles) return TransferReason::TooManyFiles;

    uint64_t total = 0;
    safeNames.reserve(offer.files.size());
    for (const TransferFile& f : offer.files) {
        if (f.size > limits_.maxFileBytes) return TransferReason::TooLarge;
        total += f.size;
        if (total > limits_.maxBatchBytes) return TransferReason::TooLarge;
        std::string safe = SafeFileName(f.name);
        if (safe.empty()) return TransferReason::BadName;
        safeNames.push_back(std::move(safe));
    }
    return TransferReason::Accepted;
}

void FileReceiver::OnChunk(std::span<const uint8_t> payload) {
    const auto chunk = ParseFileChunk(payload);
    if (!chunk || chunk->batchId != batchId_) return;
    if (state_ != FileReceiverState::Receiving) return;
    if (chunk->fileIndex != index_ || chunk->offset != received_) {
        Abort(TransferReason::Corrupt, true);
        return;
    }
    if (chunk->data.size() > files_[index_].size - received_) {
        Abort(TransferReason::Corrupt, true);
        return;
    }
    if (!fileOpen_ && !OpenCurrent()) return;
    if (!cb_.write || !cb_.write(index_, chunk->data)) {
        Abort(TransferReason::WriteFailed, true);
        return;
    }
    crc_.Update(chunk->data);
    received_ += chunk->data.size();
    batchReceived_ += chunk->data.size();
    Report();
}

void FileReceiver::OnDone(std::span<const uint8_t> payload) {
    const auto done = ParseFileDone(payload);
    if (!done || done->batchId != batchId_) return;
    if (state_ != FileReceiverState::Receiving) return;
    if (done->fileIndex != index_ || received_ != files_[index_].size) {
        Abort(TransferReason::Corrupt, true);
        return;
    }
    if (!fileOpen_ && !OpenCurrent()) return;
    if (done->crc32 != crc_.Value()) {
        CloseCurrent(false);
        Audit("corrupt", names_[index_]);
        Ack(TransferReason::Corrupt);
        Abort(TransferReason::Corrupt, false);
        return;
    }

    if (!CloseCurrent(true)) {
        Audit("write_failed", stored_[index_]);
        Ack(TransferReason::WriteFailed);
        Abort(TransferReason::WriteFailed, false);
        return;
    }
    Audit("stored", stored_[index_]);
    Ack(TransferReason::Accepted);

    ++index_;
    received_ = 0;
    crc_.Reset();
    if (index_ >= files_.size()) {
        state_ = FileReceiverState::Done;
        reason_ = TransferReason::Accepted;
        Audit("complete");
    }
    Report();
}

void FileReceiver::OnCancel(std::span<const uint8_t> payload) {
    const auto cancel = ParseFileCancel(payload);
    if (!cancel || cancel->batchId != batchId_) return;
    if (!Busy()) return;
    Abort(cancel->reason, false);
}

bool FileReceiver::OpenCurrent() {
    std::string stored = cb_.open ? cb_.open(index_, names_[index_], files_[index_].size)
                                  : std::string{};
    if (stored.empty()) {
        Abort(TransferReason::WriteFailed, true);
        return false;
    }
    stored_[index_] = std::move(stored);
    fileOpen_ = true;
    return true;
}

bool FileReceiver::CloseCurrent(bool keep) {
    if (!fileOpen_) return true;
    fileOpen_ = false;
    return cb_.close ? cb_.close(index_, keep) : true;
}

void FileReceiver::Refuse(uint32_t batchId, TransferReason reason) {
    FileAccept accept;
    accept.batchId = batchId;
    accept.reason = reason;
    Emit(BuildFileAccept(buf_, accept));
    if (cb_.onAudit)
        cb_.onAudit(TransferAuditLine(peer_, batchId, "refused", TransferReasonName(reason)));
}

void FileReceiver::Ack(TransferReason reason) {
    FileAck ack;
    ack.batchId = batchId_;
    ack.fileIndex = index_;
    ack.reason = reason;
    Emit(BuildFileAck(buf_, ack));
}

void FileReceiver::Abort(TransferReason reason, bool tellPeer) {
    CloseCurrent(false);
    if (tellPeer) {
        FileCancel cancel;
        cancel.batchId = batchId_;
        cancel.reason = reason;
        Emit(BuildFileCancel(buf_, cancel));
    }
    state_ = FileReceiverState::Failed;
    reason_ = reason;
    Audit("aborted", TransferReasonName(reason));
    Report();
}

void FileReceiver::LinkLost() {
    if (!Busy()) return;
    Abort(TransferReason::LinkLost, false);
}

TransferProgress FileReceiver::Progress() const {
    TransferProgress p;
    p.batchId = batchId_;
    p.fileIndex = index_;
    p.fileCount = uint16_t(files_.size());
    p.fileBytes = received_;
    p.batchBytes = batchReceived_;
    p.batchSize = batchSize_;
    if (index_ < files_.size()) {
        p.fileSize = files_[index_].size;
        p.name = stored_[index_].empty() ? names_[index_] : stored_[index_];
    }
    return p;
}

void FileReceiver::Report() {
    if (cb_.onProgress) cb_.onProgress(Progress());
}

void FileReceiver::Audit(std::string_view what, std::string_view detail) {
    if (cb_.onAudit) cb_.onAudit(TransferAuditLine(peer_, batchId_, what, detail));
}

void FileReceiver::Emit(size_t written) {
    if (written && cb_.send) cb_.send(std::span<const uint8_t>(buf_.data(), written));
}

}
