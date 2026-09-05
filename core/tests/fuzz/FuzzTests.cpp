#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/AnnexB.h"
#include "deskhub/media/BitWriter.h"
#include "deskhub/media/H264Sps.h"
#include "deskhub/session/host/Beacon.h"
#include "deskhub/session/client/ScreenClientSession.h"
#include "deskhub/session/host/ScreenHostSession.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/SecretText.h"
#include "deskhub/ui/UiSettings.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace deskhub;

namespace {

Datagram RandomJunk(size_t maxLen) {
    Datagram d(Rnd() % (maxLen + 1));
    for (auto& b : d) b = uint8_t(Rnd());
    return d;
}

bool ExerciseWireParsers(std::span<const uint8_t> d) {
    bool ok = true;
    const auto h = ParseCommonHeader(d);
    const auto pl = PayloadOf(d);

    if (const auto hello = ParseHello(pl))
        ok = ok && (hello->passcode.empty() || IsValidPasscode(hello->passcode));
    if (const auto ack = ParseHelloAck(pl))
        ok = ok && uint8_t(ack->reason) <= uint8_t(RejectReason::WrongPasscode);
    const std::string code = ParseListSourcesPasscode(pl);
    ok = ok && (code.empty() || IsValidPasscode(code));

    SourceInfo sources[kMaxSources];
    const size_t nSources = ParseSourceList(pl, sources);
    ok = ok && nSources <= kMaxSources;
    for (size_t i = 0; i < nSources; ++i) ok = ok && sources[i].name.size() <= 255;

    uint32_t frameId = 0;
    uint16_t indices[kMaxNackIndices];
    ok = ok && ParseNack(pl, frameId, indices) <= kMaxNackIndices;

    uint32_t firstSeq = 0;
    InputEvent events[kMaxInputEvents];
    ok = ok && ParseInputEvents(pl, firstSeq, events) <= kMaxInputEvents;

    if (const auto audio = ParseAudioPacket(pl)) {
        ok = ok && !audio->payload.empty() && audio->payload.size() <= kMaxAudioPayload;
    }

    if (const auto clip = ParseClipboardChunk(pl)) {
        ok = ok && clip->chunkCount > 0 && clip->chunkIndex < clip->chunkCount;
        ok = ok && clip->payload.size() <= kMaxClipboardChunkPayload;
    }

    ParsePingPong(pl);
    ParseFeedback(pl);
    ParseReconfig(pl);
    ParseSetFocus(pl);
    ParseInvalidateRef(pl);

    if (const auto authStart = ParseAuthStart(pl)) {
        ok = ok && !authStart->publicKey.empty() &&
             authStart->publicKey.size() <= kMaxAuthBlobBytes;
        ok = ok && authStart->clientName.size() <= kMaxClientNameBytes;
    }
    if (const auto authChallenge = ParseAuthChallenge(pl)) {
        ok = ok && uint8_t(authChallenge->mode) <= uint8_t(AuthMode::Approval);
        ok = ok && authChallenge->spake.size() <= kMaxAuthBlobBytes;
    }
    if (const auto authResponse = ParseAuthResponse(pl))
        ok = ok && authResponse->proof.size() <= kMaxAuthBlobBytes;
    if (const auto authResult = ParseAuthResult(pl))
        ok = ok && uint8_t(authResult->code) <= uint8_t(AuthResultCode::Locked);

    if (h) {
        if (const auto v = ParseVideoPacket(*h, pl)) {
            ok = ok && v->payload.size() <= kMaxVideoPayload;
            ok = ok && v->hdr.pktCount > 0 && v->hdr.pktIndex < v->hdr.pktCount;
        }
        if (const auto f = ParseFecPacket(*h, pl)) {
            ok = ok && f->parity.size() >= kFecLenPrefix;
            ok = ok && f->parity.size() <= kFecLenPrefix + kMaxVideoPayload;
            ok = ok && f->hdr.pktCount > 0;
            ok = ok && f->hdr.groupIndex < FecGroupCount(f->hdr.pktCount, f->hdr.groups);
        }
    }
    return ok;
}

Datagram Taken(const uint8_t* buf, size_t n) {
    return Datagram(buf, buf + n);
}

std::vector<SourceInfo> RandomSources() {
    std::vector<SourceInfo> v(Rnd() % (kMaxSources + 1));
    for (size_t i = 0; i < v.size(); ++i) {
        v[i].sourceId = uint8_t(Rnd());
        v[i].width = uint16_t(Rnd());
        v[i].height = uint16_t(Rnd());
        v[i].name.assign(Rnd() % 80, ' ');
        for (auto& c : v[i].name) c = char('a' + Rnd() % 26);
    }
    return v;
}

Datagram BuildRandomValidDatagram() {
    uint8_t buf[kMaxDatagram];
    size_t n = 0;
    switch (Rnd() % 22) {
        case 0: {
            Hello m{Rnd(), uint16_t(Rnd()), uint16_t(Rnd()), uint16_t(Rnd()),
                uint8_t(Rnd()), uint16_t(Rnd()), uint8_t(Rnd()), PasscodeFromRandom(Rnd())};
            n = BuildHello(buf, m);
            break;
        }
        case 1: {
            HelloAck m{Rnd(), (Rnd() % 2) ? Codec::H264 : Codec::Rejected, uint16_t(Rnd()),
                uint16_t(Rnd()), uint8_t(Rnd()), Rnd(),
                (uint64_t(Rnd()) << 32) | Rnd(), RejectReason(Rnd() % 4)};
            n = BuildHelloAck(buf, m);
            break;
        }
        case 2:
            n = BuildStart(buf, Rnd());
            break;
        case 3:
            n = BuildBye(buf, Rnd());
            break;
        case 4:
            n = BuildListSources(buf, PasscodeFromRandom(Rnd()));
            break;
        case 5: {
            const auto sources = RandomSources();
            n = BuildSourceList(buf, sources);
            break;
        }
        case 6:
            n = BuildPing(buf, Rnd(), PingPong{Rnd(), (uint64_t(Rnd()) << 32) | Rnd()});
            break;
        case 7:
            n = BuildPong(buf, Rnd(), PingPong{Rnd(), (uint64_t(Rnd()) << 32) | Rnd()});
            break;
        case 8:
            n = BuildFeedback(buf, Rnd(),
                Feedback{uint16_t(Rnd()), uint8_t(Rnd()), uint16_t(Rnd()), Rnd()});
            break;
        case 9:
            n = BuildRequestKeyframe(buf, Rnd(), KeyframeReason(Rnd() % (kKeyframeReasonCount + 2)));
            break;
        case 10:
            n = BuildReconfig(buf, Rnd(),
                Reconfig{uint16_t(Rnd()), uint16_t(Rnd()), Rnd(), uint8_t(Rnd())});
            break;
        case 11:
            n = BuildSetFocus(buf, Rnd(), Rnd() % 2 != 0);
            break;
        case 12: {
            std::vector<uint16_t> indices(1 + Rnd() % 64);
            for (auto& idx : indices) idx = uint16_t(Rnd());
            n = BuildNack(buf, Rnd(), Rnd(), indices);
            break;
        }
        case 13:
            n = BuildInvalidateRef(buf, Rnd(), Rnd());
            break;
        case 15: {
            std::vector<InputEvent> events(1 + Rnd() % kMaxInputEvents);
            for (auto& ev : events) {
                ev.type = InputType(1 + Rnd() % 4);
                ev.timestampUs = (uint64_t(Rnd()) << 32) | Rnd();
                ev.a = int32_t(Rnd());
                ev.b = int32_t(Rnd());
                ev.state = uint8_t(Rnd());
                ev.absolute = uint8_t(Rnd());
            }
            n = BuildInputEvents(buf, Rnd(), Rnd(), events);
            break;
        }
        case 14: {
            VideoHeader vh{};
            vh.frameId = Rnd();
            vh.timestampUs = (uint64_t(Rnd()) << 32) | Rnd();
            vh.pktCount = uint16_t(1 + Rnd() % 1000);
            vh.pktIndex = uint16_t(Rnd() % vh.pktCount);
            const Datagram payload = RandomJunk(kMaxVideoPayload);
            n = BuildVideoPacket(buf, Rnd(), vh, Rnd() % 2 != 0, Rnd() % 2 != 0, payload);
            break;
        }
        case 16: {
            AuthStart m;
            m.publicKey = RandomJunk(kMaxAuthBlobBytes);
            if (m.publicKey.empty()) m.publicKey.push_back(uint8_t(Rnd()));
            m.clientName.assign(Rnd() % 40, ' ');
            for (auto& c : m.clientName) c = char('a' + Rnd() % 26);
            m.hasPasscode = Rnd() % 2 != 0;
            n = BuildAuthStart(buf, m);
            break;
        }
        case 17: {
            AuthChallenge m;
            m.mode = AuthMode(Rnd() % (uint8_t(AuthMode::Approval) + 1));
            for (auto& b : m.nonce) b = uint8_t(Rnd());
            for (auto& b : m.salt) b = uint8_t(Rnd());
            m.spake = RandomJunk(kMaxAuthBlobBytes);
            n = BuildAuthChallenge(buf, m);
            break;
        }
        case 18: {
            AuthResponse m;
            m.proof = RandomJunk(kMaxAuthBlobBytes);
            for (auto& b : m.confirm) b = uint8_t(Rnd());
            n = BuildAuthResponse(buf, m);
            break;
        }
        case 19: {
            AuthResult m;
            m.code = AuthResultCode(Rnd() % (uint8_t(AuthResultCode::Locked) + 1));
            for (auto& b : m.confirm) b = uint8_t(Rnd());
            n = BuildAuthResult(buf, m);
            break;
        }
        case 20: {
            AudioHeader ah{};
            ah.seq = Rnd();
            ah.timestampUs = (uint64_t(Rnd()) << 32) | Rnd();
            Datagram frame = RandomJunk(kMaxAudioPayload);
            if (frame.empty()) frame.push_back(uint8_t(Rnd()));
            n = BuildAudioPacket(buf, Rnd(), ah, frame);
            break;
        }
        default: {
            FecHeader fh{};
            fh.frameId = Rnd();
            fh.timestampUs = (uint64_t(Rnd()) << 32) | Rnd();
            fh.pktCount = uint16_t(1 + Rnd() % 2000);
            const size_t numGroups = (size_t(fh.pktCount) + kFecGroupSize - 1) / kFecGroupSize;
            fh.groupIndex = uint8_t(Rnd() % numGroups);
            Datagram parity = RandomJunk(kMaxVideoPayload);
            parity.resize(parity.size() + kFecLenPrefix);
            n = BuildFecPacket(buf, Rnd(), fh, Rnd() % 2 != 0, parity);
            break;
        }
    }
    return Taken(buf, n);
}

Datagram Mutate(Datagram d) {
    switch (Rnd() % 4) {
        case 0: {
            const uint32_t flips = 1 + Rnd() % 8;
            for (uint32_t i = 0; i < flips && !d.empty(); ++i)
                d[Rnd() % d.size()] ^= uint8_t(1u << (Rnd() % 8));
            break;
        }
        case 1:
            d.resize(Rnd() % (d.size() + 1));
            break;
        case 2: {
            const Datagram extra = RandomJunk(64);
            d.insert(d.end(), extra.begin(), extra.end());
            break;
        }
        default: {
            if (d.empty()) break;
            const size_t at = Rnd() % d.size();
            const size_t count = 1 + Rnd() % 16;
            for (size_t i = 0; i < count && at + i < d.size(); ++i) d[at + i] = uint8_t(Rnd());
            break;
        }
    }
    return d;
}

void TestWireTruncationSweep() {
    std::printf("[fuzz] every prefix of every valid message through every parser...\n");
    bool built = true;
    bool ok = true;
    for (int i = 0; i < 64; ++i) {
        const Datagram d = BuildRandomValidDatagram();
        built = built && !d.empty();
        if (d.empty()) continue;
        ok = ok && ParseCommonHeader(d).has_value();
        for (size_t cut = 0; cut <= d.size(); ++cut)
            ok = ok && ExerciseWireParsers(std::span<const uint8_t>(d).first(cut));
    }
    Check(built, "every random valid message builds");
    Check(ok, "prefix sweep keeps every parser invariant");
}

void TestWireMutationFuzz() {
    std::printf("[fuzz] 3000 mutated datagrams through every parser...\n");
    bool ok = true;
    for (int i = 0; i < 3000; ++i) {
        Datagram d = Mutate(BuildRandomValidDatagram());
        if (Rnd() % 4 == 0) d = Mutate(std::move(d));
        ok = ok && ExerciseWireParsers(d);
    }
    Check(ok, "mutation fuzz keeps every parser invariant");
}

void TestWireRandomRoundTrips() {
    std::printf("[fuzz] random field values survive build -> parse...\n");
    uint8_t buf[kMaxDatagram];

    bool ok = true;
    for (int i = 0; i < 300; ++i) {
        const Hello m{Rnd(), uint16_t(Rnd()), uint16_t(Rnd()), uint16_t(Rnd()),
            uint8_t(Rnd()), uint16_t(Rnd()), uint8_t(Rnd()), PasscodeFromRandom(Rnd())};
        const size_t n = BuildHello(buf, m);
        const auto p = ParseHello(PayloadOf(std::span<const uint8_t>(buf, n)));
        ok = ok && p && p->clientId == m.clientId && p->codecMask == m.codecMask &&
             p->maxWidth == m.maxWidth && p->maxHeight == m.maxHeight &&
             p->desiredFps == m.desiredFps && p->features == m.features &&
             p->sourceId == m.sourceId && p->passcode == m.passcode;
    }
    Check(ok, "HELLO random round-trip");

    ok = true;
    for (int i = 0; i < 300; ++i) {
        const HelloAck m{Rnd(), (Rnd() % 2) ? Codec::H264 : Codec::Rejected, uint16_t(Rnd()),
            uint16_t(Rnd()), uint8_t(Rnd()), Rnd(), (uint64_t(Rnd()) << 32) | Rnd(),
            RejectReason(Rnd() % 4)};
        const size_t n = BuildHelloAck(buf, m);
        const auto p = ParseHelloAck(PayloadOf(std::span<const uint8_t>(buf, n)));
        ok = ok && p && p->sessionId == m.sessionId && p->codec == m.codec &&
             p->width == m.width && p->height == m.height && p->fps == m.fps &&
             p->bitrateBps == m.bitrateBps && p->timebaseUs == m.timebaseUs &&
             p->reason == m.reason;
    }
    Check(ok, "HELLO_ACK random round-trip");

    ok = true;
    for (int i = 0; i < 300; ++i) {
        const Feedback m{uint16_t(Rnd()), uint8_t(Rnd()), uint16_t(Rnd()), Rnd()};
        const size_t n = BuildFeedback(buf, Rnd(), m);
        const auto p = ParseFeedback(PayloadOf(std::span<const uint8_t>(buf, n)));
        ok = ok && p && p->lostFrames == m.lostFrames && p->lossPct == m.lossPct &&
             p->rttMs == m.rttMs && p->recvBitrateKbps == m.recvBitrateKbps;
    }
    Check(ok, "FEEDBACK random round-trip");

    ok = true;
    for (int i = 0; i < 300; ++i) {
        const Reconfig m{uint16_t(Rnd()), uint16_t(Rnd()), Rnd(), uint8_t(Rnd())};
        const size_t n = BuildReconfig(buf, Rnd(), m);
        const auto p = ParseReconfig(PayloadOf(std::span<const uint8_t>(buf, n)));
        ok = ok && p && p->width == m.width && p->height == m.height &&
             p->bitrateBps == m.bitrateBps && p->fps == m.fps;
    }
    Check(ok, "RECONFIG random round-trip");

    ok = true;
    for (int i = 0; i < 300; ++i) {
        const PingPong m{Rnd(), (uint64_t(Rnd()) << 32) | Rnd()};
        const size_t n = BuildPing(buf, Rnd(), m);
        const auto p = ParsePingPong(PayloadOf(std::span<const uint8_t>(buf, n)));
        ok = ok && p && p->pingId == m.pingId && p->sendTimeUs == m.sendTimeUs;
    }
    Check(ok, "PING random round-trip");

    ok = true;
    for (int i = 0; i < 200; ++i) {
        std::vector<uint16_t> indices(1 + Rnd() % kMaxNackIndices);
        for (auto& idx : indices) idx = uint16_t(Rnd());
        const uint32_t frameIn = Rnd();
        const size_t n = BuildNack(buf, Rnd(), frameIn, indices);
        uint32_t frameOut = 0;
        uint16_t got[kMaxNackIndices];
        const size_t count =
            ParseNack(PayloadOf(std::span<const uint8_t>(buf, n)), frameOut, got);
        ok = ok && n > 0 && count == indices.size() && frameOut == frameIn;
        for (size_t k = 0; ok && k < count; ++k) ok = got[k] == indices[k];
    }
    Check(ok, "NACK random round-trip");

    ok = true;
    for (int i = 0; i < 200; ++i) {
        std::vector<InputEvent> events(1 + Rnd() % kMaxInputEvents);
        for (auto& ev : events) {
            ev.type = InputType(1 + Rnd() % 4);
            ev.timestampUs = (uint64_t(Rnd()) << 32) | Rnd();
            ev.a = int32_t(Rnd());
            ev.b = int32_t(Rnd());
            ev.state = uint8_t(Rnd());
            ev.absolute = uint8_t(Rnd());
        }
        const uint32_t seqIn = Rnd();
        const size_t n = BuildInputEvents(buf, Rnd(), seqIn, events);
        uint32_t seqOut = 0;
        InputEvent got[kMaxInputEvents];
        const size_t count =
            ParseInputEvents(PayloadOf(std::span<const uint8_t>(buf, n)), seqOut, got);
        ok = ok && n > 0 && count == events.size() && seqOut == seqIn;
        for (size_t k = 0; ok && k < count; ++k)
            ok = got[k].type == events[k].type && got[k].timestampUs == events[k].timestampUs &&
                 got[k].a == events[k].a && got[k].b == events[k].b &&
                 got[k].state == events[k].state && got[k].absolute == events[k].absolute;
    }
    Check(ok, "INPUT_EVENT random round-trip");

    ok = true;
    for (int i = 0; i < 200; ++i) {
        VideoHeader vh{Rnd(), (uint64_t(Rnd()) << 32) | Rnd(), 0, uint16_t(1 + Rnd() % 1000)};
        vh.pktIndex = uint16_t(Rnd() % vh.pktCount);
        const Datagram payload = RandomJunk(kMaxVideoPayload);
        const bool idr = Rnd() % 2 != 0;
        const bool frameEnd = Rnd() % 2 != 0;
        const size_t n = BuildVideoPacket(buf, Rnd(), vh, idr, frameEnd, payload);
        const std::span<const uint8_t> d(buf, n);
        const auto h = ParseCommonHeader(d);
        ok = ok && n > 0 && h.has_value();
        if (!ok) break;
        const auto v = ParseVideoPacket(*h, PayloadOf(d));
        ok = v && v->hdr.frameId == vh.frameId && v->hdr.timestampUs == vh.timestampUs &&
             v->hdr.pktIndex == vh.pktIndex && v->hdr.pktCount == vh.pktCount &&
             v->idr == idr && v->frameEnd == frameEnd && v->payload.size() == payload.size() &&
             std::equal(payload.begin(), payload.end(), v->payload.begin());
    }
    Check(ok, "VIDEO_PACKET random round-trip");

    ok = true;
    for (int i = 0; i < 200; ++i) {
        FecHeader fh{Rnd(), (uint64_t(Rnd()) << 32) | Rnd(), uint16_t(1 + Rnd() % 2000), 0};
        const size_t numGroups = (size_t(fh.pktCount) + kFecGroupSize - 1) / kFecGroupSize;
        fh.groupIndex = uint8_t(Rnd() % numGroups);
        Datagram parity = RandomJunk(kMaxVideoPayload);
        parity.resize(parity.size() + kFecLenPrefix);
        const bool idr = Rnd() % 2 != 0;
        const size_t n = BuildFecPacket(buf, Rnd(), fh, idr, parity);
        const std::span<const uint8_t> d(buf, n);
        const auto h = ParseCommonHeader(d);
        ok = ok && n > 0 && h.has_value();
        if (!ok) break;
        const auto f = ParseFecPacket(*h, PayloadOf(d));
        ok = f && f->hdr.frameId == fh.frameId && f->hdr.timestampUs == fh.timestampUs &&
             f->hdr.pktCount == fh.pktCount && f->hdr.groupIndex == fh.groupIndex &&
             f->idr == idr && f->parity.size() == parity.size() &&
             std::equal(parity.begin(), parity.end(), f->parity.begin());
    }
    Check(ok, "FEC_PACKET random round-trip");
}

Datagram MakeStartCodeSoup(size_t maxLen) {
    static constexpr uint8_t kNalish[] = {0x65, 0x41, 0x67, 0x68, 0x06, 0x09, 0x25, 0x45};
    Datagram d(Rnd() % (maxLen + 1));
    for (auto& b : d) {
        const uint32_t roll = Rnd() % 16;
        if (roll < 6)
            b = 0;
        else if (roll < 8)
            b = 1;
        else if (roll < 11)
            b = kNalish[Rnd() % sizeof(kNalish)];
        else
            b = uint8_t(Rnd());
    }
    return d;
}

void TestAnnexBParseFuzz() {
    std::printf("[fuzz] ParseAnnexB invariants over 800 start-code soups...\n");
    bool ok = true;
    for (int i = 0; i < 800; ++i) {
        const Datagram d = MakeStartCodeSoup(600);
        const auto refs = media::ParseAnnexB(d);
        size_t prevEnd = 0;
        bool anyIdr = false;
        bool haveVcl = false;
        size_t firstVcl = 0;
        for (const auto& r : refs) {
            ok = ok && r.size > 0;
            ok = ok && (r.startCode == 3 || r.startCode == 4);
            ok = ok && r.offset >= r.startCode && r.offset + r.size <= d.size();
            ok = ok && r.offset - r.startCode >= prevEnd;
            ok = ok && r.header == d[r.offset];
            ok = ok && media::StartCodeLengthAt(d, r.offset - r.startCode) == r.startCode;
            if (!haveVcl && media::IsH264Vcl(r.header)) {
                haveVcl = true;
                firstVcl = r.offset - r.startCode;
            }
            anyIdr = anyIdr || media::IsH264Idr(r.header);
            prevEnd = r.offset + r.size;
        }
        ok = ok && media::ContainsIdr(d) == anyIdr;
        ok = ok && media::FirstVclOffset(d) == (haveVcl ? firstVcl : 0);
    }
    Check(ok, "ParseAnnexB/ContainsIdr/FirstVclOffset agree on fuzzed streams");
}

void TestAvccFuzz() {
    std::printf("[fuzz] AvccToAnnexB on junk + length-prefixed round-trip...\n");
    bool ok = true;
    for (int i = 0; i < 500; ++i) {
        const Datagram junk = RandomJunk(400);
        const size_t lengthSize = Rnd() % 10;
        const auto out = media::AvccToAnnexB(junk, lengthSize);
        ok = ok && out.size() <= junk.size() * 3;
        if (!lengthSize || lengthSize > 8) ok = ok && out.empty();
    }
    Check(ok, "AvccToAnnexB stays bounded on junk buffers");

    ok = true;
    for (int i = 0; i < 300; ++i) {
        const size_t lengthSize = 1 + Rnd() % 4;
        const size_t maxNal = lengthSize == 1 ? 255 : 600;
        std::vector<Datagram> nals;
        std::vector<uint8_t> avcc;
        const size_t count = 1 + Rnd() % 6;
        for (size_t k = 0; k < count; ++k) {
            Datagram nal(1 + Rnd() % maxNal);
            for (auto& b : nal) b = uint8_t(0x20 + Rnd() % 0xE0);
            media::AppendLengthPrefixed(avcc, nal, lengthSize);
            nals.push_back(std::move(nal));
        }
        const auto annexb = media::AvccToAnnexB(avcc, lengthSize);
        const auto refs = media::ParseAnnexB(annexb);
        ok = ok && refs.size() == nals.size();
        for (size_t k = 0; ok && k < refs.size(); ++k)
            ok = refs[k].size == nals[k].size() &&
                 std::equal(nals[k].begin(), nals[k].end(),
                     annexb.begin() + ptrdiff_t(refs[k].offset));
    }
    Check(ok, "AppendLengthPrefixed -> AvccToAnnexB -> ParseAnnexB round-trips");
}

std::vector<uint8_t> MakeBaselineSpsNal(uint32_t refFrames, uint32_t widthMbs,
    uint32_t heightMapUnits) {
    media::BitWriter w;
    w.StartNal(3, 7);
    w.U(8, 66);
    w.U(8, 0);
    w.U(8, 42);
    w.UE(0);
    w.UE(0);
    w.UE(2);
    w.UE(refFrames);
    w.U(1, 0);
    w.UE(widthMbs - 1);
    w.UE(heightMapUnits - 1);
    w.U(1, 1);
    w.U(1, 1);
    w.U(1, 0);
    w.U(1, 0);
    w.Trailing();
    return std::vector<uint8_t>(w.bytes().begin() + 4, w.bytes().end());
}

void TestSpsMutationFuzz() {
    std::printf("[fuzz] 1500 bit-flipped SPS NALs through the VUI rewrite...\n");
    bool ok = true;
    for (int i = 0; i < 1500; ++i) {
        std::vector<uint8_t> nal =
            MakeBaselineSpsNal(1 + Rnd() % 16, 1 + Rnd() % 120, 1 + Rnd() % 68);
        const uint32_t flips = 1 + Rnd() % 6;
        for (uint32_t f = 0; f < flips; ++f)
            nal[Rnd() % nal.size()] ^= uint8_t(1u << (Rnd() % 8));
        const auto patched = media::AnnexBSpsWithZeroReorder(nal);
        if (patched.empty()) continue;
        ok = ok && patched.size() > 5;
        ok = ok && patched[0] == 0 && patched[1] == 0 && patched[2] == 0 && patched[3] == 1;
        ok = ok && (patched[4] & 0x1F) == 7;
        const auto inner = std::span<const uint8_t>(patched).subspan(4);
        ok = ok && media::AnnexBSpsWithZeroReorder(inner).empty();
    }
    Check(ok, "every accepted mutant rewrites to a well-formed SPS with a VUI");
}

void TestSpsStreamFuzz() {
    std::printf("[fuzz] the stream rewrite is a fixpoint on 500 fuzzed streams...\n");
    bool ok = true;
    for (int i = 0; i < 500; ++i) {
        Datagram d = MakeStartCodeSoup(500);
        if (Rnd() % 2 == 0) {
            std::vector<uint8_t> sps{0, 0, 0, 1};
            const auto nal = MakeBaselineSpsNal(1 + Rnd() % 4, 1 + Rnd() % 120, 1 + Rnd() % 68);
            sps.insert(sps.end(), nal.begin(), nal.end());
            const size_t at = d.empty() ? 0 : Rnd() % d.size();
            d.insert(d.begin() + ptrdiff_t(at), sps.begin(), sps.end());
        }
        const auto out = media::AnnexBStreamWithZeroReorder(d);
        if (out.empty()) continue;
        ok = ok && media::AnnexBStreamWithZeroReorder(out).empty();
    }
    Check(ok, "rewriting a rewritten stream finds nothing left to change");
}

void TaintFrameOf(const Datagram& d, std::unordered_set<uint32_t>& tainted) {
    const auto h = ParseCommonHeader(d);
    if (!h) return;
    if (h->type == MsgType::VideoPacket) {
        if (const auto v = ParseVideoPacket(*h, PayloadOf(d))) tainted.insert(v->hdr.frameId);
    } else if (h->type == MsgType::FecPacket) {
        if (const auto v = ParseFecPacket(*h, PayloadOf(d))) tainted.insert(v->hdr.frameId);
    }
}

void ChaosFeed(Reassembler& ra, const Datagram& d, uint64_t nowUs) {
    const auto h = ParseCommonHeader(d);
    if (!h) return;
    if (h->type == MsgType::FecPacket) {
        if (const auto v = ParseFecPacket(*h, PayloadOf(d))) ra.PushFec(*v, nowUs);
        return;
    }
    if (h->type == MsgType::VideoPacket) {
        if (const auto v = ParseVideoPacket(*h, PayloadOf(d))) ra.Push(*v, nowUs);
    }
}

void TestReassemblerChaosFuzz() {
    std::printf("[fuzz] reassembler under drop + dup + reorder + corruption...\n");
    for (int round = 0; round < 8; ++round) {
        Packetizer pk;
        pk.SetSessionId(42);
        pk.SetFecEnabled(round % 2 == 1);
        Reassembler ra(16'667);
        const auto frames = MakeFrames(50, 10);
        std::unordered_map<uint32_t, const TestFrame*> byId;
        for (const auto& f : frames) byId[f.id] = &f;
        std::unordered_set<uint32_t> tainted;
        uint64_t now = 1'000'000;
        bool ok = true;
        size_t completed = 0;
        for (size_t i = 0; i + 1 < frames.size(); i += 2) {
            std::vector<Datagram> batch = Packetize(pk, frames[i], now);
            auto more = Packetize(pk, frames[i + 1], now + 16'667);
            batch.insert(batch.end(), std::make_move_iterator(more.begin()),
                std::make_move_iterator(more.end()));
            for (size_t k = batch.size(); k > 1; --k)
                std::swap(batch[k - 1], batch[Rnd() % k]);

            std::vector<Datagram> wire;
            for (auto& d : batch) {
                const uint32_t roll = Rnd() % 100;
                if (roll < 10) continue;
                if (roll < 15) {
                    Datagram m = d;
                    m[Rnd() % m.size()] ^= uint8_t(1u << (Rnd() % 8));
                    TaintFrameOf(m, tainted);
                    wire.push_back(std::move(m));
                    continue;
                }
                if (roll < 20) wire.push_back(d);
                wire.push_back(std::move(d));
            }
            for (const auto& d : wire) ChaosFeed(ra, d, now);

            while (auto out = ra.PopReady(now)) {
                ++completed;
                ok = ok && !out->nal.empty();
                const auto it = byId.find(out->frameId);
                if (it != byId.end() && tainted.count(out->frameId) == 0)
                    ok = ok && out->nal == it->second->nal && out->idr == it->second->idr;
            }
            now += 2 * 16'667;
            uint32_t nackFrame = 0;
            uint16_t nackIdx[16];
            ra.PlanNack(now, 20'000, nackFrame, nackIdx);
            ra.TakeLossEvent();
            ra.TakeMaxGapMs();
        }
        ok = ok && ra.stats().framesCompleted == completed;
        ok = ok && ra.stats().framesCompleted + ra.stats().framesDropped +
                           ra.stats().framesSkipped <=
                       frames.size() + tainted.size();
        Check(ok, "chaos round: completed frames intact, stats consistent");
    }
}

std::string MakeSettingsSoup() {
    static constexpr const char* kKeys[] = {"fps", "bitrate_mbps", "max_dim", "port",
        "allow_input", "client_control", "passcode", "junk_key"};
    std::string s;
    const size_t lines = Rnd() % 12;
    for (size_t i = 0; i < lines; ++i) {
        switch (Rnd() % 3) {
            case 0:
                s += kKeys[Rnd() % 8];
                s += '=';
                s += std::to_string(Rnd() % 1'000'000);
                break;
            case 1:
                s += kKeys[Rnd() % 8];
                s += '=';
                for (uint32_t k = Rnd() % 12; k > 0; --k) s += char(uint8_t(Rnd()));
                break;
            default:
                for (uint32_t k = Rnd() % 24; k > 0; --k) s += char(uint8_t(Rnd()));
                break;
        }
        s += '\n';
    }
    return s;
}

bool SettingsWithinBounds(const ui::UiSettings& s) {
    return s.fps >= 1 && s.fps <= ui::kMaxSettingsFps && s.bitrateMbps >= 1 &&
           s.bitrateMbps <= ui::kMaxSettingsBitrateMbps && s.maxDim <= ui::kMaxSettingsDim &&
           s.port >= 1 && s.port <= ui::kMaxSettingsPort &&
           (s.passcode.empty() || IsValidPasscode(s.passcode));
}

void TestUiSettingsFuzz() {
    std::printf("[fuzz] 800 settings soups: bounds hold, serialize is a fixpoint...\n");
    bool ok = true;
    for (int i = 0; i < 800; ++i) {
        const ui::UiSettings parsed = ui::ParseUiSettings(MakeSettingsSoup());
        ok = ok && SettingsWithinBounds(parsed);
        ok = ok && ui::ParseUiSettings(ui::SerializeUiSettings(parsed)) == parsed;
    }
    Check(ok, "fuzzed settings stay within bounds and round-trip through serialize");
}

std::string MakeDevicesSoup() {
    std::string s;
    const size_t lines = Rnd() % 16;
    for (size_t i = 0; i < lines; ++i) {
        switch (Rnd() % 4) {
            case 0:
                s += std::to_string(Rnd());
                s += " 192.168.1.";
                s += std::to_string(Rnd() % 256);
                break;
            case 1:
                s += std::to_string(Rnd());
                s += " host";
                s += std::to_string(Rnd() % 6);
                s += ' ';
                s += (Rnd() % 2) ? ui::EncodeSecret(PasscodeFromRandom(Rnd()))
                                 : std::string("not-a-passcode");
                break;
            case 2:
                s += std::to_string(Rnd());
                s += "  spaced   out  ";
                break;
            default:
                for (uint32_t k = Rnd() % 24; k > 0; --k) s += char(uint8_t(Rnd()));
                break;
        }
        s += '\n';
    }
    return s;
}

void TestRecentDevicesFuzz() {
    std::printf("[fuzz] 800 device-list soups: cap, uniqueness, round-trip...\n");
    bool ok = true;
    for (int i = 0; i < 800; ++i) {
        const auto parsed = ui::ParseRecentDevices(MakeDevicesSoup());
        ok = ok && parsed.size() <= ui::kMaxRecentDevices;
        std::unordered_set<std::string> addrs;
        for (const auto& d : parsed) {
            ok = ok && !d.addr.empty();
            ok = ok && d.lastConnectedUnix >= 0;
            ok = ok && (d.passcode.empty() || IsValidPasscode(d.passcode));
            ok = ok && addrs.insert(d.addr).second;
        }
        ok = ok && ui::ParseRecentDevices(ui::SerializeRecentDevices(parsed)) == parsed;
    }
    Check(ok, "fuzzed device lists keep their invariants and round-trip");
}

bool ValidOutbound(std::span<const uint8_t> d) {
    return !d.empty() && d.size() <= kMaxDatagram && ParseCommonHeader(d).has_value();
}

Datagram BuildKnownHello(uint32_t clientId) {
    uint8_t buf[kMaxDatagram];
    const Hello m{clientId, 1, 1920, 1080, 30, 0, 0, kTestPasscode};
    return Taken(buf, BuildHello(buf, m));
}

void TestSessionChaosFuzz() {
    std::printf("[fuzz] 6 rounds of datagram chaos through host + client + beacon...\n");
    for (int round = 0; round < 6; ++round) {
        bool outboundOk = true;
        const auto witness = [&](std::span<const uint8_t> d) {
            outboundOk = outboundOk && ValidOutbound(d);
        };

        ScreenHostCallbacks hostCb;
        hostCb.send = witness;
        hostCb.sendTo = [&](uint64_t, std::span<const uint8_t> d) { witness(d); };
        hostCb.randomBytes = TestRandomBytes;
        ScreenHostSession host(std::move(hostCb), StreamParams{1280, 720, 30, 8'000'000});
        host.SetPasscode(kTestPasscode);

        ScreenClientSessionCallbacks clientCb;
        clientCb.send = witness;
        ScreenClientSession client(std::move(clientCb));
        client.Start(Hello{1, 1, 1920, 1080, 30, 0, 0, kTestPasscode}, 1);

        Beacon beacon;
        const SourceInfo sources[2] = {
            {0, 1280, 720, "Display 1"}, {1, 1920, 1080, "Display 2"}};
        beacon.SetSources(sources);
        beacon.SetPasscode(kTestPasscode);

        uint8_t reply[kMaxDatagram];
        uint64_t now = 1'000'000;
        bool ok = true;
        for (int i = 0; i < 250; ++i) {
            Datagram d;
            if (i % 40 == 0)
                d = BuildKnownHello(100 + uint32_t(i));
            else if (Rnd() % 4 == 0)
                d = RandomJunk(kMaxDatagram);
            else
                d = Mutate(BuildRandomValidDatagram());
            const uint64_t from = kTestViewer + (Rnd() % 7);
            host.HandlePacket(d, now, from);
            client.HandlePacket(d, now);
            const size_t n = beacon.Reply(reply, d);
            ok = ok && n <= sizeof(reply);
            ok = ok && (n == 0 || ValidOutbound(std::span<const uint8_t>(reply, n)));
            now += (Rnd() % 50 == 0) ? kSessionTimeoutUs + 1 : 20'000;
            host.Tick(now);
            client.Tick(now);
            ok = ok && host.viewerCount() <= kMaxViewersPerHost;
            ok = ok && uint8_t(host.state()) <= uint8_t(ScreenHostSession::State::Streaming);
            ok = ok && uint8_t(client.state()) <= uint8_t(ScreenClientSession::State::Dead);
        }
        Check(ok && outboundOk,
            "session chaos: replies well-formed, viewer table bounded, states sane");
    }
}

void TestSecretTextFuzz() {
    std::printf("[fuzz] secrets round-trip, junk is decoded safely...\n");
    bool ok = true;
    for (int i = 0; i < 600; ++i) {
        std::string plain(Rnd() % 40, '\0');
        for (auto& c : plain) c = char(uint8_t(Rnd()));
        const std::string encoded = ui::EncodeSecret(plain);
        ok = ok && ui::DecodeSecret(encoded) == plain;
        if (!plain.empty()) ok = ok && encoded.front() == ui::kSecretPrefix;
    }
    Check(ok, "EncodeSecret/DecodeSecret round-trips random byte strings");

    ok = true;
    for (int i = 0; i < 600; ++i) {
        std::string junk(Rnd() % 40, '\0');
        for (auto& c : junk) c = char(uint8_t(Rnd()));
        const std::string decoded = ui::DecodeSecret(junk);
        if (junk.empty() || junk.front() != ui::kSecretPrefix) ok = ok && decoded == junk;
    }
    Check(ok, "DecodeSecret leaves unprefixed text alone and never crashes on junk");
}

}

void RunFuzzTests() {
    TestWireTruncationSweep();
    TestWireMutationFuzz();
    TestWireRandomRoundTrips();
    TestAnnexBParseFuzz();
    TestAvccFuzz();
    TestSpsMutationFuzz();
    TestSpsStreamFuzz();
    TestReassemblerChaosFuzz();
    TestSessionChaosFuzz();
    TestUiSettingsFuzz();
    TestRecentDevicesFuzz();
    TestSecretTextFuzz();
}
