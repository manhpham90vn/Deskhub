#include "deskhub/protocol/Wire.h"
#include "deskhub/protocol/ByteOrder.h"

#include <cstring>

namespace deskhub {

namespace {

size_t WriteCommon(std::span<uint8_t> out, MsgType type, uint8_t flags, Chan chan,
    uint32_t sessionId, size_t payloadSize) {
    const size_t total = kCommonHeaderSize + payloadSize;
    if (out.size() < total) return 0;
    out[0] = kProtocolVersion;
    out[1] = uint8_t(type);
    out[2] = flags;
    out[3] = uint8_t(chan);
    PutU32(out.data() + 4, sessionId);
    return total;
}

size_t BuildEmpty(std::span<uint8_t> out, MsgType type, uint32_t sessionId) {
    return WriteCommon(out, type, 0, Chan::Control, sessionId, 0);
}

size_t Utf8TruncLen(const std::string& s, size_t limit) {
    if (s.size() <= limit) return s.size();
    size_t n = limit;
    while (n > 0 && (uint8_t(s[n]) & 0xC0) == 0x80) --n;
    return n;
}

size_t BuildPingPongImpl(std::span<uint8_t> out, MsgType type, uint32_t sessionId,
    const PingPong& m) {
    constexpr size_t kPayload = 20;
    const size_t total = WriteCommon(out, type, 0, Chan::Control, sessionId, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.pingId);
    PutU64(p + 4, m.sendTimeUs);
    PutU64(p + 12, m.hostTimeUs);
    return total;
}

}

size_t BuildHello(std::span<uint8_t> out, const Hello& m) {
    const size_t nameLen = Utf8TruncLen(m.clientName, kMaxClientNameBytes);
    const size_t kPayload = 14 + kPasscodeDigits + 1 + nameLen;
    const size_t total = WriteCommon(out, MsgType::Hello, 0, Chan::Control, 0, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.clientId);
    PutU16(p + 4, m.codecMask);
    PutU16(p + 6, m.maxWidth);
    PutU16(p + 8, m.maxHeight);
    p[10] = m.desiredFps;
    PutU16(p + 11, m.features);
    p[13] = m.sourceId;
    const bool hasPasscode = IsValidPasscode(m.passcode);
    for (size_t i = 0; i < kPasscodeDigits; ++i)
        p[14 + i] = hasPasscode ? uint8_t(m.passcode[i]) : 0;
    p[14 + kPasscodeDigits] = uint8_t(nameLen);
    if (nameLen) std::memcpy(p + 14 + kPasscodeDigits + 1, m.clientName.data(), nameLen);
    return total;
}

size_t BuildAuthStart(std::span<uint8_t> out, const AuthStart& m) {
    if (m.publicKey.empty() || m.publicKey.size() > kMaxAuthBlobBytes) return 0;
    const size_t nameLen = Utf8TruncLen(m.clientName, kMaxClientNameBytes);
    const size_t payload = 1 + 2 + m.publicKey.size() + 1 + nameLen;
    const size_t total = WriteCommon(out, MsgType::AuthStart, 0, Chan::Control, 0, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    *p++ = m.hasPasscode ? 1 : 0;
    PutU16(p, uint16_t(m.publicKey.size()));
    p += 2;
    std::memcpy(p, m.publicKey.data(), m.publicKey.size());
    p += m.publicKey.size();
    *p++ = uint8_t(nameLen);
    if (nameLen) std::memcpy(p, m.clientName.data(), nameLen);
    return total;
}

size_t BuildAuthChallenge(std::span<uint8_t> out, const AuthChallenge& m) {
    if (m.spake.size() > kMaxAuthBlobBytes) return 0;
    const size_t payload = 1 + kAuthNonceBytes + kAuthSaltBytes + 2 + m.spake.size();
    const size_t total = WriteCommon(out, MsgType::AuthChallenge, 0, Chan::Control, 0, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    *p++ = uint8_t(m.mode);
    std::memcpy(p, m.nonce.data(), kAuthNonceBytes);
    p += kAuthNonceBytes;
    std::memcpy(p, m.salt.data(), kAuthSaltBytes);
    p += kAuthSaltBytes;
    PutU16(p, uint16_t(m.spake.size()));
    p += 2;
    if (!m.spake.empty()) std::memcpy(p, m.spake.data(), m.spake.size());
    return total;
}

size_t BuildAuthResponse(std::span<uint8_t> out, const AuthResponse& m) {
    if (m.proof.size() > kMaxAuthBlobBytes) return 0;
    const size_t payload = 2 + m.proof.size() + kAuthMacBytes;
    const size_t total = WriteCommon(out, MsgType::AuthResponse, 0, Chan::Control, 0, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU16(p, uint16_t(m.proof.size()));
    p += 2;
    if (!m.proof.empty()) std::memcpy(p, m.proof.data(), m.proof.size());
    p += m.proof.size();
    std::memcpy(p, m.confirm.data(), kAuthMacBytes);
    return total;
}

size_t BuildAuthResult(std::span<uint8_t> out, const AuthResult& m) {
    const size_t payload = 1 + kAuthMacBytes;
    const size_t total = WriteCommon(out, MsgType::AuthResult, 0, Chan::Control, 0, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    *p++ = uint8_t(m.code);
    std::memcpy(p, m.confirm.data(), kAuthMacBytes);
    return total;
}

size_t BuildListSources(std::span<uint8_t> out, std::string_view passcode) {
    const size_t total = WriteCommon(out, MsgType::ListSources, 0, Chan::Control, 0,
        kPasscodeDigits);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    const bool hasPasscode = IsValidPasscode(passcode);
    for (size_t i = 0; i < kPasscodeDigits; ++i)
        p[i] = hasPasscode ? uint8_t(passcode[i]) : 0;
    return total;
}

size_t BuildSourceList(std::span<uint8_t> out, std::span<const SourceInfo> sources,
    HostCaps caps) {
    const size_t n = sources.size() < kMaxSources ? sources.size() : kMaxSources;
    size_t payload = 1;
    for (size_t i = 0; i < n; ++i) {
        payload += 6 + Utf8TruncLen(sources[i].name, kMaxSourceNameBytes);
    }
    const size_t total =
        WriteCommon(out, MsgType::SourceList, HostCapFlags(caps), Chan::Control, 0, payload);
    if (!total) return 0;

    uint8_t* p = out.data() + kCommonHeaderSize;
    *p++ = uint8_t(n);
    for (size_t i = 0; i < n; ++i) {
        const SourceInfo& s = sources[i];
        const size_t nameLen = Utf8TruncLen(s.name, kMaxSourceNameBytes);
        *p++ = s.sourceId;
        PutU16(p, s.width);
        p += 2;
        PutU16(p, s.height);
        p += 2;
        *p++ = uint8_t(nameLen);
        if (nameLen) std::memcpy(p, s.name.data(), nameLen);
        p += nameLen;
    }
    return total;
}

size_t BuildHelloAck(std::span<uint8_t> out, const HelloAck& m) {
    constexpr size_t kFixed = 24;
    const size_t total = WriteCommon(out, MsgType::HelloAck, 0, Chan::Control, 0, kFixed + 1);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.sessionId);
    p[4] = uint8_t(m.codec);
    PutU16(p + 5, m.width);
    PutU16(p + 7, m.height);
    p[9] = m.fps;
    PutU32(p + 10, m.bitrateBps);
    PutU64(p + 14, m.timebaseUs);
    PutU16(p + 22, 0);
    p[24] = uint8_t(m.reason);
    return total;
}

size_t BuildStart(std::span<uint8_t> out, uint32_t sessionId) {
    return BuildEmpty(out, MsgType::Start, sessionId);
}

size_t BuildBye(std::span<uint8_t> out, uint32_t sessionId) {
    return BuildEmpty(out, MsgType::Bye, sessionId);
}

size_t BuildPing(std::span<uint8_t> out, uint32_t sessionId, const PingPong& m) {
    return BuildPingPongImpl(out, MsgType::Ping, sessionId, m);
}

size_t BuildPong(std::span<uint8_t> out, uint32_t sessionId, const PingPong& m) {
    return BuildPingPongImpl(out, MsgType::Pong, sessionId, m);
}

size_t BuildFeedback(std::span<uint8_t> out, uint32_t sessionId, const Feedback& m) {
    constexpr size_t kPayload = 9;
    const size_t total = WriteCommon(out, MsgType::Feedback, 0, Chan::Control, sessionId, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU16(p, m.lostFrames);
    p[2] = m.lossPct;
    PutU16(p + 3, m.rttMs);
    PutU32(p + 5, m.recvBitrateKbps);
    return total;
}

size_t BuildRequestKeyframe(std::span<uint8_t> out, uint32_t sessionId, KeyframeReason reason) {
    const size_t total = WriteCommon(out, MsgType::RequestKeyframe, 0, Chan::Control, sessionId, 1);
    if (!total) return 0;
    out[kCommonHeaderSize] = uint8_t(reason);
    return total;
}

KeyframeReason ParseRequestKeyframe(std::span<const uint8_t> payload) {
    if (payload.empty()) return KeyframeReason::Unknown;
    if (payload[0] >= kKeyframeReasonCount) return KeyframeReason::Unknown;
    return KeyframeReason(payload[0]);
}

size_t BuildSetFocus(std::span<uint8_t> out, uint32_t sessionId, bool focused) {
    const size_t total = WriteCommon(out, MsgType::SetFocus, 0, Chan::Control, sessionId, 1);
    if (!total) return 0;
    out[kCommonHeaderSize] = focused ? 1 : 0;
    return total;
}

size_t BuildNack(std::span<uint8_t> out, uint32_t sessionId, uint32_t frameId,
    std::span<const uint16_t> indices) {
    if (indices.empty() || indices.size() > kMaxNackIndices) return 0;
    const size_t payload = kNackHeaderSize + indices.size() * 2;
    const size_t total = WriteCommon(out, MsgType::Nack, 0, Chan::Control, sessionId, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, frameId);
    p[4] = uint8_t(indices.size());
    uint8_t* q = p + kNackHeaderSize;
    for (uint16_t idx : indices) {
        PutU16(q, idx);
        q += 2;
    }
    return total;
}

size_t BuildInvalidateRef(std::span<uint8_t> out, uint32_t sessionId, uint32_t frameId) {
    const size_t total = WriteCommon(out, MsgType::InvalidateRef, 0, Chan::Control, sessionId, 4);
    if (!total) return 0;
    PutU32(out.data() + kCommonHeaderSize, frameId);
    return total;
}

size_t BuildReconfig(std::span<uint8_t> out, uint32_t sessionId, const Reconfig& m) {
    constexpr size_t kPayload = 9;
    const size_t total = WriteCommon(out, MsgType::Reconfig, 0, Chan::Control, sessionId, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU16(p, m.width);
    PutU16(p + 2, m.height);
    PutU32(p + 4, m.bitrateBps);
    p[8] = m.fps;
    return total;
}

size_t BuildVideoPacket(std::span<uint8_t> out, uint32_t sessionId, const VideoHeader& vh,
    bool idr, bool frameEnd, std::span<const uint8_t> payload) {
    if (payload.size() > kMaxVideoPayload) return 0;
    const uint8_t flags = (idr ? kVideoFlagIdr : 0) | (frameEnd ? kVideoFlagFrameEnd : 0);
    const size_t total = WriteCommon(out, MsgType::VideoPacket, flags, Chan::Video, sessionId,
        kVideoHeaderSize + payload.size());
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, vh.frameId);
    PutU64(p + 4, vh.timestampUs);
    PutU16(p + 12, vh.pktIndex);
    PutU16(p + 14, vh.pktCount);
    if (!payload.empty())
        std::memcpy(p + kVideoHeaderSize, payload.data(), payload.size());
    return total;
}

size_t BuildFecPacket(std::span<uint8_t> out, uint32_t sessionId, const FecHeader& fh,
    bool idr, std::span<const uint8_t> parity) {
    if (parity.size() < kFecLenPrefix ||
        parity.size() > kFecLenPrefix + kMaxVideoPayload) return 0;
    if (fh.parityIndex >= kMaxParityPerGroup) return 0;
    const uint8_t flags = uint8_t((idr ? kVideoFlagIdr : 0) |
                                  (fh.parityIndex << kFecParityIndexShift));
    const size_t total = WriteCommon(out, MsgType::FecPacket, flags, Chan::Video, sessionId,
        kFecHeaderSize + parity.size());
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, fh.frameId);
    PutU64(p + 4, fh.timestampUs);
    PutU16(p + 12, fh.pktCount);
    p[14] = fh.groupIndex;
    p[15] = fh.groups;
    std::memcpy(p + kFecHeaderSize, parity.data(), parity.size());
    return total;
}

size_t BuildAudioPacket(std::span<uint8_t> out, uint32_t sessionId, const AudioHeader& ah,
    std::span<const uint8_t> payload) {
    if (payload.empty() || payload.size() > kMaxAudioPayload) return 0;
    const size_t total = WriteCommon(out, MsgType::AudioPacket, 0, Chan::Audio, sessionId,
        kAudioHeaderSize + payload.size());
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, ah.seq);
    PutU64(p + 4, ah.timestampUs);
    std::memcpy(p + kAudioHeaderSize, payload.data(), payload.size());
    return total;
}

size_t BuildInputEvents(std::span<uint8_t> out, uint32_t sessionId, uint32_t firstSeq,
    std::span<const InputEvent> events) {
    if (events.empty() || events.size() > kMaxInputEvents) return 0;
    const size_t payloadSize = kInputHeaderSize + events.size() * kInputEventSize;
    const size_t total = WriteCommon(out, MsgType::InputEvent, 0, Chan::Input, sessionId,
        payloadSize);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, firstSeq);
    p[4] = uint8_t(events.size());
    uint8_t* e = p + kInputHeaderSize;
    for (const auto& ev : events) {
        e[0] = uint8_t(ev.type);
        PutU64(e + 1, ev.timestampUs);
        PutU32(e + 9, uint32_t(ev.a));
        PutU32(e + 13, uint32_t(ev.b));
        e[17] = ev.state;
        e[18] = ev.absolute;
        e += kInputEventSize;
    }
    return total;
}

size_t BuildClipboardChunk(std::span<uint8_t> out, uint32_t sessionId,
    const ClipboardChunkView& chunk) {
    if (chunk.chunkCount == 0 || chunk.chunkIndex >= chunk.chunkCount) return 0;
    if (chunk.chunkCount > kMaxClipboardChunks) return 0;
    if (chunk.payload.size() > kMaxClipboardChunkPayload) return 0;
    const size_t payloadSize = kClipboardHeaderSize + chunk.payload.size();
    const size_t total = WriteCommon(out, MsgType::Clipboard, 0, Chan::Control, sessionId,
        payloadSize);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, chunk.revision);
    PutU16(p + 4, chunk.chunkIndex);
    PutU16(p + 6, chunk.chunkCount);
    if (!chunk.payload.empty())
        std::memcpy(p + kClipboardHeaderSize, chunk.payload.data(), chunk.payload.size());
    return total;
}

std::optional<CommonHeader> ParseCommonHeader(std::span<const uint8_t> datagram) {
    if (datagram.size() < kCommonHeaderSize) return std::nullopt;
    if (datagram[0] != kProtocolVersion) return std::nullopt;
    CommonHeader h;
    h.ver = datagram[0];
    h.type = MsgType(datagram[1]);
    h.flags = datagram[2];
    h.chan = Chan(datagram[3]);
    h.sessionId = GetU32(datagram.data() + 4);
    return h;
}

std::span<const uint8_t> PayloadOf(std::span<const uint8_t> datagram) {
    if (datagram.size() < kCommonHeaderSize) return {};
    return datagram.subspan(kCommonHeaderSize);
}

std::optional<AuthStart> ParseAuthStart(std::span<const uint8_t> payload) {
    if (payload.size() < 4) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[0] > 1) return std::nullopt;
    const size_t keyLen = GetU16(p + 1);
    if (keyLen == 0 || keyLen > kMaxAuthBlobBytes || payload.size() < 1 + 2 + keyLen + 1)
        return std::nullopt;

