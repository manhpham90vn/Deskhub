#include "deskhubp/net/UdpSocket.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <cstdio>
#include <cstring>

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"

#pragma comment(lib, "ws2_32.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#ifndef UDP_SEND_MSG_SIZE
#define UDP_SEND_MSG_SIZE 2
#endif

#ifndef UDP_RECV_MAX_COALESCED_SIZE
#define UDP_RECV_MAX_COALESCED_SIZE 3
#endif

#ifndef UDP_COALESCED_INFO
#define UDP_COALESCED_INFO 3
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

void* ResolveSendMsg(SOCKET s) {
    GUID id = WSAID_WSASENDMSG;
    LPFN_WSASENDMSG fn = nullptr;
    DWORD returned = 0;
    if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &id, sizeof(id),
            static_cast<void*>(&fn), sizeof(fn), &returned, nullptr, nullptr) == SOCKET_ERROR)
        return nullptr;
    return (void*)fn;
}

void* ResolveRecvMsg(SOCKET s) {
    GUID id = WSAID_WSARECVMSG;
    LPFN_WSARECVMSG fn = nullptr;
    DWORD returned = 0;
    if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &id, sizeof(id),
            static_cast<void*>(&fn), sizeof(fn), &returned, nullptr, nullptr) == SOCKET_ERROR)
        return nullptr;
    return (void*)fn;
}

size_t CoalescedSegmentSize(const WSAMSG& msg) {
    for (WSACMSGHDR* header = WSA_CMSG_FIRSTHDR(const_cast<WSAMSG*>(&msg)); header != nullptr;
        header = WSA_CMSG_NXTHDR(const_cast<WSAMSG*>(&msg), header)) {
        if (header->cmsg_level != IPPROTO_UDP || header->cmsg_type != UDP_COALESCED_INFO) continue;
        DWORD segment = 0;
        std::memcpy(&segment, WSA_CMSG_DATA(header), sizeof(segment));
        return size_t(segment);
    }
    return 0;
}

bool SegmentationOffloadRefused(int code) {
    return code == WSAEINVAL || code == WSAENOPROTOOPT || code == WSAEOPNOTSUPP ||
           code == WSAEMSGSIZE || code == WSAEAFNOSUPPORT;
}

bool SendOneSegmentedRun(SOCKET s, LPFN_WSASENDMSG sendMsg, const sockaddr_in& sa,
    std::span<const OutboundDatagram> run, bool& refused) {
    if (!sendMsg) {
        refused = true;
        return false;
    }

    WSABUF bufs[kMaxSendBatch]{};
    DWORD total = 0;
    for (size_t i = 0; i < run.size(); ++i) {
        bufs[i].buf = (CHAR*)run[i].data;
        bufs[i].len = ULONG(run[i].len);
        total += ULONG(run[i].len);
    }

    char control[WSA_CMSG_SPACE(sizeof(DWORD))]{};
    WSAMSG msg{};
    msg.name = (LPSOCKADDR)&sa;
    msg.namelen = sizeof(sa);
    msg.lpBuffers = bufs;
    msg.dwBufferCount = DWORD(run.size());
    msg.Control.buf = control;
    msg.Control.len = sizeof(control);

    WSACMSGHDR* header = WSA_CMSG_FIRSTHDR(&msg);
    header->cmsg_level = IPPROTO_UDP;
    header->cmsg_type = UDP_SEND_MSG_SIZE;
    header->cmsg_len = WSA_CMSG_LEN(sizeof(DWORD));
    const DWORD segment = DWORD(run[0].len);
    std::memcpy(WSA_CMSG_DATA(header), &segment, sizeof(segment));

    DWORD written = 0;
    if (sendMsg(s, &msg, 0, &written, nullptr, nullptr) == 0 && written == total) return true;
    refused = SegmentationOffloadRefused(WSAGetLastError());
    return false;
}

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
                "[UDP] Port %u is already in use - another Deskhub host (or another "
                "program) is still listening on it.",
                localPort);
        else
            LOGE("[UDP] bind(:%u) failed: %d", localPort, err);
        closesocket(s);
        return false;
    }

    sock_ = uint64_t(s);
    sendMsg_ = ResolveSendMsg(s);
    if (!sendMsg_) segmentationOffloadOff_ = true;
    recvMsg_ = ResolveRecvMsg(s);
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

