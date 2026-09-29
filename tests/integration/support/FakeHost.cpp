#include "support/FakeHost.h"

#include "support/FakeVideo.h"
#include "support/TestSupport.h"

#include "deskhub/control/StreamSize.h"
#include "deskhub/session/host/SourcePipeline.h"

#include "deskhubp/system/Clock.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <cmath>
#include <ctime>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace fake {
namespace {

using FakeSourceBase = deskhubp::HostSourceBase<Capture, Injector, Encoder>;

struct Pipeline : FakeSourceBase {
    Pipeline(uint32_t startBps, uint32_t minBps)
        : FakeSourceBase(startBps, minBps, deskhub::diag::ShareDiagCaps{}) {}

    uint32_t nativeWidth = 0;
    uint32_t nativeHeight = 0;
};

Pipeline& Pipe(deskhubp::HostSource& st) {
    return static_cast<Pipeline&>(st);
}

}

deskhub::media::ShareSource Source(const char* name, uint32_t width, uint32_t height,
    uint64_t targetId) {
    deskhub::media::ShareSource s;
    s.name = name;
    s.width = width;
    s.height = height;
    s.targetId = targetId;
    return s;
}

void SharingHost::StartTone(const deskhub::media::AudioFormat& format,
    const std::function<void(std::span<const int16_t>)>& offer) {
    StopTone();
    toneStop_.store(false, std::memory_order_release);
    tone_ = std::thread([this, format, offer] {
        std::vector<int16_t> pcm(format.interleavedSamples());
        const uint64_t frameUs =
            uint64_t(format.samplesPerFrame) * 1'000'000 / format.sampleRate;
        uint64_t phase = 0;
        uint64_t dueUs = NowUs();
        while (!toneStop_.load(std::memory_order_acquire)) {
            for (uint32_t i = 0; i < format.samplesPerFrame; ++i) {
                const double t = double(phase + i) / double(format.sampleRate);
                const auto sample = int16_t(8000.0 * std::sin(2.0 * 3.14159265 * 440.0 * t));
                for (uint32_t c = 0; c < format.channels; ++c)
                    pcm[size_t(i) * format.channels + c] = sample;
            }
            phase += format.samplesPerFrame;
            offer(pcm);
            dueUs += frameUs;
            const uint64_t now = NowUs();
            if (dueUs > now) SleepUs(dueUs - now);
        }
    });
}

void SharingHost::StopTone() {
    if (!tone_.joinable()) return;
    toneStop_.store(true, std::memory_order_release);
    tone_.join();
}

bool SharingHost::Start(const std::vector<deskhub::media::ShareSource>& sources, uint16_t port,
    uint32_t fps, uint32_t maxDim, bool allowInput, bool audio) {
    deskhub::media::ShareOptions opt;
    opt.fps = fps;
    opt.maxDim = maxDim;
    opt.bitrateMbps = 8;
    opt.port = port;
    opt.allowInput = allowInput;
    opt.audio = audio;

    deskhubp::HostEngine* engine = &engine_;

    deskhubp::HostEnginePolicy policy;
    policy.source = deskhubp::MakeDefaultSourcePolicy<Pipeline>();
    policy.status = deskhubp::MakeDefaultStatusHooks<Pipeline>();

    policy.startAudioCapture = [this](const deskhub::media::AudioFormat& format,
                                   std::function<void(std::span<const int16_t>)> offer) {
        StartTone(format, offer);
        return true;
    };
    policy.stopAudioCapture = [this] { StopTone(); };

    policy.source.create = [engine](const deskhub::media::ShareSource& s,
                               uint8_t sourceId) -> std::unique_ptr<deskhubp::HostSource> {
        auto p = deskhubp::MakeHostSource<Pipeline>(*engine, s, sourceId);
        p->nativeWidth = s.width;
        p->nativeHeight = s.height;
        return p;
    };

    policy.source.startCapture = [engine](deskhubp::HostSource& st) {
        Pipeline* p = &Pipe(st);
        const uint32_t fps = engine->options().fps;
        const uint32_t maxDim = engine->options().maxDim;
        auto onPacket = engine->MakePacketSink(*p);

        auto onFrame = [p, onPacket, fps, maxDim](const deskhub::media::FrameMeta& meta) {
            p->captured.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lk(p->encMutex);

            const deskhub::FrameAdmission adm =
                deskhub::AdmitCapturedFrame(*p, meta.width, meta.height, maxDim);
            if (adm.rebuildEncoder) p->encoder.reset();
            if (adm.drop) return;

            p->lastFrameUs.store(meta.timestampUs, std::memory_order_relaxed);
            if (!p->netReady.load(std::memory_order_acquire)) return;

            if (!p->encoder) {
                deskhub::media::EncoderConfig cfg =
                    deskhub::MakeEncoderConfig(*p, adm.encode, fps);
                cfg.onPacket = onPacket;
                auto enc = std::make_unique<Encoder>();
                if (!enc->Init(cfg)) {
                    p->failed.store(true);
                    return;
                }
                p->encoder = std::move(enc);
            }

            const bool idr = p->forceIdr.exchange(false);
            Encoder* enc = p->encoder.get();
            deskhubp::DiagEncode(*p, idr,
                [enc, &meta, idr] { return enc->Encode(meta, meta.timestampUs, idr); });
        };

        p->capture.Start(p->nativeWidth, p->nativeHeight, fps, onFrame);
    };

    policy.source.stopCapture = [](deskhubp::HostSource& st) {
        Pipeline& p = Pipe(st);
        p.capture.Stop();
        std::lock_guard<std::mutex> lk(p.encMutex);
        if (p.encoder) p.encoder->Finish();
    };

    policy.source.attachInput = [](deskhubp::HostSource&) {};

    policy.source.retarget = [engine](deskhubp::HostSource& st) {
        return deskhub::RetargetStream(st, engine->options().maxDim);
    };

    policy.source.applyQualityStep = [engine](deskhubp::HostSource& st,
                                         const deskhub::QualityStep& prev,
                                         const deskhub::QualityStep& next) {
        Pipeline& p = Pipe(st);
        if (prev.fps != next.fps) {
            std::lock_guard<std::mutex> lk(p.encMutex);
            p.encoder.reset();
        }
        return deskhub::RetargetStream(st, engine->options().maxDim);
    };

    const auto client = deskhubp::LoadOrCreateClientIdentity();
    const auto host = deskhubp::LoadOrCreateHostIdentity("integration-host");
    const std::string address = NetAddr{0x7F000001u, port}.ToString();
    if (!client.Valid() || !host.Valid() ||
        !GrantClientKey(client) ||
        !deskhubp::RememberTrustedHost(address, "127.0.0.1", host.fingerprint, std::time(nullptr)))
        return false;
    return engine_.Start(sources, opt, std::move(policy));
}

}
