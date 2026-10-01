#include "deskhub/session/host/SourcePipeline.h"

#include "deskhub/media/SourceLabel.h"

namespace deskhub {

SourcePipelineState* RouteDatagram(std::span<SourcePipelineState* const> live,
    const CommonHeader& header, std::span<const uint8_t> pkt) {
    if (header.type == MsgType::Hello) {
        const auto hello = ParseHello(PayloadOf(pkt));
        if (!hello) return nullptr;
        for (SourcePipelineState* p : live)
            if (p && p->sourceId == hello->sourceId) return p;
        return nullptr;
    }

    if (!header.sessionId) return nullptr;
    for (SourcePipelineState* p : live)
        if (p && p->session && p->session->sessionId() == header.sessionId) return p;
    return nullptr;
}

AcceptedDatagram AcceptDatagram(std::span<SourcePipelineState* const> live,
    std::span<const uint8_t> pkt, uint64_t packedFrom, uint64_t nowUs) {
    AcceptedDatagram out;

    const auto header = ParseCommonHeader(pkt);
    if (!header) return out;
    out.parsed = true;

    SourcePipelineState* dst = RouteDatagram(live, *header, pkt);
    if (!dst || dst->failed.load(std::memory_order_acquire)) return out;
    dst->replyPacked.store(packedFrom, std::memory_order_release);
    if (!dst->session || !dst->session->HandlePacket(pkt, nowUs, packedFrom)) return out;

    out.target = dst;
    return out;
}

StreamSize RetargetStream(SourcePipelineState& st, uint32_t maxDim) {
    const uint32_t nw = st.nativeW.load(std::memory_order_relaxed);
    const uint32_t nh = st.nativeH.load(std::memory_order_relaxed);
    if (!nw || !nh) return {};

    const StreamSize t = TargetStreamSize(nw, nh, maxDim, st.cliW, st.cliH, st.step.scalePct);
    if (!t.width || !t.height) return {};

    st.wantW.store(t.width, std::memory_order_relaxed);
    st.wantH.store(t.height, std::memory_order_relaxed);
    return t;
}

FrameAdmission AdmitCapturedFrame(SourcePipelineState& st, uint32_t nativeW, uint32_t nativeH,
    uint32_t maxDim) {
    FrameAdmission out;
    if (!nativeW || !nativeH) {
        out.drop = true;
        return out;
    }

    const uint32_t prevNativeW = st.nativeW.load(std::memory_order_relaxed);
    const uint32_t prevNativeH = st.nativeH.load(std::memory_order_relaxed);
    st.nativeW.store(nativeW, std::memory_order_relaxed);
    st.nativeH.store(nativeH, std::memory_order_relaxed);
    if (prevNativeW != nativeW || prevNativeH != nativeH) RetargetStream(st, maxDim);

    const EncodeSize target = ClampEncodeSize(nativeW, nativeH,
        st.wantW.load(std::memory_order_relaxed), st.wantH.load(std::memory_order_relaxed),
        maxDim);
    out.encode = target.size;
    if (!target.width() || !target.height()) {
        out.drop = true;
        return out;
    }

    const uint32_t prevW = st.srcW.load(), prevH = st.srcH.load();
    if (prevW != target.width() || prevH != target.height()) {
        if (prevW)
            out.sizeNote = "Encode size " + media::SourceSizeLabel(prevW, prevH) + " -> " +
                           media::SourceSizeLabel(target.width(), target.height()) + " (source " +
                           media::SourceSizeLabel(nativeW, nativeH) + "), rebuilding encoder.";
        st.srcW.store(target.width());
        st.srcH.store(target.height());
        out.rebuildEncoder = true;
        st.sizeChanged.store(true, std::memory_order_release);
    }

    if (target.tooSmall) {
        if (!st.paused.exchange(true, std::memory_order_acq_rel))
            out.pauseNote = "Source too small to encode (" +
                            media::SourceSizeLabel(target.width(), target.height()) +
                            ") \xE2\x80\x94 paused, waiting for it to grow back.";
        out.drop = true;
        return out;
    }
    if (st.paused.exchange(false, std::memory_order_acq_rel))
        out.pauseNote = "Source back to " +
                        media::SourceSizeLabel(target.width(), target.height()) + " \xE2\x80\x94 resuming.";

    return out;
}

media::EncoderConfig MakeEncoderConfig(const SourcePipelineState& st, StreamSize encode,
    uint32_t fallbackFps) {
    media::EncoderConfig cfg;
    cfg.width = encode.width;
    cfg.height = encode.height;
    cfg.fps = st.curFps.load(std::memory_order_relaxed);
    if (!cfg.fps) cfg.fps = fallbackFps;
    cfg.bitrateBps = st.curBitrateBps.load(std::memory_order_relaxed);
    return cfg;
}

OfferUpdate RefreshOffer(SourcePipelineState& st, uint8_t fallbackFps) {
    OfferUpdate out;
    out.sizeChanged = st.sizeChanged.exchange(false, std::memory_order_acq_rel);
    out.qualityChanged = st.qualityChanged.exchange(false, std::memory_order_acq_rel);

    if (st.paused.load(std::memory_order_acquire)) return out;
    if (!out.sizeChanged && !out.qualityChanged) return out;

    st.offer.width = uint16_t(st.srcW.load());
    st.offer.height = uint16_t(st.srcH.load());
    st.offer.fps = uint8_t(st.step.fps ? st.step.fps : fallbackFps);
    st.offer.bitrateBps = st.curBitrateBps.load(std::memory_order_relaxed);
    if (st.session) st.session->SetOffer(st.offer);

    if (!ViewerCountOf(st)) return out;
    if (!st.session || st.session->state() != ScreenHostSession::State::Streaming) return out;

    out.sendReconfig = true;
    out.reconfig = Reconfig{st.offer.width, st.offer.height, st.offer.bitrateBps, st.offer.fps};
    st.reconfigOut = out.reconfig;
    st.reconfigSendsLeft = kReconfigSends;
    st.lastReconfigSendUs = 0;
    return out;
}

std::optional<Reconfig> TakeReconfigToSend(SourcePipelineState& st, uint64_t nowUs) {
    if (st.reconfigSendsLeft == 0) return std::nullopt;
    const bool firstSend = st.reconfigSendsLeft == kReconfigSends;
    if (!firstSend && nowUs - st.lastReconfigSendUs < kReconfigResendIntervalUs)
        return std::nullopt;
    --st.reconfigSendsLeft;
    st.lastReconfigSendUs = nowUs;
    return st.reconfigOut;
}

FlushReason DueForFlush(const SourcePipelineState& st, uint64_t nowUs) {
    if (!st.session || st.session->state() != ScreenHostSession::State::Streaming)
        return FlushReason::None;

    const uint64_t sinceFrameUs = nowUs - st.lastFrameUs.load(std::memory_order_relaxed);
    if (st.forceIdr.load() && sinceFrameUs > kIdrFlushIdleUs) return FlushReason::ForceIdr;
    if (sinceFrameUs > kKeepaliveIdleUs && nowUs - st.lastKeepaliveUs >= kKeepaliveIntervalUs)
        return FlushReason::Keepalive;
    return FlushReason::None;
}

FlushReason TakeFlushReason(SourcePipelineState& st, uint64_t nowUs) {
    const FlushReason reason = DueForFlush(st, nowUs);
    if (reason != FlushReason::None) st.lastKeepaliveUs = nowUs;
    return reason;
}

NegotiationResult BeginNegotiation(SourcePipelineState& st, const Hello& hello, uint8_t maxFps,
    const NegotiationHooks& hooks) {
    st.step = QualityStep{};

    NegotiationResult out;
    if (!hooks.resolveSize) return out;

    const StreamSize t = hooks.resolveSize(hello.maxWidth, hello.maxHeight);
    if (!t.width || !t.height) return out;

    st.ladder = std::make_unique<QualityLadder>(uint16_t(t.width), uint16_t(t.height), maxFps);
    st.step = st.ladder->current();
    st.curFps.store(st.step.fps, std::memory_order_relaxed);

    st.offer.width = uint16_t(t.width);
    st.offer.height = uint16_t(t.height);
    st.offer.fps = uint8_t(st.step.fps);
    st.offer.bitrateBps = st.curBitrateBps.load(std::memory_order_relaxed);
    if (st.session) st.session->SetOffer(st.offer);

    out.accepted = true;
    out.size = t;
    out.rungCount = st.ladder->rungCount();
    return out;
}

media::ShareSourceStatus MakeSourceStatus(const SourcePipelineState& st,
    const StatusExtras& extras) {
    media::ShareSourceStatus row;
    row.sourceId = st.sourceId;
    row.name = st.name;
    row.width = st.srcW.load(std::memory_order_relaxed);
    row.height = st.srcH.load(std::memory_order_relaxed);
    row.viewerCount = uint32_t(ViewerCountOf(st));
    row.viewerConnected = row.viewerCount != 0;
    row.viewerAddr = row.viewerConnected ? extras.viewerAddr : std::string();
    row.viewerAddrs = row.viewerConnected ? extras.viewerAddrs : std::vector<std::string>();
    row.viewerNames = row.viewerConnected ? extras.viewerNames : std::vector<std::string>();
    row.captureFps = st.statWindow.captureFps;
    row.sendFps = st.statWindow.sendFps;
    row.sendKbps = st.statWindow.sendKbps;
    row.rttMs = st.uiRttMs.load(std::memory_order_relaxed);
    row.zeroCopy = extras.zeroCopy;
    return row;
}

SourceInfo MakeSourceInfo(const SourcePipelineState& st) {
    SourceInfo info;
    info.sourceId = st.sourceId;
    info.width = uint16_t(st.srcW.load(std::memory_order_relaxed));
    info.height = uint16_t(st.srcH.load(std::memory_order_relaxed));
    info.name = st.name;
    return info;
}

}
