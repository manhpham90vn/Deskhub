#include "deskhubp/net/SessionTransport.h"

#include "deskhub/media/ShareTypes.h"

#include <algorithm>
#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/PairedDevicesFile.h"

namespace deskhubp {

namespace {

constexpr uint32_t kEstablishPollMs = 5;
constexpr uint32_t kAuthPollMs = 2;

uint64_t StreamKey(QuicConnId conn, uint64_t stream) {
    return conn ^ (stream << 48);
}

int64_t NowUnix() {
    return NowUnixSeconds();
}

bool IsAuthMessage(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header) return false;
    switch (header->type) {
        case deskhub::MsgType::AuthStart:
        case deskhub::MsgType::AuthChallenge:
        case deskhub::MsgType::AuthResponse:
        case deskhub::MsgType::AuthResult: return true;
        default: return false;
    }
}

bool CarriesVideo(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    return header.has_value() && header->chan == deskhub::Chan::Video;
}

bool CarriesAudio(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    return header.has_value() && header->chan == deskhub::Chan::Audio;
}

Lane LaneOf(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header) return Lane::Interactive;
    switch (header->chan) {
        case deskhub::Chan::Video:
        case deskhub::Chan::Audio: return Lane::Realtime;
        case deskhub::Chan::File: return Lane::Bulk;
        default: return Lane::Interactive;
    }
}

bool IsBeaconMessage(std::span<const uint8_t> message) {
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header) return false;
    switch (header->type) {
        case deskhub::MsgType::ListSources:
        case deskhub::MsgType::SourceList:
        case deskhub::MsgType::Ping:
        case deskhub::MsgType::Pong: return true;
        default: return false;
    }
}

}

SessionTransport::SessionTransport() = default;

SessionTransport::~SessionTransport() {
    Close();
}

QuicCallbacks SessionTransport::MakeCallbacks() {
    QuicCallbacks hooks;
    hooks.onStream = [this](QuicConnId conn, uint64_t stream, std::span<const uint8_t> bytes,
                         bool) { OnStream(conn, stream, bytes); };
    hooks.onDatagram = [this](QuicConnId conn, std::span<const uint8_t> bytes) {
        Deliver(NetAddr::Unpack(conn), bytes, true);
    };
    hooks.onForeignDatagram = [this](const NetAddr& from, std::span<const uint8_t> bytes) {
        Deliver(from, bytes, false);
    };
    hooks.pauseStream = [this](uint64_t stream) {
        return stream == kQuicFileStream && BulkBlocked();
    };
    hooks.onStreamBroken = [this](QuicConnId conn, uint64_t stream) {
        brokenStreams_.emplace_back(NetAddr::Unpack(conn), stream);
    };
    hooks.onClosed = [this](QuicConnId conn, const NetAddr& peer) {
        for (auto it = framers_.begin(); it != framers_.end();) {
            if ((it->first ^ (it->first >> 48 << 48)) == conn)
                it = framers_.erase(it);
            else
                ++it;
        }
        if (onPeerGone_) onPeerGone_(peer);
    };
    return hooks;
}

bool SessionTransport::Listen(const QuicSettings& settings, uint16_t port,
    const std::string& bindIp) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.Listen(settings, bindIp, port, MakeCallbacks());
}

bool SessionTransport::Connect(const QuicSettings& settings, const NetAddr& server,
    std::string_view serverName) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.Connect(settings, server, serverName, MakeCallbacks());
}

bool SessionTransport::WaitEstablished(const NetAddr& peer, uint32_t timeoutMs) {
    const uint64_t deadline = NowUs() + uint64_t(timeoutMs) * 1000;
    for (;;) {
        {
            const std::lock_guard<std::mutex> lock(sendMutex_);
            endpoint_.Poll(NowUs(), 0);
            if (endpoint_.Established(peer.Pack())) return true;
        }
        if (NowUs() >= deadline) return false;
        endpoint_.WaitReadable(kEstablishPollMs);
    }
}

