#pragma once
#include "deskhub/control/ClockOffset.h"
#include "deskhub/diag/ScreenClientDiag.h"
#include "deskhub/input/ClientInputQueue.h"
#include "deskhub/media/VideoContract.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/LinkRecovery.h"
#include "deskhub/session/client/ScreenClient.h"
#include "deskhub/transport/Reassembler.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/client/ScreenViewerLoop.h"
#include "deskhubp/audio/AudioPlayer.h"
#include "deskhubp/client/FileUpload.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/KeepAwake.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cinttypes>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace deskhubp {

enum class ClientPhase : int32_t { Idle = 0,
    Connecting = 1,
    Streaming = 2,
    Ended = 3,
    Deciding = 4,
    Reattaching = 5 };

struct ScreenViewerConfig {
    NetAddr server{};
    uint8_t sourceId = 0;
    uint32_t screenW = 0;
    uint32_t screenH = 0;
    uint8_t desiredFps = 60;
    bool sendNacks = true;
    uint32_t overtakenLimit = 0;
    uint32_t audioDelayMs = deskhub::kDefaultAudioDelayMs;
    bool audioAdaptive = false;
    bool pacingAdaptive = false;
    uint64_t displayIntervalUs = 0;
    std::string clockOffset{};
    bool logLossRuns = true;
    bool alwaysFocused = false;
    bool wantsAudio = false;
    const char* statusSeparator = "  ";
    std::string passcode;
    std::string displayName;
    std::string hostLabel;
    std::string fecScheme;
    VideoPath videoPath = VideoPath::QuicDatagram;

    std::function<void(deskhub::TrustVerdict, std::string_view fingerprint)> onTrustAsked;
    std::function<void(uint32_t width, uint32_t height, uint8_t fps)> onParams;
    std::function<void(const char* status)> onStatus;
    std::function<void(const char* reason)> onEnded;
    std::function<void(const char* reason)> onFinished;
    std::function<void()> onDecodeThreadStart;
    std::function<void()> onDecodeThreadExit;
};

template <class Decoder, class Surface>
    requires deskhub::media::EngineDecoder<Decoder, Surface>
class ScreenViewer {
public:
    static constexpr deskhub::diag::ScreenClientDiagCaps kDecoderDiagCaps{
        deskhub::media::PresentTimingDecoder<Decoder>,
        deskhub::media::CongestionAwareDecoder<Decoder>};

    explicit ScreenViewer(deskhub::diag::ScreenClientDiagCaps caps = kDecoderDiagCaps) : diag_(caps) {}

    ~ScreenViewer() {
        Stop();
    }

    ScreenViewer(const ScreenViewer&) = delete;
    ScreenViewer& operator=(const ScreenViewer&) = delete;

