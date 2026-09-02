#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/InputSender.h"
#include "deskhub/session/host/ScreenHostSession.h"
#include "deskhub/session/host/ViewerTable.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

constexpr uint64_t kT0 = 100'000'000;
constexpr uint64_t kAlice = 0xC0A80001'0000ULL;
constexpr uint64_t kBob = 0xC0A80002'0000ULL;
constexpr uint64_t kCarol = 0xC0A80003'0000ULL;

struct Recorder {
    std::vector<Datagram> sent;
    std::vector<InputEvent> input;
    std::vector<uint64_t> joined, left, controllers;
    std::vector<std::string> joinedNames;
    int starts = 0, keyframes = 0, disconnects = 0;
    std::vector<Feedback> feedback;

    ScreenHostCallbacks Callbacks() {
        ScreenHostCallbacks cb;
        cb.send = [this](std::span<const uint8_t> d) { sent.emplace_back(d.begin(), d.end()); };
        cb.randomBytes = TestRandomBytes;
        cb.onStart = [this] { ++starts; };
        cb.onKeyframeRequest = [this](deskhub::KeyframeReason) { ++keyframes; };
        cb.onDisconnect = [this] { ++disconnects; };
        cb.onInput = [this](const InputEvent& e) { input.push_back(e); };
        cb.onFeedback = [this](const Feedback& f) { feedback.push_back(f); };
        cb.onViewerJoin = [this](uint64_t addr, size_t, std::string_view name) {
            joined.push_back(addr);
            joinedNames.emplace_back(name);
        };
        cb.onViewerLeave = [this](uint64_t addr, size_t) { left.push_back(addr); };
        cb.onControllerChange = [this](uint64_t addr) { controllers.push_back(addr); };
        return cb;
    }
};

Datagram HelloFrom(uint32_t clientId, std::string clientName = {}) {
    uint8_t buf[kMaxDatagram];
    Hello h{};
    h.passcode = kTestPasscode;
    h.clientId = clientId;
    h.codecMask = kCodecMaskH264;
    h.maxWidth = 1920;
    h.maxHeight = 1080;
    h.desiredFps = 60;
    h.clientName = std::move(clientName);
    const size_t n = BuildHello(buf, h);
    return Datagram(buf, buf + n);
}

Datagram StartFor(uint32_t sessionId) {
    uint8_t buf[kMaxDatagram];
    const size_t n = BuildStart(buf, sessionId);
    return Datagram(buf, buf + n);
}

Datagram ByeFor(uint32_t sessionId) {
    uint8_t buf[kMaxDatagram];
    const size_t n = BuildBye(buf, sessionId);
    return Datagram(buf, buf + n);
}

Datagram FeedbackFor(uint32_t sessionId, const Feedback& fb) {
    uint8_t buf[kMaxDatagram];
    const size_t n = BuildFeedback(buf, sessionId, fb);
    return Datagram(buf, buf + n);
}

Datagram KeyPressFrom(uint32_t sessionId, InputSender& sender, int32_t scancode) {
    InputEvent e;
    e.type = InputType::Key;
    e.a = scancode;
    e.state = 1;
    sender.SetSessionId(sessionId);
    sender.Queue(e);

    Datagram out;
    sender.Flush(kT0, [&out](std::span<const uint8_t> d) {
        if (out.empty()) out.assign(d.begin(), d.end());
    });
    return out;
}

std::unique_ptr<ScreenHostSession> MakeSession(Recorder& rec) {
    auto session = std::make_unique<ScreenHostSession>(rec.Callbacks(),
        StreamParams{1920, 1080, 60, 20'000'000});
    session->SetPasscode(kTestPasscode);
    return session;
}

void JoinAndStart(ScreenHostSession& s, uint32_t clientId, uint64_t addr) {
    s.HandlePacket(HelloFrom(clientId), kT0, addr);
    s.HandlePacket(StartFor(s.sessionId()), kT0, addr);
}

void TestSeveralViewersShareOneSession() {
    std::printf("[viewers] three viewers share one session id and one encode...\n");
    Recorder rec;
    auto s = MakeSession(rec);

    s->HandlePacket(HelloFrom(1), kT0, kAlice);
    const uint32_t sid = s->sessionId();
    Check(sid != 0, "the first HELLO opens the session");
    Check(s->viewerCount() == 1, "one viewer is connected");

    s->HandlePacket(HelloFrom(2), kT0, kBob);
    s->HandlePacket(HelloFrom(3), kT0, kCarol);
    Check(s->sessionId() == sid, "later viewers ride the same session id");
    Check(s->viewerCount() == 3, "all three are connected");
    Check(rec.joined.size() == 3, "each join is reported once");

    uint64_t addrs[kMaxViewersPerSource];
    Check(s->SnapshotViewerAddrs(addrs) == 3, "the send fan-out sees three addresses");
}

