#include "deskhub/session/host/ScreenHostSession.h"

#include "deskhub/media/CodecNegotiation.h"

namespace deskhub {

bool ScreenHostSession::HandlePacket(std::span<const uint8_t> pkt, uint64_t nowUs, uint64_t fromPacked) {
    const auto h = ParseCommonHeader(pkt);
    if (!h) return false;
    const auto payload = PayloadOf(pkt);

    if (h->type == MsgType::Hello) return HandleHello(payload, nowUs, fromPacked);

    if (!InSession(h->sessionId)) return false;
    ViewerSlot* viewer = viewers_.Find(fromPacked);
    if (!viewer) return false;
    viewer->lastRecvUs = nowUs;

    if (h->type == MsgType::Bye) {
        DropViewer(*viewer);
        return false;
    }
    return HandleFromViewer(*h, payload, *viewer, nowUs);
}

bool ScreenHostSession::HandleHello(std::span<const uint8_t> payload, uint64_t nowUs,
    uint64_t fromPacked) {
    const auto m = ParseHello(payload);
    if (!m || !fromPacked) return false;
    if (!PasscodeAllows(*m, nowUs)) return false;
    const Codec agreed = media::NegotiateCodec(hostCodecMask_, m->codecMask);
    if (agreed == Codec::Rejected) {
        SendReject(RejectReason::CodecMismatch);
        return false;
    }
    codec_ = agreed;

    ViewerSlot* known = viewers_.Find(fromPacked);
    if (known && known->clientId != m->clientId) {
        DropViewer(*known);
        known = nullptr;
    }
    if (!known) known = viewers_.FindByClient(m->clientId);

    if (known) {
        viewers_.Rebind(*known, fromPacked);
        viewers_.SetName(*known, m->clientName);
        viewers_.SetWantsAudio(*known, (m->features & kClientWantsAudio) != 0);
        known->lastRecvUs = nowUs;
        SendHelloAck(nowUs);
        return true;
    }

    const bool firstViewer = viewers_.empty();
    if (firstViewer && !BeginSession()) return false;

    ViewerSlot* admitted = viewers_.Admit(m->clientId, fromPacked, nowUs, m->clientName);
    if (!admitted) {
        SendReject(RejectReason::Busy);
        return false;
    }
    viewers_.SetWantsAudio(*admitted, (m->features & kClientWantsAudio) != 0);

    RefreshState();
    if (firstViewer && cb_.onHello) cb_.onHello(*m);
    if (cb_.onViewerJoin) cb_.onViewerJoin(fromPacked, viewers_.viewerCount(), m->clientName);
    SendHelloAck(nowUs);
    return true;
}

bool ScreenHostSession::HandleFromViewer(const CommonHeader& header, std::span<const uint8_t> payload,
    ViewerSlot& viewer, uint64_t nowUs) {
    const bool streaming = state() == State::Streaming;

    switch (header.type) {
        case MsgType::Start:
            if (!viewer.started) {
                viewer.started = true;
                if (!streaming) {
                    state_.store(State::Streaming, std::memory_order_release);
                    if (cb_.onStart) cb_.onStart();
                } else if (cb_.onKeyframeRequest) {
                    cb_.onKeyframeRequest(KeyframeReason::ViewerJoin);
                }
            }
            return true;
        case MsgType::Ping: {
            auto m = ParsePingPong(payload);
            if (!m) return false;
            m->hostTimeUs = nowUs;
            const size_t n = BuildPong(buf_, sessionId(), *m);
            if (n && cb_.send) cb_.send(std::span<const uint8_t>(buf_, n));
            return true;
        }
        case MsgType::RequestKeyframe:
            if (!streaming) return false;
            if (cb_.onKeyframeRequest) cb_.onKeyframeRequest(ParseRequestKeyframe(payload));
            return true;
        case MsgType::InputEvent:
            if (!streaming) return false;
            ApplyInput(payload, viewer, nowUs);
            return true;
        case MsgType::SetFocus: {
            if (!streaming) return false;
            const auto focused = ParseSetFocus(payload);
            if (!focused) return false;
            if (controllingAddr_ && controllingAddr_ != viewer.addrPacked) return true;
            if (!*focused) {
                viewer.lastInputUs = 0;
                HandOverControl(0);
            }
            if (cb_.onFocus) cb_.onFocus(*focused);
            return true;
        }
        case MsgType::Feedback: {
            const auto m = ParseFeedback(payload);
            if (!m) return false;
            viewer.feedback = *m;
            viewer.haveFeedback = true;
            if (cb_.onFeedback) cb_.onFeedback(WorstCaseFeedback(viewers_.slots()));
            return true;
        }
        case MsgType::Nack: {
            if (!streaming) return false;
            uint16_t idx[kMaxNackIndices];
            uint32_t frameId = 0;
            const size_t n = ParseNack(payload, frameId, idx);
            if (n && cb_.onNack) cb_.onNack(frameId, std::span<const uint16_t>(idx, n));
            return true;
        }
        case MsgType::InvalidateRef: {
            if (!streaming) return false;
            const auto fid = ParseInvalidateRef(payload);
            if (fid && cb_.onInvalidateRef) cb_.onInvalidateRef(*fid);
            return true;
        }
        case MsgType::Clipboard: {
            if (!streaming || !clipboardEnabled_) return false;
            const auto chunk = ParseClipboardChunk(payload);
            if (!chunk) return false;
            if (viewer.clip.Accept(*chunk)) {
                const auto text = viewer.clip.TakeCompleted();
                if (text && cb_.onClipboardText) cb_.onClipboardText(*text);
            }
            return true;
        }
        default:
            return false;
    }
}

void ScreenHostSession::ApplyInput(std::span<const uint8_t> payload, ViewerSlot& viewer,
    uint64_t nowUs) {
    if (viewers_.HigherPriorityIsDriving(viewer, nowUs)) {
        inputDenied_.fetch_add(1, std::memory_order_relaxed);
        viewer.input.HandlePacket(payload, nullptr);
        return;
    }
    viewer.lastInputUs = nowUs;
    HandOverControl(viewer.addrPacked);
    viewer.input.HandlePacket(payload, cb_.onInput);
}

void ScreenHostSession::HandOverControl(uint64_t addrPacked) {
    if (controllingAddr_ == addrPacked) return;
    controllingAddr_ = addrPacked;
    if (cb_.onControllerChange) cb_.onControllerChange(addrPacked);
}

void ScreenHostSession::Tick(uint64_t nowUs) {
    if (state() == State::Idle) return;
    for (ViewerSlot& s : viewers_.slots()) {
        if (!s.active) continue;
        if (nowUs - s.lastRecvUs > kSessionTimeoutUs) DropViewer(s);
    }
}

bool ScreenHostSession::KickViewer(uint64_t addrPacked) {
    ViewerSlot* viewer = viewers_.Find(addrPacked);
    if (!viewer || !viewer->active) return false;

    const size_t n = BuildBye(buf_, sessionId());
    if (n && cb_.sendTo) cb_.sendTo(addrPacked, std::span<const uint8_t>(buf_, n));

    DropViewer(*viewer);
    return true;
}

void ScreenHostSession::DropViewer(ViewerSlot& viewer) {
    const uint64_t addr = viewer.addrPacked;

    viewers_.Drop(viewer);
    if (cb_.onViewerLeave) cb_.onViewerLeave(addr, viewers_.viewerCount());

    if (viewers_.empty()) {
        Disconnect();
        return;
    }
    RefreshState();
    if (controllingAddr_ == addr) HandOverControl(0);
}

void ScreenHostSession::RefreshState() {
    state_.store(viewers_.anyStarted() ? State::Streaming : State::Ready,
        std::memory_order_release);
}

bool ScreenHostSession::BeginSession() {
    uint32_t sid = 0;
    if (cb_.randomBytes) {
        uint8_t b[4];
        if (cb_.randomBytes(std::span<uint8_t>(b, 4)))
            sid = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
    }
    if (!sid) {
        SendReject(RejectReason::None);
        state_.store(State::Idle, std::memory_order_release);
        return false;
    }
    sessionId_.store(sid, std::memory_order_relaxed);
    return true;
}

void ScreenHostSession::SendHelloAck(uint64_t nowUs) {
    HelloAck a;
    a.sessionId = sessionId();
    a.codec = codec_;
    a.width = offer_.width;
    a.height = offer_.height;
    a.fps = offer_.fps;
    a.bitrateBps = offer_.bitrateBps;
    a.timebaseUs = nowUs;
    const size_t n = BuildHelloAck(buf_, a);
    if (n && cb_.send) cb_.send(std::span<const uint8_t>(buf_, n));
}

bool ScreenHostSession::PasscodeAllows(const Hello& m, uint64_t nowUs) {
    if (connectionAuthenticated_) return true;
    if (!IsValidPasscode(passcode_)) {
        SendReject(RejectReason::WrongPasscode);
        return false;
    }
    if (nowUs < passcodeLockUntilUs_) return false;

    if (m.passcode == passcode_) {
        wrongPasscodes_ = 0;
        return true;
    }

    if (++wrongPasscodes_ >= kMaxPasscodeAttempts) {
        wrongPasscodes_ = 0;
        passcodeLockUntilUs_ = nowUs + kPasscodeLockoutUs;
    }
    SendReject(RejectReason::WrongPasscode);
    return false;
}

void ScreenHostSession::SendReject(RejectReason reason) {
    HelloAck a{};
    a.codec = Codec::Rejected;
    a.reason = reason;
    const size_t n = BuildHelloAck(buf_, a);
    if (n && cb_.send) cb_.send(std::span<const uint8_t>(buf_, n));
}

void ScreenHostSession::Disconnect() {
    state_.store(State::Idle, std::memory_order_release);
    sessionId_.store(0, std::memory_order_relaxed);
    viewers_.Clear();
    HandOverControl(0);
    if (cb_.onDisconnect) cb_.onDisconnect();
}

}
