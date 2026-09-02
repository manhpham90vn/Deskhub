#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/diag/ShareDiag.h"
#include "deskhub/diag/ScreenClientDiag.h"

#include <cstdio>
#include <cstring>

using namespace deskhub;
using namespace deskhub::diag;

namespace {

bool Has(const char* haystack, const char* needle) {
    return std::strstr(haystack, needle) != nullptr;
}

void TestWindowStat() {
    std::printf("[diag] WindowStat: avg/max/count and read-and-clear semantics...\n");
    WindowStat s;
    WindowStat::Snapshot e = s.TakeReset();
    Check(e.count == 0 && e.max == 0 && e.avg == 0.0, "WindowStat: an empty window yields all zeros");

    s.Add(10);
    s.Add(20);
    s.Add(30);
    e = s.TakeReset();
    Check(e.count == 3, "WindowStat: counts every sample");
    Check(e.max == 30, "WindowStat: max is the largest sample");
    Check(e.avg > 19.9 && e.avg < 20.1, "WindowStat: avg = sum/count");

    e = s.TakeReset();
    Check(e.count == 0 && e.max == 0, "WindowStat: TakeReset clears the window");
    s.Add(1);
    e = s.TakeReset();
    Check(e.max == 1, "WindowStat: a new window does not inherit the old max");

    s.Add(50);
    s.Add(5);
    e = s.TakeReset();
    Check(e.max == 50, "WindowStat: a smaller sample cannot lower max");
}

void TestWindowPercentile() {
    std::printf("[diag] WindowPercentile: the tail C1 scores encoders on, in microseconds...\n");
    WindowPercentile p;
    WindowPercentile::Snapshot s = p.TakeReset();
    Check(s.count == 0 && s.p50Us == 0 && s.p99Us == 0 && s.maxUs == 0,
        "WindowPercentile: an empty window yields all zeros");

    p.Add(4000);
    s = p.TakeReset();
    Check(s.count == 1 && s.p50Us == 4000 && s.p99Us == 4000 && s.maxUs == 4000,
        "WindowPercentile: one sample is its own p50, p99 and max, to the microsecond");

    for (uint32_t i = 0; i < 99; ++i) p.Add(2000);
    p.Add(90000);
    s = p.TakeReset();
    Check(s.count == 100 && s.maxUs == 90000, "WindowPercentile: counts and keeps the peak");
    Check(s.p50Us <= 2048 && s.p50Us >= 2000,
        "WindowPercentile: p50 lands in the bucket the bulk sits in, not on the outlier");
    Check(s.p99Us <= 2048,
        "WindowPercentile: one sample in a hundred does not move p99 - it takes the 100th");

    for (uint32_t i = 0; i < 98; ++i) p.Add(2000);
    p.Add(90000);
    p.Add(90000);
    s = p.TakeReset();
    Check(s.p99Us == 90000, "WindowPercentile: two in a hundred do move it, and p99 is the tail");

    s = p.TakeReset();
    Check(s.count == 0 && s.maxUs == 0 && s.p99Us == 0,
        "WindowPercentile: TakeReset empties the histogram, so no window inherits the last tail");

    p.Add(WindowPercentile::kCeilingUs * 4);
    s = p.TakeReset();
    Check(s.maxUs == WindowPercentile::kCeilingUs * 4 && s.p99Us == s.maxUs,
        "WindowPercentile: a sample past the last bucket is reported at its real size");
}

void TestCountMaxMin() {
    std::printf("[diag] WindowCount / WindowMax / RunningMin...\n");
    WindowCount c;
    Check(c.TakeReset() == 0, "WindowCount: empty is 0");
    c.Add();
    c.Add(4);
    Check(c.peek() == 5, "WindowCount: peek does not clear");
    Check(c.TakeReset() == 5, "WindowCount: accumulates correctly");
    Check(c.TakeReset() == 0, "WindowCount: read-and-clear");

    WindowMax m;
    m.Add(7);
    m.Add(3);
    Check(m.TakeReset() == 7, "WindowMax: keeps the largest value");
    Check(m.TakeReset() == 0, "WindowMax: read-and-clear");

    RunningMin r;
    Check(r.value() == 0, "RunningMin: 0 = no samples yet");
    r.Add(0);
    Check(r.value() == 0, "RunningMin: ignores a 0 sample");
    r.Add(900);
    r.Add(1200);
    r.Add(400);
    Check(r.value() == 400, "RunningMin: keeps the smallest ever seen");
    Check(r.value() == 400, "RunningMin: reading does not clear");
}

LinkWindow MakeWindow() {
    LinkWindow w;
    w.secs = 1.0;
    w.fps = 59.0;
    w.kbps = 8000.0;
    w.lossPct = 2.5;
    w.framesDropped = 3;
    w.packetsRecovered = 11;
    w.latePackets = 7;
    w.lateMsAvg = 42.0;
    w.lateMsMax = 88;
    return w;
}

void TestClientSum() {
    std::printf("[diag] client evt=sum: per-platform fields, read-and-clear...\n");
    const LinkWindow w = MakeWindow();
    char buf[ScreenClientDiag::kSumBufBytes];

    {
        ScreenClientDiag d;
        d.asmMs.Add(4);
        d.asmMs.Add(6);
        d.decMs.Add(12);
        d.dqDrop.Add(2);
        d.dispDrop.Add(9);
        d.loopBusyMs.Add(31);
        d.minRttUs.Add(1500);

        d.FormatSum(buf, sizeof(buf), "01:02:03", w, 25, 17'000);
        Check(Has(buf, "[DIAG] evt=sum t=01:02:03"), "client evt=sum: has the prefix and timestamp");
        Check(Has(buf, "asm_ms=5.0/6"), "client evt=sum: asm_ms avg/max");
        Check(Has(buf, "dec_ms=12.0/12"), "client evt=sum: dec_ms avg/max");
        Check(!Has(buf, "present_ms"), "client evt=sum: Ubuntu has NO present_ms");
        Check(!Has(buf, "disp_drop"), "client evt=sum: Ubuntu has NO disp_drop");
        Check(Has(buf, "dq_drop=2"), "client evt=sum: dq_drop");
        Check(Has(buf, "late=7 late_ms_avg=42 late_ms_max=88"), "client evt=sum: late packets");
        Check(Has(buf, "gap_ms_max=25"), "client evt=sum: gap_ms_max comes from the Reassembler");
        Check(Has(buf, "loop_busy_ms_max=31"), "client evt=sum: Net loop health");
        Check(Has(buf, "min_rtt_ms=1.5"), "client evt=sum: RTT floor in ms");
        Check(Has(buf, "e2e_ms=17.0"), "client evt=sum: e2e in ms");
        Check(Has(buf, "fec_rx="), "client evt=sum: parity seen on the wire is reported");
        Check(Has(buf, "fec_fix="), "client evt=sum: and what that parity actually repaired");

        d.FormatSum(buf, sizeof(buf), "01:02:04", w, 0, -1);
        Check(Has(buf, "asm_ms=0.0/0") && Has(buf, "dq_drop=0") && Has(buf, "loop_busy_ms_max=0"),
            "client evt=sum: FormatSum reads and clears the counters");
        Check(Has(buf, "e2e_ms=0.0"), "client evt=sum: a negative e2e (no samples yet) prints as 0");
        Check(Has(buf, "min_rtt_ms=1.5"), "client evt=sum: min_rtt is NOT cleared with the window");
    }

    {
        ScreenClientDiag d(ScreenClientDiagCaps{true, false});
        d.presentMs.Add(16);
        d.FormatSum(buf, sizeof(buf), "01:02:03", w, 0, 0);
        Check(Has(buf, "present_ms=16.0/16"), "client evt=sum: Windows has present_ms");
        Check(!Has(buf, "disp_drop"), "client evt=sum: Windows has NO disp_drop");
    }

    {
        ScreenClientDiag d(ScreenClientDiagCaps{false, true});
        d.dispDrop.Add(4);
        d.FormatSum(buf, sizeof(buf), "01:02:03", w, 0, 0);
        Check(Has(buf, "disp_drop=4"), "client evt=sum: Apple/Android has disp_drop");
        Check(!Has(buf, "present_ms"), "client evt=sum: Apple/Android has NO present_ms");
    }
}

void TestClientStatus() {
    std::printf("[diag] client: the once-a-second status line...\n");
    const LinkWindow w = MakeWindow();
    char buf[ScreenClientDiag::kStatusBufBytes];
    ScreenClientDiag::FormatStatus(buf, sizeof(buf), "08:30:00", w, 2400, 17'000);
    Check(Has(buf, "[Client t=08:30:00]"), "client status: timestamp in the prefix");
    Check(Has(buf, "59 fps"), "client status: fps");
    Check(Has(buf, "dropped 3 frame"), "client status: dropped frames");
    Check(Has(buf, "lost  2.5% pkts"), "client status: packet loss %, fixed width");
    Check(Has(buf, "fec+11"), "client status: packets recovered by FEC");
    Check(Has(buf, "RTT 2.4 ms"), "client status: latest RTT in ms");
    Check(Has(buf, "e2e ~17.0 ms"), "client status: e2e");

    ScreenClientDiag::FormatStatus(buf, sizeof(buf), "08:30:01", w, 0, -1);
    Check(Has(buf, "e2e ~0.0 ms"), "client status: e2e with no samples prints as 0");
}

void TestSourceDiag() {
    std::printf("[diag] host evt=sum: encode/send per source...\n");
    char buf[SourceDiag::kSumBufBytes];

    {
        SourceDiag s;
        s.encMs.Add(2);
        s.encMs.Add(4);
        s.encUs.Add(2500);
        s.encUs.Add(4100);
        s.encLatMs.Add(30);
        s.idr.Add();
        s.sendFail.Add(2);
        s.burstMs.Add(9);
        s.FormatSum(buf, sizeof(buf), "01:02:03", "Screen 1", 5, true);
        Check(Has(buf, "[DIAG][Screen 1] evt=sum t=01:02:03"), "source evt=sum: prefix + timestamp");
        Check(Has(buf, "enc_ms_avg=3.0 enc_ms_max=4"), "source evt=sum: enc_ms");
        Check(Has(buf, "enc_us_p50=2560 enc_us_p99=4100"),
            "source evt=sum: the microsecond tail, which is what a backend bake-off is scored on");
        Check(Has(buf, "enc_lat_ms=30.0/30"), "source evt=sum: the real encoder latency");
        Check(!Has(buf, "cap_idle"), "source evt=sum: Windows has NO cap_idle");
        Check(!Has(buf, "zerocopy"), "source evt=sum: Windows has NO zerocopy");
        Check(Has(buf, "idr=1 burst_ms_max=9 send_fail=2"), "source evt=sum: send + IDR");

        s.FormatSum(buf, sizeof(buf), "01:02:04", "Screen 1", 0, false);
        Check(Has(buf, "enc_ms_avg=0.0 enc_ms_max=0") && Has(buf, "idr=0 burst_ms_max=0"),
            "source evt=sum: FormatSum reads and clears the counters");
    }

    {
        SourceDiag mac(ShareDiagCaps{true, false});
        mac.FormatSum(buf, sizeof(buf), "01:02:03", "Built-in", 42, true);
        Check(Has(buf, "cap_idle=42"), "source evt=sum: macOS has cap_idle");
        Check(!Has(buf, "zerocopy"), "source evt=sum: macOS has NO zerocopy");

        SourceDiag ubu(ShareDiagCaps{false, true});
        ubu.FormatSum(buf, sizeof(buf), "01:02:03", "HDMI-1", 42, false);
        Check(Has(buf, "zerocopy=0"), "source evt=sum: Ubuntu has zerocopy, printing a real 0");
        Check(!Has(buf, "cap_idle"), "source evt=sum: Ubuntu has NO cap_idle");
    }
}

void TestSourceIdr() {
    std::printf("[diag] host evt=idr: latched on the Encode thread, printed on the Recv loop...\n");
    SourceDiag s;
    char buf[SourceDiag::kIdrBufBytes];
    Check(s.FormatIdr(buf, sizeof(buf), "Screen 1") == nullptr,
        "evt=idr: nothing latched means no line");

    s.LatchIdr(120'000, 90, 7);
    const char* line = s.FormatIdr(buf, sizeof(buf), "Screen 1");
    Check(line != nullptr, "evt=idr: once latched there is a line");
    Check(line && Has(line, "evt=idr bytes=120000 pkts=90 burst_ms=7"), "evt=idr: all three fields present");
    Check(s.FormatIdr(buf, sizeof(buf), "Screen 1") == nullptr, "evt=idr: read-and-clear, never printed twice");

    s.LatchIdr(1, 1, 1);
    s.LatchIdr(222, 2, 3);
    line = s.FormatIdr(buf, sizeof(buf), "Screen 1");
    Check(line && Has(line, "bytes=222"), "evt=idr: a new IDR overwrites the old one");
}

void TestHostKeyframeRequestSplit() {
    std::printf("[diag] host evt=kf_req_sum: A1 cannot score IDRs it cannot attribute...\n");
    SourceDiag s;
    char buf[SourceDiag::kKeyframeReqBufBytes];

    Check(s.FormatKeyframeRequests(buf, sizeof(buf), "Screen 1") == nullptr,
        "evt=kf_req_sum: a window nobody asked in prints no line");

    s.CountKeyframeRequest(KeyframeReason::Loss);
    s.CountKeyframeRequest(KeyframeReason::QOverflow);
    s.CountKeyframeRequest(KeyframeReason::QOverflow);
    s.CountKeyframeRequest(KeyframeReason::ViewerJoin);

    const char* line = s.FormatKeyframeRequests(buf, sizeof(buf), "Screen 1");
    Check(line && Has(line, "evt=kf_req_sum total=4"), "evt=kf_req_sum: the window total is there");
    Check(line && Has(line, "loss=1") && Has(line, "q_overflow=2") && Has(line, "viewer_join=1"),
        "and every reason that fired is broken out - a keyframe the viewer's own decode queue "
        "asked for is not evidence about any FEC scheme, so a total that cannot be split is "
        "the wrong objective function to score A1 on");
    Check(line && !Has(line, "wait_idr="),
        "a reason nobody used stays off the line rather than printing a zero");

    Check(s.FormatKeyframeRequests(buf, sizeof(buf), "Screen 1") == nullptr,
        "evt=kf_req_sum: read-and-clear, so windows do not accumulate into each other");
}

void TestAgentStatus() {
    std::printf("[diag] host: the status line, even before any FEEDBACK...\n");
    char buf[SourceDiag::kStatusBufBytes];
    SourceDiag::Window w;
    w.rate.captureFps = 60.0;
    w.rate.sendFps = 59.0;
    w.rate.sendKbps = 8000.0;
    w.inputApplied = 120;
    w.inputLost = 1;
    w.inputSkipped = 4;

    SourceDiag::FormatStatus(buf, sizeof(buf), "08:30:00", "Screen 1", "STREAMING", w, {});
    Check(Has(buf, "[Host t=08:30:00][Screen 1]"), "host status: prefix + timestamp + source name");
    Check(Has(buf, "STREAMING"), "host status: session state");
    Check(Has(buf, "capture 60 fps"), "host status: capture rate");
    Check(Has(buf, "send 59 fps, 8000 kbps"), "host status: send rate");
    Check(Has(buf, "input 120 (lost 1, skipped 4)"), "host status: the three input counters");
    Check(Has(buf, "| client -"), "host status: prints a dash when there is no feedback yet");

    SourceDiag::LinkView link;
    link.have = true;
    link.lossPct = 3;
    link.rttMs = 12;
    link.recvKbps = 7600;
    SourceDiag::FormatStatus(buf, sizeof(buf), "08:30:01", "Screen 1", "STREAMING", w, link);
    Check(Has(buf, "| client loss 3%, RTT 12 ms, recv 7600 kbps"),
        "host status: with feedback it prints the other end numbers");
}

void TestSharingHostSum() {
    std::printf("[diag] host evt=sum: Recv loop health...\n");
    ShareDiag a;
    char buf[ShareDiag::kSumBufBytes];
    a.loopBusyMs.Add(180);
    a.FormatSum(buf, sizeof(buf), "01:02:03", 900, 12);
    Check(Has(buf, "[DIAG][host] evt=sum t=01:02:03 loop_busy_ms_max=180"),
        "host evt=sum: Recv loop health");
    Check(Has(buf, "dgram_tx=900 dgram_refused=12"),
        "host evt=sum: the first window reports the whole counter");
    a.FormatSum(buf, sizeof(buf), "01:02:04", 1500, 12);
    Check(Has(buf, "loop_busy_ms_max=0"), "host evt=sum: read-and-clear");
    Check(Has(buf, "dgram_tx=600 dgram_refused=0"),
        "host evt=sum: later windows report the delta, not the running total");
}

void TestClientCompact() {
    std::printf("[diag] client compact line: the short form the UI overlays...\n");
    const LinkWindow w = MakeWindow();
    char buf[ScreenClientDiag::kCompactBufBytes];

    ScreenClientDiag::FormatCompact(buf, sizeof(buf), w, 2400, 17'000);
    Check(Has(buf, "59 fps"), "compact: fps");
    Check(Has(buf, "8.0 Mbps"), "compact: bitrate in Mbps, not kbps");
    Check(Has(buf, "loss 2.5%"), "compact: loss");
    Check(Has(buf, "RTT 2 ms"), "compact: RTT rounded to whole ms");
    Check(Has(buf, "e2e 17 ms"), "compact: e2e");
    Check(Has(buf, "fps  8.0"), "compact: the default separator is two spaces");

    ScreenClientDiag::FormatCompact(buf, sizeof(buf), w, 0, -1, " | ");
    Check(Has(buf, "fps | "), "compact: a custom separator is used");
    Check(Has(buf, "e2e 0 ms"), "compact: e2e with no samples prints as 0");
}

Reassembler::FrameDropInfo MakeDrop(Reassembler::DropReason reason, uint16_t missing,
    uint16_t total, uint16_t first, uint16_t last) {
    Reassembler::FrameDropInfo d;
    d.frameId = 42;
    d.reason = reason;
    d.missing = missing;
    d.total = total;
    d.firstMissing = first;
    d.lastMissing = last;
    d.waitedMs = 33;
    d.bytesGot = 900;
    return d;
}

void TestFrameDropLine() {
    std::printf("[diag] evt=frame_drop: reason names and where the hole sits...\n");
    char buf[ScreenClientDiag::kFrameDropBufBytes];

    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf),
        MakeDrop(Reassembler::DropReason::Timeout, 2, 10, 3, 5));
    Check(Has(buf, "evt=frame_drop id=42"), "frame_drop: the frame id is named");
    Check(Has(buf, "reason=timeout"), "frame_drop: timeout is spelled out");
    Check(Has(buf, "miss=2/10"), "frame_drop: missing over total");
    Check(Has(buf, "pos=mid"), "frame_drop: a hole in the middle");
    Check(Has(buf, "waited_ms=33"), "frame_drop: how long we waited");
    Check(Has(buf, "got_bytes=900"), "frame_drop: how much did arrive");

    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf),
        MakeDrop(Reassembler::DropReason::Overtaken, 1, 8, 0, 0));
    Check(Has(buf, "reason=overtaken") && Has(buf, "pos=head"),
        "frame_drop: a missing first packet reads as head");

    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf),
        MakeDrop(Reassembler::DropReason::Evicted, 1, 8, 7, 7));
    Check(Has(buf, "reason=evicted") && Has(buf, "pos=tail"),
        "frame_drop: a missing last packet reads as tail");

    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf),
        MakeDrop(Reassembler::DropReason::PreIdr, 8, 8, 0, 7));
    Check(Has(buf, "reason=pre_idr") && Has(buf, "pos=all"),
        "frame_drop: nothing arrived reads as all");

    Reassembler::FrameDropInfo none = MakeDrop(Reassembler::DropReason::Timeout, 0, 4, 0, 0);
    none.idr = true;
    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf), none);
    Check(Has(buf, "pos=-"), "frame_drop: no hole prints a dash");
    Check(Has(buf, "idr=1"), "frame_drop: an IDR drop is flagged");

    ScreenClientDiag::FormatFrameDrop(buf, sizeof(buf),
        MakeDrop(Reassembler::DropReason(9), 1, 4, 1, 1));
    Check(Has(buf, "reason=?"), "frame_drop: an unknown reason never indexes off the table");
}

