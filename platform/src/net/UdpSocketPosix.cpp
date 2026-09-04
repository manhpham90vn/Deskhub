#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "deskhubp/net/UdpSocket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

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
    const int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) return false;
    const int wanted = ms == 0 ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (wanted != flags && fcntl(fd_, F_SETFL, wanted) != 0) return false;
    timeval tv{};
    tv.tv_sec = decltype(tv.tv_sec)(ms / 1000);
    tv.tv_usec = decltype(tv.tv_usec)((ms % 1000) * 1000);
    return setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool UdpSocket::EnableReceiveCoalescing() {
#if defined(__linux__)
    if (!IsOpen()) return false;
    const int on = 1;
    if (setsockopt(fd_, IPPROTO_UDP, UDP_GRO, &on, sizeof(on)) != 0) return false;
    receiveCoalescingOn_ = true;
    return true;
#else
    return false;
#endif
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

namespace {

#if defined(__linux__)

bool SegmentationOffloadRefused(int code) {
    return code == EIO || code == EINVAL || code == ENOPROTOOPT || code == EOPNOTSUPP ||
           code == EMSGSIZE;
}

bool SendOneSegmentedRun(int fd, const sockaddr_in& sa, std::span<const OutboundDatagram> run,
    bool& refused) {
    iovec iov[kMaxSendBatch]{};
    size_t total = 0;
    for (size_t i = 0; i < run.size(); ++i) {
        iov[i].iov_base = const_cast<uint8_t*>(run[i].data);
        iov[i].iov_len = run[i].len;
        total += run[i].len;
    }

    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(uint16_t))]{};
    msghdr msg{};
    msg.msg_name = const_cast<sockaddr_in*>(&sa);
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = iov;
    msg.msg_iovlen = run.size();
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    cmsghdr* header = CMSG_FIRSTHDR(&msg);
    header->cmsg_level = IPPROTO_UDP;
    header->cmsg_type = UDP_SEGMENT;
    header->cmsg_len = CMSG_LEN(sizeof(uint16_t));
    const uint16_t segment = uint16_t(run[0].len);
    std::memcpy(CMSG_DATA(header), &segment, sizeof(segment));

    for (;;) {
        const ssize_t n = sendmsg(fd, &msg, 0);
        if (n == ssize_t(total)) return true;
        if (n < 0 && errno == EINTR) continue;
        refused = n < 0 && SegmentationOffloadRefused(errno);
        return false;
    }
}

size_t CoalescedSegmentSize(const msghdr& msg) {
    for (const cmsghdr* header = CMSG_FIRSTHDR(&msg); header != nullptr;
        header = CMSG_NXTHDR(const_cast<msghdr*>(&msg), const_cast<cmsghdr*>(header))) {
        if (header->cmsg_level != IPPROTO_UDP || header->cmsg_type != UDP_GRO) continue;
        int segment = 0;
        std::memcpy(&segment, CMSG_DATA(header), sizeof(segment));
        return segment > 0 ? size_t(segment) : 0;
    }
    return 0;
}

size_t SendEachSeparately(int fd, const sockaddr_in& sa, std::span<const OutboundDatagram> packets) {
    iovec iov[kMaxSendBatch]{};
    mmsghdr msgs[kMaxSendBatch]{};
    for (size_t i = 0; i < packets.size(); ++i) {
        iov[i].iov_base = const_cast<uint8_t*>(packets[i].data);
        iov[i].iov_len = packets[i].len;
        msgs[i].msg_hdr.msg_name = const_cast<sockaddr_in*>(&sa);
        msgs[i].msg_hdr.msg_namelen = sizeof(sa);
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

    size_t done = 0;
    while (done < packets.size()) {
        const int n = sendmmsg(fd, msgs + done, unsigned(packets.size() - done), 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
            break;
        }
        done += size_t(n);
    }
    return done;
}

#endif

}

size_t UdpSocket::SendBatch(const NetAddr& to, std::span<const OutboundDatagram> packets) {
    if (!IsOpen() || packets.empty()) return 0;

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(to.ip);
    sa.sin_port = htons(to.port);

#if defined(__linux__)
    const size_t batch = packets.size() < kMaxSendBatch ? packets.size() : kMaxSendBatch;
    size_t sent = 0;
    while (!segmentationOffloadOff_ && batch - sent >= 2) {
        const size_t run = LeadingRunOfEqualSegments(packets.subspan(sent, batch - sent));
        if (run < 2) break;
        bool refused = false;
        if (SendOneSegmentedRun(fd_, sa, packets.subspan(sent, run), refused)) {
            sent += run;
            continue;
        }
        if (!refused) return sent;
        segmentationOffloadOff_ = true;
    }
    return sent + SendEachSeparately(fd_, sa, packets.subspan(sent, batch - sent));
#else
    size_t sent = 0;
    for (const OutboundDatagram& packet : packets) {
        const ssize_t n = sendto(fd_, packet.data, packet.len, 0, (sockaddr*)&sa, sizeof(sa));
        if (n != ssize_t(packet.len)) break;
        ++sent;
    }
    return sent;
#endif
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

int UdpSocket::RecvBatch(std::span<InboundDatagram> slots) {
    if (!IsOpen()) return -1;
    if (slots.empty()) return 0;

#if defined(__linux__)
    const size_t batch = slots.size() < kMaxRecvBatch ? slots.size() : kMaxRecvBatch;
    iovec iov[kMaxRecvBatch]{};
    mmsghdr msgs[kMaxRecvBatch]{};
    sockaddr_in addrs[kMaxRecvBatch]{};
    alignas(cmsghdr) char control[kMaxRecvBatch][CMSG_SPACE(sizeof(int))]{};
    for (size_t i = 0; i < batch; ++i) {
        iov[i].iov_base = slots[i].buf;
        iov[i].iov_len = slots[i].cap;
        msgs[i].msg_hdr.msg_name = &addrs[i];
        msgs[i].msg_hdr.msg_namelen = sizeof(addrs[i]);
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        if (!receiveCoalescingOn_) continue;
        msgs[i].msg_hdr.msg_control = control[i];
        msgs[i].msg_hdr.msg_controllen = sizeof(control[i]);
    }

    int got = 0;
    for (;;) {
        got = recvmmsg(fd_, msgs, unsigned(batch), MSG_WAITFORONE, nullptr);
        if (got >= 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED) return 0;
        return -1;
    }

    for (int i = 0; i < got; ++i) {
        InboundDatagram& slot = slots[size_t(i)];
        slot.len = msgs[i].msg_len;
        slot.segment = receiveCoalescingOn_ ? CoalescedSegmentSize(msgs[i].msg_hdr) : 0;
        if (slot.segment >= slot.len) slot.segment = 0;
        slot.from.ip = ntohl(addrs[i].sin_addr.s_addr);
        slot.from.port = ntohs(addrs[i].sin_port);
        if ((msgs[i].msg_hdr.msg_flags & MSG_TRUNC) == 0) continue;
        LOGE(
            "[UDP] A coalesced read of %zu bytes did not fit the %zu-byte slot, so the tail of "
            "that burst was dropped - a slot offered to receive coalescing must hold "
            "%zu bytes.",
            slot.len, slot.cap, kMaxCoalescedBytes);
    }
    return got;
#else
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
#endif
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
