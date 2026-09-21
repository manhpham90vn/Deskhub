#include "deskhub/session/client/TerminalClient.h"

namespace deskhub {

void TerminalClient::Emit(size_t written) {
    if (written == 0 || !cb_.send) return;
    cb_.send(std::span<const uint8_t>(buf_.data(), written));
}

void TerminalClient::SendOpen() {
    TermOpen open;
    open.size = size_;
    open.resumeId = termId_;
    open.passcode = passcode_;
    open.clientName = clientName_;
    Emit(BuildTermOpen(buf_, open));
}

void TerminalClient::Open(std::string passcode, TermSize size, std::string clientName) {
    passcode_ = std::move(passcode);
    clientName_ = std::move(clientName);
    size_ = ClampTermSize(size);
    termId_ = 0;
    reason_ = TermReason::Accepted;
    state_ = TerminalClientState::Opening;
    SendOpen();
}

void TerminalClient::Reattach() {
    Resume(termId_);
}

void TerminalClient::Resume(uint32_t termId) {
    if (termId == 0 || state_ == TerminalClientState::Closed) return;
    if (state_ == TerminalClientState::Open) return;
    termId_ = termId;
    state_ = TerminalClientState::Reattaching;
    reason_ = TermReason::Accepted;
    SendOpen();
}

void TerminalClient::RequestList() {
    if (state_ == TerminalClientState::Closed) return;
    Emit(BuildTermList(buf_));
}

void TerminalClient::SendInput(std::span<const uint8_t> bytes) {
    if (state_ != TerminalClientState::Open) return;
    while (!bytes.empty()) {
        const size_t chunk = bytes.size() < kMaxTermDataBytes ? bytes.size() : kMaxTermDataBytes;
        Emit(BuildTermData(buf_, termId_, bytes.first(chunk)));
        bytes = bytes.subspan(chunk);
    }
}

void TerminalClient::Resize(TermSize size) {
    const TermSize clamped = ClampTermSize(size);
    if (clamped == size_) return;
    size_ = clamped;
    if (state_ != TerminalClientState::Open) return;
    Emit(BuildTermResize(buf_, termId_, size_));
}

void TerminalClient::CloseSession(uint32_t termId) {
    if (termId == 0 || state_ == TerminalClientState::Closed) return;
    Emit(BuildTermClose(buf_, termId));
    if (termId != termId_) return;
    termId_ = 0;
    state_ = TerminalClientState::Closed;
}

void TerminalClient::Close() {
    if (state_ == TerminalClientState::Open) Emit(BuildTermClose(buf_, termId_));
    termId_ = 0;
    state_ = TerminalClientState::Closed;
}

void TerminalClient::LinkLost() {
    if (state_ == TerminalClientState::Closed) return;
    state_ = TerminalClientState::Idle;
}

void TerminalClient::HandleMessage(std::span<const uint8_t> message) {
    const std::optional<CommonHeader> header = ParseCommonHeader(message);
    if (!header || header->chan != Chan::Terminal) return;
    const std::span<const uint8_t> payload = PayloadOf(message);

    switch (header->type) {
        case MsgType::TermOpenAck: {
            const std::optional<TermOpenAck> ack = ParseTermOpenAck(payload);
            if (!ack) return;
            if (state_ != TerminalClientState::Opening &&
                state_ != TerminalClientState::Reattaching)
                return;
            reason_ = ack->reason;
            if (ack->reason != TermReason::Accepted || ack->termId == 0) {
                const bool hostMayNotHaveNoticedTheDropYet =
                    state_ == TerminalClientState::Reattaching &&
                    ack->reason == TermReason::NoSuchSession;
                if (hostMayNotHaveNoticedTheDropYet) {
                    state_ = TerminalClientState::Idle;
                } else {
                    termId_ = 0;
                    state_ = TerminalClientState::Refused;
                }
                if (cb_.onRefused) cb_.onRefused(reason_);
                return;
            }
            termId_ = ack->termId;
            state_ = TerminalClientState::Open;
            if (cb_.onOpened) cb_.onOpened(*ack);
            return;
        }
        case MsgType::TermListAck: {
            if (state_ == TerminalClientState::Closed) return;
            const std::optional<TermSessionList> list = ParseTermListAck(payload);
            if (!list) return;
            sessions_ = *list;
            if (cb_.onSessions) cb_.onSessions(sessions_);
            return;
        }
        case MsgType::TermData:
            if (state_ != TerminalClientState::Open) return;
            if (header->sessionId != termId_) return;
            if (payload.empty() || payload.size() > kMaxTermDataBytes) return;
            if (cb_.onOutput) cb_.onOutput(payload);
            return;
        case MsgType::TermExit: {
            if (header->sessionId != termId_) return;
            const std::optional<int32_t> code = ParseTermExit(payload);
            if (!code) return;
            termId_ = 0;
            state_ = TerminalClientState::Closed;
            if (cb_.onExit) cb_.onExit(*code);
            return;
        }
        case MsgType::TermClose:
            if (header->sessionId != termId_) return;
            termId_ = 0;
            state_ = TerminalClientState::Closed;
            if (cb_.onExit) cb_.onExit(0);
            return;
        default: return;
    }
}

}
