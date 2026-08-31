#include "deskhubp/host/FileHost.h"

#include "deskhubp/diag/Log.h"

#include <utility>

namespace deskhubp {

namespace {

std::string Utf8Of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

}

FileHost::~FileHost() {
    try {
        Stop();
    } catch (...) {
        peers_.clear();
    }
}

bool FileHost::Start(SessionTransport& sock, const std::filesystem::path& dir,
    FileHostCallbacks callbacks) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load(std::memory_order_acquire)) return true;

    FileStore probe;
    if (!probe.SetDirectory(dir)) {
        LOGE("[Host] Files cannot be taken in: %s is not writable.", Utf8Of(dir).c_str());
        return false;
    }

    sock_ = &sock;
    cb_ = std::move(callbacks);
    dir_ = dir;
    peers_.clear();
    running_.store(true, std::memory_order_release);
    accepting_.store(true, std::memory_order_release);
    LOGI("[Host] Files sent by viewers land in %s.", Utf8Of(dir_).c_str());
    return true;
}

void FileHost::Stop() {
    const std::lock_guard<std::mutex> flushOrder(outboxMutex_);
    std::vector<std::string> lines;
    std::vector<OutboundRecord> outbound;
    SessionTransport* sock = nullptr;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.exchange(false, std::memory_order_acq_rel)) return;
        accepting_.store(false, std::memory_order_release);
        for (auto& [packed, peer] : peers_)
            if (peer->receiver) peer->receiver->SetAccepting(false);
        sock = sock_;
        outbound = TakeOutbox();
        for (auto& [packed, peer] : peers_)
            if (peer->receiver) peer->receiver->LinkLost();
        peers_.clear();
        sock_ = nullptr;
        outbox_.clear();
        lines = TakeAudit();
    }
    SendOutbox(sock, outbound);
    for (const std::string& line : lines)
        if (cb_.onAudit) cb_.onAudit(line);
}

void FileHost::SetAccepting(bool on) {
    const std::lock_guard<std::mutex> flushOrder(outboxMutex_);
    std::vector<std::string> lines;
    std::vector<OutboundRecord> outbound;
    SessionTransport* sock = nullptr;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        accepting_.store(on, std::memory_order_release);
        for (auto& [packed, peer] : peers_)
            if (peer->receiver) peer->receiver->SetAccepting(on);
        sock = sock_;
        outbound = TakeOutbox();
        lines = TakeAudit();
    }
    SendOutbox(sock, outbound);
    for (const std::string& line : lines)
        if (cb_.onAudit) cb_.onAudit(line);
    if (cb_.onTransfersChanged) cb_.onTransfersChanged();
}

std::filesystem::path FileHost::Directory() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return dir_;
}

std::vector<std::string> FileHost::TakeAudit() {
    std::vector<std::string> lines;
    lines.swap(audit_);
    return lines;
}

std::vector<FileHost::OutboundRecord> FileHost::TakeOutbox() {
    std::vector<OutboundRecord> records;
    records.swap(outbox_);
    return records;
}

void FileHost::SendOutbox(SessionTransport* sock, const std::vector<OutboundRecord>& records) {
    if (sock == nullptr) return;
    for (const OutboundRecord& record : records)
        sock->SendRecordOn(record.addr, kQuicFileStream, record.bytes);
}

FileHost::Peer* FileHost::PeerFor(const NetAddr& from) {
    const uint64_t packed = from.Pack();
    const auto at = peers_.find(packed);
    if (at != peers_.end()) return at->second.get();

    auto peer = std::make_unique<Peer>();
    peer->addr = from;
    peer->endpoint = from.ToString();
    peer->store = std::make_unique<FileStore>();
    peer->store->ShareBacklogWith(&diskBacklog_);
    if (!peer->store->SetDirectory(dir_)) return nullptr;

    deskhub::Fingerprint fingerprint{};
    std::string name;
    if (sock_->PeerAuth(from, fingerprint, name)) peer->name = name;

    Peer* raw = peer.get();
    deskhub::FileReceiverCallbacks hooks;
    hooks.send = [this, raw](std::span<const uint8_t> message) {
        outbox_.push_back(OutboundRecord{raw->addr, {message.begin(), message.end()}});
    };
    hooks.open = [raw](uint16_t index, const std::string& safeName, uint64_t size) {
        return raw->store->Open(index, safeName, size);
    };
    hooks.write = [raw](uint16_t index, std::span<const uint8_t> data) {
        return raw->store->Write(index, data);
    };
    hooks.close = [raw](uint16_t index, bool keep) { return raw->store->Close(index, keep); };
    hooks.onProgress = [raw](const deskhub::TransferProgress& progress) {
        raw->progress = progress;
    };
    hooks.onAudit = [this](std::string_view line) { audit_.emplace_back(line); };

    peer->receiver = std::make_unique<deskhub::FileReceiver>(std::move(hooks));
    peer->receiver->SetPeer(deskhub::TransferPeer{peer->endpoint, peer->name, fingerprint});
    peer->receiver->SetAccepting(accepting_.load(std::memory_order_acquire));

    peers_.emplace(packed, std::move(peer));
    return raw;
}

