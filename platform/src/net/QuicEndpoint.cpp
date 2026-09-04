#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "deskhubp/net/QuicEndpoint.h"

#include "deskhub/protocol/RecordStream.h"

#include <quiche.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "deskhub/protocol/Wire.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/Random.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace deskhubp {

namespace {

constexpr size_t kConnIdLen = 16;
constexpr size_t kStreamChunk = deskhub::kMaxRecordBacklog;
constexpr size_t kStreamReadBudgetPerService = 64u << 10;
constexpr uint64_t kInitialMaxData = 4u << 20;
constexpr uint64_t kInitialMaxStreamData = 1u << 20;
constexpr uint64_t kMaxStreams = 64;
constexpr uint64_t kDropLogIntervalUs = 1'000'000;
constexpr uint64_t kQuietWarnStepUs = 5'000'000;
constexpr uint64_t kPollGapWarnUs = 250'000;
constexpr size_t kDatagramQueue = 512;
constexpr size_t kMaxStreamOutbox = 4u << 20;
constexpr size_t kMaxBulkStreamOutbox = 256u << 10;
constexpr size_t kOutboxCompactAt = 64u << 10;

bool IsBulkStream(uint64_t streamId) {
    return streamId == kQuicFileStream;
}

size_t OutboxCapFor(uint64_t streamId) {
    return IsBulkStream(streamId) ? kMaxBulkStreamOutbox : kMaxStreamOutbox;
}

uint8_t UrgencyFor(uint64_t streamId) {
    return IsBulkStream(streamId) ? kQuicUrgencyBulk : kQuicUrgencyInteractive;
}

sockaddr_in ToSockAddr(const NetAddr& addr) {
    sockaddr_in out{};
    out.sin_family = AF_INET;
    out.sin_port = htons(addr.port);
    out.sin_addr.s_addr = htonl(addr.ip);
    return out;
}

quiche_cc_algorithm QuicheAlgorithmOf(QuicCongestionControl choice) {
    switch (choice) {
        case QuicCongestionControl::Reno: return QUICHE_CC_RENO;
        case QuicCongestionControl::Cubic: return QUICHE_CC_CUBIC;
        case QuicCongestionControl::Bbr2: return QUICHE_CC_BBR2_GCONGESTION;
    }
    return QUICHE_CC_CUBIC;
}

quiche_config* MakeConfig(const QuicSettings& settings, bool server) {
    quiche_config* cfg = quiche_config_new(QUICHE_PROTOCOL_VERSION);
    if (cfg == nullptr) return nullptr;

    if (server) {
        if (quiche_config_load_cert_chain_from_pem_file(cfg, settings.certPemPath.c_str()) < 0 ||
            quiche_config_load_priv_key_from_pem_file(cfg, settings.keyPemPath.c_str()) < 0) {
            LOGE("quic: could not load the host certificate or key");
            quiche_config_free(cfg);
            return nullptr;
        }
    }
    quiche_config_verify_peer(cfg, settings.verifyPeer);

    std::vector<uint8_t> alpn;
    alpn.push_back(uint8_t(settings.alpn.size()));
    alpn.insert(alpn.end(), settings.alpn.begin(), settings.alpn.end());
    quiche_config_set_application_protos(cfg, alpn.data(), alpn.size());

    quiche_config_set_max_idle_timeout(cfg, settings.idleTimeoutMs);
    quiche_config_set_max_recv_udp_payload_size(cfg, settings.maxUdpPayload);
    quiche_config_set_max_send_udp_payload_size(cfg, settings.maxUdpPayload);
    quiche_config_set_initial_max_data(cfg, kInitialMaxData);
    quiche_config_set_initial_max_stream_data_bidi_local(cfg, kInitialMaxStreamData);
    quiche_config_set_initial_max_stream_data_bidi_remote(cfg, kInitialMaxStreamData);
    quiche_config_set_initial_max_stream_data_uni(cfg, kInitialMaxStreamData);
    quiche_config_set_initial_max_streams_bidi(cfg, kMaxStreams);
    quiche_config_set_initial_max_streams_uni(cfg, kMaxStreams);
    quiche_config_set_cc_algorithm(cfg, QuicheAlgorithmOf(settings.congestionControl));
    quiche_config_enable_dgram(cfg, true, kDatagramQueue, kDatagramQueue);
    return cfg;
}

struct StreamListShape {
    const uint64_t* data = nullptr;
    size_t size = 0;
    size_t capacity = 0;
    bool operator==(const StreamListShape&) const = default;
};

StreamListShape ShapeOf(const std::vector<uint64_t>& list) {
    return {list.data(), list.size(), list.capacity()};
}

void ReportStreamListOverwritten(const char* checkpoint, const StreamListShape& was,
    const StreamListShape& now, uint64_t stream, ssize_t got, size_t budget) {
    LOGE(
        "quic: the readable-stream list on the recv thread's stack changed %s (stream %llu, "
        "got %zd, budget %zu); it was data=%p size=%zu capacity=%zu and is now data=%p "
        "size=%zu capacity=%zu. Nothing between those two points writes to that vector, so "
        "something reached into this thread's stack frame through a pointer it should no "
        "longer hold - the checkpoint names which call it came from",
        checkpoint, static_cast<unsigned long long>(stream), got, budget,
        static_cast<const void*>(was.data), was.size, was.capacity,
        static_cast<const void*>(now.data), now.size, now.capacity);
}

void FillRandomConnId(uint8_t* out, size_t len) {
    if (RandomBytes(out, len)) return;
    for (size_t i = 0; i < len; ++i) out[i] = uint8_t(i * 31 + 7);
}

}