    AuthStart m;
    m.hasPasscode = p[0] != 0;
    m.publicKey.assign(p + 3, p + 3 + keyLen);
    const size_t nameOff = 1 + 2 + keyLen;
    size_t nameLen = p[nameOff];
    if (nameLen > kMaxClientNameBytes) nameLen = 0;
    if (nameLen && payload.size() >= nameOff + 1 + nameLen) {
        m.clientName.reserve(nameLen);
        for (size_t i = 0; i < nameLen; ++i) {
            const uint8_t c = p[nameOff + 1 + i];
            if (c >= 0x20 && c != 0x7F) m.clientName.push_back(char(c));
        }
    }
    return m;
}

std::optional<AuthChallenge> ParseAuthChallenge(std::span<const uint8_t> payload) {
    constexpr size_t kFixed = 1 + kAuthNonceBytes + kAuthSaltBytes + 2;
    if (payload.size() < kFixed) return std::nullopt;
    const uint8_t* p = payload.data();
    AuthChallenge m;
    if (p[0] > uint8_t(AuthMode::Approval)) return std::nullopt;
    m.mode = AuthMode(p[0]);
    std::memcpy(m.nonce.data(), p + 1, kAuthNonceBytes);
    std::memcpy(m.salt.data(), p + 1 + kAuthNonceBytes, kAuthSaltBytes);
    const size_t blob = GetU16(p + 1 + kAuthNonceBytes + kAuthSaltBytes);
    if (blob > kMaxAuthBlobBytes || payload.size() < kFixed + blob) return std::nullopt;
    m.spake.assign(p + kFixed, p + kFixed + blob);
    return m;
}