void SessionTransport::Close() {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    endpoint_.Close();
    framers_.clear();
    for (std::deque<TransportMessage>& lane : inbox_) lane.clear();
    bulkDepth_.store(0, std::memory_order_relaxed);
    authInbox_.clear();
    brokenStreams_.clear();
    hostAuth_.clear();
    authenticated_.clear();
}

bool SessionTransport::SetRecvTimeout(uint32_t ms) {
    recvWaitMs_ = ms;
    return true;
}

VideoPath VideoPathFromName(std::string_view name, VideoPath fallback) {
    if (name == deskhub::media::kVideoPathRawUdp) return VideoPath::RawUdp;
    if (name == deskhub::media::kVideoPathQuicDatagram) return VideoPath::QuicDatagram;
    return fallback;
}

std::string_view VideoPathName(VideoPath path) {
    return path == VideoPath::RawUdp ? deskhub::media::kVideoPathRawUdp
                                     : deskhub::media::kVideoPathQuicDatagram;
}

void SessionTransport::SetVideoPath(VideoPath path) {
    videoPath_ = path;
}

VideoPath SessionTransport::videoPath() const {
    return videoPath_;
}

void SessionTransport::SetOnPeerGone(std::function<void(const NetAddr&)> fn) {
    onPeerGone_ = std::move(fn);
}

void SessionTransport::SetOnStreamBroken(
    std::function<void(const NetAddr&, uint64_t streamId)> fn) {
    onStreamBroken_ = std::move(fn);
}

void SessionTransport::ReportBrokenStreams() {
    std::vector<std::pair<NetAddr, uint64_t>> broken;
    {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        broken.swap(brokenStreams_);
    }
    if (!onStreamBroken_) return;
    for (const auto& [peer, stream] : broken) onStreamBroken_(peer, stream);
}

void SessionTransport::OnStream(QuicConnId conn, uint64_t stream,
    std::span<const uint8_t> bytes) {
    deskhub::RecordStream& framer = framers_[StreamKey(conn, stream)];
    framer.Append(bytes);
    std::vector<uint8_t> message;
    while (framer.Next(message)) Deliver(NetAddr::Unpack(conn), message, true);
    if (framer.Failed()) {
        LOGW("transport: %s sent a malformed record stream, closing it",
            NetAddr::Unpack(conn).ToString().c_str());
        endpoint_.CloseConnection(conn, 1, "bad framing");
    }
}

void SessionTransport::Deliver(const NetAddr& from, std::span<const uint8_t> message,
    bool overQuic) {
    if (message.empty()) return;

    if (!overQuic) {
        const bool rawVideo = videoPath_ == VideoPath::RawUdp && CarriesVideo(message);
        const bool beaconFromStranger =
            IsBeaconMessage(message) && !endpoint_.Established(from.Pack());
        if (!rawVideo && !beaconFromStranger) return;
    }

    if (clientAuthOn_ && IsAuthMessage(message)) {
        TransportMessage queued;
        queued.from = from;
        queued.bytes.assign(message.begin(), message.end());
        authInbox_.push_back(std::move(queued));
        return;
    }

    if (hostAuthOn_ && overQuic && !HandleHostAuth(from, message)) return;

    TransportMessage queued;
    queued.from = from;
    queued.bytes.assign(message.begin(), message.end());
    const Lane lane = LaneOf(message);
    inbox_[size_t(lane)].push_back(std::move(queued));
    if (lane == Lane::Bulk)
        bulkDepth_.store(inbox_[size_t(Lane::Bulk)].size(), std::memory_order_relaxed);
}

void SessionTransport::SetBulkReady(std::function<bool()> fn) {
    bulkReady_ = std::move(fn);
}

size_t SessionTransport::BulkQueued() const {
    return bulkDepth_.load(std::memory_order_relaxed);
}

bool SessionTransport::BulkBlocked() const {
    if (inbox_[size_t(Lane::Bulk)].size() >= kMaxBulkQueued) return true;
    return bulkReady_ && !bulkReady_();
}

bool SessionTransport::BulkServable() const {
    if (inbox_[size_t(Lane::Bulk)].empty()) return false;
    return !bulkReady_ || bulkReady_();
}

