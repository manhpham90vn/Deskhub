#include "deskhubp/net/UdpSocket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"

std::string NetAddr::ToString() const {
    char b[32];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF, port);
    return b;
}

bool ParseNetAddr(const std::string& s, NetAddr& out) {
    std::string host;
    uint16_t port = kDeskhubPort;
    if (!deskhub::ui::SplitHostPort(s, host, port)) return false;
    in_addr a{};
    if (inet_pton(AF_INET, host.c_str(), &a) != 1) return false;
    out.ip = ntohl(a.s_addr);
    out.port = port;
    return true;
}

UdpSocket::~UdpSocket() {
    Close();
}

bool UdpSocket::Open(uint16_t localPort, const std::string& bindIp) {
    lastBindAddrInUse_ = false;

    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        LOGE("[UDP] socket() failed: %d", errno);
        return false;
    }

    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    int sndbuf = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(localPort);
    if (!bindIp.empty() && inet_pton(AF_INET, bindIp.c_str(), &local.sin_addr) != 1) {
        LOGE("[UDP] bad bind address %s", bindIp.c_str());
        close(s);
        return false;
    }
    if (bind(s, (sockaddr*)&local, sizeof(local)) != 0) {
        lastBindAddrInUse_ = (errno == EADDRINUSE);
        LOGE("[UDP] bind(%s:%u) failed: %d", bindIp.empty() ? "*" : bindIp.c_str(), localPort,
            errno);
        close(s);
        return false;
    }

    fd_ = s;
    return true;
}

bool UdpSocket::SetRecvTimeout(uint32_t ms) {
    if (!IsOpen()) return false;
    timeval tv{};
    tv.tv_sec = decltype(tv.tv_sec)(ms / 1000);
    tv.tv_usec = decltype(tv.tv_usec)((ms % 1000) * 1000);
    return setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool UdpSocket::WaitReadable(uint32_t ms) {
    if (!IsOpen()) return false;
    pollfd entry{fd_, POLLIN, 0};
    return ::poll(&entry, 1, int(ms)) > 0 && (entry.revents & POLLIN) != 0;
}

bool UdpSocket::SendTo(const NetAddr& to, const uint8_t* data, size_t len) {
    if (!IsOpen()) return false;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(to.ip);
    sa.sin_port = htons(to.port);
    const ssize_t n = sendto(fd_, data, len, 0, (sockaddr*)&sa, sizeof(sa));
    return n == ssize_t(len);
}

int UdpSocket::RecvFrom(uint8_t* buf, size_t cap, NetAddr& from) {
    if (!IsOpen()) return -1;
    sockaddr_in sa{};
    socklen_t salen = sizeof(sa);
    const ssize_t n = recvfrom(fd_, buf, cap, 0, (sockaddr*)&sa, &salen);
    if (n >= 0) {
        from.ip = ntohl(sa.sin_addr.s_addr);
        from.port = ntohs(sa.sin_port);
        return int(n);
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNREFUSED)
        return 0;
    return -1;
}

uint16_t UdpSocket::LocalPort() const {
    if (!IsOpen()) return 0;
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (getsockname(fd_, (sockaddr*)&bound, &len) != 0) return 0;
    return ntohs(bound.sin_port);
}

void UdpSocket::Close() {
    if (IsOpen()) {
        close(fd_);
        fd_ = -1;
    }
}
