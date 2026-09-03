#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/RecordStream.h"
#include "deskhub/session/host/AuthThrottle.h"
#include "deskhubp/net/QuicEndpoint.h"
#include "deskhubp/auth/AuthNegotiation.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace deskhubp {

enum class VideoPath : uint8_t {
    RawUdp = 0,
    QuicDatagram = 1,
};

VideoPath VideoPathFromName(std::string_view name, VideoPath fallback);

std::string_view VideoPathName(VideoPath path);

enum class Lane : uint8_t {
    Realtime = 0,
    Interactive = 1,
    Bulk = 2,
};

inline constexpr size_t kLaneCount = 3;
inline constexpr size_t kMaxBulkQueued = 64;
inline constexpr unsigned kBulkEveryNthPop = 8;

struct TransportMessage {
    NetAddr from{};
    std::vector<uint8_t> bytes{};
};

struct TransportAuthCallbacks {
    std::function<void(const NetAddr&, const deskhub::Fingerprint&, std::string_view name)>
        onPaired;
    std::function<void(const NetAddr&, const deskhub::Fingerprint&, std::string_view name)>
        onApprovalNeeded;
    std::function<void(const NetAddr&, deskhub::AuthResultCode)> onRefused;
};

class SessionTransport {
public:
    SessionTransport();
    ~SessionTransport();
    SessionTransport(const SessionTransport&) = delete;
    SessionTransport& operator=(const SessionTransport&) = delete;

    bool Listen(const QuicSettings& settings, uint16_t port, const std::string& bindIp = {});
    bool Connect(const QuicSettings& settings, const NetAddr& server,
        std::string_view serverName);
    bool WaitEstablished(const NetAddr& peer, uint32_t timeoutMs);
    void Close();

    bool SetRecvTimeout(uint32_t ms);
    bool SendTo(const NetAddr& to, const uint8_t* data, size_t len);
    int RecvFrom(uint8_t* buf, size_t cap, NetAddr& from);

    void SetHostAuth(HostAuthConfig config, TransportAuthCallbacks callbacks);
    bool RunClientAuth(const NetAddr& server, ClientAuthConfig config, uint32_t timeoutMs,
        deskhub::AuthResultCode& outCode, bool& outHostProvedPasscode,
        const std::atomic<bool>* cancel = nullptr);
    bool SendKeepalive(const NetAddr& peer);
    void ApproveConnection(const NetAddr& peer, bool allowed);
    bool Authenticated(const NetAddr& peer) const;
    bool PeerAuth(const NetAddr& peer, deskhub::Fingerprint& fp, std::string& name) const;

    bool SendRecord(const NetAddr& to, std::span<const uint8_t> message);
    bool SendRecordOn(const NetAddr& to, uint64_t streamId, std::span<const uint8_t> message);

    void SetBulkReady(std::function<bool()> fn);
    size_t BulkQueued() const;

    void SetVideoPath(VideoPath path);
    VideoPath videoPath() const;

    void SetOnPeerGone(std::function<void(const NetAddr&)> fn);
    void SetOnStreamBroken(std::function<void(const NetAddr&, uint64_t streamId)> fn);

    std::optional<deskhub::Fingerprint> PeerFingerprint(const NetAddr& peer) const;
    bool Established(const NetAddr& peer) const;
    size_t MaxDatagramSize(const NetAddr& peer) const;
    QuicSendStats SendStats() const;

    bool IsOpen() const;
    bool lastBindAddrInUse() const;
    uint16_t LocalPort() const;

private:
    QuicCallbacks MakeCallbacks();
    void OnStream(QuicConnId conn, uint64_t stream, std::span<const uint8_t> bytes);
    void Deliver(const NetAddr& from, std::span<const uint8_t> message, bool overQuic);
    bool BulkBlocked() const;
    bool BulkServable() const;
    std::deque<TransportMessage>* NextLane();
    bool AnythingServable() const;
    void ReportBrokenStreams();
    bool SendReliable(const NetAddr& to, uint64_t streamId,
        std::span<const uint8_t> message);
    bool HandleHostAuth(const NetAddr& from, std::span<const uint8_t> message);
    void SendAuth(const NetAddr& to, std::span<const uint8_t> message);
    void SettleHostAuth(const NetAddr& peer, HostAuth& auth, const deskhub::AuthResult& result);

    QuicEndpoint endpoint_;
    std::map<uint64_t, deskhub::RecordStream> framers_;
    std::array<std::deque<TransportMessage>, kLaneCount> inbox_;
    std::deque<TransportMessage> authInbox_;
    std::vector<std::pair<NetAddr, uint64_t>> brokenStreams_;
    std::atomic<size_t> bulkDepth_{0};
    std::map<uint64_t, std::unique_ptr<HostAuth>> hostAuth_;
    std::map<uint64_t, bool> authenticated_;
    deskhub::AuthThrottle authThrottle_{};
    HostAuthConfig hostAuthConfig_{};
    TransportAuthCallbacks authCallbacks_{};
    bool hostAuthOn_ = false;
    bool clientAuthOn_ = false;
    std::function<void(const NetAddr&)> onPeerGone_;
    std::function<void(const NetAddr&, uint64_t streamId)> onStreamBroken_;
    VideoPath videoPath_ = VideoPath::QuicDatagram;
    std::function<bool()> bulkReady_;
    unsigned sinceBulkPop_ = 0;
    uint32_t recvWaitMs_ = 10;
    mutable std::mutex sendMutex_;
};

}