bool SessionTransport::AnythingServable() const {
    return !inbox_[size_t(Lane::Realtime)].empty() ||
           !inbox_[size_t(Lane::Interactive)].empty() || BulkServable();
}

std::deque<TransportMessage>* SessionTransport::NextLane() {
    const bool bulkDue = sinceBulkPop_ >= kBulkEveryNthPop;
    if (bulkDue && BulkServable()) {
        sinceBulkPop_ = 0;
        return &inbox_[size_t(Lane::Bulk)];
    }
    for (const Lane lane : {Lane::Realtime, Lane::Interactive}) {
        if (inbox_[size_t(lane)].empty()) continue;
        ++sinceBulkPop_;
        return &inbox_[size_t(lane)];
    }
    if (!BulkServable()) return nullptr;
    sinceBulkPop_ = 0;
    return &inbox_[size_t(Lane::Bulk)];
}

bool SessionTransport::HandleHostAuth(const NetAddr& from, std::span<const uint8_t> message) {
    const uint64_t key = from.Pack();
    const std::optional<deskhub::CommonHeader> header = deskhub::ParseCommonHeader(message);
    if (!header) return false;
    const std::span<const uint8_t> payload = deskhub::PayloadOf(message);

    if (header->type == deskhub::MsgType::AuthStart) {
        const std::optional<deskhub::AuthStart> start = deskhub::ParseAuthStart(payload);
        if (!start) return false;

        auto auth = std::make_unique<HostAuth>();
        auth->Configure(hostAuthConfig_);
        const std::optional<deskhub::AuthChallenge> challenge = auth->Begin(*start);
        if (!challenge) return false;

        if (challenge->mode == deskhub::AuthMode::Passcode && authThrottle_.Locked(NowUs())) {
            deskhub::AuthResult locked;
            locked.code = deskhub::AuthResultCode::Locked;
            std::vector<uint8_t> out(deskhub::kMaxRecordSize);
            out.resize(deskhub::BuildAuthResult(out, locked));
            SendAuth(from, out);
            LOGW("transport: %s is locked out after too many wrong passcodes",
                from.ToString().c_str());
            if (authCallbacks_.onRefused)
                authCallbacks_.onRefused(from, deskhub::AuthResultCode::Locked);
            return false;
        }

        std::vector<uint8_t> out(deskhub::kMaxRecordSize);
        out.resize(deskhub::BuildAuthChallenge(out, *challenge));
        SendAuth(from, out);

        if (challenge->mode == deskhub::AuthMode::Approval && authCallbacks_.onApprovalNeeded)
            authCallbacks_.onApprovalNeeded(from, auth->PeerFingerprint(), auth->PeerName());
        if (challenge->mode == deskhub::AuthMode::Denied && authCallbacks_.onRefused)
            authCallbacks_.onRefused(from, deskhub::AuthResultCode::PairingDisabled);

        hostAuth_[key] = std::move(auth);
        return false;
    }

    if (header->type == deskhub::MsgType::AuthResponse) {
        const auto at = hostAuth_.find(key);
        if (at == hostAuth_.end()) return false;
        const std::optional<deskhub::AuthResponse> response = deskhub::ParseAuthResponse(payload);
        if (!response) return false;
        const deskhub::AuthResult result = at->second->Respond(*response, NowUnix());
        if (at->second->Mode() == deskhub::AuthMode::Passcode) {
            if (result.code == deskhub::AuthResultCode::WrongPasscode)
                authThrottle_.RecordFailure(NowUs());
            if (result.code == deskhub::AuthResultCode::Accepted) authThrottle_.RecordSuccess();
        }
        SettleHostAuth(from, *at->second, result);
        return false;
    }

    const auto settled = authenticated_.find(key);
    return settled != authenticated_.end() && settled->second;
}

