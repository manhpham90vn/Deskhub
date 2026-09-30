#include "deskhubp/net/SessionTransport.h"

#include <algorithm>
#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/AuthorizedKeysFile.h"

namespace deskhubp {

namespace {

constexpr uint32_t kEstablishPollMs = 5;
constexpr uint32_t kAuthPollMs = 2;
constexpr uint64_t kCloseBadFraming = 1;
constexpr uint64_t kCloseAuthRestarted = 2;
constexpr uint64_t kCloseDeviceForgotten = 3;
constexpr uint64_t kCloseAuthVersionMismatch = 4;
constexpr uint64_t kCloseAuthExpired = 5;
constexpr uint64_t kCloseAuthCapacity = 6;
constexpr uint64_t kCloseAuthRateLimited = 7;
constexpr uint64_t kAuthorizedCheckIntervalUs = 100'000;

constexpr deskhub::Fingerprint PairingGuessKey() {
    deskhub::Fingerprint key;
    for (uint8_t& byte : key.bytes) byte = 0xFF;
    return key;
}

constexpr deskhub::Fingerprint kPairingGuessKey = PairingGuessKey();

uint64_t StreamKey(QuicConnId conn, uint64_t stream) {
    return conn ^ (stream << 48);
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
        Deliver(NetAddr::Unpack(conn), bytes);
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
        ForgetPeerAuth(peer);
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
    authFailures_.Clear();
    pendingAuthDeadlines_.clear();
    authenticated_.clear();
    nextAuthorizedCheckUs_ = 0;
}

bool SessionTransport::SetRecvTimeout(uint32_t ms) {
    recvWaitMs_ = ms;
    return true;
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
    while (framer.Next(message)) Deliver(NetAddr::Unpack(conn), message);
    if (framer.Failed()) {
        LOGW("transport: %s sent a malformed record stream, closing it",
            NetAddr::Unpack(conn).ToString().c_str());
        endpoint_.CloseConnection(conn, kCloseBadFraming, "bad framing");
    }
}

void SessionTransport::Deliver(const NetAddr& from, std::span<const uint8_t> message) {
    if (message.empty()) return;

    if (clientAuthOn_ && IsAuthMessage(message)) {
        TransportMessage queued;
        queued.from = from;
        queued.bytes.assign(message.begin(), message.end());
        authInbox_.push_back(std::move(queued));
        return;
    }

    if (hostAuthOn_ && !HandleHostAuth(from, message)) return;

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
        if (hostAuth_.contains(key)) {
            LOGW(
                "transport: %s restarted authentication on a connection that already began "
                "it, closing the connection: one connection gets one handshake, so a settled "
                "identity cannot be swapped and a refused challenge cannot be retried in place",
                from.ToString().c_str());
            ForgetPeerAuth(from);
            endpoint_.CloseConnection(key, kCloseAuthRestarted, "auth restarted");
            return false;
        }
        const std::optional<deskhub::AuthStart> start = deskhub::ParseAuthStart(payload);
        if (!start) {
            deskhub::AuthResult mismatch;
            mismatch.code = deskhub::AuthResultCode::VersionMismatch;
            std::vector<uint8_t> out(deskhub::kMaxRecordSize);
            out.resize(deskhub::BuildAuthResult(out, mismatch));
            SendAuth(from, out);
            endpoint_.CloseConnection(key, kCloseAuthVersionMismatch, "auth version mismatch");
            return false;
        }

        const auto sessionId = endpoint_.ExportAuthSessionId(key);
        if (!sessionId) {
            endpoint_.CloseConnection(key, kCloseBadFraming, "auth session unavailable");
            return false;
        }
        if (!start->pairingToken.empty() &&
            !authFailures_.Allow(kPairingGuessKey, from.ip, NowUs())) {
            endpoint_.CloseConnection(key, kCloseAuthRateLimited, "pairing token guesses rate limited");
            return false;
        }
        auto auth = std::make_unique<HostAuth>();
        HostAuthConfig config = hostAuthConfig_;
        config.sessionId = *sessionId;
        config.peerAddress = from.ToString();
        auth->Configure(std::move(config));
        const std::optional<deskhub::AuthChallenge> challenge = auth->Begin(*start);
        if (!challenge) {
            endpoint_.CloseConnection(key, kCloseBadFraming, "unsupported client key");
            return false;
        }
        if (auth->PairingTokenRejected()) {
            LOGW("transport: %s presented a pairing token that did not match", from.ToString().c_str());
            authFailures_.RecordFailure(kPairingGuessKey, from.ip, NowUs());
        }
        if (challenge->mode == deskhub::AuthMode::Signature &&
            !authFailures_.Allow(auth->PeerFingerprint(), from.ip, NowUs())) {
            endpoint_.CloseConnection(key, kCloseAuthRateLimited, "auth attempts rate limited");
            return false;
        }
        if (challenge->mode == deskhub::AuthMode::Signature &&
            pendingAuthDeadlines_.size() >= kMaxPendingAuth) {
            endpoint_.CloseConnection(key, kCloseAuthCapacity, "too many pending auth requests");
            return false;
        }

        std::vector<uint8_t> out(deskhub::kMaxRecordSize);
        out.resize(deskhub::BuildAuthChallenge(out, *challenge));
        SendAuth(from, out);

        if (challenge->mode == deskhub::AuthMode::Denied && authCallbacks_.onRefused)
            authCallbacks_.onRefused(from, deskhub::AuthResultCode::NotPaired);
        if (challenge->mode == deskhub::AuthMode::AwaitingApproval && authCallbacks_.onRefused)
            authCallbacks_.onRefused(from, deskhub::AuthResultCode::AwaitingApproval);
        if (challenge->mode == deskhub::AuthMode::ConfigError && authCallbacks_.onRefused)
            authCallbacks_.onRefused(from, deskhub::AuthResultCode::ConfigError);
        if (challenge->mode == deskhub::AuthMode::Signature)
            pendingAuthDeadlines_[key] = NowUs() + kAuthResponseTimeoutUs;

        hostAuth_[key] = std::move(auth);
        return false;
    }