std::optional<AuthResponse> ParseAuthResponse(std::span<const uint8_t> payload) {
    if (payload.size() < 2 + kAuthMacBytes) return std::nullopt;
    const uint8_t* p = payload.data();
    const size_t blob = GetU16(p);
    if (blob > kMaxAuthBlobBytes || payload.size() < 2 + blob + kAuthMacBytes)
        return std::nullopt;
    AuthResponse m;
    m.proof.assign(p + 2, p + 2 + blob);
    std::memcpy(m.confirm.data(), p + 2 + blob, kAuthMacBytes);
    return m;
}

std::optional<AuthResult> ParseAuthResult(std::span<const uint8_t> payload) {
    if (payload.size() < 1 + kAuthMacBytes) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[0] > uint8_t(AuthResultCode::Locked)) return std::nullopt;
    AuthResult m;
    m.code = AuthResultCode(p[0]);
    std::memcpy(m.confirm.data(), p + 1, kAuthMacBytes);
    return m;
}

std::optional<Hello> ParseHello(std::span<const uint8_t> payload) {
    if (payload.size() < 13) return std::nullopt;
    const uint8_t* p = payload.data();
    Hello m;
    m.clientId = GetU32(p);
    m.codecMask = GetU16(p + 4);
    m.maxWidth = GetU16(p + 6);
    m.maxHeight = GetU16(p + 8);
    m.desiredFps = p[10];
    m.features = GetU16(p + 11);
    m.sourceId = payload.size() >= 14 ? p[13] : 0;
    if (payload.size() >= 14 + kPasscodeDigits) {
        const std::string_view code(reinterpret_cast<const char*>(p + 14), kPasscodeDigits);
        if (IsValidPasscode(code)) m.passcode = code;
    }
    constexpr size_t nameLenOff = 14 + kPasscodeDigits;
    if (payload.size() > nameLenOff) {
        size_t nameLen = p[nameLenOff];
        if (nameLen > kMaxClientNameBytes) nameLen = 0;
        if (nameLen && payload.size() >= nameLenOff + 1 + nameLen) {
            m.clientName.reserve(nameLen);
            for (size_t i = 0; i < nameLen; ++i) {
                const uint8_t c = p[nameLenOff + 1 + i];
                if (c >= 0x20 && c != 0x7F) m.clientName.push_back(char(c));
            }
        }
    }
    return m;
}