void TestOneViewerTooManyIsRejectedAsBusy() {
    std::printf("[viewers] the source fills up and then says busy...\n");
    Recorder rec;
    auto s = MakeSession(rec);

    for (uint32_t i = 0; i < uint32_t(kMaxViewersPerSource); ++i)
        s->HandlePacket(HelloFrom(i + 1), kT0, kAlice + i);
    Check(s->viewerCount() == kMaxViewersPerSource, "the table is full");

    rec.sent.clear();
    const bool ok = s->HandlePacket(HelloFrom(99), kT0, 0xDEAD'0000ULL);
    Check(!ok, "one viewer too many is turned away");
    Check(s->viewerCount() == kMaxViewersPerSource, "and does not take a slot");

    Check(rec.sent.size() == 1, "a rejection went back");
    const auto ack = ParseHelloAck(PayloadOf(rec.sent[0]));
    Check(ack && ack->codec == Codec::Rejected && ack->reason == RejectReason::Busy,
        "and it says busy");
}

void TestTheHostBudgetIsSharedAcrossSources() {
    std::printf("[viewers] the whole host takes %zu viewers, however they spread out...\n",
        kMaxViewersPerHost);
    ViewerBudget budget;
    Recorder recA, recB;
    ScreenHostSession a(recA.Callbacks(), StreamParams{1920, 1080, 60, 20'000'000}, &budget);
    a.SetPasscode(kTestPasscode);
    ScreenHostSession b(recB.Callbacks(), StreamParams{1920, 1080, 60, 20'000'000}, &budget);
    b.SetPasscode(kTestPasscode);

    for (uint32_t i = 0; i < 3; ++i)
        a.HandlePacket(HelloFrom(i + 1), kT0, kAlice + i);
    for (uint32_t i = 0; i < 2; ++i)
        b.HandlePacket(HelloFrom(i + 10), kT0, kBob + i);

    Check(a.viewerCount() == 3 && b.viewerCount() == 2, "both sources filled up the budget");
    Check(budget.taken() == kMaxViewersPerHost, "which is now spent");

    recB.sent.clear();
    Check(!b.HandlePacket(HelloFrom(99), kT0, kCarol),
        "a source with free slots still turns a viewer away once the host is full");
    const auto ack = ParseHelloAck(PayloadOf(recB.sent.at(0)));
    Check(ack && ack->reason == RejectReason::Busy, "and says busy");

    a.HandlePacket(ByeFor(a.sessionId()), kT0, kAlice);
    Check(budget.taken() == kMaxViewersPerHost - 1, "a viewer leaving frees its place");
    Check(b.HandlePacket(HelloFrom(99), kT0, kCarol),
        "so the next one in is welcome, on whichever source it asked for");
    Check(budget.taken() == kMaxViewersPerHost, "and the host is full again");
}

void TestASourceGivesItsViewersBackWhenItStops() {
    std::printf("[viewers] a source that stops sharing frees the seats it held...\n");
    ViewerBudget budget;
    Recorder rec;
    {
        ScreenHostSession s(rec.Callbacks(), StreamParams{1920, 1080, 60, 20'000'000}, &budget);
        s.SetPasscode(kTestPasscode);
        s.HandlePacket(HelloFrom(1), kT0, kAlice);
        s.HandlePacket(HelloFrom(2), kT0, kBob);
        Check(budget.taken() == 2, "two seats are taken while it is shared");
    }
    Check(budget.taken() == 0, "and both come back when the source goes away");
}

void TestEveryViewerCanTypeWhenNobodyElseIs() {
    std::printf("[viewers] any viewer can drive the host while the others are idle...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    InputSender bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x22), kT0, kBob);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x22,
        "the later viewer types while the first one sits still");
    Check(s->inputDenied() == 0, "nothing was refused");
    Check(rec.controllers.back() == kBob, "and it is reported as the one driving");
}

void TestPriorityDecidesWhoWinsAClash() {
    std::printf("[viewers] when two viewers type at once the earlier one wins...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    InputSender alice, bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0, kAlice);
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x22), kT0 + 1000, kBob);

    Check(rec.input.size() == 1, "only one of the two events reached the host");
    Check(rec.input[0].a == 0x11, "and it is the one from the viewer that connected first");
    Check(s->inputDenied() == 1, "the clashing packet is counted as denied");
}

