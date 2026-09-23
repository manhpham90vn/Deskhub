#include "deskhubp/system/Pty.h"

namespace deskhubp {

struct Pty::Impl {
};

Pty::Pty() : impl_(std::make_unique<Impl>()) {
}

Pty::~Pty() = default;

std::string DefaultShell() {
    return {};
}

bool Pty::Start(const std::string&, deskhub::TermSize) {
    return false;
}

bool Pty::Running() const {
    return false;
}

int Pty::Read(uint8_t*, size_t, uint32_t) {
    return -1;
}

bool Pty::Write(std::span<const uint8_t>) {
    return false;
}

bool Pty::Resize(deskhub::TermSize) {
    return false;
}

int Pty::ExitCode() const {
    return 0;
}

void Pty::Close() {
}

}