std::string ParseListSourcesPasscode(std::span<const uint8_t> payload) {
    if (payload.size() < kPasscodeDigits) return {};
    const std::string_view code(reinterpret_cast<const char*>(payload.data()), kPasscodeDigits);
    return IsValidPasscode(code) ? std::string(code) : std::string();
}

size_t ParseSourceList(std::span<const uint8_t> payload, std::span<SourceInfo> out) {
    if (payload.empty()) return 0;
    constexpr size_t rec = 6;
    constexpr size_t lenOff = 5;

    size_t count = payload[0];
    if (count > out.size()) count = out.size();

    size_t off = 1;
    size_t written = 0;
    for (size_t i = 0; i < count; ++i) {
        if (off + rec > payload.size()) break;
        const uint8_t* p = payload.data() + off;
        const size_t nameLen = p[lenOff];
        if (off + rec + nameLen > payload.size()) break;
        SourceInfo s;
        s.sourceId = p[0];
        s.width = GetU16(p + 1);
        s.height = GetU16(p + 3);
        s.name.assign(reinterpret_cast<const char*>(p + rec), nameLen);
        out[written++] = std::move(s);
        off += rec + nameLen;
    }
    return written;
}

std::optional<HelloAck> ParseHelloAck(std::span<const uint8_t> payload) {
    if (payload.size() < 22) return std::nullopt;
    const uint8_t* p = payload.data();
    HelloAck m;
    m.sessionId = GetU32(p);
    if (p[4] != uint8_t(Codec::H264) && p[4] != uint8_t(Codec::Rejected)) return std::nullopt;
    m.codec = Codec(p[4]);
    m.width = GetU16(p + 5);
    m.height = GetU16(p + 7);
    m.fps = p[9];
    m.bitrateBps = GetU32(p + 10);
    m.timebaseUs = GetU64(p + 14);

    if (payload.size() >= 25 && p[24] <= uint8_t(RejectReason::WrongPasscode))
        m.reason = RejectReason(p[24]);
    return m;
}