void TestKeyframeRequestLog() {
    std::printf("[diag] kf_req/idr_rx: one line per request, one per arrival...\n");
    KeyframeRequestLog log;
    char buf[KeyframeRequestLog::kBufBytes];

    Check(!log.pending(), "kf log: nothing pending at rest");
    Check(log.Arrived(buf, sizeof(buf), 5'000'000, 100) == nullptr,
        "kf log: an IDR nobody asked for logs nothing");

    const char* line = log.Request(buf, sizeof(buf), 5'000'000, KeyframeReason::DecFail);
    Check(line && Has(line, "evt=kf_req reason=dec_fail"), "kf log: the first request is a line");
    Check(log.pending(), "kf log: and marks a request pending");
    Check(log.Request(buf, sizeof(buf), 5'100'000, KeyframeReason::Loss) == nullptr,
        "kf log: repeats while pending stay quiet");

    line = log.Arrived(buf, sizeof(buf), 5'250'000, 4096);
    Check(line && Has(line, "evt=idr_rx bytes=4096 after_ms=250"),
        "kf log: the arrival names the wait");
    Check(!log.pending(), "kf log: the arrival clears the pending mark");

    log.Request(buf, sizeof(buf), 0, KeyframeReason::WaitIdr);
    Check(log.pending(), "kf log: a request at t=0 is still remembered");
    line = log.Arrived(buf, sizeof(buf), 0, 10);
    Check(line && Has(line, "after_ms=0"), "kf log: a same-instant arrival waited 0 ms");

    char counts[KeyframeRequestLog::kCountsBufBytes];
    const char* summary = log.FormatCounts(counts, sizeof(counts));
    Check(summary && Has(summary, "evt=kf_sum"), "kf log: the window summary is its own line");
    Check(Has(summary, "dec_fail=1") && Has(summary, "wait_idr=1"),
        "kf log: every reason that fired is counted separately");
    Check(Has(summary, "loss=0"),
        "kf log: a repeat swallowed while a request was pending is not counted twice");
    Check(log.FormatCounts(counts, sizeof(counts)) == nullptr,
        "kf log: the summary reads and clears, so a quiet window prints nothing");
}