struct QuicEndpoint::Impl {
    struct Counters {
        std::atomic<uint64_t> packets{0};
        std::atomic<uint64_t> bursts{0};
        std::atomic<uint64_t> maxBurst{0};
        std::atomic<uint64_t> capped{0};
        std::atomic<uint64_t> datagrams{0};
        std::atomic<uint64_t> datagramsRefused{0};
        std::atomic<uint64_t> streamBytes{0};

        static void Bump(std::atomic<uint64_t>& counter, uint64_t by) {
            counter.store(counter.load(std::memory_order_relaxed) + by, std::memory_order_relaxed);
        }

        static void Raise(std::atomic<uint64_t>& counter, uint64_t to) {
            if (to > counter.load(std::memory_order_relaxed))
                counter.store(to, std::memory_order_relaxed);
        }
    };

    struct StreamOutbox {
        std::vector<uint8_t> bytes{};
        size_t at = 0;
        bool fin = false;
        bool primed = false;
        bool broken = false;

        size_t Pending() const {
            return bytes.size() - at;
        }
    };

    struct Connection {
        quiche_conn* conn = nullptr;
        NetAddr peer{};
        bool announced = false;
        uint64_t lastRecvUs = 0;
        uint64_t quietSteps = 0;
        std::map<uint64_t, StreamOutbox> outbox{};
    };

    ~Impl() {
        try {
            Shutdown();
        } catch (...) {
            connections_.clear();
        }
    }

    void Shutdown() {
        for (auto& [key, entry] : connections_) {
            if (entry.conn == nullptr) continue;
            SayGoodbye(entry);
            quiche_conn_free(entry.conn);
        }
        connections_.clear();
        socket_.Close();
        if (config_ != nullptr) {
            quiche_config_free(config_);
            config_ = nullptr;
        }
        open_ = false;
    }

    bool Start(const QuicSettings& settings, const std::string& bindIp, uint16_t port,
        bool server, QuicCallbacks callbacks) {
        Shutdown();
        settings_ = settings;
        datagramChunk_.assign(settings_.maxUdpPayload, 0);
        cb_ = std::move(callbacks);
        server_ = server;
        config_ = MakeConfig(settings_, server);
        if (config_ == nullptr) return false;
        if (!socket_.Open(port, bindIp)) {
            bindAddrInUse_ = socket_.lastBindAddrInUse();
            LOGE("quic: could not bind UDP port %u%s", unsigned(port),
                bindAddrInUse_ ? " - something else is already listening on it" : "");
            quiche_config_free(config_);
            config_ = nullptr;
            return false;
        }
        socket_.SetRecvTimeout(1);
        SizeReadBuffers(socket_.EnableReceiveCoalescing());
        localPort_ = socket_.LocalPort();
        localIp_ = bindIp;
        open_ = true;
        return true;
    }

