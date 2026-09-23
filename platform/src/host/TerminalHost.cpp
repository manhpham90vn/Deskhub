#include "deskhubp/host/TerminalHost.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "deskhub/protocol/Wire.h"
#include "deskhub/terminal/Repaint.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace deskhubp {

namespace {

constexpr uint32_t kPumpWaitUs = 2'000;
constexpr uint32_t kPtyWaitMs = 0;
constexpr int kPumpRounds = 8;
constexpr size_t kMaxPendingBytes = size_t{256} * 1024;
constexpr uint64_t kRepaintIntervalUs = 100'000;
constexpr uint64_t kShellReportIntervalUs = 1'000'000;

}

TerminalHost::~TerminalHost() {
    try {
        Stop();
    } catch (...) {
        shells_.clear();
    }
}

bool TerminalHost::Start(SessionTransport& sock, std::string shell,
    TerminalHostCallbacks callbacks) {
    if (Running()) return false;
    if (!sock.IsOpen()) return false;

    {
        const std::lock_guard<std::mutex> lock(mutex_);
        sock_ = &sock;
        shell_ = std::move(shell);
        cb_ = std::move(callbacks);
        sessions_.SetSharing(true);
    }
    {
        const std::lock_guard<std::mutex> lock(goneMutex_);
        gone_.clear();
    }

    stop_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { Loop(); });
    LOGI("terminal host: sharing a shell on the session port");
    return true;
}

void TerminalHost::Stop() {
    if (!Running()) return;
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    running_.store(false, std::memory_order_release);

    const std::lock_guard<std::mutex> lock(mutex_);
    shells_.clear();
    kicks_.clear();
    sessions_.SetSharing(false);
    sock_ = nullptr;
    LOGI("terminal host: stopped sharing");
}

void TerminalHost::KickSession(uint32_t termId) {
    const std::lock_guard<std::mutex> lock(mutex_);
    kicks_.push_back(termId);
}

size_t TerminalHost::SessionCount() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.Count();
}

std::vector<deskhub::TerminalRecord> TerminalHost::Sessions() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.Records();
}

void TerminalHost::Loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        const uint64_t nowUs = NowUs();
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            DrainGone(nowUs);
            DrainKicks();
            PumpShells(nowUs);
        }
        SleepUs(kPumpWaitUs);
    }
}

bool TerminalHost::SendToPeer(const NetAddr& peer, std::span<const uint8_t> message) {
    return sock_ != nullptr && sock_->SendRecord(peer, message);
}

uint32_t TerminalHost::TermIdFor(const NetAddr& peer) const {
    for (const auto& [id, shell] : shells_)
        if (shell.peer == peer) return id;
    return 0;
}

void TerminalHost::HandleMessage(const NetAddr& from, std::span<const uint8_t> message) {
    if (!Running()) return;
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header || header->chan != deskhub::Chan::Terminal) return;
    const std::span<const uint8_t> payload = deskhub::PayloadOf(message);

    const std::lock_guard<std::mutex> lock(mutex_);
    if (sock_ == nullptr) return;

    if (header->type == deskhub::MsgType::TermList) {
        std::vector<uint8_t> out(deskhub::kMaxDatagram);
        out.resize(deskhub::BuildTermListAck(out, sessions_.List()));
        SendToPeer(from, out);
        return;
    }

    if (header->type == deskhub::MsgType::TermClose) {
        const uint32_t asked = header->sessionId != 0 ? header->sessionId : TermIdFor(from);
        const auto target = shells_.find(asked);
        if (target == shells_.end()) return;
        const deskhub::TerminalRecord* record = sessions_.Find(asked);
        const bool anotherMachineIsInIt = record != nullptr &&
                                          record->state == deskhub::TerminalState::Live && !(target->second.peer == from);
        CloseShell(asked, 0, anotherMachineIsInIt);
        return;
    }

    if (header->type == deskhub::MsgType::TermOpen) {
        const std::optional<deskhub::TermOpen> request = deskhub::ParseTermOpen(payload);
        if (!request) return;

        deskhub::TerminalOpenRequest full;
        full.message = *request;
        full.endpoint = from.ToString();
        std::string peerName;
        sock_->PeerAuth(from, full.fingerprint, peerName);

        deskhub::TermOpenAck ack = sessions_.Open(full);
        if (ack.reason == deskhub::TermReason::Accepted && !ack.resumed) {
            Shell shell;
            shell.pty = std::make_unique<Pty>();
            shell.peer = from;
            shell.mirror =
                std::make_unique<deskhub::term::Screen>(deskhub::ClampTermSize(request->size));
            if (!shell.pty->Start(shell_, request->size)) {
                sessions_.Close(ack.termId);
                ack = deskhub::TermOpenAck{};
                ack.reason = deskhub::TermReason::TooManySessions;
            } else {
                shells_.emplace(ack.termId, std::move(shell));
                Audit(ack.termId, "opened");
            }
        } else if (ack.reason == deskhub::TermReason::Accepted && ack.resumed) {
            const auto at = shells_.find(ack.termId);
            if (at == shells_.end()) {
                sessions_.Close(ack.termId);
                ack = deskhub::TermOpenAck{};
                ack.reason = deskhub::TermReason::NoSuchSession;
            } else {
                at->second.peer = from;
                at->second.pty->Resize(request->size);
                at->second.mirror->Resize(deskhub::ClampTermSize(request->size));
                at->second.pending.clear();
                at->second.pendingAt = 0;
                at->second.behind = true;
                at->second.lastRepaintUs = 0;
                Audit(ack.termId, "reattached");
            }
        }

        std::vector<uint8_t> out(deskhub::kMaxDatagram);
        out.resize(deskhub::BuildTermOpenAck(out, ack));
        SendToPeer(from, out);
        if (cb_.onSessionsChanged) cb_.onSessionsChanged();
        return;
    }

    const uint32_t termId = header->sessionId != 0 ? header->sessionId : TermIdFor(from);
    const auto shell = shells_.find(termId);
    if (shell == shells_.end() || !(shell->second.peer == from)) return;

    switch (header->type) {
        case deskhub::MsgType::TermData:
            if (!payload.empty()) shell->second.pty->Write(payload);
            return;
        case deskhub::MsgType::TermResize:
            if (const std::optional<deskhub::TermSize> size = deskhub::ParseTermResize(payload)) {
                sessions_.Resize(termId, *size);
                shell->second.pty->Resize(*size);
                shell->second.mirror->Resize(deskhub::ClampTermSize(*size));
            }
            return;
        default: return;
    }
}

