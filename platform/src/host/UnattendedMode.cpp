#include "deskhubp/host/UnattendedMode.h"

#include <atomic>

namespace deskhubp {
namespace {

std::atomic<bool> unattendedMode{false};

}

void SetUnattendedMode(bool on) {
    unattendedMode.store(on, std::memory_order_release);
}

bool UnattendedMode() {
    return unattendedMode.load(std::memory_order_acquire);
}

}
