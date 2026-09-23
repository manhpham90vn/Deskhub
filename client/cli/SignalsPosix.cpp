#include "Signals.h"

#include <atomic>
#include <csignal>

namespace deskhubcli {

namespace {

std::atomic<int> g_interrupts{0};

extern "C" void OnInterrupt(int) {
    g_interrupts.fetch_add(1, std::memory_order_release);
}

}

void WatchForInterrupt() {
    struct sigaction action{};
    action.sa_handler = OnInterrupt;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGHUP, &action, nullptr);

    struct sigaction ignore{};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGPIPE, &ignore, nullptr);
}

bool Interrupted() {
    return g_interrupts.load(std::memory_order_acquire) > 0;
}

}
