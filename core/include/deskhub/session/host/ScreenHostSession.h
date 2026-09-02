#pragma once
#include "deskhub/input/InputReceiver.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/ViewerTable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace deskhub {

inline constexpr uint64_t kSessionTimeoutUs = 5'000'000;

inline constexpr uint32_t kMaxPasscodeAttempts = 3;
inline constexpr uint64_t kPasscodeLockoutUs = 30'000'000;

struct StreamParams {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t fps = 60;
    uint32_t bitrateBps = 20'000'000;
};

struct ScreenHostCallbacks {
    std::function<void(std::span<const uint8_t>)> send;
    std::function<void(uint64_t addrPacked, std::span<const uint8_t>)> sendTo;
    std::function<void(const Hello&)> onHello;
    std::function<void()> onStart;
    std::function<void(KeyframeReason)> onKeyframeRequest;
    std::function<void(uint64_t addrPacked, size_t viewerCount, std::string_view viewerName)>
        onViewerJoin;
    std::function<void(uint64_t addrPacked, size_t viewerCount)> onViewerLeave;
    std::function<void(uint64_t addrPacked)> onControllerChange;
    std::function<void()> onDisconnect;
    std::function<void(const Feedback&)> onFeedback;
    std::function<void(const InputEvent&)> onInput;
    std::function<void(bool focused)> onFocus;
    std::function<void(uint32_t frameId, std::span<const uint16_t> indices)> onNack;
    std::function<void(uint32_t frameId)> onInvalidateRef;
    std::function<void(std::string_view text)> onClipboardText;
    std::function<bool(std::span<uint8_t>)> randomBytes;
};

class ScreenHostSession {
public:
    enum class State : uint8_t { Idle,
        Ready,
        Streaming };

    ScreenHostSession(ScreenHostCallbacks cb, StreamParams offer, ViewerBudget* hostBudget = nullptr)
        : cb_(std::move(cb)), offer_(offer) {
        viewers_.SetHostBudget(hostBudget);
    }

    void SetOffer(const StreamParams& p) {
        offer_ = p;
    }

    void SetPasscode(std::string passcode) {
        passcode_ = IsValidPasscode(passcode) ? std::move(passcode) : std::string();
    }

    void SetConnectionAuthenticated(bool authenticated) {
        connectionAuthenticated_ = authenticated;
    }

    void SetCodecMask(uint16_t mask) {
        hostCodecMask_ = mask ? mask : kCodecMaskH264;
    }

    uint16_t hostCodecMask() const {
        return hostCodecMask_;
    }

    Codec codec() const {
        return codec_;
    }

    void SetClipboardEnabled(bool on) {
        clipboardEnabled_ = on;
    }

    bool HandlePacket(std::span<const uint8_t> pkt, uint64_t nowUs, uint64_t fromPacked);
    void Tick(uint64_t nowUs);
    bool KickViewer(uint64_t addrPacked);

    State state() const {
        return state_.load(std::memory_order_acquire);
    }
    uint32_t sessionId() const {
        return sessionId_.load(std::memory_order_relaxed);
    }
    size_t viewerCount() const {
        return viewers_.viewerCount();
    }
    size_t SnapshotViewerAddrs(std::span<uint64_t> out) const {
        return viewers_.SnapshotAddrs(out);
    }
    size_t SnapshotAudioViewerAddrs(std::span<uint64_t> out) const {
        return viewers_.SnapshotAudioAddrs(out);
    }
    size_t SnapshotViewerInfos(std::span<ViewerInfo> out) const {
        return viewers_.SnapshotInfos(out);
    }
    InputReceiver::Stats inputStats() const {
        return viewers_.inputStats();
    }
    uint64_t inputDenied() const {
        return inputDenied_.load(std::memory_order_relaxed);
    }

private:
    bool InSession(uint32_t sid) const {
        const uint32_t cur = sessionId();
        return cur != 0 && sid == cur;
    }

    bool HandleHello(std::span<const uint8_t> payload, uint64_t nowUs, uint64_t fromPacked);
    bool HandleFromViewer(const CommonHeader& header, std::span<const uint8_t> payload,
        ViewerSlot& viewer, uint64_t nowUs);
    void ApplyInput(std::span<const uint8_t> payload, ViewerSlot& viewer, uint64_t nowUs);
    void HandOverControl(uint64_t addrPacked);

    void SendHelloAck(uint64_t nowUs);
    void SendReject(RejectReason reason);
    bool BeginSession();
    void DropViewer(ViewerSlot& viewer);
    void RefreshState();
    void Disconnect();

    bool PasscodeAllows(const Hello& m, uint64_t nowUs);

    ScreenHostCallbacks cb_;
    StreamParams offer_;
    ViewerTable viewers_;
    std::atomic<State> state_{State::Idle};
    std::atomic<uint32_t> sessionId_{0};
    std::atomic<uint64_t> inputDenied_{0};
    uint64_t controllingAddr_ = 0;
    std::string passcode_;
    bool connectionAuthenticated_ = false;
    bool clipboardEnabled_ = false;
    uint16_t hostCodecMask_ = kCodecMaskH264;
    Codec codec_ = Codec::H264;
    uint32_t wrongPasscodes_ = 0;
    uint64_t passcodeLockUntilUs_ = 0;
    uint8_t buf_[kMaxDatagram] = {};
};

}
