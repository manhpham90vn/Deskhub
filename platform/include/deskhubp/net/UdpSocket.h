#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

struct NetAddr {
    uint32_t ip = 0;
    uint16_t port = 0;

    bool operator==(const NetAddr&) const = default;

    uint64_t Pack() const {
        return (uint64_t(ip) << 16) | port;
    }
    static NetAddr Unpack(uint64_t v) {
        return NetAddr{uint32_t(v >> 16), uint16_t(v)};
    }
    std::string ToString() const;
};

using deskhub::kDeskhubPort;

bool ParseNetAddr(const std::string& s, NetAddr& out);

inline bool HostKeyOf(const std::string& addr, uint64_t& key) {
    NetAddr parsed{};
    if (!ParseNetAddr(addr, parsed)) return false;
    key = parsed.Pack();
    return true;
}

inline bool SameHost(const std::string& addr, uint64_t key) {
    uint64_t other = 0;
    return HostKeyOf(addr, other) && other == key;
}

struct OutboundDatagram {
    const uint8_t* data = nullptr;
    size_t len = 0;
};

struct InboundDatagram {
    uint8_t* buf = nullptr;
    size_t cap = 0;
    size_t len = 0;
    NetAddr from{};
};

inline constexpr size_t kMaxSendBatch = 16;
inline constexpr size_t kMaxRecvBatch = 16;

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool Open(uint16_t localPort, const std::string& bindIp = {});

    bool SetRecvTimeout(uint32_t ms);

    bool WaitReadable(uint32_t ms);

    bool SendTo(const NetAddr& to, const uint8_t* data, size_t len);

    size_t SendBatch(const NetAddr& to, std::span<const OutboundDatagram> packets);

    int RecvFrom(uint8_t* buf, size_t cap, NetAddr& from);

    int RecvBatch(std::span<InboundDatagram> slots);

    void Close();

    bool IsOpen() const {
#ifdef _WIN32
        return sock_ != ~0ull;
#else
        return fd_ >= 0;
#endif
    }

    bool lastBindAddrInUse() const {
        return lastBindAddrInUse_;
    }

    uint16_t LocalPort() const;

private:
#ifdef _WIN32
    uint64_t sock_ = ~0ull;
#else
    int fd_ = -1;
#endif
    bool lastBindAddrInUse_ = false;
    bool segmentationOffloadOff_ = false;
};
