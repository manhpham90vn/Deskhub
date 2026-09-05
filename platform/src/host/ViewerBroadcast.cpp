#include "deskhubp/host/ViewerBroadcast.h"

#include "deskhub/session/host/SourcePipeline.h"
#include "deskhub/session/host/ScreenHostSession.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/HostRows.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/system/Clock.h"

#include <chrono>
#include <cinttypes>
#include <mutex>
#include <thread>

namespace deskhubp {
namespace {

constexpr int kFirstFramePollMs = 10;
constexpr int kFirstFramePolls = 1000;
constexpr int kFirstFrameTimeoutSec = kFirstFramePolls * kFirstFramePollMs / 1000;

bool AwaitingFirstFrame(const deskhub::SourcePipelineState& st, const SourcePredicate& closed) {
    if (st.failed.load()) return false;
    if (st.srcW.load()) return false;
    return !(closed && closed(st));
}

}

size_t SendToViewers(const deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> datagram) {
    uint64_t addrs[deskhub::kMaxViewersPerSource];
    const size_t viewers = deskhub::SnapshotViewerAddrs(st, addrs);
    size_t sent = 0;
    for (size_t i = 0; i < viewers; ++i)
        if (sock.SendTo(NetAddr::Unpack(addrs[i]), datagram.data(), datagram.size())) ++sent;
    return sent;
}

std::string ViewerAddrList(const deskhub::SourcePipelineState& st) {
    deskhub::ViewerInfo infos[deskhub::kMaxViewersPerSource];
    const size_t viewers = deskhub::SnapshotViewerInfos(st, infos);
    std::string joined;
    for (size_t i = 0; i < viewers; ++i) {
        if (!joined.empty()) joined += ", ";
        joined += deskhub::ui::ViewerLabel(infos[i].name,
            NetAddr::Unpack(infos[i].addrPacked).ToString());
    }
    return joined;
}

void SendEncodedFrame(deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> frame, uint64_t timestampUs, bool keyframe) {
    if (!st.session || st.session->state() != deskhub::ScreenHostSession::State::Streaming) return;

    const uint64_t nowUs = NowUs();
    const uint32_t frameAgeMs = nowUs > timestampUs ? uint32_t((nowUs - timestampUs) / 1000) : 0;
    st.diag.encLatMs.Add(frameAgeMs);
    st.frameAgeMs.Add(frameAgeMs);
    st.recovery.NoteEncoded(st.nextFrameId, keyframe,
        st.recovery.ShouldMarkLongTerm(st.nextFrameId));

    uint64_t addrs[deskhub::kMaxViewersPerSource];
    const size_t viewers = deskhub::SnapshotViewerAddrs(st, addrs);
    if (!viewers) return;

    st.packetizer.SetSessionId(st.session->sessionId());
    st.packetizer.SetFecEnabled(st.wantFec.load(std::memory_order_relaxed));
    st.packetizer.SetFecParityPerGroup(st.wantFecParity.load(std::memory_order_relaxed));
    st.pacer.SetRateBps(uint64_t(st.curBitrateBps.load(std::memory_order_relaxed)) *
                        deskhub::kPacingRateMultiple);

    const uint64_t sendT0 = NowUs();
    const size_t pkts = st.packetizer.SendFrame(frame, st.nextFrameId++, timestampUs, keyframe,
        [&st, &sock, &addrs, viewers](std::span<const uint8_t> d) {
            const uint64_t waitUs = st.pacer.Gate(d.size() * viewers, NowUs());
            if (waitUs) SleepUs(waitUs);
            for (size_t i = 0; i < viewers; ++i) {
                if (sock.SendTo(NetAddr::Unpack(addrs[i]), d.data(), d.size()))
                    st.bytesSent.fetch_add(d.size(), std::memory_order_relaxed);
                else
                    st.diag.sendFail.Add();
            }
            std::lock_guard<std::mutex> lk(st.retxMutex);
            st.retxCache.Store(d);
        });

    const uint32_t burstMs = uint32_t((NowUs() - sendT0) / 1000);
    st.diag.burstMs.Add(burstMs);
    if (!pkts) return;

    st.framesSent.fetch_add(1, std::memory_order_relaxed);
    if (!keyframe) return;
    st.diag.idr.Add();
    st.diag.LatchIdr(frame.size(), uint32_t(pkts), burstMs);
}

size_t SendAudioFrame(deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> opusFrame, uint32_t seq, uint64_t timestampUs) {
    if (!st.session || st.session->state() != deskhub::ScreenHostSession::State::Streaming) return 0;
    if (opusFrame.empty() || opusFrame.size() > deskhub::kMaxAudioPayload) return 0;

    uint64_t addrs[deskhub::kMaxViewersPerSource];
    const size_t listeners = deskhub::SnapshotAudioViewerAddrs(st, addrs);
    if (!listeners) return 0;

    uint8_t datagram[deskhub::kMaxDatagram];
    const deskhub::AudioHeader ah{seq, timestampUs};
    const size_t n = deskhub::BuildAudioPacket(datagram, st.session->sessionId(), ah, opusFrame);
    if (!n) return 0;

    size_t sent = 0;
    for (size_t i = 0; i < listeners; ++i) {
        if (sock.SendTo(NetAddr::Unpack(addrs[i]), datagram, n)) {
            st.bytesSent.fetch_add(n, std::memory_order_relaxed);
            ++sent;
        } else {
            st.diag.sendFail.Add();
        }
    }
    return sent;
}

void LogListeningAddresses(uint16_t port, const std::string& boundIp) {
    if (!boundIp.empty()) {
        LOGI("[Host] Listening on %s, UDP port %u. On the other machine, enter that address.",
            boundIp.c_str(), unsigned(port));
        return;
    }
    LOGI("[Host] Listening on UDP port %u. On the other machine, enter one of:",
        unsigned(port));
    for (const AdapterAddr& a : ListLocalIPv4())
        LOGI("    %s    (%s)", a.ip.c_str(), a.name.c_str());
}

std::vector<deskhub::SourcePipelineState*> SelectLiveSources(
    std::span<deskhub::SourcePipelineState* const> pipes, const SourcePredicate& closed,
    const std::function<bool()>& aborted,
    const std::function<void(deskhub::SourcePipelineState&)>& shutdown) {
    for (int i = 0; i < kFirstFramePolls; ++i) {
        if (aborted && aborted()) break;
        bool allKnown = true;
        for (deskhub::SourcePipelineState* p : pipes)
            if (AwaitingFirstFrame(*p, closed)) allKnown = false;
        if (allKnown) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(kFirstFramePollMs));
    }

