#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/Beacon.h"

#include <cstdio>

using namespace deskhub;

namespace {

std::vector<uint8_t> Ask(const Beacon& b, std::span<const uint8_t> req, bool trusted = false) {
    uint8_t out[kMaxDatagram];
    const size_t n = b.Reply(out, req, trusted);
    return std::vector<uint8_t>(out, out + n);
}

void TestBeaconSourcesAndProbe() {
    std::printf("[disc] Beacon: LIST_SOURCES and the session-less PING probe...\n");
    Beacon b;
    SourceInfo s;
    s.sourceId = 3;
    s.width = 3840;
    s.height = 2160;
    s.name = "DELL U2723QE";
    b.SetSources(std::span<const SourceInfo>(&s, 1));
    b.SetPasscode(kTestPasscode);

    uint8_t req[kMaxDatagram];
    size_t rn = BuildListSources(req, kTestPasscode);
    const auto rep = Ask(b, std::span<const uint8_t>(req, rn), true);
    const auto h = ParseCommonHeader(rep);
    Check(h && h->type == MsgType::SourceList, "LIST_SOURCES -> SOURCE_LIST");
    SourceInfo got[kMaxSources];
    Check(ParseSourceList(PayloadOf(rep), got) == 1, "one source listed");
    Check(got[0].sourceId == 3 && got[0].name == "DELL U2723QE",
        "the display entry survives the round trip");

    b.SetSources({});
    const auto rep2 = Ask(b, std::span<const uint8_t>(req, rn));
    const auto h2 = ParseCommonHeader(rep2);
    Check(h2 && h2->type == MsgType::SourceList, "a host sharing nothing still answers");
    Check(ParseSourceList(PayloadOf(rep2), got) == 0, "...with an empty list");

    PingPong p{7, 123'456};
    rn = BuildPing(req, 0, p);
    const auto pong = Ask(b, std::span<const uint8_t>(req, rn));
    const auto ph = ParseCommonHeader(pong);
    Check(ph && ph->type == MsgType::Pong && ph->sessionId == 0, "PING sid=0 -> PONG sid=0");
    const auto pp = ParsePingPong(PayloadOf(pong));
    Check(pp && pp->pingId == 7 && pp->sendTimeUs == 123'456,
        "PONG echoes the payload verbatim so RTT is one subtraction");

    rn = BuildPing(req, 42, p);
    Check(Ask(b, std::span<const uint8_t>(req, rn)).empty(),
        "in-session PING is not the Beacon's business");
}

void TestBeaconHidesSourcesBehindThePasscode() {
    std::printf("[disc] Beacon: a passcode hides what is shared but not that we are alive...\n");
    Beacon b;
    SourceInfo s;
    s.sourceId = 0;
    s.width = 3440;
    s.height = 1440;
    s.name = "DELL U2723QE";
    b.SetSources(std::span<const SourceInfo>(&s, 1));
    b.SetPasscode("4726");

    uint8_t req[kMaxDatagram];
    SourceInfo got[kMaxSources];

    size_t rn = BuildListSources(req);
    auto rep = Ask(b, std::span<const uint8_t>(req, rn));
    auto h = ParseCommonHeader(rep);
    Check(h && h->type == MsgType::SourceList,
        "a query without the passcode is still answered, so the host reads as online");
    Check(ParseSourceList(PayloadOf(rep), got) == 0,
        "but it learns nothing about the displays");

    rn = BuildListSources(req, "1111");
    Check(ParseSourceList(PayloadOf(Ask(b, std::span<const uint8_t>(req, rn))), got) == 0,
        "a wrong passcode learns nothing either");

    rn = BuildListSources(req, "4726");
    Check(ParseSourceList(PayloadOf(Ask(b, std::span<const uint8_t>(req, rn))), got) == 0,
        "and so does the RIGHT passcode, offered by a stranger over plain UDP");
    Check(ParseSourceList(PayloadOf(Ask(b, std::span<const uint8_t>(req, rn), true)), got) == 1,
        "the list comes back only once the asker is inside an authenticated connection");

    rn = BuildListSources(req);
    Check(ParseSourceList(PayloadOf(Ask(b, std::span<const uint8_t>(req, rn), true)), got) == 1,
        "at which point it no longer has to carry a passcode at all");
}

void TestBeaconTellsAuthenticatedAskersWhatTheHostCanDo() {
    std::printf("[disc] Beacon: what the host can do travels with the source list...\n");
    Beacon b;
    SourceInfo s;
    s.sourceId = 0;
    s.width = 1080;
    s.height = 2400;
    s.name = "Pixel";
    b.SetSources(std::span<const SourceInfo>(&s, 1));
    b.SetCaps(HostCaps{false, false});

    uint8_t req[kMaxDatagram];
    const size_t rn = BuildListSources(req);

    auto h = ParseCommonHeader(Ask(b, std::span<const uint8_t>(req, rn), true));
    Check(h && !HostCapsOfFlags(h->flags).acceptsInput && !HostCapsOfFlags(h->flags).terminal,
        "a phone answers that it takes no input and has no shell");

    b.SetCaps(HostCaps{true, true});
    h = ParseCommonHeader(Ask(b, std::span<const uint8_t>(req, rn), true));
    Check(h && HostCapsOfFlags(h->flags).acceptsInput && HostCapsOfFlags(h->flags).terminal,
        "a desktop that shares both says so");

    h = ParseCommonHeader(Ask(b, std::span<const uint8_t>(req, rn)));
    Check(h && !HostCapsOfFlags(h->flags).acceptsInput && !HostCapsOfFlags(h->flags).terminal,
        "a stranger over plain UDP is told nothing about either");
}

void TestBeaconIgnoresSessionTraffic() {
    std::printf("[disc] Beacon: leaves session traffic alone...\n");
    const Beacon b;
    uint8_t req[kMaxDatagram];

    Hello hello{};
    hello.clientId = 5;
    hello.codecMask = kCodecMaskH264;
    size_t rn = BuildHello(req, hello);
    Check(Ask(b, std::span<const uint8_t>(req, rn)).empty(), "HELLO belongs to ScreenHostSession");

    rn = BuildRequestKeyframe(req, 9, KeyframeReason::Loss);
    Check(Ask(b, std::span<const uint8_t>(req, rn)).empty(), "REQUEST_KEYFRAME ignored");

    const uint8_t junk[3] = {0xFF, 0x00, 0x7E};
    Check(Ask(b, junk).empty(), "a truncated/garbage datagram gets no reply");
    Check(Ask(b, {}).empty(), "an empty datagram gets no reply");
}

}

void RunBeaconTests() {
    TestBeaconSourcesAndProbe();
    TestBeaconHidesSourcesBehindThePasscode();
    TestBeaconTellsAuthenticatedAskersWhatTheHostCanDo();
    TestBeaconIgnoresSessionTraffic();
}
