#pragma once
#include "deskhub/session/TerminalSession.h"
#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Screen.h"
#include "deskhub/terminal/Snapshot.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/system/Pty.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace deskhubp {

struct TerminalHostCallbacks {
    std::function<void(std::string_view auditLine)> onAudit;
    std::function<void()> onSessionsChanged;
};

class TerminalHost {
public:
    TerminalHost() = default;
    ~TerminalHost();
    TerminalHost(const TerminalHost&) = delete;
    TerminalHost& operator=(const TerminalHost&) = delete;

    bool Start(SessionTransport& sock, std::string shell, TerminalHostCallbacks callbacks);
    void Stop();

    bool Running() const {
        return running_.load(std::memory_order_acquire);
    }

    void HandleMessage(const NetAddr& from, std::span<const uint8_t> message);
    void OnPeerGone(const NetAddr& peer);

    void KickSession(uint32_t termId);
    size_t SessionCount() const;
    std::vector<deskhub::TerminalRecord> Sessions() const;

    bool AttachLocal(uint32_t termId);
    void CloseLocal(uint32_t termId);
    bool LocalAlive(uint32_t termId) const;
    deskhub::term::TerminalSnapshot LocalSnapshot(uint32_t termId, size_t scrollOffset) const;
    void SendLocalKey(uint32_t termId, const deskhub::term::TermKeyEvent& key);
    void ResizeLocal(uint32_t termId, deskhub::TermSize size);

private:
    struct Shell {
        std::unique_ptr<Pty> pty{};
        NetAddr peer{};
        std::unique_ptr<deskhub::term::Screen> mirror{};
        bool local = false;
        std::vector<uint8_t> pending{};
        size_t pendingAt = 0;
        bool behind = false;
        uint64_t lastRepaintUs = 0;
        uint64_t ptyBytes = 0;
        uint64_t sentBytes = 0;
        uint64_t droppedBytes = 0;
        uint64_t sendFails = 0;
        uint64_t repaints = 0;
        uint64_t reportedAtUs = 0;
        uint64_t reportedBytes = 0;
    };

    void Loop();
    void PumpShells(uint64_t nowUs);
    bool FlushPending(uint32_t termId, Shell& shell, std::vector<uint8_t>& scratch);
    void QueueForPeer(Shell& shell, std::span<const uint8_t> bytes);
    void QueueRepaint(Shell& shell, uint64_t nowUs);
    void ReportShell(uint32_t termId, Shell& shell, uint64_t nowUs);
    void DrainKicks();
    void DrainGone(uint64_t nowUs);
    void CloseShell(uint32_t termId, int exitCode, bool tellClient);
    void WriteLocalBytes(uint32_t termId, const std::string& bytes);
    void Audit(uint32_t termId, std::string_view what);
    bool SendToPeer(const NetAddr& peer, std::span<const uint8_t> message);
    uint32_t TermIdFor(const NetAddr& peer) const;

    SessionTransport* sock_ = nullptr;
    std::string shell_{};
    TerminalHostCallbacks cb_{};
    deskhub::TerminalSessions sessions_{};
    std::map<uint32_t, Shell> shells_{};
    std::vector<uint32_t> kicks_{};
    std::vector<NetAddr> gone_{};
    mutable std::mutex mutex_{};
    std::mutex goneMutex_{};
    std::thread thread_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
};

}