    std::vector<deskhub::SourcePipelineState*> live;
    for (deskhub::SourcePipelineState* p : pipes) {
        if (!p->failed.load() && p->srcW.load()) {
            live.push_back(p);
            continue;
        }
        if (!p->failed.load())
            LOGW("[Host][%s] No frame within %ds — not sharing this source.", p->name.c_str(),
                kFirstFrameTimeoutSec);
        if (shutdown) shutdown(*p);
    }
    return live;
}

void EndScreenHostSession(deskhub::SourcePipelineState& st, SessionTransport& sock) {
    if (!st.session || st.session->state() == deskhub::ScreenHostSession::State::Idle) return;

    uint8_t bye[deskhub::kCommonHeaderSize];
    const size_t n = deskhub::BuildBye(bye, st.session->sessionId());
    if (n) SendToViewers(st, sock, std::span<const uint8_t>(bye, n));
}

std::vector<deskhub::media::ShareSourceStatus> PublishSourceStatus(
    std::span<deskhub::SourcePipelineState* const> live, deskhub::Beacon& beacon,
    const SourceStatusHooks& hooks) {
    std::vector<deskhub::media::ShareSourceStatus> rows;
    std::vector<deskhub::SourceInfo> infos;

    for (deskhub::SourcePipelineState* p : live) {
        if (p->failed.load()) continue;
        if (hooks.closed && hooks.closed(*p)) continue;

        deskhub::StatusExtras extras;
        extras.zeroCopy = hooks.zeroCopy && hooks.zeroCopy(*p);
        extras.viewerAddr = ViewerAddrList(*p);
        deskhub::ViewerInfo viewerInfos[deskhub::kMaxViewersPerSource];
        const size_t viewers = deskhub::SnapshotViewerInfos(*p, viewerInfos);
        for (size_t i = 0; i < viewers; ++i) {
            extras.viewerAddrs.push_back(NetAddr::Unpack(viewerInfos[i].addrPacked).ToString());
            extras.viewerNames.push_back(viewerInfos[i].name);
        }

        rows.push_back(deskhub::MakeSourceStatus(*p, extras));
        infos.push_back(deskhub::MakeSourceInfo(*p));
    }

    beacon.SetSources(infos);
    return rows;
}

void LogTransferTotals(std::span<deskhub::SourcePipelineState* const> pipes) {
    uint64_t frames = 0;
    double megabytes = 0;
    for (deskhub::SourcePipelineState* p : pipes) {
        frames += p->framesSent.load();
        megabytes += double(p->bytesSent.load()) / 1e6;
    }
    LOGI("[Host] Stopped. Total: %" PRIu64 " frames sent, %.2f MB.", frames, megabytes);
}

}