    bool Dial(const NetAddr& server, std::string_view serverName) {
        const sockaddr_in local = LocalSockAddr();
        const sockaddr_in peer = ToSockAddr(server);
        uint8_t scid[kConnIdLen];
        FillRandomConnId(scid, sizeof(scid));

        const std::string name(serverName);
        quiche_conn* conn = quiche_connect(name.empty() ? nullptr : name.c_str(), scid,
            sizeof(scid), reinterpret_cast<const sockaddr*>(&local), sizeof(local),
            reinterpret_cast<const sockaddr*>(&peer), sizeof(peer), config_);
        if (conn == nullptr) {
            LOGE("quic: could not start a connection to %s", server.ToString().c_str());
            return false;
        }
        Connection entry;
        entry.conn = conn;
        entry.peer = server;
        connections_.emplace(server.Pack(), entry);
        Flush(connections_[server.Pack()]);
        return true;
    }

    sockaddr_in LocalSockAddr() const {
        NetAddr local{0, localPort_};
        if (!localIp_.empty()) ParseNetAddr(localIp_ + ":0", local);
        local.port = localPort_;
        return ToSockAddr(local);
    }

    Connection* Lookup(QuicConnId id) {
        const auto at = connections_.find(id);
        return at == connections_.end() ? nullptr : &at->second;
    }

    const Connection* Lookup(QuicConnId id) const {
        const auto at = connections_.find(id);
        return at == connections_.end() ? nullptr : &at->second;
    }

    bool QueueStream(Connection& entry, uint64_t streamId, std::span<const uint8_t> bytes,
        bool fin) {
        StreamOutbox& box = entry.outbox[streamId];
        if (box.broken) return false;
        if (box.Pending() + bytes.size() > OutboxCapFor(streamId)) return false;
        if (!box.primed) {
            quiche_conn_stream_priority(entry.conn, streamId, UrgencyFor(streamId), true);
            box.primed = true;
        }
        box.bytes.insert(box.bytes.end(), bytes.begin(), bytes.end());
        if (fin) box.fin = true;
        Counters::Bump(stats_.streamBytes, bytes.size());
        DrainOutbox(entry, streamId, box);
        return true;
    }

    void BreakStream(Connection& entry, uint64_t streamId, StreamOutbox& box, ssize_t code) {
        LOGE(
            "quic: stream %llu stopped taking bytes (%zd) with %zu still queued; resetting it "
            "rather than truncating a half-written record into the peer's framer",
            (unsigned long long)(streamId), code, box.Pending());
        quiche_conn_stream_shutdown(entry.conn, streamId, QUICHE_SHUTDOWN_WRITE, 0);
        box.broken = true;
        box.bytes.clear();
        box.at = 0;
        box.fin = false;
        if (cb_.onStreamBroken) cb_.onStreamBroken(entry.peer.Pack(), streamId);
    }

    void DrainOutbox(Connection& entry, uint64_t streamId, StreamOutbox& box) {
        while (box.at < box.bytes.size()) {
            uint64_t err = 0;
            const size_t left = box.bytes.size() - box.at;
            const ssize_t written = quiche_conn_stream_send(entry.conn, streamId,
                box.bytes.data() + box.at, left, box.fin, &err);
            if (written == 0 || written == QUICHE_ERR_DONE) break;
            if (written < 0) {
                BreakStream(entry, streamId, box, written);
                return;
            }
            box.at += size_t(written);
        }
        if (box.at >= box.bytes.size()) {
            box.bytes.clear();
            box.at = 0;
            box.fin = false;
        } else if (box.at >= kOutboxCompactAt) {
            box.bytes.erase(box.bytes.begin(), box.bytes.begin() + std::ptrdiff_t(box.at));
            box.at = 0;
        }
    }

    void DrainOutboxes(Connection& entry) {
        for (auto& [streamId, box] : entry.outbox)
            if (box.Pending() > 0 && !box.broken) DrainOutbox(entry, streamId, box);
    }