bool UdpSocket::EnableReceiveCoalescing() {
    if (!IsOpen() || recvMsg_ == nullptr) return false;
    DWORD most = DWORD(kMaxCoalescedBytes);
    if (setsockopt(SOCKET(sock_), IPPROTO_UDP, UDP_RECV_MAX_COALESCED_SIZE,
            (const char*)&most, sizeof(most)) != 0)
        return false;
    receiveCoalescingOn_ = true;
    return true;
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
    if (!IsOpen() || packets.empty()) return 0;

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(to.ip);
    sa.sin_port = htons(to.port);

    const size_t batch = packets.size() < kMaxSendBatch ? packets.size() : kMaxSendBatch;
    size_t sent = 0;
    while (!segmentationOffloadOff_ && batch - sent >= 2) {
        const size_t run = LeadingRunOfEqualSegments(packets.subspan(sent, batch - sent));
        if (run < 2) break;
        bool refused = false;
        if (SendOneSegmentedRun(SOCKET(sock_), (LPFN_WSASENDMSG)sendMsg_, sa,
                packets.subspan(sent, run), refused)) {
            sent += run;
            continue;
        }
        if (!refused) return sent;
        LOGW(
            "[UDP] This stack refused UDP segmentation offload, so datagrams go out one "
            "syscall each from here on.");
        segmentationOffloadOff_ = true;
    }

    for (size_t i = sent; i < batch; ++i) {
        if (!SendTo(to, packets[i].data, packets[i].len)) break;
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

int UdpSocket::RecvCoalesced(InboundDatagram& slot) {
    sockaddr_in sa{};
    WSABUF buf{ULONG(slot.cap), (CHAR*)slot.buf};
    char control[WSA_CMSG_SPACE(sizeof(DWORD))]{};
    WSAMSG msg{};
    msg.name = (LPSOCKADDR)&sa;
    msg.namelen = sizeof(sa);
    msg.lpBuffers = &buf;
    msg.dwBufferCount = 1;
    msg.Control.buf = control;
    msg.Control.len = sizeof(control);

    DWORD read = 0;
    if (((LPFN_WSARECVMSG)recvMsg_)(SOCKET(sock_), &msg, &read, nullptr, nullptr) != 0) {
        const int err = WSAGetLastError();
        if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK || err == WSAECONNRESET ||
            err == WSAEMSGSIZE)
            return 0;
        return -1;
    }

    slot.len = size_t(read);
    slot.segment = CoalescedSegmentSize(msg);
    if (slot.segment >= slot.len) slot.segment = 0;
    slot.from.ip = ntohl(sa.sin_addr.s_addr);
    slot.from.port = ntohs(sa.sin_port);
    if ((msg.dwFlags & MSG_TRUNC) != 0)
        LOGE(
            "[UDP] A coalesced read of %zu bytes did not fit the %zu-byte slot, so the tail of "
            "that burst was dropped - a slot offered to receive coalescing must hold "
            "%zu bytes.",
            slot.len, slot.cap, kMaxCoalescedBytes);
    return int(read);
}

int UdpSocket::RecvBatch(std::span<InboundDatagram> slots) {
    if (!IsOpen()) return -1;
    if (slots.empty()) return 0;

    const size_t batch = slots.size() < kMaxRecvBatch ? slots.size() : kMaxRecvBatch;
    int filled = 0;
    for (size_t i = 0; i < batch; ++i) {
        InboundDatagram& slot = slots[i];
        const int n = receiveCoalescingOn_ ? RecvCoalesced(slot)
                                           : RecvFrom(slot.buf, slot.cap, slot.from);
        if (n < 0) return filled > 0 ? filled : -1;
        if (n == 0) break;
        if (!receiveCoalescingOn_) slot.len = size_t(n);
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
