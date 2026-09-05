#include "deskhubp/host/SharingHost.h"

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

#include "capture/ScreenCapture.h"

#include "deskhub/control/StreamSize.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/session/host/SourcePipeline.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/input/NullInputInjector.h"
#include "deskhubp/media/VtSourcePipeline.h"

namespace {

using IosSourceBase = deskhubp::VtSourcePipeline<ScreenCapture, deskhubp::NullInputInjector>;

struct SourcePipeline : IosSourceBase {
    SourcePipeline(uint32_t startBps, uint32_t minBps)
        : IosSourceBase(startBps, minBps, deskhub::diag::ShareDiagCaps{false, false}) {}
};

SourcePipeline& Pipeline(deskhubp::HostSource& st) {
    return static_cast<SourcePipeline&>(st);
}

bool AudioArrivesFromReplayKit(const deskhub::media::AudioFormat&,
    std::function<void(std::span<const int16_t>)>) {
    return true;
}

}

bool SharingHost::Start(const std::vector<ShareSource>& sources, const ShareOptions& opt) {
    deskhubp::HostEngine* engine = &engine_;

    deskhubp::HostEnginePolicy policy;
    policy.startAudioCapture = AudioArrivesFromReplayKit;
    policy.source = deskhubp::MakeDefaultSourcePolicy<SourcePipeline>();
    policy.status = deskhubp::MakeDefaultStatusHooks<SourcePipeline>();
    policy.noSourceError = "No screen to share.";
    policy.noUsableSourceError = "The broadcast ended before the first frame arrived.";

    policy.onSharing = [] {
        LOGI(
            "[Host] Viewers can watch this screen but cannot control it \xE2\x80\x94 iOS does "
            "not let an app inject input system-wide.");
    };

    policy.source.create = [engine](const ShareSource& s,
                               uint8_t sourceId) -> std::unique_ptr<deskhubp::HostSource> {
        auto p = deskhubp::MakeHostSource<SourcePipeline>(*engine, s, sourceId);
        p->nativeW.store(s.width, std::memory_order_relaxed);
        p->nativeH.store(s.height, std::memory_order_relaxed);
        return p;
    };

    policy.source.startCapture = [engine](deskhubp::HostSource& st) {
        SourcePipeline* p = &Pipeline(st);
        const uint32_t fps = engine->options().fps;
        const uint32_t maxDim = engine->options().maxDim;
        p->curFps.store(fps, std::memory_order_relaxed);

        auto onPacket = engine->MakePacketSink(*p);

        deskhubp::InstallVtEncoderFactory(p, fps, onPacket, engine->options().encoder);

        auto onFrame = [p, maxDim](const ScreenCapture::Frame& fi) {
            deskhubp::OfferVtFrame(p, maxDim, fi);
        };

        if (!p->capture.Start(deskhub::media::CaptureOptions{fps, maxDim}, onFrame)) {
            LOGE("[Host][%s] Failed to hook the broadcast frame stream.", p->name.c_str());
            p->failed.store(true);
        }
    };

    policy.source.stopCapture = deskhubp::StopVtCapture<SourcePipeline>;

    policy.source.retarget = [engine](deskhubp::HostSource& st) {
        return deskhub::RetargetStream(st, engine->options().maxDim);
    };

    policy.source.applyQualityStep = [engine](deskhubp::HostSource& st,
                                         const deskhub::QualityStep& prev,
                                         const deskhub::QualityStep& next) {
        SourcePipeline& p = Pipeline(st);
        if (prev.fps != next.fps) {
            std::lock_guard<std::mutex> lk(p.encMutex);
            if (p.encoder) p.encoder->SetFps(next.fps);
        }
        return deskhub::RetargetStream(st, engine->options().maxDim);
    };

    policy.source.flush = deskhubp::FlushVtSource<SourcePipeline>;

    return StartEngine(sources, opt, std::move(policy));
}