    bool Start(const ScreenViewerConfig& cfg) {
        if (netThread_.joinable() || decodeThread_.joinable()) {
            if (!finished_.load(std::memory_order_acquire)) return false;
            if (netThread_.joinable()) netThread_.join();
            if (decodeThread_.joinable()) decodeThread_.join();
        }
        cfg_ = cfg;
        if (!channel_)
            channel_ = link_.Open({deskhub::Chan::Control, deskhub::Chan::Video,
                deskhub::Chan::Audio, deskhub::Chan::File});

        FileUploadCallbacks uploadHooks;
        uploadHooks.send = [this](std::span<const uint8_t> message) {
            return link_.SendRecordOn(kQuicFileStream, message);
        };
        upload_ = std::make_unique<FileUpload>(std::move(uploadHooks));

        HostLinkConfig linkConfig;
        linkConfig.host = cfg_.server;
        linkConfig.hostLabel = HostLabel();
        linkConfig.passcode = cfg_.passcode;
        linkConfig.clientName =
            cfg_.displayName.empty() ? SessionDeviceName() : cfg_.displayName;
        linkConfig.connectTimeoutMs = kHandshakeTimeoutMs;
        linkConfig.authTimeoutMs = kAuthTimeoutMs;
        linkConfig.recvWaitMs = 10;
        linkConfig.recoverLink = true;
        linkConfig.recoverGraceUs = deskhub::kViewerReattachGraceUs;
        linkConfig.videoPath = cfg_.videoPath;

        HostLinkCallbacks linkHooks;
        linkHooks.onState = [this](HostLinkState state, std::string_view message) {
            OnLinkState(state, message);
        };
        linkHooks.onTrustAsked = [this](deskhub::TrustVerdict verdict,
                                     std::string_view fingerprint) {
            if (cfg_.onTrustAsked) cfg_.onTrustAsked(verdict, fingerprint);
        };
        linkHooks.onStreamBroken = [this](uint64_t stream) {
            if (stream == kQuicFileStream && upload_) upload_->LinkLost();
        };
        linkHooks.onReady = [this](bool resumed) {
            if (resumed) relink_.store(true, std::memory_order_release);
        };

        quit_.store(false);
        finished_.store(false);
        reattaching_.store(false);
        relink_.store(false);
        endedNotified_.store(false);
        phase_.store(ClientPhase::Connecting, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(textMutex_);
            statusLine_.clear();
            endReason_.clear();
        }

        {
            std::lock_guard<std::mutex> lk(surfaceMutex_);
            decodeExited_ = false;
            decodeRunning_ = true;
            surfaceAckGen_ = surfaceGen_;
        }
        if (!link_.Start(linkConfig, std::move(linkHooks))) {
            LOGE("[Client] Failed to open socket.");
            {
                std::lock_guard<std::mutex> lk(surfaceMutex_);
                decodeRunning_ = false;
            }
            return false;
        }
        if (cfg_.wantsAudio &&
            !player_.Start({}, cfg_.audioDelayMs, cfg_.audioAdaptive))
            LOGW("[Client] Watching without sound: no audio device could be opened.");
        decodeThread_ = std::thread([this] { DecodeThread(); });
        netThread_ = std::thread([this] { NetThread(); });
        LOGI("[Client] Connecting to %s (source %u) ...", cfg_.server.ToString().c_str(),
            cfg_.sourceId);
        if (LoadUiSettings().keepAwake) {
            AcquireKeepAwake();
            keepAwakeHeld_ = true;
        }
        return true;
    }

    bool Start(const NetAddr& server, uint8_t sourceId, uint32_t screenW, uint32_t screenH) {
        ScreenViewerConfig cfg;
        cfg.server = server;
        cfg.sourceId = sourceId;
        cfg.screenW = screenW;
        cfg.screenH = screenH;
        return Start(cfg);
    }

    void Stop() {
        quit_.store(true);
        if (channel_) channel_->Kick();
        decCv_.notify_all();
        surfaceCv_.notify_all();
        if (netThread_.joinable()) netThread_.join();
        if (decodeThread_.joinable()) decodeThread_.join();
        player_.Stop();
        link_.Stop();
        if (keepAwakeHeld_) {
            ReleaseKeepAwake();
            keepAwakeHeld_ = false;
        }
    }

    void AcceptFingerprint() {
        link_.AcceptFingerprint();
    }

    void RejectFingerprint() {
        link_.RejectFingerprint();
    }

    std::string Fingerprint() const {
        return link_.FingerprintText();
    }

    deskhub::TrustVerdict Verdict() const {
        return link_.Verdict();
    }

    void SetSurface(Surface surface) {
        std::unique_lock<std::mutex> lk(surfaceMutex_);
        surface_ = surface;
        ++surfaceGen_;
        surfaceCv_.notify_all();
        decCv_.notify_all();
        if (!decodeRunning_) return;
        surfaceAckCv_.wait(
            lk, [this] { return surfaceAckGen_ == surfaceGen_ || decodeExited_; });
    }

    ClientPhase phase() const {
        return phase_.load(std::memory_order_acquire);
    }