void TerminalHost::OnPeerGone(const NetAddr& peer) {
    if (!Running()) return;
    const std::lock_guard<std::mutex> lock(goneMutex_);
    gone_.push_back(peer);
}

void TerminalHost::DrainGone(uint64_t nowUs) {
    std::vector<NetAddr> gone;
    {
        const std::lock_guard<std::mutex> lock(goneMutex_);
        gone.swap(gone_);
    }
    for (const NetAddr& peer : gone) {
        bool changed = false;
        for (auto& [termId, shell] : shells_) {
            if (!(shell.peer == peer)) continue;
            sessions_.Detach(termId, nowUs);
            Audit(termId, "detached");
            changed = true;
        }
        if (changed && cb_.onSessionsChanged) cb_.onSessionsChanged();
    }
}

bool TerminalHost::FlushPending(uint32_t termId, Shell& shell, std::vector<uint8_t>& scratch) {
    while (shell.pendingAt < shell.pending.size()) {
        const size_t take = std::min<size_t>(deskhub::kMaxTermDataBytes,
            shell.pending.size() - shell.pendingAt);
        const size_t written = deskhub::BuildTermData(scratch, termId,
            std::span<const uint8_t>(shell.pending.data() + shell.pendingAt, take));
        if (written == 0) {
            shell.behind = true;
            break;
        }
        if (!SendToPeer(shell.peer, std::span<const uint8_t>(scratch.data(), written))) {
            ++shell.sendFails;
            return false;
        }
        shell.sentBytes += take;
        shell.pendingAt += take;
    }
    shell.pending.clear();
    shell.pendingAt = 0;
    return true;
}

void TerminalHost::QueueForPeer(Shell& shell, std::span<const uint8_t> bytes) {
    if (shell.behind) {
        shell.droppedBytes += bytes.size();
        return;
    }
    if (shell.pending.size() - shell.pendingAt + bytes.size() > kMaxPendingBytes) {
        shell.droppedBytes += shell.pending.size() - shell.pendingAt + bytes.size();
        shell.pending.clear();
        shell.pendingAt = 0;
        shell.behind = true;
        return;
    }
    shell.pending.insert(shell.pending.end(), bytes.begin(), bytes.end());
}

void TerminalHost::QueueRepaint(Shell& shell, uint64_t nowUs) {
    if (nowUs - shell.lastRepaintUs < kRepaintIntervalUs) return;
    const std::string paint = deskhub::term::RenderScreen(*shell.mirror);
    shell.pending.assign(paint.begin(), paint.end());
    shell.pendingAt = 0;
    shell.behind = false;
    shell.lastRepaintUs = nowUs;
    ++shell.repaints;
}

void TerminalHost::ReportShell(uint32_t termId, Shell& shell, uint64_t nowUs) {
    if (nowUs - shell.reportedAtUs < kShellReportIntervalUs) return;
    const uint64_t moved = shell.ptyBytes + shell.sentBytes + shell.droppedBytes;
    if (moved == shell.reportedBytes) {
        shell.reportedAtUs = nowUs;
        return;
    }
    shell.reportedAtUs = nowUs;
    shell.reportedBytes = moved;
    LOGI(
        "[DIAG][term] id=%u pty_bytes=%llu sent_bytes=%llu dropped_bytes=%llu send_fail=%llu "
        "repaint=%llu pending=%zu behind=%d local=%d",
        unsigned(termId), static_cast<unsigned long long>(shell.ptyBytes),
        static_cast<unsigned long long>(shell.sentBytes),
        static_cast<unsigned long long>(shell.droppedBytes),
        static_cast<unsigned long long>(shell.sendFails),
        static_cast<unsigned long long>(shell.repaints), shell.pending.size() - shell.pendingAt,
        int(shell.behind), int(shell.local));
}