void TestStateNameAndSourceRate() {
    std::printf("[diag] StateName + SourceRate windows...\n");
    Check(Has(StateName(ScreenHostSession::State::Idle), "IDLE"), "StateName: Idle");
    Check(Has(StateName(ScreenHostSession::State::Ready), "READY"), "StateName: Ready");
    Check(Has(StateName(ScreenHostSession::State::Streaming), "STREAMING"), "StateName: Streaming");
    Check(Has(StateName(ScreenHostSession::State(200)), "?"), "StateName: garbage prints ?");

    SourceRate rate;
    SourceRate::Window w = rate.Close(60, 60, 1'000'000, 10'000'000);
    Check(w.secs == 0.0 && w.captureFps == 0.0 && w.sendKbps == 0.0,
        "SourceRate: the first window has no baseline and reports zeros");

    w = rate.Close(120, 119, 2'000'000, 11'000'000);
    Check(w.secs > 0.99 && w.secs < 1.01, "SourceRate: window length in seconds");
    Check(w.captureFps > 59.9 && w.captureFps < 60.1, "SourceRate: capture fps is the delta");
    Check(w.sendFps > 58.9 && w.sendFps < 59.1, "SourceRate: send fps is the delta");
    Check(w.sendKbps > 7999.0 && w.sendKbps < 8001.0, "SourceRate: kbps from the byte delta");

    w = rate.Close(120, 119, 2'000'000, 11'000'000);
    Check(w.captureFps == 0.0 && w.sendKbps == 0.0,
        "SourceRate: a zero-length window cannot divide by zero");
}