    uint32_t videoWidth() const {
        return negW_.load();
    }
    uint32_t videoHeight() const {
        return negH_.load();
    }

    std::string StatusLine() {
        std::lock_guard<std::mutex> lk(textMutex_);
        return statusLine_;
    }

    deskhub::LinkPulseView LinkHealth() const {
        return link_.Pulse();
    }

    std::string EndReason() {
        std::lock_guard<std::mutex> lk(textMutex_);
        return endReason_;
    }

    void QueueKey(int32_t vk, int32_t scan, bool down) {
        input_.Key(vk, scan, down, NowUs());
    }
    void QueueKeyTap(int32_t vk, int32_t scan) {
        input_.KeyTap(vk, scan, NowUs());
    }
    void QueueKeyChord(int32_t modVk, int32_t modScan, int32_t vk, int32_t scan) {
        input_.KeyChord(modVk, modScan, vk, scan, NowUs());
    }
    void QueueCharTap(uint32_t codepoint) {
        input_.CharTap(codepoint, NowUs());
    }
    void QueueMouseMoveAbs(int32_t nx, int32_t ny) {
        input_.MouseMoveAbsolute(nx, ny, NowUs());
    }
    void QueueMouseMoveRel(int32_t dx, int32_t dy) {
        input_.MouseMoveRelative(dx, dy, NowUs());
    }
    void QueueMouseButton(int32_t button, bool down) {
        input_.MouseButtonEvent(button, down, NowUs());
    }
    void QueueMouseWheel(int32_t delta) {
        input_.MouseWheel(delta, NowUs());
    }
    void ReleaseAllInput() {
        input_.ReleaseAll(NowUs());
    }

    void OfferLocalClipboard(std::string text) {
        std::lock_guard<std::mutex> lk(clipMutex_);
        pendingLocalClip_ = std::move(text);
    }

    std::optional<std::string> TakeRemoteClipboard() {
        std::lock_guard<std::mutex> lk(clipMutex_);
        if (remoteClips_.empty()) return std::nullopt;
        std::string text = std::move(remoteClips_.front());
        remoteClips_.pop_front();
        return text;
    }

    void ReportPresented(uint64_t ptsUs, uint64_t shownUs) {
        if (!ptsUs) return;
        clockOffset_.AddSample(ptsUs, shownUs);
        lastE2eUs_.store(clockOffset_.LatencyUs(diag_.minRttUs.value() / 2));
    }

    deskhub::diag::ScreenClientDiag& diag() {
        return diag_;
    }

    AudioPlayer::Stats audioStats() const {
        return player_.stats();
    }

    bool audioRunning() const {
        return player_.running();
    }

    bool SendFiles(const std::vector<std::filesystem::path>& paths) {
        if (!upload_) return false;
        return upload_->Begin(paths);
    }

    bool uploading() const {
        return upload_ && upload_->Busy();
    }

    deskhub::FileSenderState uploadState() const {
        return upload_ ? upload_->State() : deskhub::FileSenderState::Idle;
    }

    deskhub::TransferProgress uploadProgress() const {
        return upload_ ? upload_->Progress() : deskhub::TransferProgress{};
    }

private:
    static constexpr size_t kMaxQueuedFrames = 3;
    static constexpr uint64_t kSlowDecodeMs = 20;

    bool TakeSurface(Surface& out) {
        std::lock_guard<std::mutex> lk(surfaceMutex_);
        out = surface_;
        return SurfaceIsUsable(out);
    }

    static bool SurfaceIsUsable(const Surface& s) {
        if constexpr (std::is_pointer_v<Surface>)
            return s != nullptr;
        else if constexpr (requires { { s.valid() } -> std::convertible_to<bool>; })
            return s.valid();
        else
            return true;
    }

