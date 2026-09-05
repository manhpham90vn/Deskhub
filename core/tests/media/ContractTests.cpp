#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/CaptureContract.h"
#include "deskhub/media/PresentCounters.h"
#include "deskhub/media/CodecNegotiation.h"
#include "deskhub/media/VideoContract.h"

#include <cstdio>
#include <cstring>

using namespace deskhub;
using namespace deskhub::media;

namespace {

struct D3D11Texture;
struct LinuxFrame;

struct WinEncoderShape {
    virtual ~WinEncoderShape() = default;
    virtual bool Encode(D3D11Texture* frame, uint64_t timestampUs, bool forceKeyframe) = 0;
    virtual bool SetBitrate(uint32_t) = 0;
    virtual bool SetFps(uint32_t) = 0;
    virtual void Finish() = 0;
    virtual const char* BackendName() const = 0;
};

struct MacEncoderShape {
    bool Encode(void* pixelBuffer, uint64_t timestampUs, bool forceKeyframe);
    bool SetBitrate(uint32_t);
    bool SetFps(uint32_t);
    void Finish();
    const char* BackendName() const;
};

struct LinuxEncoderShape {
    bool Encode(const LinuxFrame& fi, uint64_t timestampUs, bool forceKeyframe);
    bool SetBitrate(uint32_t);
    void Finish();
    const char* BackendName() const;
};

struct LinuxCaptureShape {
    void Stop();
    bool Closed() const;
    bool usingDmaBuf() const;
};

struct MacCaptureShape {
    void Stop();
    bool Closed() const;
    void SetQuality(uint32_t scalePct, uint32_t fps, uint32_t& outW, uint32_t& outH);
};

struct WinCaptureShape {
    void Stop();
    bool Closed() const;
};

struct NotACapture {
    void Stop();
};

struct FrameWithMeta {
    void* handle;
    FrameMeta meta;
};

struct FrameWithLooseFields {
    uint32_t width;
    uint32_t height;
};

static_assert(ScreenCaptureLike<LinuxCaptureShape>);
static_assert(ScreenCaptureLike<MacCaptureShape>);
static_assert(ScreenCaptureLike<WinCaptureShape>);
static_assert(!ScreenCaptureLike<NotACapture>);

static_assert(ZeroCopyCapture<LinuxCaptureShape>);
static_assert(!ZeroCopyCapture<MacCaptureShape>);
static_assert(!ZeroCopyCapture<WinCaptureShape>);

static_assert(QualityAwareCapture<MacCaptureShape>);
static_assert(!QualityAwareCapture<LinuxCaptureShape>);
static_assert(!QualityAwareCapture<WinCaptureShape>);

static_assert(CapturedFrameLike<FrameWithMeta>);
static_assert(!CapturedFrameLike<FrameWithLooseFields>);

struct AppleDecoderShape {
    bool Decode(const uint8_t* nal, size_t len, uint64_t ptsUs);
    void Shutdown();
    bool IsOpen() const;
    uint32_t TakeRenderedCount();
    uint64_t lastRenderedPtsUs() const;
    uint32_t TakeCongestionDrops();
};

struct LinuxDecoderShape {
    bool Decode(const uint8_t* nal, size_t len, uint64_t ptsUs);
    void Shutdown();
    bool IsOpen() const;
};

struct WinDecoderShape {
    virtual ~WinDecoderShape() = default;
    virtual bool Decode(const uint8_t* data, size_t size, uint64_t timestampUs) = 0;
    virtual const char* BackendName() const = 0;
};

struct WrongReturnType {
    bool Encode(void*, uint64_t, bool);
    void SetBitrate(uint32_t);
    void Finish();
    const char* BackendName() const;
};

struct WrongArgOrder {
    bool Encode(void*, bool, uint64_t);
    bool SetBitrate(uint32_t);
    void Finish();
    const char* BackendName() const;
};

struct WideBackendName {
    bool Encode(void*, uint64_t, bool);
    bool SetBitrate(uint32_t);
    void Finish();
    const wchar_t* BackendName() const;
};

struct MissingFinish {
    bool Encode(void*, uint64_t, bool);
    bool SetBitrate(uint32_t);
    const char* BackendName() const;
};

struct DecoderTakingIntPts {
    bool Decode(const uint8_t*, size_t, int);
};

static_assert(VideoEncoderLike<WinEncoderShape, D3D11Texture*>);
static_assert(VideoEncoderLike<MacEncoderShape, void*>);
static_assert(VideoEncoderLike<LinuxEncoderShape, const LinuxFrame&>);

static_assert(HotFpsEncoder<WinEncoderShape>);
static_assert(HotFpsEncoder<MacEncoderShape>);
static_assert(!HotFpsEncoder<LinuxEncoderShape>, "VA-API cannot change fps on the fly");

static_assert(VideoDecoderLike<AppleDecoderShape>);
static_assert(VideoDecoderLike<LinuxDecoderShape>);
static_assert(VideoDecoderLike<WinDecoderShape>);

static_assert(RestartableDecoder<AppleDecoderShape>);
static_assert(RestartableDecoder<LinuxDecoderShape>);
static_assert(!RestartableDecoder<WinDecoderShape>, "Windows restarts with a fresh object");

static_assert(RenderCountingDecoder<AppleDecoderShape>);
static_assert(!RenderCountingDecoder<LinuxDecoderShape>, "Ubuntu counts in VideoSink");

static_assert(CongestionAwareDecoder<AppleDecoderShape>);
static_assert(!CongestionAwareDecoder<LinuxDecoderShape>,
    "only platforms with an async display layer have disp_drop");

static_assert(!VideoEncoderLike<WrongReturnType, void*>);
static_assert(!VideoEncoderLike<WrongArgOrder, void*>);
static_assert(!VideoEncoderLike<WideBackendName, void*>);
static_assert(!VideoEncoderLike<MissingFinish, void*>);
static_assert(!VideoDecoderLike<DecoderTakingIntPts>);

void TestNegotiationPicksTheBestBothSidesHave() {
    std::printf("[codec] negotiation picks the best both ends have, never one only one has...\n");

    Check(NegotiateCodec(kCodecMaskH264, kCodecMaskH264) == Codec::H264,
        "two ends that only speak the baseline agree on the baseline");
    Check(NegotiateCodec(kCodecMaskH264 | kCodecMaskAv1, kCodecMaskH264) == Codec::H264,
        "a host that can do more than the viewer still drops to what the viewer has");
    Check(NegotiateCodec(kCodecMaskH264, kCodecMaskH264 | kCodecMaskHevc) == Codec::H264,
        "and the same the other way round");
    Check(NegotiateCodec(kCodecMaskH264 | kCodecMaskHevc, kCodecMaskH264 | kCodecMaskHevc) ==
              Codec::Hevc,
        "when both have something better than the baseline, they take it");
    Check(NegotiateCodec(0xFFFF, 0xFFFF) == Codec::Av1,
        "and with everything on both sides the head of the preference list wins");
    Check(NegotiateCodec(kCodecMaskHevc, kCodecMaskAv1) == Codec::Rejected,
        "two ends with nothing in common are rejected rather than guessed at");
    Check(NegotiateCodec(0, 0) == Codec::Rejected, "a side that offers nothing is rejected");
}

void TestBaselineIsAlwaysReachable() {
    std::printf("[codec] the universal baseline is reachable from every capability set...\n");

    for (Codec codec : CodecPreference()) {
        Check(CodecMaskOf(codec) != 0, "every codec in the preference list has a mask bit");
        Check(NegotiateCodec(CodecMaskOf(codec) | kCodecMaskH264,
                  CodecMaskOf(codec) | kCodecMaskH264) == codec,
            "a pair that shares a codec agrees on it rather than falling back");
        Check(NegotiateCodec(CodecMaskOf(codec) | kCodecMaskH264, kCodecMaskH264) == Codec::H264,
            "and any pair still meets on the baseline when that is all they share");
    }

    Check(CodecIsUniversal(Codec::H264), "H264 4:2:0 is the one every platform must decode");
    for (Codec codec : CodecPreference())
        if (codec != Codec::H264)
            Check(!CodecIsUniversal(codec), "nothing else may be assumed present");

    Check(CodecPreference().back() == Codec::H264,
        "the baseline sits last in the preference list, so it is the fallback and never the "
        "first choice when something better is shared");
}

}

