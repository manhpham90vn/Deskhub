#include "Tests.h"
#include "support/FakeHost.h"
#include "support/FakeDecoder.h"
#include "support/FakeVideo.h"
#include "support/LoadRig.h"
#include "support/TestSupport.h"

#include "deskhubp/client/ScreenViewer.h"
#include "deskhubp/host/FileHost.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/client/TerminalViewer.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/Pty.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

using Viewer = deskhubp::ScreenViewer<fake::Decoder, void*>;

namespace {

constexpr uint32_t kConnectTimeoutMs = 30'000;
constexpr uint32_t kTransferTimeoutMs = 120'000;
constexpr uint32_t kFloodTimeoutMs = 60'000;
constexpr uint32_t kEchoTimeoutMs = 10'000;
constexpr uint64_t kBaselineWindowUs = 1'000'000;
constexpr uint64_t kSampleUs = 20'000;
constexpr uint64_t kWorstGapMs = 1500;
constexpr uint64_t kEchoLatencyCeilingMs = 2000;
constexpr size_t kTransferBytes = 32u << 20;

void* const kDummySurface = reinterpret_cast<void*>(0x1);

using load::Continuity;
using load::DecodedFrames;
using load::Pattern;
using load::ReadBytes;
using load::Scratch;
using load::ViewerConfig;
using load::WriteBytes;

#if defined(_WIN32)
constexpr const char* kFloodCommand =
    "for ($i=0; $i -lt 30000; $i++) { echo \"deskhub flood line $i\" }\r";
constexpr const char* kFloodDoneCommand = "echo (52000+7)\r";
constexpr const char* kEchoCommand = "echo (41000+59)\r";
#else
constexpr const char* kFloodCommand =
    "i=0; while [ $i -lt 100000 ]; do echo deskhub flood line $i; i=$((i+1)); done\r";
constexpr const char* kFloodDoneCommand = "echo $((52000+7))\r";
constexpr const char* kEchoCommand = "echo $((41000+59))\r";
#endif
constexpr const char* kFloodDoneMarker = "52007";
constexpr const char* kEchoMarker = "41059";
constexpr const char* kCanaryCommand = "echo deskhub-canary\r";
constexpr const char* kCanaryMarker = "deskhub-canary";
constexpr uint32_t kCanaryTimeoutMs = 15'000;

struct TerminalPeer {
    deskhubp::TerminalViewer viewer{};
    std::mutex mutex{};
    std::string output{};
    size_t scannedTo = 0;
    std::atomic<uint64_t> activity{0};

    bool Open(uint16_t port) {
        deskhubp::TerminalViewerConfig cfg;
        cfg.host = NetAddr{0x7F000001u, port};
        cfg.hostLabel = "load-test-host";
        cfg.passcode = kTestPasscode;
        cfg.clientName = "load-test";
        cfg.size = deskhub::TermSize{120, 30};

        deskhubp::TerminalViewerCallbacks hooks;
        hooks.onOutput = [this](std::span<const uint8_t> bytes) {
            const std::lock_guard<std::mutex> lock(mutex);
            output.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            activity.fetch_add(bytes.size(), std::memory_order_relaxed);
        };
        hooks.onRedraw = [this] { activity.fetch_add(1, std::memory_order_relaxed); };
        hooks.onTrustAsked = [this](deskhub::TrustVerdict, std::string_view) {
            viewer.AcceptFingerprint();
        };

        if (!viewer.Start(cfg, std::move(hooks))) return false;
        return WaitFor(
            [this] { return viewer.State() == deskhubp::TerminalViewerState::Live; },
            kConnectTimeoutMs);
    }

    bool SawInOutput(const char* marker) {
        const std::string wanted(marker);
        const std::lock_guard<std::mutex> lock(mutex);
        const size_t from = scannedTo > wanted.size() ? scannedTo - wanted.size() : 0;
        const bool found = output.find(wanted, from) != std::string::npos;
        scannedTo = output.size();
        return found;
    }

    bool SeesOnScreen(const char* marker) {
        const deskhubp::TerminalSnapshot snap = viewer.Snapshot();
        std::string text;
        text.reserve(snap.cells.size() + snap.size.rows);
        size_t cell = 0;
        for (uint16_t row = 0; row < snap.size.rows && cell < snap.cells.size(); ++row) {
            for (uint16_t col = 0; col < snap.size.cols && cell < snap.cells.size();
                ++col, ++cell) {
                const char32_t ch = snap.cells[cell].ch;
                text.push_back(ch >= 32 && ch < 127 ? char(ch) : ' ');
            }
            text.push_back('\n');
        }
        return text.find(marker) != std::string::npos;
    }

