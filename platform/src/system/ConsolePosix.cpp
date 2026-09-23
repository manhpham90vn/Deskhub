#include "deskhubp/system/Console.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace deskhubp {

namespace {

termios g_savedAttrs{};
bool g_savedValid = false;

std::atomic<bool> g_resized{false};

extern "C" void OnWindowChange(int) {
    g_resized.store(true, std::memory_order_release);
}

bool ApplyAttrs(const termios& attrs) {
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &attrs) == 0;
}

bool SaveAttrs() {
    if (!StdinIsTty()) return false;
    if (tcgetattr(STDIN_FILENO, &g_savedAttrs) != 0) return false;
    g_savedValid = true;
    return true;
}

void RestoreAttrs() {
    if (!g_savedValid) return;
    ApplyAttrs(g_savedAttrs);
    g_savedValid = false;
}

}

bool StdinIsTty() {
    return isatty(STDIN_FILENO) == 1;
}

ConsoleSize ConsoleSizeNow() {
    ConsoleSize size;
    winsize window{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0) return size;
    if (window.ws_col) size.columns = window.ws_col;
    if (window.ws_row) size.rows = window.ws_row;
    return size;
}

bool ConsoleResized() {
    return g_resized.exchange(false, std::memory_order_acq_rel);
}

EchoOff::EchoOff() {
    if (!SaveAttrs()) return;
    termios quiet = g_savedAttrs;
    quiet.c_lflag &= tcflag_t(~ECHO);
    if (!ApplyAttrs(quiet)) {
        g_savedValid = false;
        return;
    }
    active_ = true;
}

EchoOff::~EchoOff() {
    if (!active_) return;
    RestoreAttrs();
}

RawConsole::RawConsole() {
    if (!SaveAttrs()) return;

    termios raw = g_savedAttrs;
    raw.c_lflag &= tcflag_t(~(ECHO | ICANON | ISIG | IEXTEN));
    raw.c_iflag &= tcflag_t(~(IXON | ICRNL | BRKINT | INPCK | ISTRIP));
    raw.c_oflag &= tcflag_t(~OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (!ApplyAttrs(raw)) {
        g_savedValid = false;
        return;
    }

    struct sigaction action{};
    action.sa_handler = OnWindowChange;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &action, nullptr);

    active_ = true;
}

RawConsole::~RawConsole() {
    if (!active_) return;
    RestoreAttrs();
}

int ReadStdinByte(uint32_t timeoutMs) {
    pollfd waiting{STDIN_FILENO, POLLIN, 0};
    const int ready = poll(&waiting, 1, int(timeoutMs));
    if (ready <= 0) return kStdinTimedOut;

    unsigned char byte = 0;
    const ssize_t got = read(STDIN_FILENO, &byte, 1);
    if (got == 0) return kStdinClosed;
    if (got < 0) return kStdinTimedOut;
    return int(byte);
}

}
