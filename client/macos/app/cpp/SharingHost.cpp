#include "deskhubp/host/SharingHost.h"

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

#include "Permissions.h"
#include "capture/ScreenCapture.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/input/LocalInput.h"
#include "deskhubp/media/VtSourcePipeline.h"
#include "deskhubp/system/Clock.h"
#include "input/InputInjector.h"

#include "deskhub/control/StreamSize.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/session/host/SourcePipeline.h"

namespace {

using MacSourceBase = deskhubp::VtSourcePipeline<ScreenCapture, InputInjector>;

struct SourcePipeline : MacSourceBase {
    SourcePipeline(uint32_t startBps, uint32_t minBps)
        : MacSourceBase(startBps, minBps, deskhub::diag::ShareDiagCaps{true, false}) {}

    uint32_t displayId = 0;
};

SourcePipeline& Pipeline(deskhubp::HostSource& st) {
    return static_cast<SourcePipeline&>(st);
}

bool ForEachLiveCapture(deskhubp::HostEngine& engine,
    const std::function<void(ScreenCapture&)>& fn) {
    bool any = false;
    engine.ForEachLiveSource([&](deskhubp::HostSource& st) {
        fn(Pipeline(st).capture);
        any = true;
    });
    return any;
}

}

bool SharingHost::Start(const std::vector<ShareSource>& sources, const ShareOptions& opt) {
    deskhubp::HostEngine* engine = &engine_;

    deskhubp::HostEnginePolicy policy;
    policy.startAudioCapture = [engine](const deskhub::media::AudioFormat&,
                                   std::function<void(std::span<const int16_t>)> onFrame) {
        return ForEachLiveCapture(*engine, [&onFrame](ScreenCapture& capture) {
            capture.SetAudioHandler(onFrame);
        });
    };
    policy.stopAudioCapture = [engine] {
        ForEachLiveCapture(*engine, [](ScreenCapture& capture) {
            capture.SetAudioHandler(nullptr);
        });
    };
    policy.source = deskhubp::MakeDefaultSourcePolicy<SourcePipeline>();
    policy.status = deskhubp::MakeDefaultStatusHooks<SourcePipeline>();
    policy.noSourceError = "No display to share.";
    policy.preflight = [] {
        if (!macperm::HasScreenRecording())
            LOGW(
                "[Host] Screen Recording permission not detected \xE2\x80\x94 "
                "capture will likely fail. Grant it in System Settings and restart.");
        return std::string();
    };

    policy.onSharing = [] {
        const bool ax = macperm::HasAccessibility();
        LOGI("[Host] Client control allowed (mouse + keyboard). Accessibility: %s%s",
            ax ? "YES" : "NO",
            ax ? "" : " \xE2\x80\x94 input will be silently dropped until it is granted");
    };

    policy.source.create = [engine](const ShareSource& s,
                               uint8_t sourceId) -> std::unique_ptr<deskhubp::HostSource> {
        auto p = deskhubp::MakeHostSource<SourcePipeline>(*engine, s, sourceId);
        p->displayId = uint32_t(s.targetId);
        return p;
    };

    policy.source.startCapture = [engine](deskhubp::HostSource& st) {
        SourcePipeline* p = &Pipeline(st);
        const uint32_t fps = engine->options().fps;
        const uint32_t maxDim = engine->options().maxDim;
        p->curFps.store(fps, std::memory_order_relaxed);

        auto onPacket = engine->MakePacketSink(*p);

        deskhubp::InstallVtEncoderFactory(p, fps, onPacket, engine->options().encoder);

        auto onFrame = [p, maxDim](const MacFrameInfo& fi) {
            deskhubp::OfferVtFrame(p, maxDim, fi);
        };

        if (!p->capture.Start(p->displayId,
                deskhub::media::CaptureOptions{fps, engine->options().maxDim,
                    engine->options().audio},
                onFrame)) {
            LOGE("[Host][%s] Failed to start capture \xE2\x80\x94 skipping this source.",
                p->name.c_str());
            p->failed.store(true);
        }
    };

    policy.source.stopCapture = deskhubp::StopVtCapture<SourcePipeline>;

    policy.source.attachInput = [engine](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        p.injector.SetLocalMonitor(&engine->localInput());
        p.injector.SetEnabled(p.injector.Init(p.displayId));
    };

    policy.source.retarget = [](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        uint32_t w = 0, h = 0;
        p.capture.SetClientSize(uint16_t(p.cliW), uint16_t(p.cliH), w, h);
        return deskhub::StreamSize{w, h};
    };

    policy.source.applyQualityStep = [](deskhubp::HostSource& st, const deskhub::QualityStep&,
                                         const deskhub::QualityStep& next) {
        SourcePipeline& p = Pipeline(st);
        {
            std::lock_guard<std::mutex> lk(p.encMutex);
            if (p.encoder) p.encoder->SetFps(next.fps);
        }
        uint32_t w = 0, h = 0;
        p.capture.SetQuality(next.scalePct, next.fps, w, h);
        return deskhub::StreamSize{w, h};
    };

    policy.source.takeIdleFrames = [](deskhubp::HostSource& st) {
        return Pipeline(st).capture.TakeIdleCount();
    };

    policy.source.flush = deskhubp::FlushVtSource<SourcePipeline>;

    return StartEngine(sources, opt, std::move(policy));
}