    bool Saw(const char* marker) {
        return SawInOutput(marker) || SeesOnScreen(marker);
    }

    void PrintScreenTail(const char* when) {
        const deskhubp::TerminalSnapshot snap = viewer.Snapshot();
        std::printf("        terminal screen %s:\n", when);
        size_t printed = 0;
        for (uint16_t row = 0; row < snap.size.rows && printed < 8; ++row) {
            std::string line;
            for (uint16_t col = 0; col < snap.size.cols; ++col) {
                const size_t cell = size_t(row) * snap.size.cols + col;
                if (cell >= snap.cells.size()) break;
                const char32_t ch = snap.cells[cell].ch;
                line.push_back(ch >= 32 && ch < 127 ? char(ch) : ' ');
            }
            while (!line.empty() && line.back() == ' ') line.pop_back();
            if (line.empty()) continue;
            std::printf("        | %s\n", line.c_str());
            ++printed;
        }
        if (printed == 0) std::printf("        | (blank screen)\n");
    }

    bool ProveShellAnswers() {
        viewer.SendText(kCanaryCommand);
        const bool alive =
            WaitFor([this] { return Saw(kCanaryMarker); }, kCanaryTimeoutMs);
        if (!alive) PrintScreenTail("after the canary went unanswered");
        Check(alive, "typed characters come back from the shell at all");
        return alive;
    }
};

struct CrossSession {
    uint16_t port = 0;
    load::Upload upload{};
    fake::SharingHost host{};
    deskhubp::TerminalHost terminals{};
    deskhubp::FileHost files{};
    Viewer viewer{};
    TerminalPeer term{};
    std::filesystem::path landing{};
    std::vector<uint8_t> payload{};
    std::filesystem::path file{};

    ~CrossSession() {
        upload.client.Stop();
        term.viewer.Stop();
        viewer.Stop();
        files.Stop();
        terminals.Stop();
        host.Stop();
    }

    bool Open(const std::string& leaf, bool withFiles) {
        fake::Host().Reset();
        fake::Decoded().Reset();
        deskhubp::ForgetAllPairedDevices();

        port = NextTestPort();
        if (!host.Start({fake::Source("Display 1", 1280, 720, 1)}, port)) {
            Check(false, "the host could not start");
            return false;
        }
        if (!terminals.Start(host.socket(), "", deskhubp::TerminalHostCallbacks{})) {
            Check(false, "the host could not open its terminal service");
            return false;
        }
        host.SetTerminal(&terminals);

        if (withFiles) {
            landing = Scratch(leaf + "-land");
            payload = Pattern(kTransferBytes);
            file = WriteBytes(Scratch(leaf + "-send"), "bulk.bin", payload);
            if (!files.Start(host.socket(), landing, deskhubp::FileHostCallbacks{})) {
                Check(false, "the host could not take files in");
                return false;
            }
            host.SetFiles(&files);
        }

        deskhubp::ScreenViewerConfig cfg = ViewerConfig(port, false);
        cfg.onTrustAsked = [this](deskhub::TrustVerdict, std::string_view) {
            viewer.AcceptFingerprint();
        };
        viewer.SetSurface(kDummySurface);
        if (!viewer.Start(cfg)) {
            Check(false, "the viewer could not open its socket");
            return false;
        }
        if (!WaitFor([this] { return viewer.phase() == deskhubp::ClientPhase::Streaming; },
                kConnectTimeoutMs)) {
            Check(false, "the session never reached Streaming");
            return false;
        }
        if (!WaitFor([] { return DecodedFrames() >= 5; }, kConnectTimeoutMs)) {
            Check(false, "the stream never produced frames");
            return false;
        }
        if (!term.Open(port)) {
            Check(false, "the terminal viewer never went Live beside the video session");
            return false;
        }
        return true;
    }