void FileHost::RefreshLimits(Peer& peer) {
    deskhub::FileReceiverLimits limits;
    const uint64_t free = peer.store->FreeBytes();
    if (free < limits.maxBatchBytes) limits.maxBatchBytes = free;
    if (free < limits.maxFileBytes) limits.maxFileBytes = free;
    peer.receiver->SetLimits(limits);
}

void FileHost::HandleMessage(const NetAddr& from, std::span<const uint8_t> message) {
    const std::lock_guard<std::mutex> flushOrder(outboxMutex_);
    std::vector<std::string> lines;
    std::vector<OutboundRecord> outbound;
    SessionTransport* sock = nullptr;
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!running_.load(std::memory_order_acquire) || sock_ == nullptr) return;
        if (!sock_->Authenticated(from)) return;

        const auto header = deskhub::ParseCommonHeader(message);
        if (!header || header->chan != deskhub::Chan::File) return;

        Peer* peer = PeerFor(from);
        if (peer == nullptr) return;

        if (header->type == deskhub::MsgType::FileOffer) RefreshLimits(*peer);

        const bool wasLive = peer->receiver->Busy();
        peer->receiver->HandleMessage(message);
        peer->live = peer->receiver->Busy();
        peer->reason = peer->receiver->Reason();
        changed = wasLive != peer->live;
        sock = sock_;
        outbound = TakeOutbox();
        lines = TakeAudit();
    }

    SendOutbox(sock, outbound);
    for (const std::string& line : lines) {
        LOGI("%s", line.c_str());
        if (cb_.onAudit) cb_.onAudit(line);
    }
    if (changed && cb_.onTransfersChanged) cb_.onTransfersChanged();
}

void FileHost::OnPeerGone(const NetAddr& peer) {
    const std::lock_guard<std::mutex> lock(goneMutex_);
    gone_.push_back(peer);
}

void FileHost::DrainGone() {
    std::vector<NetAddr> gone;
    {
        const std::lock_guard<std::mutex> lock(goneMutex_);
        gone.swap(gone_);
    }
    for (const NetAddr& peer : gone) DropPeer(peer);
}

void FileHost::DropPeer(const NetAddr& peer) {
    std::vector<std::string> lines;
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const auto at = peers_.find(peer.Pack());
        if (at == peers_.end()) return;
        if (at->second->receiver) at->second->receiver->LinkLost();
        changed = at->second->live;
        peers_.erase(at);
        outbox_.clear();
        lines = TakeAudit();
    }
    for (const std::string& line : lines) {
        LOGI("%s", line.c_str());
        if (cb_.onAudit) cb_.onAudit(line);
    }
    if (changed && cb_.onTransfersChanged) cb_.onTransfersChanged();
}

std::vector<deskhub::TransferRecord> FileHost::Transfers() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::vector<deskhub::TransferRecord> out;
    out.reserve(peers_.size());
    for (const auto& [packed, peer] : peers_) {
        deskhub::TransferRecord record;
        record.peerPacked = packed;
        record.endpoint = peer->endpoint;
        record.peerName = peer->name;
        record.batchId = peer->progress.batchId;
        record.fileIndex = peer->progress.fileIndex;
        record.fileCount = peer->progress.fileCount;
        record.batchBytes = peer->progress.batchBytes;
        record.batchSize = peer->progress.batchSize;
        record.name = peer->progress.name;
        record.live = peer->live;
        record.reason = peer->reason;
        out.push_back(std::move(record));
    }
    return out;
}

}