void SessionTransport::SettleHostAuth(const NetAddr& peer, HostAuth& auth,
    const deskhub::AuthResult& result) {
    std::vector<uint8_t> out(deskhub::kMaxRecordSize);
    out.resize(deskhub::BuildAuthResult(out, result));
    SendAuth(peer, out);

    const bool accepted = result.code == deskhub::AuthResultCode::Accepted;
    authenticated_[peer.Pack()] = accepted;
    if (accepted) {
        LOGI("transport: %s is allowed in (%s)", peer.ToString().c_str(),
            deskhub::ShortFingerprint(auth.PeerFingerprint()).c_str());
        if (authCallbacks_.onPaired)
            authCallbacks_.onPaired(peer, auth.PeerFingerprint(), auth.PeerName());
        return;
    }
    LOGW("transport: %s was turned away", peer.ToString().c_str());
    if (authCallbacks_.onRefused) authCallbacks_.onRefused(peer, result.code);
}

void SessionTransport::SendAuth(const NetAddr& to, std::span<const uint8_t> message) {
    if (message.empty()) return;
    SendReliable(to, kQuicControlStream, message);
}

void SessionTransport::SetHostAuth(HostAuthConfig config, TransportAuthCallbacks callbacks) {
    hostAuthConfig_ = std::move(config);
    authCallbacks_ = std::move(callbacks);
    hostAuthOn_ = true;
}

void SessionTransport::ApproveConnection(const NetAddr& peer, bool allowed) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    const auto at = hostAuth_.find(peer.Pack());
    if (at == hostAuth_.end()) return;
    SettleHostAuth(peer, *at->second, at->second->Approve(allowed, NowUnix()));
}

bool SessionTransport::Authenticated(const NetAddr& peer) const {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    const auto at = authenticated_.find(peer.Pack());
    return at != authenticated_.end() && at->second;
}

bool SessionTransport::PeerAuth(const NetAddr& peer, deskhub::Fingerprint& fp,
    std::string& name) const {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    const auto authed = authenticated_.find(peer.Pack());
    if (authed == authenticated_.end() || !authed->second) return false;
    const auto at = hostAuth_.find(peer.Pack());
    if (at == hostAuth_.end()) return false;
    fp = at->second->PeerFingerprint();
    name = at->second->PeerName();
    return true;
}

bool SessionTransport::SendRecord(const NetAddr& to, std::span<const uint8_t> message) {
    return SendRecordOn(to, kQuicControlStream, message);
}

bool SessionTransport::SendRecordOn(const NetAddr& to, uint64_t streamId,
    std::span<const uint8_t> message) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (!endpoint_.Established(to.Pack())) return false;
    return SendReliable(to, streamId, message);
}

bool SessionTransport::SendKeepalive(const NetAddr& peer) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.SendKeepalive(peer.Pack());
}

bool SessionTransport::RunClientAuth(const NetAddr& server, ClientAuthConfig config,
    uint32_t timeoutMs, deskhub::AuthResultCode& outCode, bool& outHostProvedPasscode,
    const std::atomic<bool>* cancel) {
    clientAuthOn_ = true;
    outCode = deskhub::AuthResultCode::NotPaired;
    outHostProvedPasscode = false;

    ClientAuth client;
    client.Configure(std::move(config));

    std::vector<uint8_t> out(deskhub::kMaxRecordSize);
    out.resize(deskhub::BuildAuthStart(out, client.Begin()));
    if (out.empty()) return false;
    {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        SendAuth(server, out);
    }

    const uint64_t deadline = NowUs() + uint64_t(timeoutMs) * 1000;
    bool answered = false;
    while (NowUs() < deadline) {
        if (cancel != nullptr && cancel->load(std::memory_order_acquire)) break;
        if (authInbox_.empty()) {
            const std::lock_guard<std::mutex> lock(sendMutex_);
            endpoint_.Poll(NowUs(), kAuthPollMs);
            continue;
        }

        const TransportMessage message = std::move(authInbox_.front());
        authInbox_.pop_front();
        const std::optional<deskhub::CommonHeader> header =
            deskhub::ParseCommonHeader(message.bytes);
        if (!header) continue;
        const std::span<const uint8_t> payload = deskhub::PayloadOf(message.bytes);

        if (header->type == deskhub::MsgType::AuthChallenge && !answered) {
            const std::optional<deskhub::AuthChallenge> challenge =
                deskhub::ParseAuthChallenge(payload);
            if (!challenge) continue;
            if (challenge->mode == deskhub::AuthMode::Denied) {
                outCode = deskhub::AuthResultCode::PairingDisabled;
                clientAuthOn_ = false;
                return false;
            }
            if (challenge->mode == deskhub::AuthMode::Approval) {
                answered = true;
                continue;
            }
            const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
            if (!response) {
                outCode = deskhub::AuthResultCode::WrongPasscode;
                clientAuthOn_ = false;
                return false;
            }
            std::vector<uint8_t> reply(deskhub::kMaxRecordSize);
            reply.resize(deskhub::BuildAuthResponse(reply, *response));
            {
                const std::lock_guard<std::mutex> lock(sendMutex_);
                SendAuth(server, reply);
            }
            answered = true;
            continue;
        }

        if (header->type == deskhub::MsgType::AuthResult) {
            const std::optional<deskhub::AuthResult> result = deskhub::ParseAuthResult(payload);
            if (!result) continue;
            outCode = result->code;
            outHostProvedPasscode = client.HostProvedThePasscode(*result);
            clientAuthOn_ = false;
            return result->code == deskhub::AuthResultCode::Accepted;
        }
    }

    outCode = deskhub::AuthResultCode::TimedOut;
    clientAuthOn_ = false;
    return false;
}