std::optional<PingPong> ParsePingPong(std::span<const uint8_t> payload) {
    if (payload.size() < 12) return std::nullopt;
    const uint8_t* p = payload.data();
    PingPong m{GetU32(p), GetU64(p + 4)};
    if (payload.size() >= 20) m.hostTimeUs = GetU64(p + 12);
    return m;
}

std::optional<Feedback> ParseFeedback(std::span<const uint8_t> payload) {
    if (payload.size() < 9) return std::nullopt;
    const uint8_t* p = payload.data();
    Feedback m;
    m.lostFrames = GetU16(p);
    m.lossPct = p[2];
    m.rttMs = GetU16(p + 3);
    m.recvBitrateKbps = GetU32(p + 5);
    return m;
}

std::optional<Reconfig> ParseReconfig(std::span<const uint8_t> payload) {
    if (payload.size() < 8) return std::nullopt;
    const uint8_t* p = payload.data();
    const uint8_t fps = payload.size() >= 9 ? p[8] : 0;
    return Reconfig{GetU16(p), GetU16(p + 2), GetU32(p + 4), fps};
}

std::optional<bool> ParseSetFocus(std::span<const uint8_t> payload) {
    if (payload.empty()) return std::nullopt;
    return payload[0] != 0;
}

size_t ParseNack(std::span<const uint8_t> payload, uint32_t& frameId,
    std::span<uint16_t> out) {
    if (payload.size() < kNackHeaderSize) return 0;
    const uint8_t* p = payload.data();
    size_t count = p[4];
    if (count == 0) return 0;
    if (payload.size() < kNackHeaderSize + count * 2) return 0;
    if (count > out.size()) count = out.size();
    frameId = GetU32(p);
    const uint8_t* q = p + kNackHeaderSize;
    for (size_t i = 0; i < count; ++i) out[i] = GetU16(q + i * 2);
    return count;
}

std::optional<uint32_t> ParseInvalidateRef(std::span<const uint8_t> payload) {
    if (payload.size() < 4) return std::nullopt;
    return GetU32(payload.data());
}

std::optional<VideoPacketView> ParseVideoPacket(const CommonHeader& h,
    std::span<const uint8_t> payload) {
    if (payload.size() < kVideoHeaderSize) return std::nullopt;
    if (payload.size() > kVideoHeaderSize + kMaxVideoPayload) return std::nullopt;
    const uint8_t* p = payload.data();
    VideoPacketView v;
    v.hdr.frameId = GetU32(p);
    v.hdr.timestampUs = GetU64(p + 4);
    v.hdr.pktIndex = GetU16(p + 12);
    v.hdr.pktCount = GetU16(p + 14);
    v.idr = (h.flags & kVideoFlagIdr) != 0;
    v.frameEnd = (h.flags & kVideoFlagFrameEnd) != 0;
    v.payload = payload.subspan(kVideoHeaderSize);
    if (v.hdr.pktCount == 0 || v.hdr.pktIndex >= v.hdr.pktCount) return std::nullopt;
    return v;
}

std::optional<AudioPacketView> ParseAudioPacket(std::span<const uint8_t> payload) {
    if (payload.size() <= kAudioHeaderSize) return std::nullopt;
    if (payload.size() > kAudioHeaderSize + kMaxAudioPayload) return std::nullopt;
    const uint8_t* p = payload.data();
    AudioPacketView v;
    v.hdr.seq = GetU32(p);
    v.hdr.timestampUs = GetU64(p + 4);
    v.payload = payload.subspan(kAudioHeaderSize);
    return v;
}

std::optional<FecPacketView> ParseFecPacket(const CommonHeader& h,
    std::span<const uint8_t> payload) {
    if (payload.size() < kFecHeaderSize + kFecLenPrefix) return std::nullopt;
    if (payload.size() > kFecHeaderSize + kFecLenPrefix + kMaxVideoPayload)
        return std::nullopt;
    const uint8_t* p = payload.data();
    FecPacketView v;
    v.hdr.frameId = GetU32(p);
    v.hdr.timestampUs = GetU64(p + 4);
    v.hdr.pktCount = GetU16(p + 12);
    v.hdr.groupIndex = p[14];
    v.hdr.groups = p[15];
    v.hdr.parityIndex = uint8_t(h.flags >> kFecParityIndexShift);
    v.idr = (h.flags & kVideoFlagIdr) != 0;
    v.parity = payload.subspan(kFecHeaderSize);
    if (v.hdr.pktCount == 0) return std::nullopt;
    if (v.hdr.groupIndex >= FecGroupCount(v.hdr.pktCount, v.hdr.groups)) return std::nullopt;
    return v;
}

