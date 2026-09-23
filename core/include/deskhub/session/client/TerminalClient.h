#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace deskhub {

enum class TerminalClientState : uint8_t {
    Idle = 0,
    Opening = 1,
    Open = 2,
    Reattaching = 3,
    Refused = 4,
    Closed = 5,
};

struct TerminalClientCallbacks {
    std::function<void(std::span<const uint8_t>)> send;
    std::function<void(std::span<const uint8_t>)> onOutput;
    std::function<void(const TermOpenAck&)> onOpened;
    std::function<void(TermReason)> onRefused;
    std::function<void(const TermSessionList&)> onSessions;
    std::function<void(int32_t exitCode)> onExit;
};

class TerminalClient {
public:
    explicit TerminalClient(TerminalClientCallbacks cb) : cb_(std::move(cb)) {}

    void Open(TermSize size, std::string clientName);
    void Reattach();
    void Resume(uint32_t termId);
    void RequestList();
    void HandleMessage(std::span<const uint8_t> message);
    void SendInput(std::span<const uint8_t> bytes);
    void Resize(TermSize size);
    void CloseSession(uint32_t termId);
    void Close();
    void LinkLost();

    TerminalClientState State() const {
        return state_;
    }
    uint32_t TermId() const {
        return termId_;
    }
    TermSize Size() const {
        return size_;
    }
    const TermSessionList& Sessions() const {
        return sessions_;
    }
    bool CanReattach() const {
        return termId_ != 0 && state_ == TerminalClientState::Idle;
    }

private:
    void SendOpen();
    void Emit(size_t written);

    TerminalClientCallbacks cb_;
    TerminalClientState state_ = TerminalClientState::Idle;
    uint32_t termId_ = 0;
    TermSize size_{};
    std::string clientName_{};
    std::vector<uint8_t> buf_ = std::vector<uint8_t>(kMaxRecordSize);
    TermSessionList sessions_{};
};

}