    if (header->type == deskhub::MsgType::AuthResponse) {
        const auto at = hostAuth_.find(key);
        if (at == hostAuth_.end()) return false;
        if (!authFailures_.Allow(at->second->PeerFingerprint(), from.ip, NowUs())) {
            ForgetPeerAuth(from);
            endpoint_.CloseConnection(key, kCloseAuthRateLimited, "auth attempts rate limited");
            return false;
        }
        const auto deadline = pendingAuthDeadlines_.find(key);
        if (deadline != pendingAuthDeadlines_.end() && NowUs() >= deadline->second) {
            ForgetPeerAuth(from);
            endpoint_.CloseConnection(key, kCloseAuthExpired, "auth response expired");
            return false;
        }
        const std::optional<deskhub::AuthResponse> response = deskhub::ParseAuthResponse(payload);
        if (!response) {
            endpoint_.CloseConnection(key, kCloseBadFraming, "invalid auth response");
            return false;
        }
        const deskhub::AuthResult result = at->second->Respond(*response);
        SettleHostAuth(from, *at->second, result);
        return false;
    }

    const auto settled = authenticated_.find(key);
    return settled != authenticated_.end() && settled->second;
}

void SessionTransport::SettleHostAuth(const NetAddr& peer, HostAuth& auth,
    const deskhub::AuthResult& result) {
    pendingAuthDeadlines_.erase(peer.Pack());
    std::vector<uint8_t> out(deskhub::kMaxRecordSize);
    out.resize(deskhub::BuildAuthResult(out, result));
    SendAuth(peer, out);

    const bool accepted = result.code == deskhub::AuthResultCode::Accepted;
    if (accepted)
        authFailures_.RecordSuccess(auth.PeerFingerprint(), peer.ip);
    else
        authFailures_.RecordFailure(auth.PeerFingerprint(), peer.ip, NowUs());
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

void SessionTransport::ForgetPeerAuth(const NetAddr& peer) {
    hostAuth_.erase(peer.Pack());
    pendingAuthDeadlines_.erase(peer.Pack());
    authenticated_.erase(peer.Pack());
}

void SessionTransport::ExpirePendingAuth(uint64_t nowUs) {
    std::vector<uint64_t> expired;
    for (const auto& [key, deadline] : pendingAuthDeadlines_)
        if (nowUs >= deadline) expired.push_back(key);
    for (const uint64_t key : expired) {
        ForgetPeerAuth(NetAddr::Unpack(key));
        endpoint_.CloseConnection(key, kCloseAuthExpired, "auth response expired");
    }
}

void SessionTransport::DropQueuedFrom(const NetAddr& peer) {
    for (std::deque<TransportMessage>& lane : inbox_)
        std::erase_if(lane, [&](const TransportMessage& m) { return m.from == peer; });
    bulkDepth_.store(inbox_[size_t(Lane::Bulk)].size(), std::memory_order_relaxed);
}

void SessionTransport::RevokeForgottenPeers() {
    if (!hostAuthOn_) return;
    const uint64_t generation = AuthorizedKeysGeneration();
    const uint64_t nowUs = NowUs();
    if (generation == authorizedGenerationSeen_ && nowUs < nextAuthorizedCheckUs_) return;
    authorizedGenerationSeen_ = generation;
    nextAuthorizedCheckUs_ = nowUs + kAuthorizedCheckIntervalUs;

    std::vector<NetAddr> revoked;
    for (const auto& [key, admitted] : authenticated_) {
        if (!admitted) continue;
        const auto at = hostAuth_.find(key);
        if (at == hostAuth_.end()) continue;
        if (IsClientKeyAuthorized(at->second->PeerPublicKey()))
            continue;
        revoked.push_back(NetAddr::Unpack(key));
    }
    for (const NetAddr& peer : revoked) {
        LOGW("transport: %s was forgotten on the Devices page, closing its connection",
            peer.ToString().c_str());
        ForgetPeerAuth(peer);
        DropQueuedFrom(peer);
        endpoint_.CloseConnection(peer.Pack(), kCloseDeviceForgotten, "device forgotten");
    }
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
    if (hostAuthOn_) {
        const auto admission = authenticated_.find(to.Pack());
        if (admission == authenticated_.end() || !admission->second) return false;
    }
    if (!endpoint_.Established(to.Pack())) return false;
    return SendReliable(to, streamId, message);
}

bool SessionTransport::SendKeepalive(const NetAddr& peer) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    return endpoint_.SendKeepalive(peer.Pack());
}

bool SessionTransport::RunClientAuth(const NetAddr& server, ClientAuthConfig config,
    uint32_t timeoutMs, deskhub::AuthResultCode& outCode,
    const std::atomic<bool>* cancel) {
    outCode = deskhub::AuthResultCode::NotPaired;
    authInbox_.clear();

    {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        const auto sessionId = endpoint_.ExportAuthSessionId(server.Pack());
        if (!sessionId) {
            outCode = deskhub::AuthResultCode::Refused;
            return false;
        }
        config.sessionId = *sessionId;
    }

    ClientAuth client;
    client.Configure(std::move(config));

    std::vector<uint8_t> out(deskhub::kMaxRecordSize);
    out.resize(deskhub::BuildAuthStart(out, client.Begin()));
    if (out.empty()) return false;
    clientAuthOn_ = true;
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
        if (message.from != server) continue;
        const std::optional<deskhub::CommonHeader> header =
            deskhub::ParseCommonHeader(message.bytes);
        if (!header) {
            outCode = deskhub::AuthResultCode::Refused;
            break;
        }
        const std::span<const uint8_t> payload = deskhub::PayloadOf(message.bytes);

        if (header->type == deskhub::MsgType::AuthChallenge) {
            if (answered) {
                outCode = deskhub::AuthResultCode::Refused;
                break;
            }
            const std::optional<deskhub::AuthChallenge> challenge =
                deskhub::ParseAuthChallenge(payload);
            if (!challenge) {
                outCode = payload.empty() || payload[0] != deskhub::kAuthVersion
                              ? deskhub::AuthResultCode::VersionMismatch
                              : deskhub::AuthResultCode::Refused;
                clientAuthOn_ = false;
                const std::lock_guard<std::mutex> lock(sendMutex_);
                endpoint_.CloseConnection(server.Pack(), kCloseAuthVersionMismatch,
                    "auth challenge rejected");
                return false;
            }
            if (challenge->mode == deskhub::AuthMode::Denied) {
                outCode = deskhub::AuthResultCode::NotPaired;
                clientAuthOn_ = false;
                return false;
            }
            if (challenge->mode == deskhub::AuthMode::ConfigError) {
                outCode = deskhub::AuthResultCode::ConfigError;
                clientAuthOn_ = false;
                return false;
            }
            if (challenge->mode == deskhub::AuthMode::AwaitingApproval) {
                outCode = deskhub::AuthResultCode::AwaitingApproval;
                clientAuthOn_ = false;
                return false;
            }
            if (challenge->mode != deskhub::AuthMode::Signature) {
                outCode = deskhub::AuthResultCode::NotPaired;
                clientAuthOn_ = false;
                return false;
            }
            const std::optional<deskhub::AuthResponse> response = client.Answer(*challenge);
            if (!response) {
                outCode = deskhub::AuthResultCode::LocalKeyUnavailable;
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
            if (!result) {
                outCode = deskhub::AuthResultCode::VersionMismatch;
                clientAuthOn_ = false;
                const std::lock_guard<std::mutex> lock(sendMutex_);
                endpoint_.CloseConnection(server.Pack(), kCloseAuthVersionMismatch,
                    "auth result rejected");
                return false;
            }
            if (!answered && result->code != deskhub::AuthResultCode::VersionMismatch) {
                outCode = deskhub::AuthResultCode::Refused;
                break;
            }
            outCode = result->code;
            clientAuthOn_ = false;
            return result->code == deskhub::AuthResultCode::Accepted;
        }

        outCode = deskhub::AuthResultCode::Refused;
        break;
    }

    if (outCode == deskhub::AuthResultCode::NotPaired)
        outCode = deskhub::AuthResultCode::TimedOut;
    clientAuthOn_ = false;
    if (outCode == deskhub::AuthResultCode::Refused) {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        endpoint_.CloseConnection(server.Pack(), kCloseBadFraming, "auth message out of order");
    }
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
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (hostAuthOn_) {
        const auto admission = authenticated_.find(to.Pack());
        if (admission == authenticated_.end() || !admission->second) return false;
    }
    if (CarriesVideo(message)) return endpoint_.SendDatagram(to.Pack(), message);
    if (!endpoint_.Established(to.Pack())) return endpoint_.SendRaw(to, message);
    if (CarriesAudio(message)) return endpoint_.SendDatagram(to.Pack(), message);
    return SendReliable(to, kQuicControlStream, message);
}

int SessionTransport::RecvFrom(uint8_t* buf, size_t cap, NetAddr& from) {
    if (!endpoint_.IsOpen()) return -1;
    {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        RevokeForgottenPeers();
        ExpirePendingAuth(NowUs());
    }
    if (!AnythingServable()) {
        endpoint_.WaitReadable(recvWaitMs_);
        const std::lock_guard<std::mutex> lock(sendMutex_);
        ExpirePendingAuth(NowUs());
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