    void Flush(Connection& entry) {
        static_assert(kMaxFlushBurst <= kMaxSendBatch);
        uint8_t out[kMaxFlushBurst][kQuicMaxUdpPayload];
        OutboundDatagram batch[kMaxFlushBurst];
        size_t burst = 0;
        for (;;) {
            if (burst >= kMaxFlushBurst) {
                moreToSend_.store(true, std::memory_order_relaxed);
                Counters::Bump(stats_.capped, 1);
                break;
            }
            quiche_send_info info{};
            const ssize_t written =
                quiche_conn_send(entry.conn, out[burst], kQuicMaxUdpPayload, &info);
            if (written == QUICHE_ERR_DONE) break;
            if (written < 0) {
                LOGW("quic: send failed (%zd)", written);
                break;
            }
            batch[burst] = OutboundDatagram{out[burst], size_t(written)};
            ++burst;
        }
        if (burst == 0) return;
        if (socket_.SendBatch(entry.peer, std::span<const OutboundDatagram>(batch, burst)) < burst)
            ReportSendFail(entry.peer);
        Counters::Bump(stats_.bursts, 1);
        Counters::Bump(stats_.packets, burst);
        Counters::Raise(stats_.maxBurst, burst);
    }

    void SayGoodbye(Connection& entry) {
        if (!open_ || quiche_conn_is_closed(entry.conn)) return;
        constexpr std::string_view kReason = "bye";
        quiche_conn_close(entry.conn, true, 0,
            reinterpret_cast<const uint8_t*>(kReason.data()), kReason.size());
        Flush(entry);
    }

    void Accept(const NetAddr& from, std::span<const uint8_t> packet) {
        uint32_t version = 0;
        uint8_t type = 0;
        uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
        uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
        uint8_t token[256];
        size_t scidLen = sizeof(scid);
        size_t dcidLen = sizeof(dcid);
        size_t tokenLen = sizeof(token);
        if (quiche_header_info(packet.data(), packet.size(), kConnIdLen, &version, &type, scid,
                &scidLen, dcid, &dcidLen, token, &tokenLen) < 0)
            return;

        const sockaddr_in local = LocalSockAddr();
        const sockaddr_in peer = ToSockAddr(from);
        uint8_t ourScid[kConnIdLen];
        FillRandomConnId(ourScid, sizeof(ourScid));
        quiche_conn* conn = quiche_accept(ourScid, sizeof(ourScid), nullptr, 0,
            reinterpret_cast<const sockaddr*>(&local), sizeof(local),
            reinterpret_cast<const sockaddr*>(&peer), sizeof(peer), config_);
        if (conn == nullptr) return;

        Connection entry;
        entry.conn = conn;
        entry.peer = from;
        connections_.emplace(from.Pack(), entry);
    }

    void Receive(const NetAddr& from, std::span<const uint8_t> packet) {
        if (deskhub::ClassifyPacket(packet) != deskhub::PacketKind::Quic) {
            if (cb_.onForeignDatagram) cb_.onForeignDatagram(from, packet);
            return;
        }

        Connection* entry = Lookup(from.Pack());
        if (entry == nullptr) {
            if (!server_) return;
            if (connections_.size() >= kMaxConnections) return;
            ReportStranger(from, packet);
            Accept(from, packet);
            entry = Lookup(from.Pack());
            if (entry == nullptr) return;
        }

        const sockaddr_in local = LocalSockAddr();
        const sockaddr_in peer = ToSockAddr(from);
        quiche_recv_info info{};
        info.from = reinterpret_cast<sockaddr*>(const_cast<sockaddr_in*>(&peer));
        info.from_len = sizeof(peer);
        info.to = reinterpret_cast<sockaddr*>(const_cast<sockaddr_in*>(&local));
        info.to_len = sizeof(local);

        std::vector<uint8_t> copy(packet.begin(), packet.end());
        const ssize_t read = quiche_conn_recv(entry->conn, copy.data(), copy.size(), &info);
        if (read < 0) {
            ReportDrop(from, read);
            return;
        }
        entry->lastRecvUs = NowUs();
        entry->quietSteps = 0;
    }

    void ReportSendFail(const NetAddr& peer) {
        ++sendFails_;
        const uint64_t nowUs = NowUs();
        if (nowUs - lastSendFailLogUs_ < kDropLogIntervalUs) return;
        lastSendFailLogUs_ = nowUs;
        LOGW(
            "quic: the socket refused %llu packet(s) for %s; quiche counts them as sent, so "
            "every one of them has to be recovered as a loss",
            static_cast<unsigned long long>(sendFails_), peer.ToString().c_str());
        sendFails_ = 0;
    }

