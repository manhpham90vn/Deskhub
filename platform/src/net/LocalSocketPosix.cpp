#include "deskhubp/net/LocalSocket.h"

#include <cerrno>
#include <cstring>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __APPLE__
#include <libproc.h>
#include <sys/ucred.h>
#else
#include <climits>
#endif

namespace deskhubp {
namespace {

constexpr int kBacklog = 16;

bool FitsSocketPath(const std::string& path) {
    sockaddr_un probe{};
    return !path.empty() && path.size() < sizeof(probe.sun_path);
}

sockaddr_un AddressOf(const std::string& path) {
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return address;
}

bool KeepPrivateToProcess(int fd) {
    const int flags = fcntl(fd, F_GETFD);
    if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != 0) return false;
#ifdef SO_NOSIGPIPE
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    return true;
}

int NewStreamSocket() {
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (!KeepPrivateToProcess(fd)) {
        close(fd);
        return -1;
    }
    return fd;
}

int SendFlags() {
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

#ifdef __APPLE__
std::optional<LocalPeer> ReadPeer(int fd) {
    LocalPeer peer;
    uid_t uid = 0;
    gid_t gid = 0;
    if (getpeereid(fd, &uid, &gid) != 0) return std::nullopt;
    peer.uid = uint32_t(uid);
    pid_t pid = 0;
    socklen_t pidLen = sizeof(pid);
    if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERPID, &pid, &pidLen) != 0) return std::nullopt;
    peer.pid = int32_t(pid);
    char exe[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(pid, exe, sizeof(exe)) > 0) peer.exePath = exe;
#ifdef LOCAL_PEERTOKEN
    audit_token_t token{};
    socklen_t tokenLen = sizeof(token);
    if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERTOKEN, &token, &tokenLen) == 0 &&
        tokenLen == sizeof(token)) {
        const auto* raw = reinterpret_cast<const uint8_t*>(&token);
        peer.auditToken.assign(raw, raw + sizeof(token));
    }
#endif
    return peer;
}
#else
std::optional<LocalPeer> ReadPeer(int fd) {
    ucred credentials{};
    socklen_t len = sizeof(credentials);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &len) != 0) return std::nullopt;
    LocalPeer peer;
    peer.pid = int32_t(credentials.pid);
    peer.uid = uint32_t(credentials.uid);
    const std::string link = "/proc/" + std::to_string(credentials.pid) + "/exe";
    char exe[PATH_MAX] = {};
    const ssize_t n = readlink(link.c_str(), exe, sizeof(exe) - 1);
    if (n > 0) peer.exePath.assign(exe, size_t(n));
    return peer;
}
#endif

}

LocalSocket::~LocalSocket() {
    Close();
}

LocalSocket::LocalSocket(LocalSocket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

LocalSocket& LocalSocket::operator=(LocalSocket&& other) noexcept {
    if (this != &other) {
        Close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

LocalSocket LocalSocket::Listen(const std::string& path, uint32_t mode) {
    if (!FitsSocketPath(path)) return LocalSocket();
    const int fd = NewStreamSocket();
    if (fd < 0) return LocalSocket();
    unlink(path.c_str());
    const sockaddr_un address = AddressOf(path);
    const bool bound = bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
                       chmod(path.c_str(), mode_t(mode)) == 0 && listen(fd, kBacklog) == 0;
    if (!bound) {
        close(fd);
        unlink(path.c_str());
        return LocalSocket();
    }
    return LocalSocket(fd);
}

LocalSocket LocalSocket::Connect(const std::string& path) {
    if (!FitsSocketPath(path)) return LocalSocket();
    const int fd = NewStreamSocket();
    if (fd < 0) return LocalSocket();
    const sockaddr_un address = AddressOf(path);
    if (connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        close(fd);
        return LocalSocket();
    }
    return LocalSocket(fd);
}

std::optional<LocalSocket> LocalSocket::Accept(int timeoutMs) {
    if (fd_ < 0) return std::nullopt;
    pollfd waiter{};
    waiter.fd = fd_;
    waiter.events = POLLIN;
    int ready = 0;
    do {
        ready = poll(&waiter, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);
    if (ready <= 0) return std::nullopt;
    int client = -1;
    do {
        client = accept(fd_, nullptr, nullptr);
    } while (client < 0 && errno == EINTR);
    if (client < 0) return std::nullopt;
    if (!KeepPrivateToProcess(client)) {
        close(client);
        return std::nullopt;
    }
    return LocalSocket(client);
}

bool LocalSocket::WriteAll(std::span<const uint8_t> bytes) {
    if (fd_ < 0) return false;
    size_t sent = 0;
    while (sent < bytes.size()) {
        const ssize_t n = send(fd_, bytes.data() + sent, bytes.size() - sent, SendFlags());
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        sent += size_t(n);
    }
    return true;
}

bool LocalSocket::ReadSome(std::span<uint8_t> into, size_t& got) {
    got = 0;
    if (fd_ < 0 || into.empty()) return false;
    ssize_t n = 0;
    do {
        n = read(fd_, into.data(), into.size());
    } while (n < 0 && errno == EINTR);
    if (n <= 0) return false;
    got = size_t(n);
    return true;
}

bool LocalSocket::ReadExactly(std::span<uint8_t> into) {
    size_t filled = 0;
    while (filled < into.size()) {
        size_t got = 0;
        if (!ReadSome(into.subspan(filled), got)) return false;
        filled += got;
    }
    return true;
}

std::optional<LocalPeer> LocalSocket::Peer() const {
    if (fd_ < 0) return std::nullopt;
    return ReadPeer(fd_);
}

void LocalSocket::Shutdown() {
    if (fd_ >= 0) shutdown(fd_, SHUT_RDWR);
}

void LocalSocket::Close() {
    if (fd_ < 0) return;
    close(fd_);
    fd_ = -1;
}

}