void TerminalHost::PumpShells(uint64_t nowUs) {
    std::vector<uint32_t> finished;
    std::vector<uint8_t> chunk(kPtyReadChunk);
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);

    for (auto& [termId, shell] : shells_) {
        if (!shell.local) {
            if (FlushPending(termId, shell, message) && shell.behind)
                QueueRepaint(shell, nowUs);
        }
        for (int round = 0; round < kPumpRounds; ++round) {
            const int got = shell.pty->Read(chunk.data(), chunk.size(), kPtyWaitMs);
            if (got < 0) {
                finished.push_back(termId);
                break;
            }
            if (got == 0) break;
            const std::span<const uint8_t> fresh(chunk.data(), size_t(got));
            shell.ptyBytes += size_t(got);
            shell.mirror->Write(fresh);
            const std::string reply = shell.mirror->TakeResponse();
            if (shell.local) {
                if (!reply.empty())
                    shell.pty->Write(std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(reply.data()), reply.size()));
                continue;
            }
            QueueForPeer(shell, fresh);
        }
        if (!shell.local) FlushPending(termId, shell, message);
        ReportShell(termId, shell, nowUs);
    }

    for (uint32_t termId : finished) {
        const auto at = shells_.find(termId);
        const int code = at == shells_.end() ? 0 : at->second.pty->ExitCode();
        CloseShell(termId, code, true);
    }
}

void TerminalHost::DrainKicks() {
    if (kicks_.empty()) return;
    const std::vector<uint32_t> kicks = std::move(kicks_);
    kicks_.clear();
    for (uint32_t termId : kicks) CloseShell(termId, 0, true);
}

void TerminalHost::CloseShell(uint32_t termId, int exitCode, bool tellClient) {
    const auto at = shells_.find(termId);
    if (at == shells_.end()) return;
    if (tellClient && !at->second.local) {
        std::vector<uint8_t> out(deskhub::kMaxDatagram);
        out.resize(deskhub::BuildTermExit(out, termId, exitCode));
        SendToPeer(at->second.peer, out);
    }
    Audit(termId, "closed");
    shells_.erase(at);
    sessions_.Close(termId);
    if (cb_.onSessionsChanged) cb_.onSessionsChanged();
}

bool TerminalHost::AttachLocal(uint32_t termId) {
    if (!Running()) return false;
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    if (at == shells_.end()) return false;
    if (at->second.local) return true;

    const deskhub::TerminalRecord* record = sessions_.Find(termId);
    const bool peerListening =
        record != nullptr && record->state == deskhub::TerminalState::Live;
    if (!sessions_.AttachLocal(termId)) return false;
    if (peerListening) {
        std::vector<uint8_t> out(deskhub::kMaxDatagram);
        out.resize(deskhub::BuildTermExit(out, termId, 0));
        SendToPeer(at->second.peer, out);
    }
    at->second.local = true;
    at->second.peer = NetAddr{};
    Audit(termId, "attached locally");
    if (cb_.onSessionsChanged) cb_.onSessionsChanged();
    return true;
}

void TerminalHost::CloseLocal(uint32_t termId) {
    if (!Running()) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    if (at == shells_.end() || !at->second.local) return;
    CloseShell(termId, 0, false);
}

bool TerminalHost::LocalAlive(uint32_t termId) const {
    if (!Running()) return false;
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    return at != shells_.end() && at->second.local;
}

deskhub::term::TerminalSnapshot TerminalHost::LocalSnapshot(uint32_t termId,
    size_t scrollOffset) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    if (at == shells_.end() || !at->second.local) return {};
    return deskhub::term::SnapshotScreen(*at->second.mirror, scrollOffset);
}

void TerminalHost::WriteLocalBytes(uint32_t termId, const std::string& bytes) {
    if (bytes.empty()) return;
    const auto at = shells_.find(termId);
    if (at == shells_.end() || !at->second.local) return;
    at->second.pty->Write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
}

void TerminalHost::SendLocalKey(uint32_t termId, const deskhub::term::TermKeyEvent& key) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    if (at == shells_.end() || !at->second.local) return;
    WriteLocalBytes(termId, deskhub::term::EncodeKey(key, at->second.mirror->Modes()));
}

void TerminalHost::ResizeLocal(uint32_t termId, deskhub::TermSize size) {
    const deskhub::TermSize clamped = deskhub::ClampTermSize(size);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto at = shells_.find(termId);
    if (at == shells_.end() || !at->second.local) return;
    if (at->second.mirror->Size() == clamped) return;
    sessions_.Resize(termId, clamped);
    at->second.mirror->Resize(clamped);
    at->second.pty->Resize(clamped);
}

void TerminalHost::Audit(uint32_t termId, std::string_view what) {
    const deskhub::TerminalRecord* record = sessions_.Find(termId);
    if (record == nullptr) return;
    const std::string line = deskhub::TerminalAuditLine(*record, what);
    LOGI("%s", line.c_str());
    if (cb_.onAudit) cb_.onAudit(line);
}

}