    void ReportStranger(const NetAddr& from, std::span<const uint8_t> packet) {
        constexpr uint8_t kLongHeaderBit = 0x80;
        if (connections_.empty()) return;
        if (packet.empty() || (packet[0] & kLongHeaderBit) != 0) return;
        const uint64_t nowUs = NowUs();
        if (nowUs - lastStrangerLogUs_ < kDropLogIntervalUs) return;
        lastStrangerLogUs_ = nowUs;
        LOGW(
            "quic: a packet came from %s, which matches none of the %zu open connection(s); "
            "connections are keyed by address, so a peer whose port moved cannot be followed "
            "and its old link will time out instead",
            from.ToString().c_str(), connections_.size());
    }

    void ReportQuiet(Connection& entry, uint64_t nowUs) {
        if (entry.lastRecvUs == 0) return;
        if (nowUs <= entry.lastRecvUs) return;
        const uint64_t quietUs = nowUs - entry.lastRecvUs;
        const uint64_t steps = quietUs / kQuietWarnStepUs;
        if (steps <= entry.quietSteps) return;
        entry.quietSteps = steps;
        LOGW("quic: %s has sent nothing for %llu s; the link dies at %llu s of silence",
            entry.peer.ToString().c_str(), static_cast<unsigned long long>(quietUs / 1'000'000),
            static_cast<unsigned long long>(settings_.idleTimeoutMs / 1000));
    }

    void ReportDrop(const NetAddr& from, ssize_t code) {
        ++drops_;
        const uint64_t nowUs = NowUs();
        if (nowUs - lastDropLogUs_ < kDropLogIntervalUs) return;
        lastDropLogUs_ = nowUs;
        LOGW("quic: dropped %llu unusable packet(s), last from %s (%zd)",
            static_cast<unsigned long long>(drops_), from.ToString().c_str(), code);
        drops_ = 0;
    }

    void DrainStreams(QuicConnId id, Connection& entry) {
        quiche_stream_iter* it = quiche_conn_readable(entry.conn);
        if (it == nullptr) return;
        uint64_t streamId = 0;
        std::vector<uint64_t> ready;
        while (quiche_stream_iter_next(it, &streamId)) ready.push_back(streamId);
        quiche_stream_iter_free(it);
        if (ready.empty()) return;

        std::vector<uint8_t>& chunk = streamChunk_;
        size_t budget = kStreamReadBudgetPerService;
        const StreamListShape listed = ShapeOf(ready);
        const auto listStillIntact = [&](const char* checkpoint, uint64_t stream, ssize_t got) {
            const StreamListShape now = ShapeOf(ready);
            if (now == listed) return true;
            ReportStreamListOverwritten(checkpoint, listed, now, stream, got, budget);
            return false;
        };
        for (uint64_t stream : ready) {
            if (!listStillIntact("between two streams of the same connection", stream, 0)) return;
            for (;;) {
                if (budget == 0) {
                    moreToRead_.store(true, std::memory_order_relaxed);
                    return;
                }
                const bool paused = cb_.pauseStream && cb_.pauseStream(stream);
                if (!listStillIntact("inside the pauseStream callback", stream, 0)) return;
                if (paused) break;
                bool fin = false;
                uint64_t err = 0;
                const size_t want = std::min(chunk.size(), budget);
                const ssize_t got = quiche_conn_stream_recv(entry.conn, stream, chunk.data(),
                    want, &fin, &err);
                if (!listStillIntact("inside quiche_conn_stream_recv", stream, got)) return;
                if (got < 0) break;
                budget -= size_t(got);
                if (cb_.onStream) {
                    cb_.onStream(id, stream, std::span<const uint8_t>(chunk.data(), size_t(got)),
                        fin);
                    if (!listStillIntact("inside the onStream callback", stream, got)) return;
                    if (Lookup(id) != &entry) return;
                }
                if (fin || size_t(got) < want) break;
            }
        }
    }

    void DrainDatagrams(QuicConnId id, Connection& entry) {
        std::vector<uint8_t>& chunk = datagramChunk_;
        for (;;) {
            const ssize_t got = quiche_conn_dgram_recv(entry.conn, chunk.data(), chunk.size());
            if (got < 0) return;
            if (cb_.onDatagram) {
                cb_.onDatagram(id, std::span<const uint8_t>(chunk.data(), size_t(got)));
                if (Lookup(id) != &entry) return;
            }
        }
    }