bool SessionTransport::SendReliable(const NetAddr& to, uint64_t streamId,
    std::span<const uint8_t> message) {
    std::vector<uint8_t> record(deskhub::kRecordPrefixSize + message.size());
    const size_t written = deskhub::BuildRecord(record, message);
    if (written == 0) return false;
    return endpoint_.SendStream(to.Pack(), streamId,
        std::span<const uint8_t>(record.data(), written));
}

bool SessionTransport::SendTo(const NetAddr& to, const uint8_t* data, size_t len) {
    const std::span<const uint8_t> message(data, len);

    if (CarriesVideo(message)) {
        if (videoPath_ == VideoPath::RawUdp) return endpoint_.SendRaw(to, message);
        const std::lock_guard<std::mutex> lock(sendMutex_);
        return endpoint_.SendDatagram(to.Pack(), message);
    }

    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (!endpoint_.Established(to.Pack())) return endpoint_.SendRaw(to, message);
    if (CarriesAudio(message)) return endpoint_.SendDatagram(to.Pack(), message);
    return SendReliable(to, kQuicControlStream, message);
}

int SessionTransport::RecvFrom(uint8_t* buf, size_t cap, NetAddr& from) {
    if (!endpoint_.IsOpen()) return -1;
    if (!AnythingServable()) {
        endpoint_.WaitReadable(recvWaitMs_);
        const std::lock_guard<std::mutex> lock(sendMutex_);
        endpoint_.Poll(NowUs(), 0);
    }
    ReportBrokenStreams();
    std::deque<TransportMessage>* lane = NextLane();
    if (lane == nullptr) return 0;

    const TransportMessage message = std::move(lane->front());
    lane->pop_front();
    bulkDepth_.store(inbox_[size_t(Lane::Bulk)].size(), std::memory_order_relaxed);
    from = message.from;
    const size_t take = std::min(cap, message.bytes.size());
    std::copy_n(message.bytes.begin(), take, buf);
    return int(take);
}

std::optional<deskhub::Fingerprint> SessionTransport::PeerFingerprint(
    const NetAddr& peer) const {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.PeerFingerprint(peer.Pack());
}

bool SessionTransport::Established(const NetAddr& peer) const {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.Established(peer.Pack());
}

QuicSendStats SessionTransport::SendStats() const {
    return endpoint_.SendStats();
}

size_t SessionTransport::MaxDatagramSize(const NetAddr& peer) const {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.MaxDatagramSize(peer.Pack());
}

bool SessionTransport::IsOpen() const {
    return endpoint_.IsOpen();
}

bool SessionTransport::lastBindAddrInUse() const {
    return endpoint_.LastBindAddrInUse();
}

uint16_t SessionTransport::LocalPort() const {
    return endpoint_.LocalPort();
}

}