size_t ParseInputEvents(std::span<const uint8_t> payload, uint32_t& firstSeq,
    std::span<InputEvent> out) {
    if (payload.size() < kInputHeaderSize) return 0;
    const uint8_t* p = payload.data();
    const size_t count = p[4];
    if (count == 0 || count > out.size()) return 0;
    if (payload.size() < kInputHeaderSize + count * kInputEventSize) return 0;
    firstSeq = GetU32(p);
    const uint8_t* e = p + kInputHeaderSize;
    for (size_t i = 0; i < count; ++i) {
        InputEvent ev;
        ev.type = InputType(e[0]);
        ev.timestampUs = GetU64(e + 1);
        ev.a = int32_t(GetU32(e + 9));
        ev.b = int32_t(GetU32(e + 13));
        ev.state = e[17];
        ev.absolute = e[18];
        out[i] = ev;
        e += kInputEventSize;
    }
    return count;
}

std::optional<ClipboardChunkView> ParseClipboardChunk(std::span<const uint8_t> payload) {
    if (payload.size() < kClipboardHeaderSize) return std::nullopt;
    ClipboardChunkView v;
    const uint8_t* p = payload.data();
    v.revision = GetU32(p);
    v.chunkIndex = GetU16(p + 4);
    v.chunkCount = GetU16(p + 6);
    if (v.chunkCount == 0 || v.chunkCount > kMaxClipboardChunks) return std::nullopt;
    if (v.chunkIndex >= v.chunkCount) return std::nullopt;
    v.payload = payload.subspan(kClipboardHeaderSize);
    if (v.payload.size() > kMaxClipboardChunkPayload) return std::nullopt;
    return v;
}

std::string TruncateClipboardText(std::string_view text) {
    std::string out(text);
    out.resize(Utf8TruncLen(out, kMaxClipboardTextBytes));
    return out;
}

size_t BuildRecord(std::span<uint8_t> out, std::span<const uint8_t> message) {
    if (message.empty() || message.size() > kMaxRecordSize) return 0;
    const size_t total = kRecordPrefixSize + message.size();
    if (out.size() < total) return 0;
    PutU16(out.data(), uint16_t(message.size()));
    std::memcpy(out.data() + kRecordPrefixSize, message.data(), message.size());
    return total;
}

RecordView ReadRecord(std::span<const uint8_t> buffer) {
    RecordView v;
    if (buffer.size() < kRecordPrefixSize) return v;
    const size_t len = GetU16(buffer.data());
    if (len == 0 || len > kMaxRecordSize) {
        v.status = RecordStatus::Invalid;
        return v;
    }
    if (buffer.size() < kRecordPrefixSize + len) return v;
    v.status = RecordStatus::Ok;
    v.message = buffer.subspan(kRecordPrefixSize, len);
    v.consumed = kRecordPrefixSize + len;
    return v;
}

PacketKind ClassifyPacket(std::span<const uint8_t> datagram) {
    if (datagram.empty()) return PacketKind::Unknown;
    constexpr uint8_t kQuicHeaderBits = 0xC0;
    if ((datagram[0] & kQuicHeaderBits) != 0) return PacketKind::Quic;
    if (datagram[0] == 0 || datagram[0] > kProtocolVersion) return PacketKind::Unknown;
    if (datagram.size() < kCommonHeaderSize) return PacketKind::Unknown;
    return PacketKind::Deskhub;
}

bool IsValidTermSize(TermSize size) {
    return size.cols >= kMinTermCols && size.cols <= kMaxTermCols &&
           size.rows >= kMinTermRows && size.rows <= kMaxTermRows;
}

TermSize ClampTermSize(TermSize size) {
    TermSize out = size;
    if (out.cols < kMinTermCols) out.cols = kMinTermCols;
    if (out.cols > kMaxTermCols) out.cols = kMaxTermCols;
    if (out.rows < kMinTermRows) out.rows = kMinTermRows;
    if (out.rows > kMaxTermRows) out.rows = kMaxTermRows;
    return out;
}

size_t BuildTermOpen(std::span<uint8_t> out, const TermOpen& m) {
    constexpr size_t kFixed = 13;
    const size_t nameLen = Utf8TruncLen(m.clientName, kMaxClientNameBytes);
    const size_t total = WriteCommon(out, MsgType::TermOpen, 0, Chan::Terminal, 0,
        kFixed + nameLen);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    const TermSize size = ClampTermSize(m.size);
    PutU16(p, size.cols);
    PutU16(p + 2, size.rows);
    PutU32(p + 4, m.resumeId);
    const bool hasPasscode = IsValidPasscode(m.passcode);
    for (size_t i = 0; i < kPasscodeDigits; ++i)
        p[8 + i] = hasPasscode ? uint8_t(m.passcode[i]) : 0;
    p[12] = uint8_t(nameLen);
    if (nameLen) std::memcpy(p + kFixed, m.clientName.data(), nameLen);
    return total;
}

