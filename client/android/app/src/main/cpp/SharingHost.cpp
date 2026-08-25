#include "deskhubp/host/SharingHost.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

#include "capture/ScreenCapture.h"
#include "encode/MediaCodecEncoder.h"

#include "deskhub/control/StreamSize.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/media/H264Encode.h"
#include "deskhub/session/host/SourcePipeline.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/input/NullInputInjector.h"

namespace {

using AndroidSourceBase =
    deskhubp::HostSourceBase<ScreenCapture, deskhubp::NullInputInjector, MediaCodecEncoder>;

struct SourcePipeline : AndroidSourceBase {
    SourcePipeline(uint32_t startBps, uint32_t minBps)
        : AndroidSourceBase(startBps, minBps, deskhub::diag::ShareDiagCaps{false, true}) {}

    std::atomic<uint32_t> displayW{0}, displayH{0};
    deskhub::media::PacketHandler sink;
    uint32_t fps = 0;
    uint32_t maxDim = 0;
};

SourcePipeline& Pipeline(deskhubp::HostSource& st) {
    return static_cast<SourcePipeline&>(st);
}

deskhub::StreamSize PlanStreamSize(SourcePipeline& p) {
    const deskhub::StreamSize target = deskhub::RetargetStream(p, p.maxDim);
    const uint32_t align = deskhub::media::kH264MacroblockPx;
    const deskhub::StreamSize aligned{target.width - target.width % align,
        target.height - target.height % align};
    p.wantW.store(aligned.width, std::memory_order_relaxed);
    p.wantH.store(aligned.height, std::memory_order_relaxed);
    return aligned;
}

deskhub::media::PacketHandler MakeEncoderSink(SourcePipeline& p, MediaCodecEncoder& encoder) {
    return [&p, &encoder](const uint8_t* data, size_t size, uint64_t tsUs, bool keyframe) {
        p.captured.fetch_add(1, std::memory_order_relaxed);
        p.lastFrameUs.store(tsUs, std::memory_order_relaxed);
        if (p.netReady.load(std::memory_order_acquire) && p.sink)
            p.sink(data, size, tsUs, keyframe);
        if (p.forceIdr.exchange(false)) encoder.Encode(nullptr, tsUs, true);
    };
}

bool RebuildPipeline(SourcePipeline& p) {
    const deskhub::FrameAdmission adm = deskhub::AdmitCapturedFrame(p,
        p.displayW.load(std::memory_order_relaxed), p.displayH.load(std::memory_order_relaxed),
        p.maxDim);
    if (!adm.sizeNote.empty()) LOGI("[Host][%s] %s", p.name.c_str(), adm.sizeNote.c_str());
    if (!adm.pauseNote.empty()) LOGI("[Host][%s] %s", p.name.c_str(), adm.pauseNote.c_str());

    if (p.encoder && p.encoder->IsOpen() && !adm.rebuildEncoder) return true;

    p.capture.DetachEncoderSurface();
    p.encoder.reset();
    if (adm.drop) return false;

    auto encoder = std::make_unique<MediaCodecEncoder>();
    EncoderConfig cfg = deskhub::MakeEncoderConfig(p, adm.encode, p.fps);
    cfg.onPacket = MakeEncoderSink(p, *encoder);
    if (!encoder->Init(cfg)) {
        LOGE("[Host][%s] MediaCodec refused to start an encoder.", p.name.c_str());
        p.failed.store(true);
        return false;
    }
    if (!p.capture.AttachEncoderSurface(encoder->inputWindow(), adm.encode.width,
            adm.encode.height)) {
        p.failed.store(true);
        return false;
    }
    p.encoder = std::move(encoder);
    p.forceIdr.store(true);
    return true;
}

bool RebuildLocked(SourcePipeline& p) {
    std::lock_guard<std::mutex> lk(p.encMutex);
    return RebuildPipeline(p);
}

bool AudioArrivesFromMediaProjection(const deskhub::media::AudioFormat&,
    std::function<void(std::span<const int16_t>)>) {
    return true;
}

}

bool SharingHost::Start(const std::vector<ShareSource>& sources, const ShareOptions& opt) {
    deskhubp::HostEngine* engine = &engine_;

    deskhubp::HostEnginePolicy policy;
    policy.startAudioCapture = AudioArrivesFromMediaProjection;
    policy.source = deskhubp::MakeDefaultSourcePolicy<SourcePipeline>();
    policy.status = deskhubp::MakeDefaultStatusHooks<SourcePipeline>();
    policy.noSourceError = "No screen to share.";
    policy.noUsableSourceError =
        "Screen capture never started \xE2\x80\x94 the recording permission was withdrawn.";
    policy.status.zeroCopy = [](const deskhubp::HostSource&) { return true; };

    policy.onSharing = [] {
        LOGI(
            "[Host] Viewers can watch this screen but cannot control it \xE2\x80\x94 Android "
            "does not let an app inject input system-wide.");
    };

    policy.source.create = [engine](const ShareSource& s,
                               uint8_t sourceId) -> std::unique_ptr<deskhubp::HostSource> {
        auto p = deskhubp::MakeHostSource<SourcePipeline>(*engine, s, sourceId);
        p->displayW.store(s.width, std::memory_order_relaxed);
        p->displayH.store(s.height, std::memory_order_relaxed);
        p->nativeW.store(s.width, std::memory_order_relaxed);
        p->nativeH.store(s.height, std::memory_order_relaxed);
        return p;
    };

    policy.source.startCapture = [engine](deskhubp::HostSource& st) {
        SourcePipeline* p = &Pipeline(st);
        p->fps = engine->options().fps;
        p->maxDim = engine->options().maxDim;
        p->curFps.store(p->fps, std::memory_order_relaxed);
        p->sink = engine->MakePacketSink(*p);

        auto onDisplaySize = [p](uint32_t width, uint32_t height) {
            p->displayW.store(width, std::memory_order_relaxed);
            p->displayH.store(height, std::memory_order_relaxed);
            PlanStreamSize(*p);
            RebuildLocked(*p);
        };

        if (!p->capture.Start(deskhub::media::CaptureOptions{p->fps, p->maxDim},
                onDisplaySize)) {
            LOGE("[Host][%s] Failed to start screen capture.", p->name.c_str());
            p->failed.store(true);
            return;
        }

        PlanStreamSize(*p);
        if (!RebuildLocked(*p)) p->failed.store(true);
    };

    policy.source.stopCapture = [](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        p.capture.Stop();
        std::lock_guard<std::mutex> lk(p.encMutex);
        p.encoder.reset();
    };

    policy.source.retarget = [](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        const deskhub::StreamSize target = PlanStreamSize(p);
        RebuildLocked(p);
        return target;
    };

    policy.source.applyQualityStep = [](deskhubp::HostSource& st,
                                         const deskhub::QualityStep& prev,
                                         const deskhub::QualityStep& next) {
        SourcePipeline& p = Pipeline(st);
        const deskhub::StreamSize target = PlanStreamSize(p);
        std::lock_guard<std::mutex> lk(p.encMutex);
        if (prev.fps != next.fps && p.encoder) p.encoder->SetFps(next.fps);
        RebuildPipeline(p);
        return target;
    };

    return StartEngine(sources, opt, std::move(policy));
}