void TestTheLowerPriorityViewerTakesOverOnceTheHoldLapses() {
    std::printf("[viewers] a viewer that stops typing releases the host to the next one...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    InputSender alice, bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0, kAlice);
    rec.input.clear();

    const uint64_t lapsed = kT0 + kInputControlHoldUs + 1;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x22), lapsed, kBob);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x22,
        "once the hold lapses the other viewer gets through");
    Check(rec.controllers.back() == kBob, "control is reported as handed over");
}

void TestTheEarlierViewerPreemptsMidStream() {
    std::printf("[viewers] the earlier viewer can cut in and take the host back...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    InputSender alice, bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x22), kT0, kBob);
    Check(rec.controllers.back() == kBob, "the later viewer is driving");

    rec.input.clear();
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0 + 1000, kAlice);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x11, "the earlier viewer cuts straight in");
    Check(rec.controllers.back() == kAlice,
        "and the handover is reported, so the host can drop held keys");

    rec.input.clear();
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x33), kT0 + 2000, kBob);
    Check(rec.input.empty(), "the viewer that was cut off has to wait its turn again");
}

void TestDeniedInputDoesNotReplayLater() {
    std::printf("[viewers] input refused during a clash is not applied late...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    InputSender alice, bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0, kAlice);
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x22), kT0 + 1000, kBob);
    rec.input.clear();

    const uint64_t lapsed = kT0 + kInputControlHoldUs + 1;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x44), lapsed, kBob);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x44,
        "only the fresh event lands, never the one that lost the clash");
}

void TestControlIsFreedWhenTheDriverLeaves() {
    std::printf("[viewers] the host lets go of held keys when the driver disconnects...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);
    JoinAndStart(*s, 3, kCarol);

    InputSender alice;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0, kAlice);
    rec.controllers.clear();
    rec.input.clear();

    s->HandlePacket(ByeFor(s->sessionId()), kT0, kAlice);
    Check(s->viewerCount() == 2, "the driver is gone");
    Check(rec.controllers.size() == 1 && rec.controllers[0] == 0,
        "and the host is told nobody is driving, so nothing stays pressed");
    Check(rec.disconnects == 0, "the session stays up for the rest");

    InputSender bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x33), kT0 + 1000, kBob);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x33,
        "and the next viewer can type straight away");
}

void TestRejoiningGoesToTheBackOfTheQueue() {
    std::printf("[viewers] a viewer that leaves and comes back loses its priority...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    s->HandlePacket(ByeFor(s->sessionId()), kT0, kAlice);
    JoinAndStart(*s, 1, kAlice);
    Check(s->viewerCount() == 2, "both are back");

    rec.input.clear();
    InputSender alice, bob;
    s->HandlePacket(KeyPressFrom(s->sessionId(), bob, 0x55), kT0, kBob);
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x44), kT0 + 1000, kAlice);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x55,
        "the viewer that never left wins the clash");
}

