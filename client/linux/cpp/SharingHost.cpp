#include "deskhubp/host/SharingHost.h"

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "capture/ScreenCapture.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/input/LocalInput.h"
#include "deskhubp/media/PortalScreenCast.h"
#include "encode/HwEncoder.h"
#include "input/InputInjector.h"

#include "deskhub/control/FrameGate.h"
#include "deskhub/control/StreamSize.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/media/FrameMailbox.h"
#include "deskhub/media/RgbDownscale.h"
#include "deskhub/session/host/SourcePipeline.h"

namespace {

using LinuxSourceBase = deskhubp::HostSourceBase<ScreenCapture, InputInjector, HwEncoder>;

struct SourcePipeline : LinuxSourceBase {
    SourcePipeline(uint32_t startBps, uint32_t minBps)
        : LinuxSourceBase(startBps, minBps, deskhub::diag::ShareDiagCaps{false, true, true}) {}

    uint32_t nodeId = 0;
    int32_t srcX = 0, srcY = 0;

    deskhub::FrameGate frameGate;

    deskhub::media::RgbDownscaler scaler;
    deskhub::media::FrameMailbox<CopiedFrame> encodeBox;
    std::thread encodeThread;
    uint32_t encoderW = 0, encoderH = 0;

    std::vector<uint8_t> TakePixelBuffer() {
        std::lock_guard<std::mutex> lk(pixelPoolMutex_);
        if (pixelPool_.empty()) return {};
        std::vector<uint8_t> buffer = std::move(pixelPool_.back());
        pixelPool_.pop_back();
        return buffer;
    }

    void ReturnPixelBuffer(std::vector<uint8_t>&& buffer) {
        std::lock_guard<std::mutex> lk(pixelPoolMutex_);
        if (pixelPool_.size() < kPixelPoolDepth) pixelPool_.push_back(std::move(buffer));
    }

private:
    static constexpr size_t kPixelPoolDepth = 2;

    std::mutex pixelPoolMutex_;
    std::vector<std::vector<uint8_t>> pixelPool_;
};

SourcePipeline& Pipeline(deskhubp::HostSource& st) {
    return static_cast<SourcePipeline&>(st);
}

const SourcePipeline& Pipeline(const deskhubp::HostSource& st) {
    return static_cast<const SourcePipeline&>(st);
}

}

