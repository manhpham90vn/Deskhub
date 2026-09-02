#pragma once
#include "deskhub/control/QualityLadder.h"
#include "deskhub/control/StreamSize.h"
#include "deskhub/media/ShareTypes.h"
#include "deskhub/media/VideoTypes.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/Beacon.h"
#include "deskhub/session/host/SourcePipelineState.h"
#include "deskhubp/audio/AudioBroadcaster.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/input/LocalInput.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/host/ViewerBroadcast.h"
#include "deskhubp/host/HostNetLoop.h"
#include "deskhubp/system/Clock.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace deskhubp {

class TerminalHost;
class FileHost;

using HostSource = deskhub::SourcePipelineState;

struct HostSourcePolicy {
    std::function<std::unique_ptr<HostSource>(const deskhub::media::ShareSource&,
        uint8_t sourceId)>
        create;
    std::function<void(HostSource&)> startCapture;
    std::function<void(HostSource&)> stopCapture;
    std::function<void(HostSource&)> attachInput;
    std::function<void(HostSource&)> releaseInput;
    std::function<void(HostSource&, const deskhub::InputEvent&)> applyInput;
    std::function<bool(HostSource&, uint32_t bitrateBps)> setEncoderBitrate;
    std::function<deskhub::StreamSize(HostSource&)> retarget;
    std::function<deskhub::StreamSize(HostSource&, const deskhub::QualityStep& prev,
        const deskhub::QualityStep& next)>
        applyQualityStep;
    std::function<void(HostSource&, uint64_t nowUs)> flush;
    std::function<uint64_t(const HostSource&)> inputSkipped;
    std::function<uint32_t(HostSource&)> takeIdleFrames;
};

struct HostEnginePolicy {
    std::function<std::string()> preflight;
    std::function<std::string()> afterSocket;
    std::function<void()> onSharing;
    std::function<std::string(const SessionTransport&)> portError;
    std::function<void()> onPaired;
    std::function<void(uint64_t addrPacked, std::string shortKey, std::string name)>
        onApprovalNeeded;

    std::function<bool(const deskhub::media::AudioFormat&,
        std::function<void(std::span<const int16_t>)>)>
        startAudioCapture;
    std::function<void()> stopAudioCapture;

    std::string noSourceError = "No source selected.";
    std::string noUsableSourceError = "No usable source \xE2\x80\x94 stopping.";

    HostSourcePolicy source;
    SourceStatusHooks status;
};

class HostEngine {
public:
    static constexpr uint32_t kMinBitrateBps = 1'000'000;

    HostEngine() = default;
    ~HostEngine();
    HostEngine(const HostEngine&) = delete;
    HostEngine& operator=(const HostEngine&) = delete;

    bool Start(const std::vector<deskhub::media::ShareSource>& sources,
        const deskhub::media::ShareOptions& opt, HostEnginePolicy policy);
    void Stop();

    void RequestStopSource(uint8_t sourceId);
    void RequestKickViewer(uint8_t sourceId, uint64_t addrPacked);
    void AnswerPairingRequest(uint64_t addrPacked, bool allowed);

    void SetTerminal(TerminalHost* terminal) {
        terminal_.store(terminal, std::memory_order_release);
    }

    void SetFiles(FileHost* files) {
        files_.store(files, std::memory_order_release);
    }

    TerminalHost* terminal() const {
        return terminal_.load(std::memory_order_acquire);
    }

    FileHost* files() const {
        return files_.load(std::memory_order_acquire);
    }

    bool running() const {
        return running_.load(std::memory_order_acquire);
    }

    std::vector<deskhub::media::ShareSourceStatus> Status();
    std::string LastError();
    std::string BindWarning();

    void ForEachLiveSource(const std::function<void(HostSource&)>& fn) {
        for (HostSource* st : live_)
            if (st) fn(*st);
    }

    void OfferAudio(std::span<const int16_t> pcm) {
        audio_.Offer(pcm);
    }

    bool audioRunning() const {
        return audio_.running();
    }

    const deskhub::media::AudioFormat& audioFormat() const {
        return audio_.format();
    }

    void OfferLocalClipboard(std::string text);
    std::optional<std::string> TakeRemoteClipboard();

    deskhub::media::PacketHandler MakePacketSink(HostSource& st) {
        return [this, p = &st](const uint8_t* data, size_t size, uint64_t tsUs, bool keyframe) {
            SendEncodedFrame(*p, sock_, std::span<const uint8_t>(data, size), tsUs, keyframe);
        };
    }