    bool AckSurfaceSwap(Decoder& decoder) {
        std::unique_lock<std::mutex> lk(surfaceMutex_);
        if (surfaceAckGen_ == surfaceGen_) return false;

        const uint64_t generation = surfaceGen_;
        lk.unlock();
        const bool hadDecoder = decoder.IsOpen();
        decoder.Shutdown();
        lk.lock();

        surfaceAckGen_ = generation;
        surfaceAckCv_.notify_all();
        rebuildDecoder_.store(false);
        if (hadDecoder) decodeFailed_.store(true, std::memory_order_release);
        return true;
    }

    bool NextFrame(deskhub::Reassembler::Frame& out) {
        std::unique_lock<std::mutex> lk(decMutex_);
        decCv_.wait_for(lk, std::chrono::milliseconds(20),
            [this] { return quit_.load() || !decQueue_.empty(); });
        if (decQueue_.empty()) return false;
        out = std::move(decQueue_.front());
        decQueue_.pop_front();
        return true;
    }

    bool EnsureDecoder(Decoder& decoder) {
        if (rebuildDecoder_.exchange(false) && decoder.IsOpen()) decoder.Shutdown();
        if (decoder.IsOpen()) return true;

        const uint32_t w = negW_.load(), h = negH_.load();
        if (!w || !h) return false;

        Surface surface{};
        if (!TakeSurface(surface)) return false;

        if (!decoder.Init(surface, int(w), int(h))) {
            decodeFailed_.store(true, std::memory_order_release);
            return false;
        }
        return true;
    }

    void HarvestDecoder(Decoder& decoder) {
        if constexpr (deskhub::media::RenderCountingDecoder<Decoder>) {
            if (const uint32_t n = decoder.TakeRenderedCount())
                stRendered_.fetch_add(n, std::memory_order_relaxed);
        }
        if constexpr (deskhub::media::CongestionAwareDecoder<Decoder>) {
            if (const uint32_t n = decoder.TakeCongestionDrops()) {
                diag_.dispDrop.Add(n);
                displayCongested_.store(true, std::memory_order_release);
            }
        }
        if constexpr (deskhub::media::PresentTimingDecoder<Decoder>) {
            if (const uint32_t ms = decoder.TakePresentDelayMs()) diag_.presentMs.Add(ms);
        }
        if constexpr (deskhub::media::RenderCountingDecoder<Decoder>) {
            if (const uint64_t pts = decoder.lastRenderedPtsUs()) {
                uint64_t shownUs = NowUs();
                if constexpr (deskhub::media::PresentTimingDecoder<Decoder>) {
                    if (const uint64_t at = decoder.lastRenderedAtUs()) shownUs = at;
                }
                ReportPresented(pts, shownUs);
            }
        }
    }

    void DecodeThread() {
        if (cfg_.onDecodeThreadStart) cfg_.onDecodeThreadStart();

        {
            Decoder decoder;
            if constexpr (deskhub::media::PacedDecoder<Decoder>) {
                decoder.SetPacing(cfg_.pacingAdaptive, cfg_.displayIntervalUs);
                if (!cfg_.clockOffset.empty() && !decoder.SetClockOffset(cfg_.clockOffset))
                    LOGW(
                        "[Client] No clock offset estimator by that name is built in, so the "
                        "pacer keeps the default");
            }
            clockOffset_.Reset();

            for (;;) {
                AckSurfaceSwap(decoder);
                if (quit_.load()) break;

                deskhub::Reassembler::Frame frame;
                if (!NextFrame(frame)) continue;
                if (!EnsureDecoder(decoder)) continue;

                const uint64_t t0 = NowUs();
                const bool ok = decoder.Decode(frame.nal.data(), frame.nal.size(),
                    frame.timestampUs);
                const uint64_t decMs = (NowUs() - t0) / 1000;
                diag_.decMs.Add(uint32_t(decMs));
                if (decMs > kSlowDecodeMs)
                    LOGW("[Client] decode took %" PRIu64 " ms for one frame", decMs);

                if (!ok) {
                    decodeFailed_.store(true, std::memory_order_release);
                    decoder.Shutdown();
                    continue;
                }

                HarvestDecoder(decoder);
            }

            decoder.Shutdown();
        }

        {
            std::lock_guard<std::mutex> lk(surfaceMutex_);
            decodeExited_ = true;
            decodeRunning_ = false;
            surfaceAckGen_ = surfaceGen_;
        }
        surfaceAckCv_.notify_all();

        if (cfg_.onDecodeThreadExit) cfg_.onDecodeThreadExit();
    }

