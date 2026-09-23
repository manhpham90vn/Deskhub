#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/session/client/TerminalClient.h"
#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Screen.h"
#include "deskhub/terminal/Snapshot.h"
#include "deskhubp/client/HostLink.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace deskhubp {

enum class TerminalViewerState : uint8_t {
    Idle = 0,
    Connecting = 1,
    Deciding = 2,
    Opening = 3,
    Live = 4,
    Reattaching = 5,
    Refused = 6,
    Failed = 7,
    Ended = 8,
};

struct TerminalViewerConfig {
    NetAddr host{};
    std::string hostLabel{};
    std::string passcode{};
    std::string clientName{};
    deskhub::TermSize size{};
    uint32_t resumeId = 0;
    bool deferOpen = false;
};

struct TerminalViewerCallbacks {
    std::function<void()> onRedraw;
    std::function<void(std::span<const uint8_t> bytes)> onOutput;
    std::function<void(TerminalViewerState, std::string_view message)> onState;
    std::function<void(deskhub::TrustVerdict, std::string_view fingerprint)> onTrustAsked;
    std::function<void(const deskhub::TermSessionList&)> onSessions;
};

using TerminalSnapshot = deskhub::term::TerminalSnapshot;

class TerminalViewer {
public:
    TerminalViewer() = default;
    ~TerminalViewer();
    TerminalViewer(const TerminalViewer&) = delete;
    TerminalViewer& operator=(const TerminalViewer&) = delete;

    bool Start(const TerminalViewerConfig& config, TerminalViewerCallbacks callbacks);
    void Stop();

    void AcceptFingerprint();
    void RejectFingerprint();

    void SendKey(const deskhub::term::TermKeyEvent& key);
    void SendText(std::string_view text);
    void Resize(deskhub::TermSize size);
    void RequestSessions();
    void OpenNew();
    void ResumeSession(uint32_t termId);
    void CloseSession(uint32_t termId);
    std::vector<deskhub::TermSessionEntry> Sessions() const;

    TerminalViewerState State() const {
        return state_.load(std::memory_order_acquire);
    }
    bool Running() const {
        return running_.load(std::memory_order_acquire);
    }
    std::string Message() const;
    std::string Fingerprint() const;
    deskhub::TrustVerdict Verdict() const {
        return link_.Verdict();
    }
    TerminalSnapshot Snapshot(size_t scrollOffset = 0) const;

private:
    void Loop();
    void HandleLinkReady(bool resumed);
    void RetryResume(uint64_t nowUs);
    void ResetResumeBackoff();
    void OnLinkState(HostLinkState state, std::string_view message);
    void SetState(TerminalViewerState state, std::string_view message);
    void Post(std::function<void()> command);
    void RunCommands();
    void SendRecord(std::span<const uint8_t> message);
    void FlushOutbox();
    void SendBytes(const std::string& bytes);
    deskhub::term::TerminalModes CurrentModes() const;

    TerminalViewerConfig config_{};
    TerminalViewerCallbacks cb_{};
    HostLink link_{};
    std::shared_ptr<HostLinkChannel> channel_{};
    std::unique_ptr<deskhub::TerminalClient> client_{};
    deskhub::term::Screen screen_{};
    std::string message_{};
    std::deque<std::vector<uint8_t>> outbox_{};
    size_t outboxBytes_ = 0;
    deskhub::TermSessionList sessions_{};

    mutable std::mutex mutex_{};
    std::mutex commandMutex_{};
    std::vector<std::function<void()>> commands_{};
    std::thread thread_{};
    std::atomic<TerminalViewerState> state_{TerminalViewerState::Idle};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    uint64_t lostAtUs_ = 0;
    uint64_t resumeRetryAtUs_ = 0;
    uint32_t resumeAttempts_ = 0;
};

}
