#include "deskhub/diag/ScreenClientDiag.h"

#include "deskhub/diag/TextAppend.h"

#include <cinttypes>
#include <cstdio>

namespace deskhub::diag {

const char* ScreenClientDiag::FormatSum(char* buf, size_t cap, const char* hms, const LinkWindow& w,
    uint32_t gapMsMax, int64_t e2eUs, int64_t absoluteE2eUs) {
    const WindowStat::Snapshot a = asmMs.TakeReset();
    const WindowStat::Snapshot d = decMs.TakeReset();
    const WindowStat::Snapshot pr = presentMs.TakeReset();
    const uint32_t dq = dqDrop.TakeReset();
    const uint32_t disp = dispDrop.TakeReset();
    const uint32_t busy = loopBusyMs.TakeReset();

    if (!cap) return buf;
    char* p = buf;
    char* const end = buf + cap;
    *p = '\0';

    Append(p, end, "[DIAG] evt=sum t=%s asm_ms=%.1f/%u dec_ms=%.1f/%u", hms, a.avg, a.max, d.avg,
        d.max);
    if (caps_.presentMs) Append(p, end, " present_ms=%.1f/%u", pr.avg, pr.max);

    Append(p, end, " dq_drop=%u", dq);
    if (caps_.dispDrop) Append(p, end, " disp_drop=%u", disp);

    Append(p, end, " fec_rx=%" PRIu64 " fec_fix=%" PRIu64, w.fecReceived, w.packetsRecovered);
    Append(p, end, " late=%" PRIu64 " late_ms_avg=%.0f late_ms_max=%" PRIu64, w.latePackets,
        w.lateMsAvg, w.lateMsMax);
    Append(p, end, " gap_ms_max=%u loop_busy_ms_max=%u", gapMsMax, busy);
    Append(p, end, " min_rtt_ms=%.1f e2e_ms=%.1f", minRttUs.value() / 1000.0,
        e2eUs >= 0 ? e2eUs / 1000.0 : 0.0);
    if (absoluteE2eUs >= 0) Append(p, end, " e2e_abs_ms=%.1f", absoluteE2eUs / 1000.0);
    return buf;
}

const char* ScreenClientDiag::FormatStatus(char* buf, size_t cap, const char* hms, const LinkWindow& w,
    uint32_t rttUs, int64_t e2eUs) {
    std::snprintf(buf, cap,
        "[Client t=%s] %2.0f fps | %6.0f kbps | dropped %" PRIu64
        " frame | lost %4.1f%% pkts"
        " | fec+%" PRIu64 " | RTT %.1f ms | e2e ~%.1f ms",
        hms, w.fps, w.kbps, w.framesDropped, w.lossPct, w.packetsRecovered, rttUs / 1000.0,
        e2eUs >= 0 ? e2eUs / 1000.0 : 0.0);
    return buf;
}

const char* ScreenClientDiag::FormatCompact(char* buf, size_t cap, const LinkWindow& w, uint32_t rttUs,
    int64_t e2eUs, const char* sep) {
    std::snprintf(buf, cap, "%.0f fps%s%.1f Mbps%sloss %.1f%%%sRTT %.0f ms%se2e %.0f ms", w.fps,
        sep, w.kbps / 1000.0, sep, w.lossPct, sep, rttUs / 1000.0, sep,
        e2eUs >= 0 ? e2eUs / 1000.0 : 0.0);
    return buf;
}

const char* ScreenClientDiag::FormatFrameDrop(char* buf, size_t cap,
    const Reassembler::FrameDropInfo& d) {
    static const char* const kReason[] = {"timeout", "overtaken", "evicted", "pre_idr"};
    const size_t r = size_t(d.reason);

    const char* pos = "-";
    if (d.missing) {
        const bool head = d.firstMissing == 0;
        const bool tail = d.lastMissing + 1 == d.total;
        pos = head && tail ? "all" : tail ? "tail"
                                 : head   ? "head"
                                          : "mid";
    }

    std::snprintf(buf, cap,
        "[DIAG] evt=frame_drop id=%u reason=%s miss=%u/%u pos=%s idr=%u waited_ms=%u "
        "got_bytes=%u",
        d.frameId, r < 4 ? kReason[r] : "?", d.missing, d.total, pos, d.idr ? 1 : 0, d.waitedMs,
        d.bytesGot);
    return buf;
}

const char* KeyframeReasonName(KeyframeReason reason) {
    switch (reason) {
        case KeyframeReason::Loss: return "loss";
        case KeyframeReason::WaitIdr: return "wait_idr";
        case KeyframeReason::QOverflow: return "q_overflow";
        case KeyframeReason::DecFail: return "dec_fail";
        case KeyframeReason::DisplayCongested: return "display_congested";
        case KeyframeReason::ViewerJoin: return "viewer_join";
        case KeyframeReason::Unknown: return "unknown";
    }
    return "?";
}

const char* KeyframeRequestLog::Request(char* buf, size_t cap, uint64_t nowUs,
    KeyframeReason reason) {
    if (reqUs_) return nullptr;
    reqUs_ = nowUs ? nowUs : 1;
    ++counts_[size_t(reason)];
    std::snprintf(buf, cap, "[DIAG] evt=kf_req reason=%s", KeyframeReasonName(reason));
    return buf;
}

const char* KeyframeRequestLog::FormatCounts(char* buf, size_t cap) {
    if (!cap) return nullptr;
    uint32_t total = 0;
    for (uint32_t n : counts_) total += n;
    if (!total) return nullptr;

    char* p = buf;
    char* const end = buf + cap;
    *p = '\0';
    Append(p, end, "[DIAG] evt=kf_sum");
    for (size_t i = 0; i < kKeyframeReasonCount; ++i) {
        Append(p, end, " %s=%u", KeyframeReasonName(KeyframeReason(i)), counts_[i]);
        counts_[i] = 0;
    }
    return buf;
}

const char* KeyframeRequestLog::Arrived(char* buf, size_t cap, uint64_t nowUs, size_t idrBytes) {
    if (!reqUs_) return nullptr;
    const uint64_t afterMs = nowUs > reqUs_ ? (nowUs - reqUs_) / 1000 : 0;
    reqUs_ = 0;
    std::snprintf(buf, cap, "[DIAG] evt=idr_rx bytes=%zu after_ms=%" PRIu64, idrBytes, afterMs);
    return buf;
}

}
