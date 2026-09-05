#include "deskhubp/host/HostNetLoop.h"

#include "deskhub/diag/ShareDiag.h"
#include "deskhub/session/host/ViewerFeedback.h"
#include "deskhub/session/host/SourcePipeline.h"
#include "deskhub/ui/HostRows.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/ViewerBroadcast.h"
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/Random.h"

#include <string>
#include <utility>
#include <vector>

namespace deskhubp {
namespace {

constexpr uint64_t kStatIntervalUs = 1'000'000;
constexpr uint32_t kLoopStallWarnMs = 250;

bool Alive(const deskhub::SourcePipelineState& st, const HostSourceHooks& hooks) {
    if (st.failed.load()) return false;
    return !(hooks.closed && hooks.closed(st));
}

void SendReconfig(SessionTransport& sock, deskhub::SourcePipelineState& st,
    const deskhub::Reconfig& reconfig) {
    uint8_t buf[deskhub::kMaxDatagram];
    const size_t n = deskhub::BuildReconfig(buf, st.session->sessionId(), reconfig);
    if (!n) return;
    SendToViewers(st, sock, std::span<const uint8_t>(buf, n));
}

void ReportWindow(deskhub::SourcePipelineState& st, const HostSourceHooks& hooks,
    const std::string& hms, uint64_t nowUs, char* line, size_t lineBytes) {
    const deskhub::InputReceiver::Stats inputStats = st.session->inputStats();
    st.statWindow = st.statRate.Close(st.captured.load(), st.framesSent.load(),
        st.bytesSent.load(), nowUs);

    deskhub::diag::SourceDiag::Window window;
    window.rate = st.statWindow;
    window.inputApplied = inputStats.applied;
    window.inputLost = inputStats.lost;
    window.inputSkipped = (hooks.inputSkipped ? hooks.inputSkipped(st) : 0) +
                          st.session->inputDenied();

    deskhub::diag::SourceDiag::LinkView link;
    link.have = st.haveFeedback.load(std::memory_order_acquire);
    link.lossPct = st.uiLossPct.load(std::memory_order_relaxed);
    link.rttMs = st.uiRttMs.load(std::memory_order_relaxed);
    link.recvKbps = st.uiRecvKbps.load(std::memory_order_relaxed);

    LOGI("%s", deskhub::diag::SourceDiag::FormatStatus(line, lineBytes, hms.c_str(),
                   st.name.c_str(), deskhub::diag::StateName(st.session->state()), window, link));

    const uint32_t idleFrames = hooks.takeIdleFrames ? hooks.takeIdleFrames(st) : 0;
    const bool zeroCopy = hooks.zeroCopy && hooks.zeroCopy(st);
    LOGI("%s", st.diag.FormatSum(line, lineBytes, hms.c_str(), st.name.c_str(), idleFrames,
                   zeroCopy));
}

}

deskhub::ScreenHostCallbacks MakeScreenHostCallbacks(deskhub::SourcePipelineState& st,
    ScreenHostSessionHooks hooks) {
    auto shared = std::make_shared<ScreenHostSessionHooks>(std::move(hooks));
    deskhub::SourcePipelineState* p = &st;

    deskhub::ScreenHostCallbacks cb;
    cb.send = [shared](std::span<const uint8_t> d) {
        if (shared->send) shared->send(d);
    };
    cb.sendTo = [shared](uint64_t addrPacked, std::span<const uint8_t> d) {
        if (shared->sendToAddr) shared->sendToAddr(addrPacked, d);
    };
    cb.randomBytes = [](std::span<uint8_t> out) {
        return RandomBytes(out.data(), out.size());
    };

    cb.onHello = [p, shared](const deskhub::Hello& hello) {
        deskhub::NegotiationHooks negotiation;
        negotiation.resolveSize = [p, shared](uint16_t clientW, uint16_t clientH) {
            p->cliW = clientW;
            p->cliH = clientH;
            return shared->retarget ? shared->retarget() : deskhub::StreamSize{};
        };
        const deskhub::NegotiationResult r =
            deskhub::BeginNegotiation(*p, hello, uint8_t(shared->fps), negotiation);
        if (!r.accepted) return;
        LOGI("[Host][%s] Quality ladder: ceiling %ux%u @%ufps, %d rung(s).", p->name.c_str(),
            r.size.width, r.size.height, p->step.fps, r.rungCount);
    };

    cb.onStart = [p] {
        p->forceIdr.store(true);
        LOGI("[Host][%s] Client START — beginning video push.", p->name.c_str());
    };

    cb.onKeyframeRequest = [p](deskhub::KeyframeReason reason) {
        p->diag.CountKeyframeRequest(reason);
        p->forceIdr.store(true);
    };

    cb.onInvalidateRef = [p](uint32_t lostFrameId) {
        const deskhub::media::RecoveryDecision d = p->recovery.OnReferenceLost(lostFrameId);
        switch (d.action) {
            case deskhub::media::RecoveryAction::None: break;
            case deskhub::media::RecoveryAction::InvalidateReference:
                p->invalidateBeforeFrame.store(d.referenceFrameId + 1);
                break;
            case deskhub::media::RecoveryAction::IntraRefresh:
                p->wantIntraRefresh.store(true);
                break;
            case deskhub::media::RecoveryAction::Keyframe: p->forceIdr.store(true); break;
        }
        LOGI("[Host][%s] recovery=%s after losing frame %u", p->name.c_str(),
            deskhub::media::RecoveryActionName(d.action), lostFrameId);
    };

    cb.onNack = [p, shared](uint32_t frameId, std::span<const uint16_t> indices) {
        if (!shared->sendToRequester) return;
        deskhub::RespondToNack(*p, frameId, indices, shared->sendToRequester);
    };

    cb.onInput = [shared](const deskhub::InputEvent& e) {
        if (shared->applyInput) shared->applyInput(e);
    };

    cb.onClipboardText = [shared](std::string_view text) {
        if (shared->applyClipboard) shared->applyClipboard(text);
    };

    cb.onFocus = [shared](bool focused) {
        if (!focused && shared->releaseInput) shared->releaseInput();
    };

    cb.onViewerJoin = [p](uint64_t addrPacked, size_t viewerCount, std::string_view viewerName) {
        const std::string label = deskhub::ui::ViewerLabel(std::string(viewerName),
            NetAddr::Unpack(addrPacked).ToString());
        LOGI("[Host][%s] Viewer %s joined (%zu connected).", p->name.c_str(), label.c_str(),
            viewerCount);
    };

    cb.onViewerLeave = [p](uint64_t addrPacked, size_t viewerCount) {
        LOGI("[Host][%s] Viewer %s left (%zu connected).", p->name.c_str(),
            NetAddr::Unpack(addrPacked).ToString().c_str(), viewerCount);
    };

    cb.onControllerChange = [p, shared](uint64_t addrPacked) {
        if (shared->releaseInput) shared->releaseInput();
        if (!addrPacked) {
            LOGI("[Host][%s] Input is idle \xE2\x80\x94 any viewer may take it.", p->name.c_str());
            return;
        }
        LOGI("[Host][%s] Input is being driven by %s.", p->name.c_str(),
            NetAddr::Unpack(addrPacked).ToString().c_str());
    };

    cb.onDisconnect = [p] {
        deskhub::ForgetViewers(*p);
        LOGI("[Host][%s] Last viewer left (BYE/timeout).", p->name.c_str());
    };

    cb.onFeedback = [p, shared](const deskhub::Feedback& fb) {
        deskhub::FeedbackHooks feedback;
        feedback.setEncoderBitrate = shared->setEncoderBitrate;
        feedback.applyQualityStep = shared->applyQualityStep;

        const deskhub::FeedbackOutcome out =
            deskhub::ApplyFeedback(*p, fb, NowUs(), feedback);

        if (out.fecToggled)
            LOGI("[Host][%s] FEC %s (loss %u%%).", p->name.c_str(),
                out.fecEnabled ? "on" : "off", fb.lossPct);
        if (out.bitrateChanged)
            LOGI("[Host][%s] Bitrate %.1f -> %.1f Mbps (loss %u%%, RTT %u ms)", p->name.c_str(),
                out.previousBitrateBps / 1e6, out.bitrateBps / 1e6, fb.lossPct, fb.rttMs);
        if (out.qualityChanged)
            LOGI("[Host][%s] Quality %u%%@%ufps -> %u%%@%ufps (%ux%u, budget %.1f Mbps)",
                p->name.c_str(), out.previousStep.scalePct, out.previousStep.fps,
                out.step.scalePct, out.step.fps, out.size.width, out.size.height,
                p->rate->bitrateBps() / 1e6);
    };

    return cb;
}

void RunHostNetLoop(SessionTransport& sock, deskhub::Beacon& beacon,
    std::span<deskhub::SourcePipelineState* const> live, const HostNetLoopHooks& hooks) {
    const std::vector<deskhub::SourcePipelineState*> liveStates(live.begin(), live.end());

    uint8_t buf[deskhub::kMaxRecordSize];
    uint8_t beaconBuf[deskhub::kMaxDatagram];
    char line[deskhub::diag::SourceDiag::kStatusBufBytes];

    uint64_t lastStatUs = NowUs();
    deskhub::diag::ShareDiag loopDiag;

    while (!hooks.stopped || !hooks.stopped()) {
        if (hooks.onTick) hooks.onTick();

        bool anyAlive = false;
        for (deskhub::SourcePipelineState* st : live)
            if (Alive(*st, hooks.source)) anyAlive = true;
        if (!anyAlive && !(hooks.keepAlive && hooks.keepAlive())) {
            LOGI("[Host] No source left alive — session over.");
            break;
        }

        NetAddr from;
        const int n = sock.RecvFrom(buf, sizeof(buf), from);
        const uint64_t now = NowUs();
        if (n < 0) {
            LOGE("[Host] Socket error — stopping.");
            if (hooks.onSocketError) hooks.onSocketError();
            break;
        }

        if (n > 0) {
            const auto pkt = std::span<const uint8_t>(buf, size_t(n));
            const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(pkt);
            if (header && header->chan == deskhub::Chan::Terminal) {
                if (hooks.onTerminal) hooks.onTerminal(from, pkt);
            } else if (header && header->chan == deskhub::Chan::File) {
                if (hooks.onFile) hooks.onFile(from, pkt);
            } else if (const size_t rn =
                           beacon.Reply(beaconBuf, pkt, sock.Authenticated(from));
                rn) {
                sock.SendTo(from, beaconBuf, rn);
            } else {
                deskhub::AcceptDatagram(liveStates, pkt, from.Pack(), now);
            }
        }

        for (deskhub::SourcePipelineState* st : live) {
            if (st->failed.load()) continue;

            if (hooks.source.closed && hooks.source.closed(*st) && !st->shutdownDone) {
                LOGI("[Host][%s] Source closed — stopping this source.", st->name.c_str());
                if (hooks.source.shutdown) hooks.source.shutdown(*st);
                if (hooks.publishStatus) hooks.publishStatus();
                continue;
            }

            st->session->Tick(now);

            st->clipOut.SetSessionId(st->session->sessionId());
            st->clipOut.Flush(now, [&sock, st](std::span<const uint8_t> d) {
                SendToViewers(*st, sock, d);
            });

            if (const char* idrLine = st->diag.FormatIdr(line, sizeof(line), st->name.c_str()))
                LOGI("%s", idrLine);

            if (const char* kfLine =
                    st->diag.FormatKeyframeRequests(line, sizeof(line), st->name.c_str()))
                LOGI("%s", kfLine);

            const deskhub::OfferUpdate update =
                deskhub::RefreshOffer(*st, uint8_t(hooks.fallbackFps));
            if (update.sendReconfig) {
                SendReconfig(sock, *st, update.reconfig);
                st->forceIdr.store(true);
            }

            if (hooks.source.flush &&
                deskhub::TakeFlushReason(*st, now) != deskhub::FlushReason::None)
                hooks.source.flush(*st, now);
        }

        if (now - lastStatUs >= kStatIntervalUs) {
            const std::string hms = LocalTimeHms();
            for (deskhub::SourcePipelineState* st : live) {
                if (st->failed.load()) continue;
                ReportWindow(*st, hooks.source, hms, now, line, sizeof(line));
            }
            const QuicSendStats send = sock.SendStats();
            LOGI("%s", loopDiag.FormatSum(line, sizeof(line), hms.c_str(), send.datagrams,
                           send.datagramsRefused));
            if (hooks.publishStatus) hooks.publishStatus();
            lastStatUs = now;
        }

        const uint32_t busyMs = uint32_t((NowUs() - now) / 1000);
        loopDiag.loopBusyMs.Add(busyMs);
        if (busyMs > kLoopStallWarnMs) LOGW("[DIAG][host] evt=recv_stall busy_ms=%u", busyMs);
    }
}

}