void TestLateJoinerGetsAKeyframe() {
    std::printf("[viewers] a viewer joining mid-stream asks for an IDR...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    Check(rec.starts == 1 && rec.keyframes == 0, "the first START begins the push");

    JoinAndStart(*s, 2, kBob);
    Check(rec.starts == 1, "the stream is not restarted");
    Check(rec.keyframes == 1, "but a keyframe is forced for the newcomer");
    Check(s->state() == ScreenHostSession::State::Streaming, "and the source keeps streaming");
}

void TestSessionEndsOnlyWhenTheLastViewerLeaves() {
    std::printf("[viewers] the session closes when nobody is left...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);
    const uint32_t sid = s->sessionId();

    InputSender alice;
    s->HandlePacket(KeyPressFrom(sid, alice, 0x11), kT0, kAlice);

    s->HandlePacket(ByeFor(sid), kT0, kBob);
    Check(s->state() == ScreenHostSession::State::Streaming, "one BYE does not end the stream");
    Check(rec.disconnects == 0, "and does not report a disconnect");

    s->HandlePacket(ByeFor(sid), kT0, kAlice);
    Check(s->state() == ScreenHostSession::State::Idle, "the last BYE ends it");
    Check(s->sessionId() == 0, "the session id is cleared");
    Check(rec.disconnects == 1, "and the disconnect is reported once");
    Check(rec.controllers.back() == 0, "with nobody left holding input control");
}

void TestTimeoutDropsOneViewerAtATime() {
    std::printf("[viewers] a silent viewer times out without taking the others down...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    const uint64_t later = kT0 + kSessionTimeoutUs + 1;
    s->HandlePacket(FeedbackFor(s->sessionId(), Feedback{}), later, kBob);
    s->Tick(later);

    Check(s->viewerCount() == 1, "the quiet viewer is gone");
    Check(rec.left.size() == 1 && rec.left[0] == kAlice, "and it is the one that went quiet");
    Check(rec.disconnects == 0, "the session survives");
}

void TestFeedbackTakesTheWorstLink() {
    std::printf("[viewers] the shared encode follows the weakest viewer...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    s->HandlePacket(FeedbackFor(s->sessionId(), Feedback{1, 2, 30, 9000}), kT0, kAlice);
    s->HandlePacket(FeedbackFor(s->sessionId(), Feedback{7, 25, 12, 4000}), kT0, kBob);

    Check(rec.feedback.size() == 2, "both reports were taken");
    const Feedback& merged = rec.feedback.back();
    Check(merged.lossPct == 25, "the worst loss wins");
    Check(merged.rttMs == 30, "the worst RTT wins");
    Check(merged.lostFrames == 7, "the worst frame loss wins");
    Check(merged.recvBitrateKbps == 4000, "and the lowest received rate wins");
}

void TestWorstCaseFeedbackIgnoresSilentViewers() {
    std::printf("[viewers] viewers that have not reported yet do not skew the merge...\n");
    ViewerSlot slots[3];
    slots[0].active = true;
    slots[0].haveFeedback = true;
    slots[0].feedback = Feedback{2, 5, 20, 8000};
    slots[1].active = true;
    slots[2].active = true;
    slots[2].haveFeedback = true;
    slots[2].feedback = Feedback{1, 9, 10, 12000};

    const Feedback worst = WorstCaseFeedback(std::span<const ViewerSlot>(slots, 3));
    Check(worst.lossPct == 9 && worst.rttMs == 20, "only reported links are merged");
    Check(worst.recvBitrateKbps == 8000, "and an unreported viewer never reads as 0 kbps");
}

void TestViewerAddressCanMove() {
    std::printf("[viewers] a viewer that changes port keeps its slot and its priority...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    const uint64_t roamed = kAlice + 7;
    s->HandlePacket(HelloFrom(1), kT0, roamed);
    Check(s->viewerCount() == 2, "no extra slot was taken");

    uint64_t addrs[kMaxViewersPerSource];
    const size_t n = s->SnapshotViewerAddrs(addrs);
    bool sawRoamed = false;
    for (size_t i = 0; i < n; ++i)
        if (addrs[i] == roamed) sawRoamed = true;
    Check(sawRoamed, "the new address is what frames go to");

    rec.input.clear();
    InputSender alice;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x66), kT0, roamed);
    Check(rec.input.size() == 1, "and its input still reaches the host");
}

void TestANewClientOnAnOldAddressTakesTheSlotOver() {
    std::printf("[viewers] a fresh client on a recycled address replaces the stale one...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);
    JoinAndStart(*s, 2, kBob);

    s->HandlePacket(HelloFrom(77), kT0, kBob);
    Check(s->viewerCount() == 2, "the address still holds exactly one viewer");
    Check(rec.left.size() == 1 && rec.left[0] == kBob, "the stale viewer was dropped");
    Check(rec.joined.back() == kBob, "and the new one took its place");

    rec.input.clear();
    InputSender alice, newcomer;
    s->HandlePacket(KeyPressFrom(s->sessionId(), alice, 0x11), kT0, kAlice);
    s->HandlePacket(KeyPressFrom(s->sessionId(), newcomer, 0x77), kT0 + 1000, kBob);
    Check(rec.input.size() == 1 && rec.input[0].a == 0x11,
        "the newcomer joins at the back of the input queue");
}

void TestNonsenseFromAViewerIsIgnored() {
    std::printf("[viewers] a connected viewer cannot drive the host with junk...\n");
    Recorder rec;
    auto s = MakeSession(rec);
    JoinAndStart(*s, 1, kAlice);

    uint8_t buf[kMaxDatagram];
    HelloAck ack{};
    ack.sessionId = s->sessionId();
    ack.codec = Codec::H264;
    const size_t n = BuildHelloAck(buf, ack);
    Check(!s->HandlePacket(std::span<const uint8_t>(buf, n), kT0, kAlice),
        "a message only a host may send is refused");

    const Datagram stray = StartFor(s->sessionId() ^ 0xFFFF);
    Check(!s->HandlePacket(stray, kT0, kAlice), "so is a packet naming another session");
    Check(!s->HandlePacket(StartFor(s->sessionId()), kT0, kCarol),
        "and one from an address that never said HELLO");
    Check(s->viewerCount() == 1 && s->state() == ScreenHostSession::State::Streaming,
        "none of it disturbed the viewer that is really there");
}

void TestViewerNamesReachTheHost() {
    std::printf("[viewers] each viewer's display name is stored and published...\n");
    Recorder rec;
    auto s = MakeSession(rec);

    s->HandlePacket(HelloFrom(1, "Anh's laptop"), kT0, kAlice);
    s->HandlePacket(HelloFrom(2), kT0, kBob);
    Check(rec.joinedNames.size() == 2 && rec.joinedNames[0] == "Anh's laptop" &&
              rec.joinedNames[1].empty(),
        "the join callback carries each viewer's name");

    ViewerInfo infos[kMaxViewersPerSource];
    size_t n = s->SnapshotViewerInfos(infos);
    Check(n == 2, "the snapshot sees both viewers");
    bool aliceNamed = false, bobUnnamed = false;
    for (size_t i = 0; i < n; ++i) {
        if (infos[i].addrPacked == kAlice) aliceNamed = infos[i].name == "Anh's laptop";
        if (infos[i].addrPacked == kBob) bobUnnamed = infos[i].name.empty();
    }
    Check(aliceNamed, "the named viewer keeps its name");
    Check(bobUnnamed, "an unnamed viewer publishes an empty name");

    s->HandlePacket(HelloFrom(2, "Bob's phone"), kT0, kBob);
    Check(s->viewerCount() == 2, "a re-HELLO with a name takes no extra slot");
    n = s->SnapshotViewerInfos(infos);
    bool bobNamed = false;
    for (size_t i = 0; i < n; ++i)
        if (infos[i].addrPacked == kBob) bobNamed = infos[i].name == "Bob's phone";
    Check(bobNamed, "a re-HELLO updates the published name");

    const uint64_t roamed = kAlice + 7;
    s->HandlePacket(HelloFrom(1, "Anh's laptop"), kT0, roamed);
    n = s->SnapshotViewerInfos(infos);
    bool roamedNamed = false;
    for (size_t i = 0; i < n; ++i)
        if (infos[i].addrPacked == roamed) roamedNamed = infos[i].name == "Anh's laptop";
    Check(roamedNamed, "the name follows a viewer whose address moves");
}

void TestAClientWithoutH264IsTurnedAway() {
    std::printf("[viewers] a viewer that cannot decode H.264 is told why...\n");
    Recorder rec;
    auto s = MakeSession(rec);

    uint8_t buf[kMaxDatagram];
    Hello h{};
    h.passcode = kTestPasscode;
    h.clientId = 5;
    h.codecMask = 0;
    h.maxWidth = 1920;
    h.maxHeight = 1080;
    const size_t n = BuildHello(buf, h);
    Check(!s->HandlePacket(std::span<const uint8_t>(buf, n), kT0, kAlice), "the HELLO is refused");
    Check(s->viewerCount() == 0, "and no slot is taken");

    Check(rec.sent.size() == 1, "a rejection went back");
    const auto ack = ParseHelloAck(PayloadOf(rec.sent[0]));
    Check(ack && ack->reason == RejectReason::CodecMismatch, "naming the codec as the reason");
}

}

void RunViewerTableTests() {
    TestSeveralViewersShareOneSession();
    TestOneViewerTooManyIsRejectedAsBusy();
    TestTheHostBudgetIsSharedAcrossSources();
    TestASourceGivesItsViewersBackWhenItStops();
    TestEveryViewerCanTypeWhenNobodyElseIs();
    TestPriorityDecidesWhoWinsAClash();
    TestTheLowerPriorityViewerTakesOverOnceTheHoldLapses();
    TestTheEarlierViewerPreemptsMidStream();
    TestDeniedInputDoesNotReplayLater();
    TestControlIsFreedWhenTheDriverLeaves();
    TestRejoiningGoesToTheBackOfTheQueue();
    TestLateJoinerGetsAKeyframe();
    TestSessionEndsOnlyWhenTheLastViewerLeaves();
    TestTimeoutDropsOneViewerAtATime();
    TestFeedbackTakesTheWorstLink();
    TestWorstCaseFeedbackIgnoresSilentViewers();
    TestViewerAddressCanMove();
    TestANewClientOnAnOldAddressTakesTheSlotOver();
    TestNonsenseFromAViewerIsIgnored();
    TestViewerNamesReachTheHost();
    TestAClientWithoutH264IsTurnedAway();
}