bool SharingHost::Start(const std::vector<ShareSource>& sources, const ShareOptions& opt) {
    deskhubp::HostEngine* engine = &engine_;

    deskhubp::HostEnginePolicy policy;
    deskhubp::UseSystemAudioCapture(policy);
    policy.source = deskhubp::MakeDefaultSourcePolicy<SourcePipeline>();
    policy.status = deskhubp::MakeDefaultStatusHooks<SourcePipeline>();
    policy.noSourceError = "No display to share.";
    policy.noUsableSourceError =
        "No usable source \xE2\x80\x94 the compositor sent no frame.";
    policy.preflight = [] {
        if (deskhubp::PortalScreenCast::Instance().isOpen()) return std::string();
        return std::string(
            "The screen-capture permission is gone \xE2\x80\x94 press Share "
            "again.");
    };

    policy.status.zeroCopy = [](const deskhubp::HostSource& st) {
        return Pipeline(st).capture.usingDmaBuf();
    };

    policy.source.create = [engine](const ShareSource& s,
                               uint8_t sourceId) -> std::unique_ptr<deskhubp::HostSource> {
        auto p = deskhubp::MakeHostSource<SourcePipeline>(*engine, s, sourceId);
        p->nodeId = uint32_t(s.targetId);
        p->srcX = s.x;
        p->srcY = s.y;
        return p;
    };

    policy.source.startCapture = [engine](deskhubp::HostSource& st) {
        SourcePipeline* p = &Pipeline(st);
        const uint32_t fps = engine->options().fps;
        const uint32_t maxDim = engine->options().maxDim;
        const std::string encoderBackend = engine->options().encoder;

        auto onPacket = engine->MakePacketSink(*p);

        auto ensureEncoder = [p, fps, onPacket, encoderBackend](uint32_t w, uint32_t h,
                                 FrameMemory frameKind, uint32_t drmFormat) -> bool {
            if (p->encoder && p->encoder->IsOpen() && p->encoderW == w && p->encoderH == h)
                return true;
            p->encoder.reset();
            p->SetCachedFrame(false);
            EncoderConfig cfg = deskhub::MakeEncoderConfig(*p, {w, h}, fps);
            cfg.onPacket = onPacket;
            auto enc = std::make_unique<HwEncoder>();
            if (!enc->Init(cfg, frameKind, drmFormat, encoderBackend)) {
                LOGE("[Host][%s] No hardware encoder would start (NVENC or VA-API).",
                    p->name.c_str());
                p->failed.store(true);
                return false;
            }
            LOGI("[Host][%s] Encoding with %s.", p->name.c_str(), enc->BackendName());
            p->encoder = std::move(enc);
            p->encoderW = w;
            p->encoderH = h;
            return true;
        };

        auto encodeAt = [p, ensureEncoder](const LinuxFrameInfo& fi, uint32_t encodeW,
                            uint32_t encodeH) {
            std::lock_guard<std::mutex> lk(p->encMutex);
            if (!ensureEncoder(encodeW, encodeH, fi.memory, fi.drmFormat)) return;

            const bool idr = p->forceIdr.exchange(false);
            HwEncoder* enc = p->encoder.get();
            const bool ok = deskhubp::DiagEncode(*p, idr,
                [enc, &fi, idr] { return enc->Encode(fi, fi.meta.timestampUs, idr); });
            if (!ok) {
                p->encoder.reset();
                p->SetCachedFrame(false);
                p->forceIdr.store(true);
                return;
            }
            p->SetCachedFrame(enc->haveSourceFrame());
        };

        auto onFrame = [p, encodeAt, maxDim](const LinuxFrameInfo& fi) {
            p->captured.fetch_add(1, std::memory_order_relaxed);
            if (p->failed.load()) return;

            if (!p->frameGate.Admit(p->curFps.load(std::memory_order_relaxed),
                    fi.meta.timestampUs))
                return;

            const deskhub::FrameAdmission adm = deskhub::AdmitCapturedFrame(*p, fi.meta.width,
                fi.meta.height, maxDim);
            if (!adm.sizeNote.empty())
                LOGI("[Host][%s] %s", p->name.c_str(), adm.sizeNote.c_str());
            if (!adm.pauseNote.empty())
                LOGI("[Host][%s] %s", p->name.c_str(), adm.pauseNote.c_str());
            if (adm.drop) return;

            p->lastFrameUs.store(fi.meta.timestampUs, std::memory_order_relaxed);
            if (!p->netReady.load(std::memory_order_acquire)) return;

            if (fi.memory == FrameMemory::DmaBuf) {
                encodeAt(fi, adm.encode.width, adm.encode.height);
                return;
            }

            if (!p->scaler.Matches(fi.meta.width, fi.meta.height, adm.encode.width,
                    adm.encode.height))
                p->scaler.Configure(fi.meta.width, fi.meta.height, adm.encode.width,
                    adm.encode.height);
            if (!p->scaler.ready()) return;

            CopiedFrame copy;
            copy.pixels = p->TakePixelBuffer();
            copy.stride = adm.encode.width * deskhub::media::kPackedPixelBytes;
            copy.pixels.resize(size_t(copy.stride) * adm.encode.height);
            p->scaler.Scale(fi.handle, fi.stride, copy.pixels.data(), copy.stride);
            copy.drmFormat = fi.drmFormat;
            copy.meta = fi.meta;
            copy.meta.width = adm.encode.width;
            copy.meta.height = adm.encode.height;
            if (auto displaced = p->encodeBox.Put(std::move(copy))) {
                p->diag.queueDrop.Add();
                p->ReturnPixelBuffer(std::move(displaced->pixels));
            }
        };

        p->encodeThread = std::thread([p, encodeAt] {
            CopiedFrame copy;
            while (p->encodeBox.TakeWait(copy)) {
                encodeAt(FrameFromCopy(copy), copy.meta.width, copy.meta.height);
                p->ReturnPixelBuffer(std::move(copy.pixels));
            }
        });

        if (!p->capture.Start(p->nodeId, deskhub::media::CaptureOptions{fps, maxDim}, onFrame)) {
            LOGE("[Host][%s] Failed to start capture \xE2\x80\x94 skipping this source.",
                p->name.c_str());
            p->failed.store(true);
            p->encodeBox.Close();
            p->encodeThread.join();
        }
    };

    policy.source.stopCapture = [](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        p.capture.Stop();
        p.encodeBox.Close();
        if (p.encodeThread.joinable()) p.encodeThread.join();
        std::lock_guard<std::mutex> lk(p.encMutex);
        if (p.encoder) p.encoder->Finish();
        p.SetCachedFrame(false);
    };

    policy.source.attachInput = [engine](deskhubp::HostSource& st) {
        SourcePipeline& p = Pipeline(st);
        const ShareOptions& o = engine->options();
        p.injector.SetLocalMonitor(&engine->localInput());
        p.injector.SetEnabled(p.injector.Init(p.srcX, p.srcY, p.nativeW.load(), p.nativeH.load(),
            o.desktopX, o.desktopY, o.desktopW, o.desktopH));
    };

    policy.source.retarget = [engine](deskhubp::HostSource& st) {
        return deskhub::RetargetStream(st, engine->options().maxDim);
    };

    policy.source.applyQualityStep = [engine](deskhubp::HostSource& st,
                                         const deskhub::QualityStep& prev,
                                         const deskhub::QualityStep& next) {
        SourcePipeline& p = Pipeline(st);
        const deskhub::StreamSize t = deskhub::RetargetStream(st, engine->options().maxDim);
        if (prev.fps != next.fps) {
            std::lock_guard<std::mutex> lk(p.encMutex);
            p.encoder.reset();
            p.SetCachedFrame(false);
        }
        return t;
    };

    policy.source.flush = [](deskhubp::HostSource& st, uint64_t nowUs) {
        SourcePipeline& p = Pipeline(st);
        auto lk = deskhubp::TryHoldEncoder(p.encMutex);
        if (!lk.owns_lock() || !p.encoder || !p.hasCachedFrame()) return;
        const bool idr = p.forceIdr.exchange(false);
        HwEncoder* enc = p.encoder.get();
        const bool ok = deskhubp::DiagEncode(p, idr,
            [enc, nowUs, idr] { return enc->EncodeLast(nowUs, idr); });
        if (!ok) {
            p.encoder.reset();
            p.SetCachedFrame(false);
            p.forceIdr.store(true);
        }
    };

    return StartEngine(sources, opt, std::move(policy));
}