    const deskhub::media::ShareOptions& options() const {
        return opt_;
    }
    uint32_t startBitrateBps() const {
        return startBitrateBps_;
    }
    LocalInputMonitor& localInput() {
        return localInputMon_;
    }
    SessionTransport& socket() {
        return sock_;
    }

private:
    bool Fail(std::string message);
    void RefuseFiles(const NetAddr& from, std::span<const uint8_t> message);
    void StartAudio();
    void AttachSession(HostSource& st);
    void ShutdownSource(HostSource& st);
    void PublishStatus();
    std::vector<HostSource*> AllSources();
    void RecvLoop();
    void DrainControlRequests();
    void DrainLocalClipboard();
    HostSource* FindLiveSource(uint8_t sourceId);

    deskhub::media::ShareOptions opt_;
    HostEnginePolicy policy_;

    SessionTransport sock_;
    std::atomic<TerminalHost*> terminal_{nullptr};
    std::atomic<FileHost*> files_{nullptr};
    std::thread recvThread_;
    std::atomic<bool> quit_{false};
    std::atomic<bool> running_{false};

    deskhub::ViewerBudget viewerBudget_;
    std::vector<std::unique_ptr<HostSource>> pipes_;
    std::vector<HostSource*> live_;
    uint8_t nextSourceId_ = 0;

    std::mutex statusMutex_;
    std::vector<deskhub::media::ShareSourceStatus> statusRows_;

    std::mutex controlMutex_;
    std::vector<uint8_t> pendingSourceStops_;
    std::vector<std::pair<uint8_t, uint64_t>> pendingViewerKicks_;
    std::vector<std::pair<uint64_t, bool>> pendingPairAnswers_;

    std::mutex clipMutex_;
    std::optional<std::string> pendingLocalClip_;
    std::deque<std::string> remoteClips_;

    std::mutex errMutex_;
    std::string lastError_;
    std::string bindWarning_;

    LocalInputMonitor localInputMon_;
    AudioBroadcaster audio_;
    deskhub::Beacon beacon_;

    uint32_t startBitrateBps_ = 0;
    bool keepAwakeHeld_ = false;
};

template <class Capture, class Injector, class Encoder>
struct HostSourceBase : HostSource {
    HostSourceBase(uint32_t startBps, uint32_t minBps, deskhub::diag::ShareDiagCaps caps)
        : HostSource(startBps, minBps, caps) {}

    Capture capture;
    Injector injector;

    std::mutex encMutex;
    std::unique_ptr<Encoder> encoder;

    bool hasCachedFrame() const {
        return haveCached_.load(std::memory_order_acquire);
    }

    void SetCachedFrame(bool present) {
        haveCached_.store(present, std::memory_order_release);
    }

private:
    std::atomic<bool> haveCached_{false};
};

inline std::unique_lock<std::mutex> TryHoldEncoder(std::mutex& encoderMutex) {
    return std::unique_lock<std::mutex>(encoderMutex, std::try_to_lock);
}

template <class Pipeline>
std::unique_ptr<Pipeline> MakeHostSource(HostEngine& engine,
    const deskhub::media::ShareSource& s, uint8_t sourceId) {
    auto p = std::make_unique<Pipeline>(engine.startBitrateBps(), HostEngine::kMinBitrateBps);
    p->sourceId = sourceId;
    p->name = s.name;
    return p;
}

template <class Pipeline>
HostSourcePolicy MakeDefaultSourcePolicy() {
    HostSourcePolicy sp;
    sp.releaseInput = [](HostSource& st) { static_cast<Pipeline&>(st).injector.ReleaseAll(); };
    sp.applyInput = [](HostSource& st, const deskhub::InputEvent& e) {
        static_cast<Pipeline&>(st).injector.Apply(e);
    };
    sp.setEncoderBitrate = [](HostSource& st, uint32_t bitrateBps) {
        Pipeline& p = static_cast<Pipeline&>(st);
        auto lk = TryHoldEncoder(p.encMutex);
        return lk.owns_lock() && p.encoder && p.encoder->SetBitrate(bitrateBps);
    };
    sp.inputSkipped = [](const HostSource& st) {
        return static_cast<const Pipeline&>(st).injector.skipped();
    };
    return sp;
}

void UseSystemAudioCapture(HostEnginePolicy& policy);

template <class Pipeline>
SourceStatusHooks MakeDefaultStatusHooks() {
    SourceStatusHooks hooks;
    hooks.closed = [](const HostSource& st) {
        return static_cast<const Pipeline&>(st).capture.Closed();
    };
    return hooks;
}

template <class Fn>
bool DiagEncode(HostSource& st, bool idr, Fn&& encode) {
    const uint64_t t0 = NowUs();
    const bool ok = std::forward<Fn>(encode)();
    const uint32_t us = uint32_t(NowUs() - t0);
    const uint32_t ms = us / 1000;
    st.diag.encMs.Add(ms);
    st.diag.encUs.Add(us);
    if (!ok) LOGW("[DIAG][%s] evt=enc_fail idr=%d ms=%u", st.name.c_str(), idr ? 1 : 0, ms);
    return ok;
}

}
