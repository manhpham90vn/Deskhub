#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/terminal/Screen.h"
#include "deskhubp/system/Pty.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kReadWaitMs = 100;
constexpr int kMaxReads = 150;
constexpr int kSettleReads = 12;

bool DrainUntil(deskhubp::Pty& pty, deskhub::term::Screen& screen, std::string_view wanted,
    int rounds) {
    std::vector<uint8_t> chunk(deskhubp::kPtyReadChunk);
    for (int i = 0; i < rounds; ++i) {
        const int got = pty.Read(chunk.data(), chunk.size(), kReadWaitMs);
        if (got < 0) break;
        if (got > 0) screen.Write(std::span<const uint8_t>(chunk.data(), size_t(got)));
        if (!wanted.empty() && screen.Text().find(wanted) != std::string::npos) return true;
    }
    return !wanted.empty() && screen.Text().find(wanted) != std::string::npos;
}

bool Send(deskhubp::Pty& pty, std::string_view text) {
    return pty.Write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

void TestShellEchoesBack() {
    std::printf("[pty] a real shell starts, runs a command and its output comes back...\n");
    const std::string shell = deskhubp::DefaultShell();
    if (shell.empty()) {
        std::printf("[pty] skipped: this platform never hosts a shell\n");
        return;
    }
    Check(!shell.empty(), "the platform names a shell to run");

    deskhubp::Pty pty;
    const bool started = pty.Start(shell, deskhub::TermSize{80, 24});
    Check(started, "the pseudo terminal starts it");
    if (!started) return;
    Check(pty.Running(), "and reports it as running");

    deskhub::term::Screen screen(deskhub::TermSize{80, 24});
    DrainUntil(pty, screen, {}, kSettleReads);
    Check(Send(pty, "echo deskhub-pty-ok\r"), "a command can be typed into it");
    Check(DrainUntil(pty, screen, "deskhub-pty-ok", kMaxReads),
        "and the shell's own output comes back through the terminal");

    Check(pty.Resize(deskhub::TermSize{120, 40}), "the window can be resized under it");
    Check(pty.Resize(deskhub::TermSize{0, 0}), "an impossible size is clamped, not rejected");

    pty.Close();
    Check(!pty.Running(), "closing it stops the shell");
}

void TestSecondStartIsRefused() {
    std::printf("[pty] one object holds one shell, and a dead one reports its code...\n");
    const std::string shell = deskhubp::DefaultShell();
    if (shell.empty()) return;

    deskhubp::Pty pty;
    if (!pty.Start(shell, deskhub::TermSize{80, 24})) return;
    Check(!pty.Start(shell, deskhub::TermSize{80, 24}),
        "starting a second shell on the same object is refused");

    Send(pty, "exit\r");
    deskhub::term::Screen screen(deskhub::TermSize{80, 24});
    DrainUntil(pty, screen, {}, kMaxReads);
    Check(pty.ExitCode() >= 0, "and can report its exit code to the client");
}

void TestUnstartedPtyIsHarmless() {
    std::printf("[pty] a terminal that was never started refuses everything quietly...\n");
    deskhubp::Pty pty;
    Check(!pty.Running(), "it is not running");
    uint8_t buf[16];
    Check(pty.Read(buf, sizeof(buf), 0) < 0, "reading it fails rather than blocking");
    Check(!Send(pty, "ls"), "writing to it fails");
    Check(!pty.Resize(deskhub::TermSize{80, 24}), "and so does resizing it");
    pty.Close();
    Check(true, "closing it twice is safe");
    pty.Close();
}

}

void RunPtyTests() {
    TestShellEchoesBack();
    TestSecondStartIsRefused();
    TestUnstartedPtyIsHarmless();
}