    std::string HostLabel() const {
        return cfg_.hostLabel.empty() ? cfg_.server.ToString() : cfg_.hostLabel;
    }

    void EnterReattach() {
        if (reattaching_.exchange(true, std::memory_order_acq_rel)) return;
        phase_.store(ClientPhase::Reattaching, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(textMutex_);
            statusLine_ = deskhub::ui::kTerminalReattaching;
        }
        if (cfg_.onStatus) cfg_.onStatus(deskhub::ui::kTerminalReattaching);
    }

    void NotifyEnded(const char* reason) {
        if (endedNotified_.exchange(true, std::memory_order_acq_rel)) return;
        if (cfg_.onEnded) cfg_.onEnded(reason && *reason ? reason : deskhub::ui::kDisconnected);
    }

    void OnLinkState(HostLinkState state, std::string_view message) {
        switch (state) {
            case HostLinkState::Deciding:
                phase_.store(ClientPhase::Deciding, std::memory_order_release);
                return;
            case HostLinkState::Recovering:
                EnterReattach();
                return;
            case HostLinkState::Connecting:
            case HostLinkState::Authing: {
                ClientPhase expected = ClientPhase::Deciding;
                phase_.compare_exchange_strong(expected, ClientPhase::Connecting,
                    std::memory_order_acq_rel);
                return;
            }
            case HostLinkState::Refused:
            case HostLinkState::Failed:
            case HostLinkState::Ended: {
                std::lock_guard<std::mutex> lk(textMutex_);
                if (endReason_.empty()) endReason_.assign(message);
                return;
            }
            default: return;
        }
    }

    bool AwaitAdmission() {
        while (!quit_.load()) {
            if (link_.State() == HostLinkState::Ready) return true;
            if (link_.Settled()) return false;
            SleepUs(kTrustPollUs);
        }
        return false;
    }

    int TakeMessage(uint8_t* buf, size_t cap) {
        const auto message = channel_->Poll();
        if (!message) return 0;
        const size_t take = std::min(cap, message->size());
        std::copy_n(message->begin(), take, buf);
        return int(take);
    }

    deskhub::ScreenClientCallbacks MakeScreenClientCallbacks() {
        deskhub::ScreenClientCallbacks cb;
        cb.send = [this](std::span<const uint8_t> d) { link_.Send(d); };
        cb.onFrame = [this](deskhub::Reassembler::Frame&& f) {
            {
                std::lock_guard<std::mutex> lk(decMutex_);
                if (decQueue_.size() >= kMaxQueuedFrames) {
                    decQueue_.pop_front();
                    queueOverflow_.store(true, std::memory_order_release);
                    diag_.dqDrop.Add();
                }
                decQueue_.push_back(std::move(f));
            }
            decCv_.notify_one();
        };
        cb.onAudioPacket = [this](const deskhub::AudioPacketView& v) { player_.Push(v); };
        cb.onClipboardText = [this](std::string_view text) {
            std::lock_guard<std::mutex> lk(clipMutex_);
            remoteClips_.emplace_back(text);
            while (remoteClips_.size() > 4) remoteClips_.pop_front();
        };
        cb.onParams = [this](const deskhub::NegotiatedParams& np, bool reconfigured) {
            negW_.store(np.width);
            negH_.store(np.height);
            if (reconfigured) rebuildDecoder_.store(true);
            if (cfg_.onParams) cfg_.onParams(np.width, np.height, np.fps);
        };
        cb.onEnded = [this](const char* reason, deskhub::ScreenSessionEnd cause) {
            if (cause == deskhub::ScreenSessionEnd::Timeout && !quit_.load() &&
                !link_.Settled()) {
                EnterReattach();
                link_.RequestRedial();
                return;
            }
            {
                std::lock_guard<std::mutex> lk(textMutex_);
                endReason_ = reason;
            }
            NotifyEnded(reason);
            quit_.store(true);
        };
        cb.takeRenderedCount = [this] {
            return stRendered_.exchange(0, std::memory_order_relaxed);
        };
        cb.latencyUs = [this] { return lastE2eUs_.load(); };
        cb.onStatus = [this](const char* compact) {
            {
                std::lock_guard<std::mutex> lk(textMutex_);
                statusLine_ = compact;
            }
            if (cfg_.onStatus) cfg_.onStatus(compact);
        };
        cb.localTime = [] { return LocalTimeHms(); };
        cb.log = [](bool warn, const char* line) {
            if (warn) {
                LOGW("%s", line);
                return;
            }
            LOGI("%s", line);
        };
        return cb;
    }