    bool LandedWhole() const {
        return ReadBytes(landing / "bulk.bin") == payload;
    }
};

void TestATerminalFloodNeverStallsTheVideo() {
    std::printf("[lag] a terminal flood rides alongside the video without stalling it...\n");
    if (deskhubp::DefaultShell().empty()) {
        std::printf("[lag] skipped: this build has no shell to host a terminal\n");
        return;
    }

    CrossSession s;
    if (!s.Open("term-flood", false)) return;
    if (!s.term.ProveShellAnswers()) return;

    Continuity idle(DecodedFrames);
    idle.Begin();
    for (uint64_t spent = 0; spent < kBaselineWindowUs; spent += kSampleUs) {
        SleepUs(kSampleUs);
        idle.Sample();
    }
    const double idleFps = idle.perSecond();
    Check(idleFps > 1.0, "the stream is genuinely running before the flood starts");

    Continuity video(DecodedFrames);
    Continuity termFlow([&s] { return s.term.activity.load(std::memory_order_relaxed); });
    video.Begin();
    termFlow.Begin();
    const uint64_t activityBefore = s.term.activity.load(std::memory_order_relaxed);
    s.term.viewer.SendText(kFloodCommand);
    s.term.viewer.SendText(kFloodDoneCommand);

    bool floodDone = false;
    for (uint32_t waited = 0; waited < kFloodTimeoutMs * 1000 / kSampleUs; ++waited) {
        SleepUs(kSampleUs);
        video.Sample();
        termFlow.Sample();
        if (s.term.Saw(kFloodDoneMarker)) {
            floodDone = true;
            break;
        }
    }
    const uint64_t flooded = s.term.activity.load(std::memory_order_relaxed) - activityBefore;
    const double busyFps = video.perSecond();

    std::printf(
        "        flood moved %llu terminal bytes/redraws, video %.1f -> %.1f fps, worst video "
        "gap %llu ms, worst terminal gap %llu ms\n",
        static_cast<unsigned long long>(flooded), idleFps, busyFps,
        static_cast<unsigned long long>(video.worstGapMs()),
        static_cast<unsigned long long>(termFlow.worstGapMs()));

    if (!floodDone) s.term.PrintScreenTail("after the flood never finished");
    Check(floodDone, "the flood ran to completion inside the deadline");
    Check(flooded > 0, "the flood genuinely crossed to the terminal viewer");
    Check(video.moved() > 0, "frames kept arriving while the terminal was flooded");
    Check(video.worstGapMs() < kWorstGapMs,
        "and never went dark for anything like the freeze a terminal flood monopolising the "
        "receive loop used to cause");
    Check(busyFps > idleFps / 3.0,
        "the frame rate under a terminal flood stays in the same league as the idle rate");
    Check(termFlow.worstGapMs() < kWorstGapMs,
        "the terminal itself also kept moving, in bounded slices instead of one long gulp");
}

void TestTheTerminalStaysLiveDuringABigTransfer() {
    std::printf("[lag] a keystroke's echo crosses a bulk-flooded link promptly...\n");
    if (deskhubp::DefaultShell().empty()) {
        std::printf("[lag] skipped: this build has no shell to host a terminal\n");
        return;
    }

    CrossSession s;
    if (!s.Open("term-echo", true)) return;
    if (!s.term.ProveShellAnswers()) return;

    Check(s.upload.Start(s.port, s.file), "the batch is offered");
    Check(WaitFor([&s] { return s.upload.batchBytes() > kTransferBytes / 8; },
              kTransferTimeoutMs),
        "the transfer is well under way");

    const bool stillRunning = s.upload.busy();
    const uint64_t sentUs = NowUs();
    s.term.viewer.SendText(kEchoCommand);
    const bool echoed = WaitFor([&s] { return s.term.Saw(kEchoMarker); }, kEchoTimeoutMs);
    const uint64_t latencyMs = (NowUs() - sentUs) / 1000;

    std::printf("        the shell answered %llu ms after the keystrokes\n",
        static_cast<unsigned long long>(latencyMs));

    if (!echoed) s.term.PrintScreenTail("after the echo went unanswered");
    Check(stillRunning, "the upload really was in flight when the command was typed");
    Check(echoed, "the shell answered while the file lane was flooded");
    Check(latencyMs < kEchoLatencyCeilingMs,
        "and promptly, so the interactive terminal lane is not queued behind however many "
        "file chunks are in front of it");

    Check(WaitFor([&s] { return !s.upload.busy(); }, kTransferTimeoutMs),
        "the transfer still finishes");
    Check(s.upload.done(), "as done");
    Check(s.LandedWhole(), "with the file intact");
}

}

void RunLagUnderCrossLoadTests() {
    TestATerminalFloodNeverStallsTheVideo();
    TestTheTerminalStaysLiveDuringABigTransfer();
}