    void ReportClose(const Connection& entry) {
        quiche_stats stats{};
        quiche_conn_stats(entry.conn, &stats);
        const std::string peer = entry.peer.ToString();

        bool app = false;
        uint64_t code = 0;
        const uint8_t* reason = nullptr;
        size_t reasonLen = 0;

        if (quiche_conn_is_timed_out(entry.conn)) {
            LOGW(
                "quic: %s answered nothing for the whole %llu ms idle timeout, so the link is "
                "gone; nobody refused anything, the peer just went quiet (recv %zu pkt/%llu B, "
                "sent %zu pkt/%llu B, lost %zu)",
                peer.c_str(), static_cast<unsigned long long>(settings_.idleTimeoutMs),
                stats.recv, static_cast<unsigned long long>(stats.recv_bytes), stats.sent,
                static_cast<unsigned long long>(stats.sent_bytes), stats.lost);
            return;
        }
        if (quiche_conn_peer_error(entry.conn, &app, &code, &reason, &reasonLen)) {
            LOGW("quic: %s closed the link from its own end (%s error %llu: %.*s)", peer.c_str(),
                app ? "application" : "transport", static_cast<unsigned long long>(code),
                int(reasonLen), reason == nullptr ? "" : reinterpret_cast<const char*>(reason));
            return;
        }
        if (quiche_conn_local_error(entry.conn, &app, &code, &reason, &reasonLen)) {
            LOGW("quic: this host closed the link to %s (%s error %llu: %.*s)", peer.c_str(),
                app ? "application" : "transport", static_cast<unsigned long long>(code),
                int(reasonLen), reason == nullptr ? "" : reinterpret_cast<const char*>(reason));
            return;
        }
        LOGI("quic: %s left (recv %zu pkt, sent %zu pkt, lost %zu)", peer.c_str(), stats.recv,
            stats.sent, stats.lost);
    }

    void Service() {
        moreToSend_.store(false, std::memory_order_relaxed);
        moreToRead_.store(false, std::memory_order_relaxed);
        const uint64_t nowUs = NowUs();
        std::array<QuicConnId, kMaxConnections> live{};
        std::array<QuicConnId, kMaxConnections> dead{};
        size_t liveCount = 0;
        size_t deadCount = 0;
        for (const auto& connection : connections_) {
            if (liveCount == live.size()) break;
            live[liveCount++] = connection.first;
        }
        for (size_t at = 0; at < liveCount; ++at) {
            const QuicConnId id = live[at];
            Connection* found = Lookup(id);
            if (found == nullptr) continue;
            Connection& entry = *found;
            if (quiche_conn_is_established(entry.conn) && !entry.announced) {
                entry.announced = true;
                if (entry.lastRecvUs == 0) entry.lastRecvUs = nowUs;
                if (cb_.onConnected) cb_.onConnected(id, entry.peer);
                if (Lookup(id) != &entry) continue;
            }
            if (entry.announced) ReportQuiet(entry, nowUs);
            if (entry.announced) {
                DrainStreams(id, entry);
                DrainDatagrams(id, entry);
                if (Lookup(id) != &entry) continue;
            }
            DrainOutboxes(entry);
            Flush(entry);
            if (quiche_conn_is_closed(entry.conn) && deadCount < dead.size())
                dead[deadCount++] = id;
        }
        for (size_t gone = 0; gone < deadCount; ++gone) {
            const QuicConnId id = dead[gone];
            const auto at = connections_.find(id);
            if (at == connections_.end()) continue;
            const NetAddr peer = at->second.peer;
            const bool announced = at->second.announced;
            if (announced) ReportClose(at->second);
            quiche_conn* conn = at->second.conn;
            connections_.erase(at);
            if (announced && cb_.onClosed) cb_.onClosed(id, peer);
            quiche_conn_free(conn);
        }
    }

    void ReportPollGap() {
        const uint64_t nowUs = NowUs();
        const uint64_t sinceUs = lastPollUs_ == 0 ? 0 : nowUs - lastPollUs_;
        lastPollUs_ = nowUs;
        if (sinceUs <= kPollGapWarnUs || connections_.empty()) return;
        LOGW(
            "quic: %llu ms went by without reading the socket, so nothing was acked or timed "
            "out in that window",
            static_cast<unsigned long long>(sinceUs / 1000));
    }

    bool Backlogged() const {
        return moreToSend_.load(std::memory_order_relaxed) ||
               moreToRead_.load(std::memory_order_relaxed);
    }

