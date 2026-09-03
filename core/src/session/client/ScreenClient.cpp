#include "deskhub/session/client/ScreenClient.h"

#include <cstdio>
#include <utility>

namespace deskhub {

ScreenClient::ScreenClient(ScreenClientCallbacks cb, diag::ScreenClientDiag& diag)
    : cb_(std::move(cb)),
      diag_(diag),
      session_(MakeSessionCallbacks()),
      linkStats_(0) {}

ScreenClientSessionCallbacks ScreenClient::MakeSessionCallbacks() {
    ScreenClientSessionCallbacks sc;
    sc.send = [this](std::span<const uint8_t> d) {
        if (cb_.send) cb_.send(d);
    };
    sc.onReady = [this](const NegotiatedParams& np) {
        std::snprintf(line_, sizeof(line_),
            "[Client] Negotiation done: H264 %ux%u @%ufps, %.1f Mbps", np.width, np.height,
            np.fps, np.bitrateBps / 1e6);
        Log(line_);
        if (cb_.onParams) cb_.onParams(np, false);
    };
    sc.onReconfig = [this](const NegotiatedParams& np) {
        if (reasm_) reasm_->SetFps(np.fps);
        std::snprintf(line_, sizeof(line_),
            "[Client] Host reconfigured: %ux%u @%ufps, %.1f Mbps", np.width, np.height, np.fps,
            np.bitrateBps / 1e6);
        Log(line_);
        if (cb_.onParams) cb_.onParams(np, true);
    };
    sc.onRtt = [this](uint32_t rttUs) {
        diag_.minRttUs.Add(rttUs);
        if (reasm_) reasm_->SetRttUs(diag_.minRttUs.value());
    };
    sc.onClipboardText = [this](std::string_view text) {
        if (cb_.onClipboardText) cb_.onClipboardText(text);
    };
    sc.onDisconnect = [this](const char* reason, ScreenSessionEnd cause) {
        std::snprintf(line_, sizeof(line_), "[Client] Disconnected: %s",
            reason ? reason : "disconnected");
        Log(line_);
        if (cb_.onEnded) cb_.onEnded(reason ? reason : "disconnected", cause);
    };
    return sc;
}

void ScreenClient::Start(const ScreenClientConfig& cfg, uint64_t nowUs) {
    cfg_ = cfg;
    linkStats_ = LinkStats(nowUs);
    windowBytes_ = 0;
    reasm_.reset();

    Hello hello;
    hello.clientId = cfg.clientId;
    hello.codecMask = kCodecMaskH264;
    hello.maxWidth = cfg.maxWidth;
    hello.maxHeight = cfg.maxHeight;
    hello.desiredFps = cfg.desiredFps;
    hello.features = cfg.wantsAudio ? kClientWantsAudio : 0;
    hello.sourceId = cfg.sourceId;
    hello.passcode = cfg.passcode;
    hello.clientName = cfg.displayName;
    session_.Start(hello, nowUs);
}

void ScreenClient::EnsureReassembler() {
    if (reasm_) return;
    const uint32_t fps = session_.params().fps ? session_.params().fps : kDefaultClientFps;
    reasm_ = std::make_unique<Reassembler>(1'000'000 / fps);
    reasm_->SetNackEnabled(cfg_.sendNacks);
    if (cfg_.overtakenLimit) reasm_->SetOvertakenLimit(cfg_.overtakenLimit);
    if (cfg_.stallTimeoutFrames) reasm_->SetStallTimeoutMultiple(cfg_.stallTimeoutFrames);
    if (!cfg_.fecScheme.empty() && !reasm_->SetFecScheme(cfg_.fecScheme))
        LogWarn(
            "[Client] No FEC scheme by that name is built in, so parity from a host "
            "running a different one will not be understood; staying on the default");
    reasm_->onFrameDrop = [this](const Reassembler::FrameDropInfo& d) {
        char drop[diag::ScreenClientDiag::kFrameDropBufBytes];
        LogWarn(diag::ScreenClientDiag::FormatFrameDrop(drop, sizeof(drop), d));
    };
    reasm_->onReferenceLost = [this](uint32_t frameId) {
        session_.SendInvalidateRef(frameId);
    };
    reasm_->SetRttUs(diag_.minRttUs.value());
}

void ScreenClient::OnDatagram(std::span<const uint8_t> pkt, uint64_t nowUs) {
    const auto h = ParseCommonHeader(pkt);
    if (!h) return;

    if (h->chan == Chan::Audio) {
        if (h->sessionId != session_.sessionId() || session_.sessionId() == 0) return;
        if (!cfg_.wantsAudio || !cb_.onAudioPacket) return;
        if (const auto v = ParseAudioPacket(PayloadOf(pkt))) {
            session_.NotifyVideoPacket(nowUs);
            cb_.onAudioPacket(*v);
            windowBytes_ += v->payload.size();
        }
        return;
    }
    if (h->chan != Chan::Video) {
        session_.HandlePacket(pkt, nowUs);
        return;
    }
    if (h->sessionId != session_.sessionId() || session_.sessionId() == 0) return;

    const auto pl = PayloadOf(pkt);
    EnsureReassembler();

    if (h->type == MsgType::FecPacket) {
        if (const auto v = ParseFecPacket(*h, pl)) {
            session_.NotifyVideoPacket(nowUs);
            reasm_->PushFec(*v, nowUs);
            windowBytes_ += v->parity.size();
        }
        return;
    }
    if (const auto v = ParseVideoPacket(*h, pl)) {
        session_.NotifyVideoPacket(nowUs);
        reasm_->Push(*v, nowUs);
        windowBytes_ += v->payload.size();
    }
}

void ScreenClient::RequestKeyframe(diag::KeyframeReason reason, uint64_t nowUs) {
    char kf[diag::KeyframeRequestLog::kBufBytes];
    if (const char* l = kfLog_.Request(kf, sizeof(kf), nowUs, reason)) Log(l);
    session_.RequestKeyframe(reason);
}

void ScreenClient::PlanNacks(uint64_t nowUs) {
    if (!cfg_.sendNacks || !reasm_) return;
    uint16_t indices[kNackBatchMax];
    uint32_t frameId = 0;
    const size_t n = reasm_->PlanNack(nowUs, diag_.minRttUs.value(), frameId, indices);
    if (n) session_.SendNack(frameId, std::span<const uint16_t>(indices, n));
}

void ScreenClient::PollFrames(uint64_t nowUs) {
    if (!reasm_) return;

    while (auto f = reasm_->PopReady(nowUs)) {
        if (f->idr) {
            session_.CancelKeyframeRequest();
            char kf[diag::KeyframeRequestLog::kBufBytes];
            if (const char* l = kfLog_.Arrived(kf, sizeof(kf), nowUs, f->nal.size())) Log(l);
        }
        if (f->firstSeenUs) diag_.asmMs.Add(uint32_t((nowUs - f->firstSeenUs) / 1000));
        lastAbsoluteE2eUs_ =
            session_.clockSync().AbsoluteLatencyUs(f->timestampUs, f->firstSeenUs
                                                                       ? f->firstSeenUs
                                                                       : nowUs);
        if (cb_.onFrame) cb_.onFrame(std::move(*f));
    }

    if (reasm_->TakeLossEvent())
        RequestKeyframe(diag::KeyframeReason::Loss, nowUs);
    else if (reasm_->WaitingForIdr())
        RequestKeyframe(diag::KeyframeReason::WaitIdr, nowUs);
}

void ScreenClient::Report(uint64_t nowUs) {
    const auto st = reasm_ ? reasm_->stats() : Reassembler::Stats{};
    const uint32_t rendered = cb_.takeRenderedCount ? cb_.takeRenderedCount() : 0;
    const LinkWindow w = linkStats_.Close(st, windowBytes_, rendered, nowUs);

    const int64_t e2e = cb_.latencyUs ? cb_.latencyUs() : -1;
    const std::string hms = cb_.localTime ? cb_.localTime() : std::string();

    Log(diag::ScreenClientDiag::FormatStatus(line_, sizeof(line_), hms.c_str(), w,
        session_.lastRttUs(), e2e));

    if (cfg_.logLossRuns && w.lossRunTotal) {
        std::snprintf(line_, sizeof(line_),
            "[Client]   loss runs: 1x%llu 2x%llu 3x%llu 4-7x%llu 8-15x%llu 16-31x%llu "
            "32+x%llu  | longest ever %llu pkts",
            (unsigned long long)w.lossRuns[0], (unsigned long long)w.lossRuns[1],
            (unsigned long long)w.lossRuns[2], (unsigned long long)w.lossRuns[3],
            (unsigned long long)w.lossRuns[4], (unsigned long long)w.lossRuns[5],
            (unsigned long long)w.lossRuns[6], (unsigned long long)w.lossRunMax);
        Log(line_);
    }

    if (cfg_.logLossRuns && w.absentRunTotal) {
        std::snprintf(line_, sizeof(line_),
            "[Client]   wire runs: 1x%llu 2x%llu 3x%llu 4-7x%llu 8-15x%llu 16-31x%llu "
            "32+x%llu  | longest ever %llu pkts",
            (unsigned long long)w.absentRuns[0], (unsigned long long)w.absentRuns[1],
            (unsigned long long)w.absentRuns[2], (unsigned long long)w.absentRuns[3],
            (unsigned long long)w.absentRuns[4], (unsigned long long)w.absentRuns[5],
            (unsigned long long)w.absentRuns[6], (unsigned long long)w.absentRunMax);
        Log(line_);
    }

    char compact[diag::ScreenClientDiag::kCompactBufBytes];
    diag::ScreenClientDiag::FormatCompact(compact, sizeof(compact), w, session_.lastRttUs(), e2e,
        cfg_.statusSeparator ? cfg_.statusSeparator : "  ");
    if (cb_.onStatus) cb_.onStatus(compact);

    session_.SendFeedback(MakeFeedback(w, session_.lastRttUs()));

    Log(diag_.FormatSum(line_, sizeof(line_), hms.c_str(), w,
        reasm_ ? reasm_->TakeMaxGapMs() : 0, e2e, lastAbsoluteE2eUs_));

    char kfCounts[diag::KeyframeRequestLog::kCountsBufBytes];
    if (const char* l = kfLog_.FormatCounts(kfCounts, sizeof(kfCounts))) Log(l);

    windowBytes_ = 0;
}

bool ScreenClient::Tick(uint64_t nowUs) {
    session_.Tick(nowUs);
    if (session_.state() == ScreenClientSession::State::Dead) return false;

    if (linkStats_.Due(nowUs)) Report(nowUs);
    return true;
}

void ScreenClient::CountLoopBusy(uint64_t startedUs, uint64_t nowUs) {
    const uint32_t busyMs = uint32_t((nowUs - startedUs) / 1000);
    diag_.loopBusyMs.Add(busyMs);
    if (busyMs > kLoopStallWarnMs) {
        std::snprintf(line_, sizeof(line_), "[DIAG] evt=recv_stall busy_ms=%u", busyMs);
        LogWarn(line_);
    }
}

}