size_t BuildTermOpenAck(std::span<uint8_t> out, const TermOpenAck& m) {
    constexpr size_t kPayload = 6;
    const size_t total = WriteCommon(out, MsgType::TermOpenAck, 0, Chan::Terminal, m.termId,
        kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.termId);
    p[4] = uint8_t(m.reason);
    p[5] = m.resumed ? 1 : 0;
    return total;
}

size_t BuildTermData(std::span<uint8_t> out, uint32_t termId, std::span<const uint8_t> data) {
    if (data.empty() || data.size() > kMaxTermDataBytes) return 0;
    const size_t total = WriteCommon(out, MsgType::TermData, 0, Chan::Terminal, termId,
        data.size());
    if (!total) return 0;
    std::memcpy(out.data() + kCommonHeaderSize, data.data(), data.size());
    return total;
}

size_t BuildTermResize(std::span<uint8_t> out, uint32_t termId, TermSize size) {
    const size_t total = WriteCommon(out, MsgType::TermResize, 0, Chan::Terminal, termId, 4);
    if (!total) return 0;
    const TermSize clamped = ClampTermSize(size);
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU16(p, clamped.cols);
    PutU16(p + 2, clamped.rows);
    return total;
}

size_t BuildTermClose(std::span<uint8_t> out, uint32_t termId) {
    return WriteCommon(out, MsgType::TermClose, 0, Chan::Terminal, termId, 0);
}

size_t BuildTermExit(std::span<uint8_t> out, uint32_t termId, int32_t exitCode) {
    const size_t total = WriteCommon(out, MsgType::TermExit, 0, Chan::Terminal, termId, 4);
    if (!total) return 0;
    PutU32(out.data() + kCommonHeaderSize, uint32_t(exitCode));
    return total;
}

std::optional<TermOpen> ParseTermOpen(std::span<const uint8_t> payload) {
    constexpr size_t kFixed = 13;
    if (payload.size() < kFixed) return std::nullopt;
    const uint8_t* p = payload.data();
    TermOpen m;
    m.size.cols = GetU16(p);
    m.size.rows = GetU16(p + 2);
    if (!IsValidTermSize(m.size)) return std::nullopt;
    m.resumeId = GetU32(p + 4);
    const std::string_view code(reinterpret_cast<const char*>(p + 8), kPasscodeDigits);
    if (IsValidPasscode(code)) m.passcode = code;
    size_t nameLen = p[12];
    if (nameLen > kMaxClientNameBytes) nameLen = 0;
    if (nameLen && payload.size() >= kFixed + nameLen) {
        m.clientName.reserve(nameLen);
        for (size_t i = 0; i < nameLen; ++i) {
            const uint8_t c = p[kFixed + i];
            if (c >= 0x20 && c != 0x7F) m.clientName.push_back(char(c));
        }
    }
    return m;
}

std::optional<TermOpenAck> ParseTermOpenAck(std::span<const uint8_t> payload) {
    if (payload.size() < 6) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[4] > uint8_t(TermReason::NoSuchSession)) return std::nullopt;
    TermOpenAck m;
    m.termId = GetU32(p);
    m.reason = TermReason(p[4]);
    m.resumed = p[5] != 0;
    return m;
}

std::optional<TermSize> ParseTermResize(std::span<const uint8_t> payload) {
    if (payload.size() < 4) return std::nullopt;
    const TermSize size{GetU16(payload.data()), GetU16(payload.data() + 2)};
    if (!IsValidTermSize(size)) return std::nullopt;
    return size;
}

std::optional<int32_t> ParseTermExit(std::span<const uint8_t> payload) {
    if (payload.size() < 4) return std::nullopt;
    return int32_t(GetU32(payload.data()));
}

bool IsWireLegalFileName(std::string_view name) {
    if (name.empty() || name.size() > kMaxTransferNameBytes) return false;
    if (name == "." || name == "..") return false;
    for (char c : name) {
        const uint8_t u = uint8_t(c);
        if (u < 0x20 || u == 0x7F) return false;
        if (c == '/' || c == '\\') return false;
    }
    return true;
}

size_t BuildFileOffer(std::span<uint8_t> out, const FileOffer& m) {
    if (m.files.empty() || m.files.size() > kMaxTransferFiles) return 0;
    size_t payload = kFileOfferHeaderSize;
    uint64_t totalBytes = 0;
    for (const TransferFile& f : m.files) {
        if (!IsWireLegalFileName(f.name)) return 0;
        if (f.size > kMaxTransferFileBytes) return 0;
        totalBytes += f.size;
        if (totalBytes > kMaxTransferBatchBytes) return 0;
        payload += kFileOfferEntryOverhead + Utf8TruncLen(f.name, kMaxTransferNameBytes);
    }
    const size_t total = WriteCommon(out, MsgType::FileOffer, 0, Chan::File, m.batchId, payload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.batchId);
    p += 4;
    *p++ = uint8_t(m.files.size());
    for (const TransferFile& f : m.files) {
        const size_t nameLen = Utf8TruncLen(f.name, kMaxTransferNameBytes);
        PutU64(p, f.size);
        p += 8;
        *p++ = uint8_t(nameLen);
        std::memcpy(p, f.name.data(), nameLen);
        p += nameLen;
    }
    return total;
}

size_t BuildFileAccept(std::span<uint8_t> out, const FileAccept& m) {
    constexpr size_t kPayload = 5;
    const size_t total = WriteCommon(out, MsgType::FileAccept, 0, Chan::File, m.batchId,
        kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.batchId);
    p[4] = uint8_t(m.reason);
    return total;
}

