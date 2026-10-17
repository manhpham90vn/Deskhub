#include "deskhubp/net/LocalSocket.h"

#include <utility>

namespace deskhubp {

LocalSocket::~LocalSocket() = default;

LocalSocket::LocalSocket(LocalSocket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

LocalSocket& LocalSocket::operator=(LocalSocket&& other) noexcept {
    if (this != &other) fd_ = std::exchange(other.fd_, -1);
    return *this;
}

LocalSocket LocalSocket::Listen(const std::string&, uint32_t) {
    return LocalSocket();
}

LocalSocket LocalSocket::Connect(const std::string&) {
    return LocalSocket();
}

std::optional<LocalSocket> LocalSocket::Accept(int) {
    return std::nullopt;
}

bool LocalSocket::WriteAll(std::span<const uint8_t>) {
    return false;
}

bool LocalSocket::ReadSome(std::span<uint8_t>, size_t& got) {
    got = 0;
    return false;
}

bool LocalSocket::ReadExactly(std::span<uint8_t>) {
    return false;
}

std::optional<LocalPeer> LocalSocket::Peer() const {
    return std::nullopt;
}

void LocalSocket::Shutdown() {}

void LocalSocket::Close() {
    fd_ = -1;
}

}