void RunMediaContractTests() {
    std::printf("[media] encoder/decoder signature contract for all five platforms (checked at compile time)...\n");

    TestNegotiationPicksTheBestBothSidesHave();
    TestBaselineIsAlwaysReachable();

    std::printf("[media] capture contract: the concepts reject a type that does not fit...\n");
    Check(!ScreenCaptureLike<NotACapture>, "a capture with no Closed() is not a capture");
    Check(!CapturedFrameLike<FrameWithLooseFields>,
        "loose width/height fields do not satisfy the frame metadata contract");
    Check(ZeroCopyCapture<LinuxCaptureShape> && !ZeroCopyCapture<WinCaptureShape>,
        "zero-copy is opt-in, not assumed");
    Check(QualityAwareCapture<MacCaptureShape> && !QualityAwareCapture<LinuxCaptureShape>,
        "so is capture-side rescaling");

    const FrameMeta fm;
    Check(fm.width == 0 && fm.height == 0 && fm.timestampUs == 0 && fm.frameId == 0,
        "FrameMeta starts zeroed, so a capture that forgets a field cannot leak stale values");
    const CaptureOptions co;
    Check(co.fps == 60 && co.maxDim == 0, "CaptureOptions: 60 fps, no cap, by default");

    std::printf("[media] EncoderConfig/DecoderConfig defaults...\n");
    const EncoderConfig ec;
    Check(ec.fps == 60, "EncoderConfig: fps defaults to 60");
    Check(ec.rc == RateControl::CBR, "EncoderConfig: rate control defaults to CBR");
    Check(ec.lowLatency, "EncoderConfig: low latency is the default");
    Check(ec.bitrateBps == 20'000'000, "EncoderConfig: bitrate defaults to 20 Mbps");
    Check(ec.srcWidth == 0 && ec.srcHeight == 0,
        "EncoderConfig: 0 means 'same as width/height' — backends that cannot crop must "
        "refuse a mismatch");
    Check(!ec.onPacket, "EncoderConfig: with onPacket unset there is no output path");

    const DecoderConfig dc;
    Check(dc.fps == 60, "DecoderConfig: fps defaults to 60");
    Check(dc.width == 0 && dc.height == 0,
        "DecoderConfig: 0 = unknown, the decoder reads it back from the SPS");

    std::printf("[media] shared present counters (harvested by the client engine)...\n");
    PresentCounters pc;
    pc.FramePresented(1'000, 2'000);
    pc.FramePresented(0, 9'999);
    pc.RecordPresentDelayMs(7);
    pc.CountCongestionDrop();
    Check(pc.TakeRenderedCount() == 2, "every presented frame is counted");
    Check(pc.TakeRenderedCount() == 0, "and the count resets on harvest");
    Check(pc.lastRenderedPtsUs() == 1'000 && pc.lastRenderedAtUs() == 2'000,
        "a zero pts does not clobber the last timed frame");
    Check(pc.TakePresentDelayMs() == 7 && pc.TakePresentDelayMs() == 0,
        "the present delay is a take-once value");
    Check(pc.TakeCongestionDrops() == 1, "congestion drops are counted");
    pc.FramePresented(5, 6);
    pc.Reset();
    Check(pc.TakeRenderedCount() == 0 && pc.lastRenderedPtsUs() == 0,
        "a decoder restart clears everything");
}
