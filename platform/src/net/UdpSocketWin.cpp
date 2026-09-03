#include "deskhubp/net/UdpSocket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"

#pragma comment(lib, "ws2_32.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

std::string NetAddr::ToString() const {
    char b[32];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u:%u",
        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
    return b;
}

bool ParseNetAddr(const std::string& s, NetAddr& out) {
    std::string host;
    uint16_t port = kDeskhubPort;
    if (!deskhub::ui::SplitHostPort(s, host, port)) return false;
    IN_ADDR a{};
    if (InetPtonA(AF_INET, host.c_str(), &a) != 1) return false;
    out.ip = ntohl(a.S_un.S_addr);
    out.port = port;
    return true;
}

namespace {

bool WinsockReady() {
    static const bool ready = [] {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    return ready;
}

}

UdpSocket::~UdpSocket() {
    Close();
}

bool UdpSocket::Open(uint16_t localPort, const std::string& bindIp) {
    lastBindAddrInUse_ = false;
    if (!WinsockReady()) {
        LOGE("[UDP] WSAStartup failed.");
        return false;
    }

    const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        LOGE("[UDP] socket() failed: %d", WSAGetLastError());
        return false;
    }

    BOOL off = FALSE;
    DWORD bytes = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &bytes, nullptr, nullptr);

    int rcvbuf = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));

    int sndbuf = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf, sizeof(sndbuf));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(localPort);
    if (!bindIp.empty() && InetPtonA(AF_INET, bindIp.c_str(), &local.sin_addr) != 1) {
        LOGE("[UDP] bad bind address %s", bindIp.c_str());
        closesocket(s);
        return false;
    }
    if (bind(s, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        lastBindAddrInUse_ = (err == WSAEADDRINUSE);
        if (lastBindAddrInUse_)
            LOGE(
                "[UDP] Port %u is already in use — another Deskhub host (or another "
                "program) is still listening on it.",
                localPort);
        else
            LOGE("[UDP] bind(:%u) failed: %d", localPort, err);
        closesocket(s);
        return false;
    }

    sock_ = uint64_t(s);
    return true;
}

bool UdpSocket::SetRecvTimeout(uint32_t ms) {
    if (!IsOpen()) return false;
    u_long refuseToWait = ms == 0 ? 1 : 0;
    if (ioctlsocket(SOCKET(sock_), FIONBIO, &refuseToWait) != 0) return false;
    DWORD t = ms;
    return setsockopt(SOCKET(sock_), SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&t, sizeof(t)) == 0;
}

bool UdpSocket::WaitReadable(uint32_t ms) {
    if (!IsOpen()) return false;
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(SOCKET(sock_), &readable);
    TIMEVAL tv{long(ms / 1000), long((ms % 1000) * 1000)};
    return select(0, &readable, nullptr, nullptr, &tv) > 0;
}

bool UdpSocket::SendTo(const NetAddr& to, const uint8_t* data, size_t len) {
    if (!IsOpen()) return false;
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(to.ip);
    sa.sin_port = htons(to.port);
    return sendto(SOCKET(sock_), (const char*)data, int(len), 0,
               (sockaddr*)&sa, sizeof(sa)) == int(len);
}

size_t UdpSocket::SendBatch(const NetAddr& to, std::span<const OutboundDatagram> packets) {
    size_t sent = 0;
    for (const OutboundDatagram& packet : packets) {
        if (!SendTo(to, packet.data, packet.len)) break;
        ++sent;
    }
    return sent;
}

int UdpSocket::RecvFrom(uint8_t* buf, size_t cap, NetAddr& from) {
    if (!IsOpen()) return -1;
    sockaddr_in sa{};
    int salen = sizeof(sa);
    const int n = recvfrom(SOCKET(sock_), (char*)buf, int(cap), 0, (sockaddr*)&sa, &salen);
    if (n >= 0) {
        from.ip = ntohl(sa.sin_addr.s_addr);
        from.port = ntohs(sa.sin_port);
        return n;
    }
    const int err = WSAGetLastError();
    if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK || err == WSAECONNRESET ||
        err == WSAEMSGSIZE)
        return 0;
    return -1;
}

int UdpSocket::RecvBatch(std::span<InboundDatagram> slots) {
    if (!IsOpen()) return -1;
    if (slots.empty()) return 0;

    const size_t batch = slots.size() < kMaxRecvBatch ? slots.size() : kMaxRecvBatch;
    int filled = 0;
    for (size_t i = 0; i < batch; ++i) {
        InboundDatagram& slot = slots[i];
        const int n = RecvFrom(slot.buf, slot.cap, slot.from);
        if (n < 0) return filled > 0 ? filled : -1;
        if (n == 0) break;
        slot.len = size_t(n);
        ++filled;
    }
    return filled;
}

uint16_t UdpSocket::LocalPort() const {
    if (!IsOpen()) return 0;
    sockaddr_in bound{};
    int len = sizeof(bound);
    if (getsockname(SOCKET(sock_), (sockaddr*)&bound, &len) == SOCKET_ERROR) return 0;
    return ntohs(bound.sin_port);
}

void UdpSocket::Close() {
    if (IsOpen()) {
        closesocket(SOCKET(sock_));
        sock_ = ~0ull;
    }
}
