#include "deskhub/diag/ShareDiag.h"

#include "deskhub/diag/ScreenClientDiag.h"
#include "deskhub/diag/TextAppend.h"

#include <cinttypes>
#include <cstdio>

namespace deskhub::diag {

const char* StateName(ScreenHostSession::State s) {
    switch (s) {
        case ScreenHostSession::State::Idle: return "IDLE";
        case ScreenHostSession::State::Ready: return "READY";
        case ScreenHostSession::State::Streaming: return "STREAMING";
    }
    return "?";
}

SourceRate::Window SourceRate::Close(uint32_t captured, uint64_t framesSent, uint64_t bytesSent,
    uint64_t nowUs) {
    Window w;
    if (lastUs_) {
        w.secs = (nowUs - lastUs_) / 1e6;
        if (w.secs > 0.0) {
            w.captureFps = (captured - lastCaptured_) / w.secs;
            w.sendFps = double(framesSent - lastFrames_) / w.secs;
            w.sendKbps = double(bytesSent - lastBytes_) * 8.0 / 1000.0 / w.secs;
        }
    }
    lastCaptured_ = captured;
    lastFrames_ = framesSent;
    lastBytes_ = bytesSent;
    lastUs_ = nowUs;
    return w;
}

void SourceDiag::LatchIdr(uint64_t bytes, uint32_t pkts, uint32_t burst) {
    idrPkts_.store(pkts, std::memory_order_relaxed);
    idrBurstMs_.store(burst, std::memory_order_relaxed);
    idrBytes_.store(bytes, std::memory_order_release);
}

void SourceDiag::CountKeyframeRequest(KeyframeReason reason) {
    const size_t slot = size_t(reason);
    if (slot < kKeyframeReasonCount) kfReq_[slot].fetch_add(1, std::memory_order_relaxed);
}

const char* SourceDiag::FormatKeyframeRequests(char* buf, size_t cap, const char* name) {
    uint32_t taken[kKeyframeReasonCount] = {};
    uint32_t total = 0;
    for (size_t i = 0; i < kKeyframeReasonCount; ++i) {
        taken[i] = kfReq_[i].exchange(0, std::memory_order_relaxed);
        total += taken[i];
    }
    if (!total || !cap) return nullptr;

    char* p = buf;
    char* const end = buf + cap;
    Append(p, end, "[DIAG][%s] evt=kf_req_sum total=%u", name, total);
    for (size_t i = 0; i < kKeyframeReasonCount; ++i)
        if (taken[i])
            Append(p, end, " %s=%u", KeyframeReasonName(KeyframeReason(i)), taken[i]);
    return buf;
}

const char* SourceDiag::FormatIdr(char* buf, size_t cap, const char* name) {
    const uint64_t bytes = idrBytes_.exchange(0, std::memory_order_acquire);
    if (!bytes) return nullptr;
    std::snprintf(buf, cap, "[DIAG][%s] evt=idr bytes=%" PRIu64 " pkts=%u burst_ms=%u", name,
        bytes, idrPkts_.load(std::memory_order_relaxed),
        idrBurstMs_.load(std::memory_order_relaxed));
    return buf;
}

const char* SourceDiag::FormatSum(char* buf, size_t cap, const char* hms, const char* name,
    uint32_t capIdle, bool zerocopy) {
    const WindowStat::Snapshot e = encMs.TakeReset();
    const WindowPercentile::Snapshot u = encUs.TakeReset();
    const WindowStat::Snapshot l = encLatMs.TakeReset();
    const uint32_t idrN = idr.TakeReset();
    const uint32_t fail = sendFail.TakeReset();
    const uint32_t queued = queueDrop.TakeReset();
    const uint32_t burst = burstMs.TakeReset();

    if (!cap) return buf;
    char* p = buf;
    char* const end = buf + cap;
    *p = '\0';

    Append(p, end, "[DIAG][%s] evt=sum t=%s enc_ms_avg=%.1f enc_ms_max=%u", name, hms, e.avg,
        e.max);
    Append(p, end, " enc_us_p50=%u enc_us_p99=%u", u.p50Us, u.p99Us);
    Append(p, end, " enc_lat_ms=%.1f/%u", l.avg, l.max);
    if (caps_.capIdle) Append(p, end, " cap_idle=%u", capIdle);
    Append(p, end, " idr=%u burst_ms_max=%u send_fail=%u", idrN, burst, fail);
    if (caps_.queueDrop) Append(p, end, " q_drop=%u", queued);
    if (caps_.zerocopy) Append(p, end, " zerocopy=%d", zerocopy ? 1 : 0);
    return buf;
}

const char* SourceDiag::FormatStatus(char* buf, size_t cap, const char* hms, const char* name,
    const char* state, const Window& w, const LinkView& link) {
    if (!cap) return buf;
    char* p = buf;
    char* const end = buf + cap;
    *p = '\0';

    Append(p, end, "[Host t=%s][%s] %-9s | capture %.0f fps | send %.0f fps, %.0f kbps", hms,
        name, state, w.rate.captureFps, w.rate.sendFps, w.rate.sendKbps);
    Append(p, end, " | input %" PRIu64 " (lost %" PRIu64 ", skipped %" PRIu64 ")", w.inputApplied,
        w.inputLost, w.inputSkipped);

    if (link.have)
        Append(p, end, " | client loss %u%%, RTT %u ms, recv %u kbps", link.lossPct, link.rttMs,
            link.recvKbps);
    else
        Append(p, end, " | client -");
    return buf;
}

const char* ShareDiag::FormatSum(char* buf, size_t cap, const char* hms, uint64_t datagramsSent,
    uint64_t datagramsRefused) {
    const uint64_t sent = datagramsSent - lastDatagramsSent_;
    const uint64_t refused = datagramsRefused - lastDatagramsRefused_;
    lastDatagramsSent_ = datagramsSent;
    lastDatagramsRefused_ = datagramsRefused;
    std::snprintf(buf, cap,
        "[DIAG][host] evt=sum t=%s loop_busy_ms_max=%u dgram_tx=%" PRIu64
        " dgram_refused=%" PRIu64,
        hms, loopBusyMs.TakeReset(), sent, refused);
    return buf;
}

}