    bool WaitReadable(uint32_t waitMs) {
        if (!open_) return false;
        return socket_.WaitReadable(Backlogged() ? 0 : waitMs);
    }

    void SizeReadBuffers(bool coalescing) {
        readStride_ = coalescing ? kMaxCoalescedBytes : kQuicMaxUdpPayload;
        const size_t slots = coalescing ? kCoalescedReadBurst : kReadBurst;
        readBuf_.assign(slots * readStride_, 0);
        readSlots_.assign(slots, InboundDatagram{});
    }

    void Poll(uint32_t waitMs) {
        if (!open_) return;
        ReportPollGap();
        socket_.SetRecvTimeout(Backlogged() ? 0 : waitMs);
        const size_t burst = readSlots_.size();
        for (int round = 0; round < kReadRoundsPerPoll; ++round) {
            for (size_t i = 0; i < burst; ++i)
                readSlots_[i] = InboundDatagram{readBuf_.data() + i * readStride_, readStride_, 0,
                    0, NetAddr{}};
            const int got = socket_.RecvBatch(std::span<InboundDatagram>(readSlots_));
            if (got <= 0) break;
            for (int i = 0; i < got; ++i) {
                const InboundDatagram& slot = readSlots_[size_t(i)];
                const size_t count = DatagramsIn(slot);
                for (size_t part = 0; part < count; ++part)
                    Receive(slot.from, DatagramAt(slot, part));
            }
            if (size_t(got) < burst) break;
        }
        for (auto& [id, entry] : connections_) {
            if (quiche_conn_timeout_as_millis(entry.conn) == 0) quiche_conn_on_timeout(entry.conn);
        }
        Service();
    }

    static constexpr size_t kMaxConnections = 32;
    static constexpr size_t kReadBurst = kMaxRecvBatch;
    static constexpr size_t kCoalescedReadBurst = 4;
    static constexpr int kReadRoundsPerPoll = 16;
    static_assert(kReadBurst <= kMaxRecvBatch && kCoalescedReadBurst <= kMaxRecvBatch);

    UdpSocket socket_{};
    quiche_config* config_ = nullptr;
    QuicSettings settings_{};
    QuicCallbacks cb_{};
    std::unordered_map<QuicConnId, Connection> connections_{};
    uint16_t localPort_ = 0;
    std::string localIp_{};
    bool bindAddrInUse_ = false;
    bool server_ = false;
    bool open_ = false;
    uint64_t drops_ = 0;
    uint64_t lastDropLogUs_ = 0;
    uint64_t sendFails_ = 0;
    uint64_t lastSendFailLogUs_ = 0;
    uint64_t lastStrangerLogUs_ = 0;
    uint64_t lastPollUs_ = 0;
    size_t readStride_ = kQuicMaxUdpPayload;
    std::vector<uint8_t> readBuf_{};
    std::vector<InboundDatagram> readSlots_{};
    std::vector<uint8_t> streamChunk_ = std::vector<uint8_t>(kStreamChunk);
    std::vector<uint8_t> datagramChunk_ = std::vector<uint8_t>(kQuicMaxUdpPayload);
    std::atomic<bool> moreToSend_{false};
    std::atomic<bool> moreToRead_{false};
    Counters stats_{};
};

QuicEndpoint::QuicEndpoint() : impl_(std::make_unique<Impl>()) {
}

QuicEndpoint::~QuicEndpoint() = default;

bool QuicEndpoint::Listen(const QuicSettings& settings, const std::string& bindIp, uint16_t port,
    QuicCallbacks callbacks) {
    return impl_->Start(settings, bindIp, port, true, std::move(callbacks));
}

bool QuicEndpoint::Connect(const QuicSettings& settings, const NetAddr& server,
    std::string_view serverName, QuicCallbacks callbacks) {
    if (!impl_->Start(settings, {}, 0, false, std::move(callbacks))) return false;
    return impl_->Dial(server, serverName);
}

void QuicEndpoint::Poll(uint64_t, uint32_t waitMs) {
    impl_->Poll(waitMs);
}

bool QuicEndpoint::WaitReadable(uint32_t waitMs) {
    return impl_->WaitReadable(waitMs);
}

