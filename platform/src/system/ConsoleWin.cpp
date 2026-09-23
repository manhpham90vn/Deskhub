#include "deskhubp/system/Console.h"

#include <windows.h>

namespace deskhubp {

namespace {

DWORD g_savedMode = 0;
bool g_savedValid = false;
COORD g_lastSize{0, 0};

HANDLE StdinHandle() {
    return GetStdHandle(STD_INPUT_HANDLE);
}

HANDLE StdoutHandle() {
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

bool SaveMode() {
    DWORD mode = 0;
    if (!GetConsoleMode(StdinHandle(), &mode)) return false;
    g_savedMode = mode;
    g_savedValid = true;
    return true;
}

void RestoreMode() {
    if (!g_savedValid) return;
    SetConsoleMode(StdinHandle(), g_savedMode);
    g_savedValid = false;
}

}

bool StdinIsTty() {
    DWORD mode = 0;
    return GetConsoleMode(StdinHandle(), &mode) != 0;
}

ConsoleSize ConsoleSizeNow() {
    ConsoleSize size;
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(StdoutHandle(), &info)) return size;

    const int columns = info.srWindow.Right - info.srWindow.Left + 1;
    const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (columns > 0) size.columns = uint16_t(columns);
    if (rows > 0) size.rows = uint16_t(rows);
    return size;
}

bool ConsoleResized() {
    const ConsoleSize now = ConsoleSizeNow();
    const COORD current{SHORT(now.columns), SHORT(now.rows)};
    if (current.X == g_lastSize.X && current.Y == g_lastSize.Y) return false;
    g_lastSize = current;
    return true;
}

EchoOff::EchoOff() {
    if (!SaveMode()) return;
    if (!SetConsoleMode(StdinHandle(), g_savedMode & ~DWORD(ENABLE_ECHO_INPUT))) {
        g_savedValid = false;
        return;
    }
    active_ = true;
}

EchoOff::~EchoOff() {
    if (!active_) return;
    RestoreMode();
}

RawConsole::RawConsole() {
    if (!SaveMode()) return;

    DWORD raw = g_savedMode;
    raw &= ~DWORD(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    raw |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode(StdinHandle(), raw)) {
        g_savedValid = false;
        return;
    }

    DWORD out = 0;
    if (GetConsoleMode(StdoutHandle(), &out))
        SetConsoleMode(StdoutHandle(), out | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    ConsoleResized();
    active_ = true;
}

RawConsole::~RawConsole() {
    if (!active_) return;
    RestoreMode();
}

int ReadStdinByte(uint32_t timeoutMs) {
    const HANDLE handle = StdinHandle();

    DWORD unusedMode = 0;
    if (!GetConsoleMode(handle, &unusedMode)) {
        DWORD waiting = 0;
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &waiting, nullptr)) return kStdinClosed;
        if (waiting == 0) {
            Sleep(timeoutMs);
            return kStdinTimedOut;
        }
    } else if (WaitForSingleObject(handle, timeoutMs) != WAIT_OBJECT_0) {
        return kStdinTimedOut;
    }

    char byte = 0;
    DWORD got = 0;
    if (!ReadFile(handle, &byte, 1, &got, nullptr)) return kStdinClosed;
    if (got == 0) return kStdinClosed;
    return int(static_cast<unsigned char>(byte));
}

}
