#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/ShareTypes.h"
#include "deskhub/session/client/ScreenClient.h"
#include "deskhub/transport/FecScheme.h"
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

using namespace deskhub;

namespace {

struct FecCase {
    size_t pktsPerFrame;
    std::vector<size_t> dropIdx;
};

enum class Delivery { None,
    Identical,
    Corrupted };

Delivery Deliver(std::string_view scheme, size_t packets, size_t groups,
    const std::vector<size_t>& dropDataIdx, bool corruptParity) {
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    if (!pk.SetFecScheme(scheme)) return Delivery::None;
    pk.SetFecGroups(groups);

    Reassembler ra(16'667);
    if (!ra.SetFecScheme(scheme)) return Delivery::None;

    TestFrame f{0, true, {}};
    f.nal.resize(packets * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    auto pkts = Packetize(pk, f, 1'000'000);
    std::vector<size_t> pos;
    for (size_t d : dropDataIdx) pos.push_back(NthDataPacket(pkts, d));
    std::sort(pos.begin(), pos.end(), std::greater<size_t>());
    for (size_t p : pos)
        if (p < pkts.size()) pkts.erase(pkts.begin() + p);

    for (auto& d : pkts) {
        if (corruptParity && IsFec(d)) {
            d[kCommonHeaderSize + kFecHeaderSize] ^= 0xFF;
            d[kCommonHeaderSize + kFecHeaderSize + 1] ^= 0xFF;
        }
        Feed(ra, d, 1'000'000);
    }

    const auto out = ra.PopReady(1'000'000);
    if (!out) return Delivery::None;
    return SameFrame(*out, f) ? Delivery::Identical : Delivery::Corrupted;
}

std::vector<size_t> LossesPerGroup(size_t packets, size_t groups, size_t perGroup) {
    std::vector<size_t> idx;
    const size_t effective = FecGroupCount(uint16_t(packets), uint8_t(groups));
    for (size_t group = 0; group < effective; ++group) {
        size_t taken = 0;
        for (size_t i = group; i < packets && taken < perGroup; i += effective, ++taken)
            idx.push_back(i);
    }
    return idx;
}

std::vector<size_t> Burst(size_t start, size_t len) {
    std::vector<size_t> idx;
    for (size_t i = 0; i < len; ++i) idx.push_back(start + i);
    return idx;
}

void RunFecCase(const FecCase& c, std::vector<uint32_t>& got, Reassembler::Stats& stats,
    std::vector<TestFrame>& frames) {
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Reassembler ra(16'667);
    frames.clear();
    for (uint32_t i = 0; i < 20; ++i) {
        TestFrame f{i, (i % 10) == 0, {}};
        f.nal.resize(c.pktsPerFrame * kMaxVideoPayload - 100);
        for (auto& b : f.nal) b = uint8_t(Rnd());
        frames.push_back(std::move(f));
    }
    uint64_t now = 1'000'000;
    for (const auto& f : frames) {
        auto pkts = Packetize(pk, f, now);
        if (!f.idr) {
            std::vector<size_t> pos;
            for (size_t d : c.dropIdx) pos.push_back(NthDataPacket(pkts, d));
            std::sort(pos.begin(), pos.end(), std::greater<size_t>());
            for (size_t p : pos)
                if (p < pkts.size()) pkts.erase(pkts.begin() + p);
        }
        for (const auto& d : pkts) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) got.push_back(out->frameId);
        now += 16'667;
    }
    stats = ra.stats();
}

void TestFecDisabledByDefault() {
    std::printf("[fec] off by default -> no parity packets on the wire...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    TestFrame f{0, true, {}};
    f.nal.resize(3 * kMaxVideoPayload);
    for (auto& b : f.nal) b = uint8_t(Rnd());
    const auto pkts = Packetize(pk, f, 1'000'000);
    Check(pkts.size() == 3, "3 data packets, no parity when FEC disabled");
    for (const auto& d : pkts) Check(!IsFec(d), "no FEC packet when disabled");
}

void TestFecRecoverOne() {
    std::printf("[fec] 1 packet lost per frame -> recovered, no drop...\n");
    std::vector<uint32_t> got;
    Reassembler::Stats st{};
    std::vector<TestFrame> frames;
    RunFecCase({5, {2}}, got, st, frames);

    std::vector<uint32_t> want;
    for (uint32_t i = 0; i < 20; ++i) want.push_back(i);
    Check(got == want, "all frames delivered despite 1 loss each");
    Check(st.framesDropped == 0, "no frame dropped (FEC)");
    Check(st.packetsRecovered == 18, "18 non-IDR frames each recovered 1 packet");
}

void TestFecRecoverLastPacket() {
    std::printf("[fec] lost packet is the SHORT last one -> length restored...\n");
    std::vector<uint32_t> got;
    Reassembler::Stats st{};
    std::vector<TestFrame> frames;
    RunFecCase({5, {4}}, got, st, frames);

    std::vector<uint32_t> want;
    for (uint32_t i = 0; i < 20; ++i) want.push_back(i);
    Check(got == want, "all frames delivered when the short tail packet is lost");
    Check(st.framesDropped == 0, "no frame dropped (FEC, tail packet)");
}

void TestFecContentIntact() {
    std::printf("[fec] recovered frame is byte-identical...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Reassembler ra(16'667);

    TestFrame f{0, true, {}};
    f.nal.resize(5 * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    auto pkts = Packetize(pk, f, 1'000'000);
    pkts.erase(pkts.begin() + NthDataPacket(pkts, 3));
    for (const auto& d : pkts) Feed(ra, d, 1'000'000);

    auto out = ra.PopReady(1'000'000);
    Check(out.has_value(), "frame completed via FEC");
    if (out) Check(SameFrame(*out, f), "recovered frame byte-identical to original");
}

void TestFecTwoLossesSameGroup() {
    std::printf("[fec] 2 losses in one group -> cannot recover, old policy...\n");
    std::vector<uint32_t> got;
    Reassembler::Stats st{};
    std::vector<TestFrame> frames;
    RunFecCase({5, {1, 3}}, got, st, frames);

    Check(st.packetsRecovered == 0, "nothing recovered with 2 losses in a group");
    Check(st.framesDropped > 0, "frames still dropped with 2 losses in a group");
    Check(std::find(got.begin(), got.end(), 10u) != got.end(), "recovers at next IDR");
}

void TestFecInterleavedBurst() {
    std::printf("[fec] interleaved: consecutive burst loss -> recovered...\n");
    std::vector<uint32_t> got;
    Reassembler::Stats st{};
    std::vector<TestFrame> frames;
    RunFecCase({20, {3, 4}}, got, st, frames);

    std::vector<uint32_t> want;
    for (uint32_t i = 0; i < 20; ++i) want.push_back(i);
    Check(got == want, "all frames delivered despite a 2-packet consecutive burst");
    Check(st.framesDropped == 0, "no frame dropped (interleaved FEC beats the burst)");
    Check(st.packetsRecovered == 36, "18 non-IDR frames each recovered 2 burst packets");
}

void TestFecInterleavedSameGroup() {
    std::printf("[fec] interleaved: 2 losses aligned to one group -> cannot recover...\n");
    std::vector<uint32_t> got;
    Reassembler::Stats st{};
    std::vector<TestFrame> frames;
    RunFecCase({20, {3, 6}}, got, st, frames);
    Check(st.packetsRecovered == 0, "nothing recovered when both losses hit the same group");
    Check(st.framesDropped > 0, "frames still dropped when losses align to one group");
    Check(std::find(got.begin(), got.end(), 10u) != got.end(), "recovers at next IDR");
}

void TestFecSinglePacketFrame() {
    std::printf("[fec] 1-packet frame, data lost, parity alone rebuilds it...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Reassembler ra(16'667);

    TestFrame f{0, true, {}};
    f.nal.resize(300);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    auto pkts = Packetize(pk, f, 1'000'000);
    Check(pkts.size() == 2, "1 data packet + 1 parity packet");
    pkts.erase(pkts.begin() + NthDataPacket(pkts, 0));
    for (const auto& d : pkts) Feed(ra, d, 1'000'000);

    auto out = ra.PopReady(1'000'000);
    Check(out.has_value(), "single-packet frame rebuilt from parity alone");
    if (out) Check(SameFrame(*out, f), "rebuilt single-packet frame identical");
}

void TestFecParityFitsTheGroup() {
    std::printf("[fec] parity is cut to the longest data packet in its group...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);

    TestFrame small{0, true, {}};
    small.nal.resize(300);
    for (auto& b : small.nal) b = uint8_t(Rnd());

    for (const auto& d : Packetize(pk, small, 1'000'000))
        if (IsFec(d))
            Check(d.size() == kCommonHeaderSize + kFecHeaderSize + kFecLenPrefix + 300,
                "a 300-byte frame pays for 300 bytes of parity, not a full MTU");

    TestFrame full{1, true, {}};
    full.nal.resize(5 * kMaxVideoPayload - 100);
    for (auto& b : full.nal) b = uint8_t(Rnd());

    for (const auto& d : Packetize(pk, full, 1'016'667))
        if (IsFec(d))
            Check(d.size() ==
                      kCommonHeaderSize + kFecHeaderSize + Packetizer::kParityStride,
                "a group holding a full-MTU packet still gets full-stride parity");
}

void TestSchemeContract(std::string_view name) {
    std::printf("[fec] contract: '%.*s' delivers exactly what it claims...\n", int(name.size()),
        name.data());

    const std::unique_ptr<FecScheme> probe = MakeFecScheme(name);
    Check(probe != nullptr, "every listed scheme name builds a scheme");
    if (!probe) return;
    Check(probe->Name() == name, "a scheme answers with the name it was asked for");

    const size_t capable = probe->MaxRecoverablePerGroup();
    Check(capable >= 1, "a scheme that recovers nothing has no reason to be registered");

    Check(Deliver(name, 20, 0, {}, false) == Delivery::Identical,
        "a frame that loses nothing arrives byte-identical");
    Check(Deliver(name, 20, 0, LossesPerGroup(20, 0, capable), false) == Delivery::Identical,
        "as many losses per group as the scheme claims are all recovered");
    Check(Deliver(name, 1, 0, {0}, false) == Delivery::Identical,
        "a frame that fits one packet is rebuilt from parity alone");
    Check(Deliver(name, 20, 0, LossesPerGroup(20, 0, capable + 1), false) != Delivery::Corrupted,
        "more losses than the scheme claims must yield no frame, never a wrong one");
    Check(Deliver(name, 20, 0, LossesPerGroup(20, 0, capable), true) != Delivery::Corrupted,
        "corrupt parity must yield no frame, never a wrong one");
}

void TestEverySchemeKeepsTheContract() {
    for (std::string_view name : FecSchemesUnderTest()) TestSchemeContract(name);
    Check(MakeFecScheme("no-such-scheme") == nullptr, "an unknown scheme name builds nothing");
    Check(IsFecSchemeName(kDefaultFecScheme), "the default scheme is one of the registered ones");
}

void TestReedSolomonRecoversWhatXorCannot() {
    if (!SchemeIsUnderTest("rs")) return;

    std::printf("[fec] rs with 2 parity per group survives what one parity cannot...\n");

    const std::unique_ptr<FecScheme> rs = MakeFecScheme("rs");
    Check(rs != nullptr, "rs is registered");
    if (!rs) return;
    Check(rs->ParityPerGroup() == 2, "rs carries two parity packets a group by default");
    Check(rs->SetParityPerGroup(3) && rs->ParityPerGroup() == 3,
        "and the parity ratio is settable, because it is a sweep axis");
    Check(!rs->SetParityPerGroup(0), "zero parity is refused");

    const std::unique_ptr<FecScheme> singleParity = MakeFecScheme("xor");
    Check(singleParity && !singleParity->SetParityPerGroup(2),
        "xor refuses to pretend it can carry two, rather than silently carrying one");

    Check(Deliver("xor", 8, 0, {2, 5}, false) == Delivery::None,
        "one parity packet cannot rebuild two losses in the same group");
    Check(Deliver("rs", 8, 0, {2, 5}, false) == Delivery::Identical,
        "two parity packets can, and the frame comes back byte-identical");
    Check(Deliver("rs", 8, 0, {1, 3, 6}, false) == Delivery::None,
        "three losses beat two parity packets, and no wrong frame is produced");
    Check(Deliver("rs", 20, 0, LossesPerGroup(20, 0, 2), false) == Delivery::Identical,
        "two losses in every group of a larger frame are all rebuilt");
}

void TestOnlyTheWinnerIsReachableWithoutTheCommandLine() {
    std::printf("[fec] production takes the winner, the reference schemes take --fec...\n");

    Check(Packetizer{}.fecScheme() == kDefaultFecScheme,
        "a packetizer nobody configured sends the scheme the bake-off left in place");
    Check(Reassembler{}.fecScheme() == kDefaultFecScheme,
        "and a reassembler nobody configured expects the same one");
    Check(media::ShareOptions{}.fecScheme.empty() && ScreenClientConfig{}.fecScheme.empty(),
        "neither end names a scheme until something asks for one, and only deskhub-cli "
        "--fec does - a reference implementation that production could select would be a "
        "second shipping scheme, not a reference");
}

void TestParityCountTravelsOnTheWire() {
    std::printf("[fec] each parity packet of a group carries its own index...\n");

    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Check(pk.SetFecScheme("rs"), "the sender takes rs");
    Check(pk.fecParityPerGroup() == 2, "and reports what it will send");

    TestFrame f{0, true, {}};
    f.nal.resize(8 * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    std::vector<uint8_t> indices;
    for (const auto& d : Packetize(pk, f, 1'000'000)) {
        if (!IsFec(d)) continue;
        const auto header = ParseCommonHeader(d);
        Check(header.has_value(), "every parity packet parses");
        if (!header) continue;
        const auto view = ParseFecPacket(*header, PayloadOf(d));
        Check(view.has_value(), "and carries a readable FEC header");
        if (view) indices.push_back(view->hdr.parityIndex);
    }

    Check(indices.size() == 2, "8 packets in one group get two parity packets");
    Check(indices.size() == 2 && indices[0] == 0 && indices[1] == 1,
        "and they are numbered, so the receiver can tell them apart");
}

void TestFecGroupCountTravelsOnTheWire() {
    std::printf("[fec] the group count is signalled, not re-derived by the receiver...\n");

    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Check(pk.fecGroups() == 0, "by default the sender signals nothing and both ends derive");

    TestFrame f{0, true, {}};
    f.nal.resize(8 * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    size_t parity = 0;
    for (const auto& d : Packetize(pk, f, 1'000'000)) parity += IsFec(d) ? 1 : 0;
    Check(parity == 1, "8 packets still derive one group, exactly as before");

    pk.SetFecGroups(4);
    parity = 0;
    for (const auto& d : Packetize(pk, f, 1'016'667)) parity += IsFec(d) ? 1 : 0;
    Check(parity == 4, "an explicit count of 4 puts 4 parity packets on the wire");

    Check(Deliver("xor", 8, 0, Burst(0, 4), false) == Delivery::None,
        "at the derived depth an 8-packet frame is one group, so a 4-packet burst is fatal");
    Check(Deliver("xor", 8, 4, Burst(0, 4), false) == Delivery::Identical,
        "the same burst at depth 4 is one loss per group, and every packet comes back");
}

void TestFecGroupCountIsClamped() {
    std::printf("[fec] a group count past what one byte can say is clamped, not wrapped...\n");

    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    pk.SetFecGroups(4000);
    Check(pk.fecGroups() == kMaxSignalledFecGroups,
        "the sender clamps to what the header byte can carry");

    Check(FecGroupCount(10, 200) == 10, "a signalled count above the packet count is clamped");
    Check(FecGroupCount(80, 0) == 10, "a signalled zero means the old derived count");
    Check(FecGroupCount(0, 5) == 0, "a frame of no packets has no groups");

    Check(Deliver("xor", 6, 200, LossesPerGroup(6, 200, 1), false) == Delivery::Identical,
        "clamped to one group per packet, every packet is its own parity and all recover");
}

void TestFecCorruptParityRejected() {
    std::printf("[fec] corrupt parity -> recovery refused, no garbage NAL...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Reassembler ra(16'667);

    TestFrame f{0, true, {}};
    f.nal.resize(5 * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());

    auto pkts = Packetize(pk, f, 1'000'000);
    pkts.erase(pkts.begin() + NthDataPacket(pkts, 2));
    for (auto& d : pkts) {
        if (IsFec(d)) {
            d[kCommonHeaderSize + kFecHeaderSize] = 0xFF;
            d[kCommonHeaderSize + kFecHeaderSize + 1] = 0xFF;
        }
        Feed(ra, d, 1'000'000);
    }
    Check(!ra.PopReady(1'000'000).has_value(), "corrupt parity doesn't complete the frame");
    Check(ra.stats().packetsRecovered == 0, "nothing 'recovered' from a corrupt parity");
}

void TestFecTooManyGroupsSendsPlain() {
    std::printf("[fec] frame needing > kMaxFecGroups groups -> sent plain, no parity...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);

    const size_t count = kMaxFecGroups * kFecGroupSize + 1;
    TestFrame f{0, true, {}};
    f.nal.resize(count * kMaxVideoPayload);

    const auto pkts = Packetize(pk, f, 1'000'000);
    Check(pkts.size() == count, "all data packets still sent");
    size_t fec = 0;
    for (const auto& d : pkts) fec += IsFec(d) ? 1 : 0;
    Check(fec == 0, "no parity when the frame needs more than kMaxFecGroups groups");
}

}

void RunFecTests() {
    TestFecDisabledByDefault();
    TestFecRecoverOne();
    TestFecRecoverLastPacket();
    TestFecContentIntact();
    TestFecTwoLossesSameGroup();
    TestFecInterleavedBurst();
    TestFecInterleavedSameGroup();
    TestFecSinglePacketFrame();
    TestFecParityFitsTheGroup();
    TestFecCorruptParityRejected();
    TestFecTooManyGroupsSendsPlain();
    TestEverySchemeKeepsTheContract();
    TestReedSolomonRecoversWhatXorCannot();
    TestOnlyTheWinnerIsReachableWithoutTheCommandLine();
    TestParityCountTravelsOnTheWire();
    TestFecGroupCountTravelsOnTheWire();
    TestFecGroupCountIsClamped();
}
