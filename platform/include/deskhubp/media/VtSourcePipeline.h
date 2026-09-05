#pragma once
#include "deskhub/control/FrameGate.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/media/EncoderBackend.h"
#include "deskhub/session/host/SourcePipeline.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/media/VtEncoder.h"
#include "deskhubp/host/HostEngine.h"

#include <CoreVideo/CVPixelBuffer.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace deskhubp {

template <class Capture, class Injector>
struct VtSourcePipeline : HostSourceBase<Capture, Injector, VtEncoder> {
    using Base = HostSourceBase<Capture, Injector, VtEncoder>;

    VtSourcePipeline(uint32_t startBps, uint32_t minBps, deskhub::diag::ShareDiagCaps caps)
        : Base(startBps, minBps, caps) {}

    ~VtSourcePipeline() override {
        ReleaseCached();
    }

    std::function<bool(uint32_t, uint32_t)> ensureEncoderFn;

    deskhub::FrameGate frameGate;

    void* cachedPb = nullptr;

    void ReleaseCached() {
        if (!cachedPb) return;
        CVPixelBufferRelease(static_cast<CVPixelBufferRef>(cachedPb));
        cachedPb = nullptr;
        this->SetCachedFrame(false);
    }

    void EncodeTimed(void* pb, uint64_t tsUs, bool idr) {
        const bool ok = DiagEncode(*this, idr,
            [this, pb, tsUs, idr] { return this->encoder->Encode(pb, tsUs, idr); });
        if (ok) return;
        this->encoder.reset();
        this->forceIdr.store(true);
    }
};

template <class Pipeline>
void InstallVtEncoderFactory(Pipeline* p, uint32_t fps, deskhub::media::PacketHandler onPacket,
    std::string backend = {}) {
    p->ensureEncoderFn = [p, fps, backend = std::move(backend),
                             onPacket = std::move(onPacket)](uint32_t w, uint32_t h) {
        if (p->encoder && p->encoder->IsOpen()) return true;
        if (!backend.empty() && backend != deskhub::media::kEncoderBackendAuto &&
            backend != deskhub::media::kEncoderBackendVideoToolbox) {
            LOGE("[Host][%s] This build has no backend called \"%s\" - it has videotoolbox.",
                p->name.c_str(), backend.c_str());
            p->failed.store(true);
            return false;
        }
        EncoderConfig cfg = deskhub::MakeEncoderConfig(*p, {w, h}, fps);
        cfg.onPacket = onPacket;
        auto enc = std::make_unique<VtEncoder>();
        if (!enc->Init(cfg)) {
            LOGE("[Host][%s] VideoToolbox refused to start an encoder.", p->name.c_str());
            p->failed.store(true);
            return false;
        }
        p->encoder = std::move(enc);
        return true;
    };
}

template <class Pipeline, class Frame>
void OfferVtFrame(Pipeline* p, uint32_t maxDim, const Frame& fi) {
    p->captured.fetch_add(1, std::memory_order_relaxed);
    if (p->failed.load()) return;

    std::lock_guard<std::mutex> lk(p->encMutex);

    const deskhub::FrameAdmission adm =
        deskhub::AdmitCapturedFrame(*p, fi.meta.width, fi.meta.height, maxDim);
    if (adm.rebuildEncoder) {
        p->encoder.reset();
        p->ReleaseCached();
    }
    if (!adm.sizeNote.empty()) LOGI("[Host][%s] %s", p->name.c_str(), adm.sizeNote.c_str());
    if (!adm.pauseNote.empty()) LOGI("[Host][%s] %s", p->name.c_str(), adm.pauseNote.c_str());
    if (adm.drop) return;

    if (p->cachedPb) CVPixelBufferRelease(static_cast<CVPixelBufferRef>(p->cachedPb));
    p->cachedPb = CVPixelBufferRetain(static_cast<CVPixelBufferRef>(fi.handle));
    p->SetCachedFrame(p->cachedPb != nullptr);
    p->lastFrameUs.store(fi.meta.timestampUs, std::memory_order_relaxed);

    if (!p->netReady.load(std::memory_order_acquire)) return;
    if (!p->frameGate.Admit(p->curFps.load(std::memory_order_relaxed), fi.meta.timestampUs)) return;
    if (!p->ensureEncoderFn(adm.encode.width, adm.encode.height)) return;
    p->EncodeTimed(fi.handle, fi.meta.timestampUs, p->forceIdr.exchange(false));
}

template <class Pipeline>
void StopVtCapture(HostSource& st) {
    auto& p = static_cast<Pipeline&>(st);
    p.capture.Stop();
    std::lock_guard<std::mutex> lk(p.encMutex);
    if (p.encoder) p.encoder->Finish();
    p.ReleaseCached();
}

template <class Pipeline>
void FlushVtSource(HostSource& st, uint64_t nowUs) {
    auto& p = static_cast<Pipeline&>(st);
    if (!p.hasCachedFrame()) return;
    auto lk = TryHoldEncoder(p.encMutex);
    if (!lk.owns_lock()) return;
    if (!p.cachedPb || !p.ensureEncoderFn(p.srcW.load(), p.srcH.load())) return;
    p.EncodeTimed(p.cachedPb, nowUs, p.forceIdr.exchange(false));
    if (p.encoder) p.encoder->Flush();
}

}