size_t BuildFileChunk(std::span<uint8_t> out, uint32_t batchId, uint16_t fileIndex,
    uint64_t offset, std::span<const uint8_t> data) {
    if (data.empty() || data.size() > kMaxFileChunkBytes) return 0;
    if (fileIndex >= kMaxTransferFiles) return 0;
    const size_t total = WriteCommon(out, MsgType::FileChunk, 0, Chan::File, batchId,
        kFileChunkHeaderSize + data.size());
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, batchId);
    PutU16(p + 4, fileIndex);
    PutU64(p + 6, offset);
    std::memcpy(p + kFileChunkHeaderSize, data.data(), data.size());
    return total;
}

size_t BuildFileDone(std::span<uint8_t> out, const FileDone& m) {
    constexpr size_t kPayload = 10;
    const size_t total = WriteCommon(out, MsgType::FileDone, 0, Chan::File, m.batchId, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.batchId);
    PutU16(p + 4, m.fileIndex);
    PutU32(p + 6, m.crc32);
    return total;
}

size_t BuildFileAck(std::span<uint8_t> out, const FileAck& m) {
    constexpr size_t kPayload = 7;
    const size_t total = WriteCommon(out, MsgType::FileAck, 0, Chan::File, m.batchId, kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.batchId);
    PutU16(p + 4, m.fileIndex);
    p[6] = uint8_t(m.reason);
    return total;
}

size_t BuildFileCancel(std::span<uint8_t> out, const FileCancel& m) {
    constexpr size_t kPayload = 5;
    const size_t total = WriteCommon(out, MsgType::FileCancel, 0, Chan::File, m.batchId,
        kPayload);
    if (!total) return 0;
    uint8_t* p = out.data() + kCommonHeaderSize;
    PutU32(p, m.batchId);
    p[4] = uint8_t(m.reason);
    return total;
}

std::optional<FileOffer> ParseFileOffer(std::span<const uint8_t> payload) {
    if (payload.size() < kFileOfferHeaderSize) return std::nullopt;
    const uint8_t* p = payload.data();
    FileOffer m;
    m.batchId = GetU32(p);
    const size_t count = p[4];
    if (count == 0 || count > kMaxTransferFiles) return std::nullopt;
    size_t at = kFileOfferHeaderSize;
    uint64_t totalBytes = 0;
    m.files.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        if (payload.size() < at + kFileOfferEntryOverhead) return std::nullopt;
        TransferFile f;
        f.size = GetU64(p + at);
        if (f.size > kMaxTransferFileBytes) return std::nullopt;
        totalBytes += f.size;
        if (totalBytes > kMaxTransferBatchBytes) return std::nullopt;
        const size_t nameLen = p[at + 8];
        at += kFileOfferEntryOverhead;
        if (payload.size() < at + nameLen) return std::nullopt;
        f.name.assign(reinterpret_cast<const char*>(p + at), nameLen);
        if (!IsWireLegalFileName(f.name)) return std::nullopt;
        at += nameLen;
        m.files.push_back(std::move(f));
    }
    return m;
}

std::optional<FileAccept> ParseFileAccept(std::span<const uint8_t> payload) {
    if (payload.size() < 5) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[4] > kMaxTransferReason) return std::nullopt;
    FileAccept m;
    m.batchId = GetU32(p);
    m.reason = TransferReason(p[4]);
    return m;
}

std::optional<FileChunkView> ParseFileChunk(std::span<const uint8_t> payload) {
    if (payload.size() <= kFileChunkHeaderSize) return std::nullopt;
    const uint8_t* p = payload.data();
    FileChunkView m;
    m.batchId = GetU32(p);
    m.fileIndex = GetU16(p + 4);
    if (m.fileIndex >= kMaxTransferFiles) return std::nullopt;
    m.offset = GetU64(p + 6);
    m.data = payload.subspan(kFileChunkHeaderSize);
    if (m.data.size() > kMaxFileChunkBytes) return std::nullopt;
    if (m.offset > kMaxTransferFileBytes - m.data.size()) return std::nullopt;
    return m;
}

std::optional<FileDone> ParseFileDone(std::span<const uint8_t> payload) {
    if (payload.size() < 10) return std::nullopt;
    const uint8_t* p = payload.data();
    FileDone m;
    m.batchId = GetU32(p);
    m.fileIndex = GetU16(p + 4);
    if (m.fileIndex >= kMaxTransferFiles) return std::nullopt;
    m.crc32 = GetU32(p + 6);
    return m;
}

std::optional<FileAck> ParseFileAck(std::span<const uint8_t> payload) {
    if (payload.size() < 7) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[6] > kMaxTransferReason) return std::nullopt;
    FileAck m;
    m.batchId = GetU32(p);
    m.fileIndex = GetU16(p + 4);
    if (m.fileIndex >= kMaxTransferFiles) return std::nullopt;
    m.reason = TransferReason(p[6]);
    return m;
}

std::optional<FileCancel> ParseFileCancel(std::span<const uint8_t> payload) {
    if (payload.size() < 5) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[4] > kMaxTransferReason) return std::nullopt;
    FileCancel m;
    m.batchId = GetU32(p);
    m.reason = TransferReason(p[4]);
    return m;
}

}