void TestZeroCapacityBuffers() {
    std::printf("[diag] a zero-byte buffer is legal and writes nothing...\n");
    ScreenClientDiag d;
    char one = 0x55;
    Check(d.FormatSum(&one, 0, "01:02:03", MakeWindow(), 0, 0) == &one,
        "client evt=sum: cap=0 returns without writing");
    Check(one == 0x55, "client evt=sum: the byte is untouched");

    SourceDiag s;
    Check(s.FormatSum(&one, 0, "01:02:03", "Screen 1", 0, false) == &one,
        "source evt=sum: cap=0 returns without writing");
    SourceDiag::Window w;
    Check(SourceDiag::FormatStatus(&one, 0, "01:02:03", "Screen 1", "IDLE", w, {}) == &one,
        "source status: cap=0 returns without writing");
    Check(one == 0x55, "source formats: the byte is untouched");
}

void TestTruncation() {
    std::printf("[diag] a tight buffer TRUNCATES instead of overflowing...\n");
    struct Guarded {
        char pre[8];
        char buf[24];
        char post[8];
    } g;
    std::memset(&g, 0x7E, sizeof(g));

    ScreenClientDiag d;
    d.FormatSum(g.buf, sizeof(g.buf), "01:02:03", MakeWindow(), 0, 0);
    Check(std::strlen(g.buf) < sizeof(g.buf), "truncation: the string always terminates inside the buffer");
    bool intact = true;
    for (char c : g.pre) intact = intact && c == 0x7E;
    for (char c : g.post) intact = intact && c == 0x7E;
    Check(intact, "truncation: nothing is written past the buffer");

    SourceDiag s;
    s.FormatSum(g.buf, sizeof(g.buf), "01:02:03", "Screen 1", 0, false);
    Check(std::strlen(g.buf) < sizeof(g.buf), "truncation: the host version also terminates inside the buffer");
    intact = true;
    for (char c : g.pre) intact = intact && c == 0x7E;
    for (char c : g.post) intact = intact && c == 0x7E;
    Check(intact, "truncation: the host version writes nothing past the buffer");
}

}

void RunDiagTests() {
    TestWindowStat();
    TestWindowPercentile();
    TestCountMaxMin();
    TestClientSum();
    TestClientStatus();
    TestSourceDiag();
    TestSourceIdr();
    TestHostKeyframeRequestSplit();
    TestAgentStatus();
    TestSharingHostSum();
    TestClientCompact();
    TestFrameDropLine();
    TestKeyframeRequestLog();
    TestStateNameAndSourceRate();
    TestZeroCapacityBuffers();
    TestTruncation();
}