    void NetThread() {
        if (!AwaitAdmission()) {
            quit_.store(true);
            phase_.store(ClientPhase::Ended, std::memory_order_release);
            finished_.store(true, std::memory_order_release);
            decCv_.notify_all();
            std::string reason;
            {
                std::lock_guard<std::mutex> lk(textMutex_);
                if (endReason_.empty()) endReason_ = deskhub::ui::kTrustReject;
                reason = endReason_;
            }
            NotifyEnded(reason.c_str());
            if (cfg_.onFinished) cfg_.onFinished(reason.c_str());
            return;
        }

        deskhub::ScreenClient screen(MakeScreenClientCallbacks(), diag_);

        deskhub::ScreenClientConfig pcfg;
        pcfg.clientId = MakeClientId(cfg_.sourceId);
        pcfg.maxWidth = uint16_t(cfg_.screenW);
        pcfg.maxHeight = uint16_t(cfg_.screenH);
        pcfg.sourceId = cfg_.sourceId;
        pcfg.desiredFps = cfg_.desiredFps;
        pcfg.sendNacks = cfg_.sendNacks;
        pcfg.overtakenLimit = cfg_.overtakenLimit;
        pcfg.wantsAudio = cfg_.wantsAudio;
        pcfg.logLossRuns = cfg_.logLossRuns;
        pcfg.statusSeparator = cfg_.statusSeparator;
        pcfg.passcode = cfg_.passcode;
        pcfg.displayName = cfg_.displayName;
        pcfg.fecScheme = cfg_.fecScheme;
        screen.Start(pcfg, NowUs());

        std::vector<deskhub::InputEvent> batch;

        ScreenViewerLoopHooks hooks;
        hooks.stopped = [this] { return quit_.load(); };
        hooks.onFile = [this](std::span<const uint8_t> message) {
            if (upload_) upload_->HandleMessage(message);
        };
        hooks.pumpFiles = [this] {
            if (upload_) upload_->Pump();
        };
        hooks.afterFrames = [this](deskhub::ScreenClient& p, uint64_t now) {
            if (decodeFailed_.exchange(false, std::memory_order_acq_rel))
                p.RequestKeyframe(deskhub::diag::KeyframeReason::DecFail, now);
            if (displayCongested_.exchange(false, std::memory_order_acq_rel))
                p.RequestKeyframe(deskhub::diag::KeyframeReason::DisplayCongested, now);
            if (queueOverflow_.exchange(false, std::memory_order_acq_rel))
                p.RequestKeyframe(deskhub::diag::KeyframeReason::QOverflow, now);
        };
        hooks.beforeTick = [this, &batch, pcfg](deskhub::ScreenClient& p, uint64_t now) {
            if (relink_.exchange(false, std::memory_order_acq_rel)) {
                rebuildDecoder_.store(true);
                p.Start(pcfg, now);
            }
            input_.Drain(now, batch);
            for (const auto& e : batch) p.QueueInput(e);
            if (cfg_.alwaysFocused || input_.wantsFocus()) p.SetFocused(true);
            std::optional<std::string> clip;
            {
                std::lock_guard<std::mutex> lk(clipMutex_);
                clip.swap(pendingLocalClip_);
            }
            if (clip) p.QueueClipboard(*clip);
        };
        hooks.onPhase = [this](bool streaming) {
            if (streaming) {
                reattaching_.store(false, std::memory_order_release);
                phase_.store(ClientPhase::Streaming, std::memory_order_release);
                return;
            }
            phase_.store(reattaching_.load(std::memory_order_acquire)
                             ? ClientPhase::Reattaching
                             : ClientPhase::Connecting,
                std::memory_order_release);
        };
        hooks.onSocketError = [this] {
            LOGE("[Client] Socket error.");
            std::lock_guard<std::mutex> lk(textMutex_);
            if (endReason_.empty()) endReason_ = "socket error";
        };
        hooks.onSessionDead = [this] {
            return reattaching_.load(std::memory_order_acquire) && !quit_.load() &&
                   !link_.Settled();
        };

        RunScreenViewerLoop(
            [this](uint8_t* buf, size_t cap) {
                if (const int taken = TakeMessage(buf, cap)) return taken;
                if (link_.Settled()) return -1;
                channel_->WaitWork(10);
                return TakeMessage(buf, cap);
            },
            screen, hooks);

        if (upload_) upload_->LinkLost();
        quit_.store(true);
        decCv_.notify_all();
        std::string reason;
        {
            std::lock_guard<std::mutex> lk(textMutex_);
            if (endReason_.empty()) endReason_ = "stopped";
            reason = endReason_;
        }
        phase_.store(ClientPhase::Ended, std::memory_order_release);
        finished_.store(true, std::memory_order_release);
        NotifyEnded(reason.c_str());
        if (cfg_.onFinished) cfg_.onFinished(reason.c_str());
        LOGI("[Client] Session ended.");
    }