bool QuicEndpoint::SendStream(QuicConnId conn, uint64_t streamId, std::span<const uint8_t> bytes,
    bool fin) {
    Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr || !quiche_conn_is_established(entry->conn)) return false;
    if (bytes.empty()) return true;
    const bool queued = impl_->QueueStream(*entry, streamId, bytes, fin);
    entry = impl_->Lookup(conn);
    if (entry == nullptr) return false;
    if (!queued) return false;
    impl_->Flush(*entry);
    return true;
}

bool QuicEndpoint::SendDatagram(QuicConnId conn, std::span<const uint8_t> bytes) {
    Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr || !quiche_conn_is_established(entry->conn)) return false;
    const ssize_t sent = quiche_conn_dgram_send(entry->conn, bytes.data(), bytes.size());
    if (sent > 0)
        Impl::Counters::Bump(impl_->stats_.datagrams, 1);
    else
        Impl::Counters::Bump(impl_->stats_.datagramsRefused, 1);
    impl_->Flush(*entry);
    return sent > 0;
}

bool QuicEndpoint::SendKeepalive(QuicConnId conn) {
    Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr || !quiche_conn_is_established(entry->conn)) return false;
    const ssize_t queued = quiche_conn_send_ack_eliciting(entry->conn);
    impl_->Flush(*entry);
    return queued >= 0;
}

bool QuicEndpoint::SendRaw(const NetAddr& to, std::span<const uint8_t> bytes) {
    return impl_->socket_.SendTo(to, bytes.data(), bytes.size());
}

QuicSendStats QuicEndpoint::SendStats() const {
    const Impl::Counters& c = impl_->stats_;
    QuicSendStats out;
    out.packets = c.packets.load(std::memory_order_relaxed);
    out.bursts = c.bursts.load(std::memory_order_relaxed);
    out.maxBurst = c.maxBurst.load(std::memory_order_relaxed);
    out.capped = c.capped.load(std::memory_order_relaxed);
    out.datagrams = c.datagrams.load(std::memory_order_relaxed);
    out.datagramsRefused = c.datagramsRefused.load(std::memory_order_relaxed);
    out.streamBytes = c.streamBytes.load(std::memory_order_relaxed);
    return out;
}

size_t QuicEndpoint::MaxDatagramSize(QuicConnId conn) const {
    const Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr) return 0;
    const ssize_t max = quiche_conn_dgram_max_writable_len(entry->conn);
    return max <= 0 ? 0 : size_t(max);
}

std::optional<deskhub::Fingerprint> QuicEndpoint::PeerFingerprint(QuicConnId conn) const {
    const Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr) return std::nullopt;
    const uint8_t* der = nullptr;
    size_t len = 0;
    quiche_conn_peer_cert(entry->conn, &der, &len);
    if (der == nullptr || len == 0) return std::nullopt;
    return FingerprintOfCertDer(std::span<const uint8_t>(der, len));
}

bool QuicEndpoint::Established(QuicConnId conn) const {
    const Impl::Connection* entry = impl_->Lookup(conn);
    return entry != nullptr && quiche_conn_is_established(entry->conn);
}

void QuicEndpoint::CloseConnection(QuicConnId conn, uint64_t errorCode, std::string_view reason) {
    Impl::Connection* entry = impl_->Lookup(conn);
    if (entry == nullptr) return;
    quiche_conn_close(entry->conn, true, errorCode,
        reinterpret_cast<const uint8_t*>(reason.data()), reason.size());
    impl_->Flush(*entry);
}

void QuicEndpoint::Close() {
    impl_->Shutdown();
}

bool QuicEndpoint::IsOpen() const {
    return impl_->open_;
}

bool QuicEndpoint::IsServer() const {
    return impl_->server_;
}

bool QuicEndpoint::LastBindAddrInUse() const {
    return impl_->bindAddrInUse_;
}

size_t QuicEndpoint::ConnectionCount() const {
    return impl_->connections_.size();
}

QuicConnId QuicEndpoint::FirstConnection() const {
    if (impl_->connections_.empty()) return 0;
    return impl_->connections_.begin()->first;
}

uint16_t QuicEndpoint::LocalPort() const {
    return impl_->localPort_;
}

std::vector<QuicConnId> QuicEndpoint::Connections() const {
    std::vector<QuicConnId> out;
    out.reserve(impl_->connections_.size());
    for (const auto& [id, entry] : impl_->connections_) out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

}
