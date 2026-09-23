#include "Signals.h"

#include <atomic>

#include <windows.h>

namespace deskhubcli {

namespace {

std::atomic<int> g_interrupts{0};

BOOL WINAPI OnConsoleEvent(DWORD type) {
    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT && type != CTRL_CLOSE_EVENT) return FALSE;
    g_interrupts.fetch_add(1, std::memory_order_release);
    return TRUE;
}

}

void WatchForInterrupt() {
    SetConsoleCtrlHandler(OnConsoleEvent, TRUE);
}

bool Interrupted() {
    return g_interrupts.load(std::memory_order_acquire) > 0;
}

}