    static constexpr uint32_t kHandshakeTimeoutMs = 5'000;
    static constexpr uint32_t kAuthTimeoutMs = 65'000;
    static constexpr uint64_t kTrustPollUs = 20'000;

    ScreenViewerConfig cfg_{};
    HostLink link_;
    std::shared_ptr<HostLinkChannel> channel_;
    std::unique_ptr<FileUpload> upload_;

    std::thread netThread_;
    std::thread decodeThread_;

    std::atomic<bool> quit_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> reattaching_{false};
    std::atomic<bool> relink_{false};
    std::atomic<bool> endedNotified_{false};
    std::atomic<ClientPhase> phase_{ClientPhase::Idle};

    mutable std::mutex textMutex_;
    std::string statusLine_;
    std::string endReason_;

    std::atomic<uint32_t> negW_{0}, negH_{0};
    std::atomic<bool> rebuildDecoder_{false};

    std::mutex surfaceMutex_;
    std::condition_variable surfaceCv_;
    std::condition_variable surfaceAckCv_;
    Surface surface_{};
    uint64_t surfaceGen_ = 0;
    uint64_t surfaceAckGen_ = 0;
    bool decodeExited_ = false;
    bool decodeRunning_ = false;

    std::mutex decMutex_;
    std::condition_variable decCv_;
    std::deque<deskhub::Reassembler::Frame> decQueue_;

    std::atomic<bool> decodeFailed_{false};
    std::atomic<bool> displayCongested_{false};
    std::atomic<bool> queueOverflow_{false};
    std::atomic<uint32_t> stRendered_{0};

    std::mutex clipMutex_;
    std::optional<std::string> pendingLocalClip_;
    std::deque<std::string> remoteClips_;

    deskhub::ClientInputQueue input_;
    deskhub::diag::ScreenClientDiag diag_;
    AudioPlayer player_;

    std::atomic<int64_t> lastE2eUs_{-1};
    deskhub::ClockOffset clockOffset_;
    bool keepAwakeHeld_ = false;
};

}
