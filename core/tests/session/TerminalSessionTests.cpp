#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/ByteOrder.h"
#include "deskhub/protocol/RecordStream.h"
#include "deskhub/session/client/TerminalClient.h"
#include "deskhub/session/TerminalSession.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

Fingerprint TestFingerprint(uint8_t seed) {
    Fingerprint fp;
    for (size_t i = 0; i < kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(seed * 3 + i + 1);
    return fp;
}

TerminalOpenRequest MakeRequest(uint32_t resumeId = 0) {
    TerminalOpenRequest request;
    request.message.size = TermSize{100, 30};
    request.message.resumeId = resumeId;
    request.message.clientName = "Pixel 9";
    request.endpoint = "192.168.1.20:47777";
    request.fingerprint = TestFingerprint(4);
    return request;
}

void TestSharingGate() {
    std::printf("[term] nothing opens until the host has switched terminal sharing on...\n");
    TerminalSessions host;
    Check(!host.Sharing(), "sharing a terminal is off until asked for");
    Check(host.Open(MakeRequest()).reason == TermReason::NotShared,
        "a shell cannot be opened on a host that is not sharing one");
    Check(host.Count() == 0, "and nothing is recorded for it");

    host.SetSharing(true);
    Check(host.Open(MakeRequest()).reason == TermReason::Accepted, "with sharing on it opens");
    Check(host.Count() == 1, "and the session is on the list");

    host.SetSharing(false);
    Check(host.Count() == 0, "switching sharing off ends every open session");
}

void TestSessionCap() {
    std::printf("[term] the number of shells one host will hand out is capped...\n");
    TerminalSessions host;
    host.SetSharing(true);
    for (size_t i = 0; i < kMaxTerminalSessions; ++i)
        Check(host.Open(MakeRequest()).reason == TermReason::Accepted,
            "each shell up to the cap opens");
    const TermOpenAck refused = host.Open(MakeRequest());
    Check(refused.reason == TermReason::TooManySessions, "the one past the cap is refused");
    Check(refused.termId == 0, "a refusal never names a session");
    Check(host.Count() == kMaxTerminalSessions, "and the cap really holds");

    Check(host.Close(1) && host.Count() == kMaxTerminalSessions - 1,
        "closing one makes room");
    Check(host.Open(MakeRequest()).reason == TermReason::Accepted, "so the next one opens");
    Check(!host.Close(9999), "closing a session that never existed reports nothing happened");
}

void TestIdentityIsRecorded() {
    std::printf("[term] every session records who opened it, for the log 5.2 asks for...\n");
    TerminalSessions host;
    host.SetSharing(true);
    const TermOpenAck ack = host.Open(MakeRequest());
    const TerminalRecord* record = host.Find(ack.termId);
    Check(record != nullptr, "the session can be looked up by id");
    Check(record && record->clientEndpoint == "192.168.1.20:47777", "its address is kept");
    Check(record && record->clientName == "Pixel 9", "its name is kept");
    Check(record && record->clientFingerprint == TestFingerprint(4), "and its key");
    Check(host.Find(9999) == nullptr, "an id we never issued has no record");

    const std::string line = TerminalAuditLine(*record, "opened");
    Check(line.find("192.168.1.20:47777") != std::string::npos, "the log line names the address");
    Check(line.find("Pixel 9") != std::string::npos, "and the machine");
    Check(line.find(FormatFingerprint(TestFingerprint(4))) != std::string::npos,
        "and the key we trusted");

    TerminalRecord anonymous;
    anonymous.termId = 7;
    const std::string blank = TerminalAuditLine(anonymous, "opened");
    Check(blank.find("none") != std::string::npos,
        "a session with no key on record says so rather than printing nothing");
}

void TestResizeAndDetach() {
    std::printf("[term] a window size follows the session, and a lost link does not kill it...\n");
    TerminalSessions host;
    host.SetSharing(true);
    const uint32_t id = host.Open(MakeRequest()).termId;

    Check(host.Resize(id, TermSize{132, 43}), "a resize reaches the session");
    Check(host.Find(id)->size == TermSize{132, 43}, "and is remembered");
    Check(host.Resize(id, TermSize{0, 0}), "an impossible size is still accepted");
    Check(host.Find(id)->size.cols == kMinTermCols, "but clamped to something real");
    Check(!host.Resize(9999, TermSize{80, 24}), "a resize for a session we do not have is refused");

    Check(host.Detach(id, 1000), "losing the link detaches the session");
    Check(host.Find(id)->state == TerminalState::Detached, "which is a state we can see");
    Check(host.Count() == 1, "the shell is still alive even though nobody is attached");
    Check(!host.Detach(id, 2000), "detaching twice reports nothing happened");
    Check(!host.Detach(9999, 1000), "and neither does detaching a stranger");
}

void TestReattach() {
    std::printf("[term] a client that comes back gets its own shell, not a new one...\n");
    TerminalSessions host;
    host.SetSharing(true);
    const uint32_t id = host.Open(MakeRequest()).termId;
    host.Detach(id, 1000);

    const TermOpenAck back = host.Open(MakeRequest(id));
    Check(back.reason == TermReason::Accepted && back.termId == id,
        "coming back with the old id lands on the old session");
    Check(back.resumed, "and the client is told it was resumed, not started fresh");
    Check(host.Count() == 1, "no second session was created");
    Check(host.Find(id)->state == TerminalState::Live, "and it is live again");

    Check(host.Open(MakeRequest(id)).reason == TermReason::NoSuchSession,
        "reattaching to a session that is already attached is refused");
    Check(host.Open(MakeRequest(4242)).reason == TermReason::NoSuchSession,
        "and so is an id the host never issued");
}

void TestAttachLocal() {
    std::printf("[term] the host can take a shell for itself, and keeps it for good...\n");
    TerminalSessions host;
    host.SetSharing(true);
    const uint32_t id = host.Open(MakeRequest()).termId;

    Check(host.AttachLocal(id), "a live shell can be taken over at the host");
    Check(host.Find(id)->state == TerminalState::Local, "which is a state we can see");
    Check(host.Count() == 1, "it no longer counts as a remote session, but it still holds its slot");
    Check(!host.AttachLocal(9999), "taking over a stranger does nothing");

    Check(host.Count() == 1,
        "a locally attached shell is never given up, however long it runs");
    Check(!host.Detach(id, 1000), "losing an old link cannot detach it");
    Check(host.Open(MakeRequest(id)).reason == TermReason::NoSuchSession,
        "and the old client cannot reattach to a shell the host took");

    const uint32_t dropped = host.Open(MakeRequest()).termId;
    host.Detach(dropped, 4000);
    Check(host.AttachLocal(dropped), "a detached shell can be taken over too");
    Check(host.Find(dropped)->detachedUs == 0, "and stops waiting for its old client");

    Check(host.Close(id), "closing a local shell frees its slot");
    Check(host.Find(id) == nullptr, "and forgets it");
}

void TestDetachedPersists() {
    std::printf("[term] a detached shell is kept until it ends, never timed out...\n");
    TerminalSessions host;
    host.SetSharing(true);
    const uint32_t kept = host.Open(MakeRequest()).termId;
    const uint32_t dropped = host.Open(MakeRequest()).termId;
    host.Detach(dropped, 1000);

    Check(host.Count() == 2, "two shells are on the list");
    Check(host.Find(dropped) != nullptr && host.Find(dropped)->state == TerminalState::Detached,
        "a session that detached stays on the list, detached");
    Check(host.Find(kept) != nullptr, "and so does the live one");

    const TermOpenAck back = host.Open(MakeRequest(dropped));
    Check(back.reason == TermReason::Accepted && back.termId == dropped && back.resumed,
        "a client that comes back still gets its shell");
    Check(host.Close(kept) && host.Close(dropped), "closing ends them for good");
    Check(host.Count() == 0, "and the list is empty again");
}

void TestSessionList() {
    std::printf("[term] the host can describe every shell it is keeping...\n");
    TerminalSessions host;
    host.SetSharing(true);
    Check(host.List().sessions.empty(), "with nothing open the list is empty");

    const uint32_t first = host.Open(MakeRequest()).termId;
    const uint32_t second = host.Open(MakeRequest()).termId;
    host.Detach(second, 3000);
    host.AttachLocal(first);

    const TermSessionList list = host.List();
    Check(list.sessions.size() == 2, "every open shell is described, whatever its state");
    Check(list.sessions[0].termId == first && list.sessions[0].state == TerminalState::Local,
        "the first was taken over at the host");
    Check(list.sessions[0].size == TermSize{100, 30}, "with its size");
    Check(list.sessions[0].clientName == "Pixel 9", "and who opened it");
    Check(list.sessions[1].termId == second && list.sessions[1].state == TerminalState::Detached,
        "the second is waiting for its client");
}

void TestRecordStream() {
    std::printf("[term] the stream framer cuts messages out of a byte stream...\n");
    uint8_t one[kMaxDatagram];
    uint8_t two[kMaxDatagram];
    const size_t a = BuildTermClose(one, 5);
    const size_t b = BuildTermExit(two, 5, 0);

    std::vector<uint8_t> wire(kMaxRecordSize);
    size_t used = BuildRecord(wire, std::span<const uint8_t>(one, a));
    used += BuildRecord(std::span<uint8_t>(wire).subspan(used), std::span<const uint8_t>(two, b));
    wire.resize(used);

    RecordStream stream;
    std::vector<uint8_t> message;
    for (uint8_t byte : wire) stream.Append(std::span<const uint8_t>(&byte, 1));
    Check(stream.Next(message) && message.size() == a, "the first message comes out whole");
    Check(stream.Next(message) && message.size() == b, "and so does the second");
    Check(!stream.Next(message), "and then the stream is empty");

    RecordStream bad;
    const uint8_t junk[] = {0x00, 0x00};
    bad.Append(junk);
    Check(!bad.Next(message) && bad.Failed(),
        "a length that cannot be right marks the stream as broken");
    bad.Append(wire);
    Check(!bad.Next(message), "and a broken stream never yields another message");
    bad.Reset();
    bad.Append(wire);
    Check(bad.Next(message), "until it is reset");

    RecordStream sliced;
    const std::vector<uint8_t> biggest(kMaxRecordSize, 0x5A);
    std::vector<uint8_t> framed(kRecordPrefixSize + kMaxRecordSize);
    framed.resize(BuildRecord(framed, biggest));
    std::vector<uint8_t> pair(framed);
    pair.insert(pair.end(), framed.begin(), framed.end());

    constexpr size_t kTransportSlice = 8192;
    size_t taken = 0;
    for (size_t at = 0; at < pair.size(); at += kTransportSlice) {
        const size_t slice = std::min(kTransportSlice, pair.size() - at);
        sliced.Append(std::span<const uint8_t>(pair).subspan(at, slice));
        while (sliced.Next(message)) {
            Check(message.size() == kMaxRecordSize, "each record comes back whole");
            ++taken;
        }
    }
    Check(!sliced.Failed() && taken == 2,
        "the largest record survives arriving in slices that straddle its end");

    RecordStream flood;
    std::vector<uint8_t> unfinished(kMaxRecordBacklog + 1, 0);
    PutU16(unfinished.data(), uint16_t(kMaxRecordSize));
    flood.Append(unfinished);
    Check(!flood.Failed(), "a first slice is always taken");
    flood.Append(unfinished);
    Check(flood.Failed(), "a caller that never drains cannot make us grow forever");

    Check(!RecordStream().Next(message), "an empty stream yields nothing");
    RecordStream nothing;
    nothing.Append(std::span<const uint8_t>());
    Check(!nothing.Next(message), "appending nothing changes nothing");
}

struct ClientHarness {
    TerminalClient client;
    std::vector<std::vector<uint8_t>> sent{};
    std::string output{};
    std::vector<TermReason> refusals{};
    std::vector<int32_t> exits{};
    size_t opens = 0;
    bool lastResumed = false;
    std::vector<TermSessionList> lists{};

    ClientHarness()
        : client(TerminalClientCallbacks{}) {
    }
};

std::unique_ptr<ClientHarness> MakeClient() {
    auto harness = std::make_unique<ClientHarness>();
    ClientHarness* raw = harness.get();
    TerminalClientCallbacks cb;
    cb.send = [raw](std::span<const uint8_t> m) {
        raw->sent.emplace_back(m.begin(), m.end());
    };
    cb.onOutput = [raw](std::span<const uint8_t> m) {
        raw->output.append(reinterpret_cast<const char*>(m.data()), m.size());
    };
    cb.onOpened = [raw](const TermOpenAck& ack) {
        ++raw->opens;
        raw->lastResumed = ack.resumed;
    };
    cb.onRefused = [raw](TermReason reason) { raw->refusals.push_back(reason); };
    cb.onExit = [raw](int32_t code) { raw->exits.push_back(code); };
    cb.onSessions = [raw](const TermSessionList& list) { raw->lists.push_back(list); };
    harness->client = TerminalClient(std::move(cb));
    return harness;
}

std::vector<uint8_t> AckMessage(uint32_t termId, TermReason reason, bool resumed) {
    std::vector<uint8_t> out(kMaxDatagram);
    const TermOpenAck ack{termId, reason, resumed};
    out.resize(BuildTermOpenAck(out, ack));
    return out;
}

void TestClientLifecycle() {
    std::printf("[term] the client side: open, type, resize, and be told when it ends...\n");
    auto h = MakeClient();
    Check(h->client.State() == TerminalClientState::Idle, "a fresh client is idle");
    Check(!h->client.CanReattach(), "with nothing to come back to");

    h->client.Open(TermSize{100, 30}, "Pixel 9");
    Check(h->client.State() == TerminalClientState::Opening, "opening puts it in flight");
    Check(h->sent.size() == 1, "and one message goes out");
    const auto request = ParseTermOpen(PayloadOf(h->sent[0]));
    Check(request && request->resumeId == 0, "which asks for a new shell");

    h->client.SendInput(std::span<const uint8_t>());
    Check(h->sent.size() == 1, "typing before the shell is open sends nothing");

    h->client.HandleMessage(AckMessage(9, TermReason::Accepted, false));
    Check(h->client.State() == TerminalClientState::Open && h->client.TermId() == 9,
        "the acknowledgement opens it");
    Check(h->opens == 1 && !h->lastResumed, "and the caller is told it is a fresh shell");

    const std::string typed = "ls -la\r";
    h->client.SendInput(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(typed.data()), typed.size()));
    Check(h->sent.size() == 2, "typing now reaches the wire");
    const auto header = ParseCommonHeader(h->sent[1]);
    Check(header && header->type == MsgType::TermData && header->sessionId == 9,
        "as terminal data for our session");

    h->client.Resize(TermSize{132, 43});
    Check(h->sent.size() == 3 && h->client.Size() == TermSize{132, 43}, "a resize goes out");
    h->client.Resize(TermSize{132, 43});
    Check(h->sent.size() == 3, "resizing to the size we already have sends nothing");

    std::vector<uint8_t> data(kMaxDatagram);
    const uint8_t payload[] = {'o', 'k'};
    data.resize(BuildTermData(data, 9, payload));
    h->client.HandleMessage(data);
    Check(h->output == "ok", "output from the shell reaches the screen");

    std::vector<uint8_t> wrongSession(kMaxDatagram);
    wrongSession.resize(BuildTermData(wrongSession, 77, payload));
    h->client.HandleMessage(wrongSession);
    Check(h->output == "ok", "output addressed to another session is dropped");

    std::vector<uint8_t> exit(kMaxDatagram);
    exit.resize(BuildTermExit(exit, 9, 130));
    h->client.HandleMessage(exit);
    Check(h->client.State() == TerminalClientState::Closed && h->exits.size() == 1 &&
              h->exits[0] == 130,
        "when the shell exits the client is told the code");
    Check(h->client.TermId() == 0, "and no longer holds a session");
}

void TestClientReattachAndRefusal() {
    std::printf("[term] the client keeps its id across a dropped link and reattaches...\n");
    auto h = MakeClient();
    h->client.Open(TermSize{80, 24}, "Pixel 9");
    h->client.HandleMessage(AckMessage(4, TermReason::Accepted, false));

    h->client.LinkLost();
    Check(h->client.State() == TerminalClientState::Idle, "a dropped link puts it back to idle");
    Check(h->client.CanReattach() && h->client.TermId() == 4,
        "but it still knows which shell was its own");

    const size_t before = h->sent.size();
    h->client.Reattach();
    Check(h->sent.size() == before + 1 && h->client.State() == TerminalClientState::Reattaching,
        "reattaching sends a fresh request");
    const auto again = ParseTermOpen(PayloadOf(h->sent.back()));
    Check(again && again->resumeId == 4, "naming the session it wants back");

    h->client.HandleMessage(AckMessage(4, TermReason::Accepted, true));
    Check(h->client.State() == TerminalClientState::Open && h->lastResumed,
        "and the host confirms it was resumed rather than started again");

    auto racing = MakeClient();
    racing->client.Open(TermSize{80, 24}, "Pixel 9");
    racing->client.HandleMessage(AckMessage(7, TermReason::Accepted, false));
    racing->client.LinkLost();
    racing->client.Reattach();
    racing->client.HandleMessage(AckMessage(0, TermReason::NoSuchSession, false));
    Check(racing->refusals.size() == 1 && racing->refusals[0] == TermReason::NoSuchSession,
        "a host that has not noticed the drop yet says there is no such session");
    Check(racing->client.CanReattach() && racing->client.TermId() == 7,
        "which is not final, so the shell is still there to ask for again");
    racing->client.Reattach();
    racing->client.HandleMessage(AckMessage(7, TermReason::Accepted, true));
    Check(racing->client.State() == TerminalClientState::Open && racing->lastResumed,
        "and asking once more gets the same shell back");

    auto gone = MakeClient();
    gone->client.Open(TermSize{80, 24}, "Pixel 9");
    gone->client.HandleMessage(AckMessage(8, TermReason::Accepted, false));
    gone->client.LinkLost();
    gone->client.Reattach();
    gone->client.HandleMessage(AckMessage(0, TermReason::TooManySessions, false));
    Check(gone->client.State() == TerminalClientState::Refused && !gone->client.CanReattach(),
        "any other refusal of a reattach is final, and drops the session");

    auto refused = MakeClient();
    refused->client.Open(TermSize{80, 24}, "Pixel 9");
    refused->client.HandleMessage(AckMessage(0, TermReason::NotShared, false));
    Check(refused->client.State() == TerminalClientState::Refused &&
              refused->refusals.size() == 1 &&
              refused->refusals[0] == TermReason::NotShared,
        "a refusal is reported with its reason");
    Check(refused->client.TermId() == 0 && !refused->client.CanReattach(),
        "and leaves nothing to come back to");

    auto lying = MakeClient();
    lying->client.Open(TermSize{80, 24}, "");
    lying->client.HandleMessage(AckMessage(0, TermReason::Accepted, false));
    Check(lying->client.State() == TerminalClientState::Refused,
        "an acceptance that names no session is treated as a refusal");

    auto closed = MakeClient();
    closed->client.Open(TermSize{80, 24}, "");
    closed->client.HandleMessage(AckMessage(3, TermReason::Accepted, false));
    closed->client.Close();
    Check(closed->client.State() == TerminalClientState::Closed, "closing ends the session");
    closed->client.LinkLost();
    Check(closed->client.State() == TerminalClientState::Closed,
        "and a link that drops afterwards does not revive it");
    Check(!closed->client.CanReattach(), "a closed shell is never reattached to");
    closed->client.Reattach();
    Check(closed->client.State() == TerminalClientState::Closed,
        "and asking anyway does nothing");
}

void TestClientListAndResume() {
    std::printf("[term] the client can ask what the host is keeping, and name the one it wants...\n");
    auto h = MakeClient();
    const size_t before = h->sent.size();
    h->client.RequestList();
    Check(h->sent.size() == before + 1, "asking sends one message");
    const auto asked = ParseCommonHeader(h->sent.back());
    Check(asked && asked->type == MsgType::TermList && asked->chan == Chan::Terminal,
        "which is a session listing on the terminal channel");

    TermSessionList offered;
    TermSessionEntry kept;
    kept.termId = 6;
    kept.state = TerminalState::Detached;
    kept.size = TermSize{80, 24};
    kept.clientName = "Pixel 9";
    offered.sessions.push_back(kept);
    std::vector<uint8_t> reply(kMaxDatagram);
    reply.resize(BuildTermListAck(reply, offered));
    h->client.HandleMessage(reply);
    Check(h->lists.size() == 1 && h->lists[0].sessions.size() == 1,
        "the answer is remembered and reported");
    Check(h->client.Sessions().sessions.size() == 1 &&
              h->client.Sessions().sessions[0].termId == 6 &&
              h->client.Sessions().sessions[0].state == TerminalState::Detached,
        "and can be read back");

    h->client.Resume(6);
    Check(h->client.State() == TerminalClientState::Reattaching, "resuming puts it in flight");
    const auto again = ParseTermOpen(PayloadOf(h->sent.back()));
    Check(again && again->resumeId == 6, "naming the session it wants back");
    h->client.HandleMessage(AckMessage(6, TermReason::Accepted, true));
    Check(h->client.State() == TerminalClientState::Open, "and the host hands it back");
    const size_t attached = h->sent.size();
    h->client.Resume(7);
    Check(h->sent.size() == attached, "naming another while attached sends nothing");

    h->client.Resume(0);
    Check(h->client.State() == TerminalClientState::Open, "resuming nothing changes nothing");

    auto shut = MakeClient();
    shut->client.Open(TermSize{80, 24}, "");
    shut->client.HandleMessage(AckMessage(3, TermReason::Accepted, false));
    shut->client.Close();
    const size_t quiet = shut->sent.size();
    shut->client.RequestList();
    shut->client.Resume(3);
    Check(shut->sent.size() == quiet, "a closed client sends nothing more");
}

void TestClientClosesAShellItIsNotIn() {
    std::printf("[term] the client can end a shell it never attached to...\n");
    auto h = MakeClient();
    const size_t before = h->sent.size();
    h->client.CloseSession(6);
    Check(h->sent.size() == before + 1, "asking to close sends one message");
    const auto asked = ParseCommonHeader(h->sent.back());
    Check(asked && asked->type == MsgType::TermClose && asked->chan == Chan::Terminal,
        "which is a close on the terminal channel");
    Check(asked && asked->sessionId == 6, "naming the shell to end");
    Check(h->client.State() != TerminalClientState::Closed,
        "and ending someone else's shell does not end the client");

    h->client.CloseSession(0);
    Check(h->sent.size() == before + 1, "there is no shell zero to close");

    auto mine = MakeClient();
    mine->client.Open(TermSize{80, 24}, "");
    mine->client.HandleMessage(AckMessage(4, TermReason::Accepted, false));
    mine->client.CloseSession(4);
    const auto ended = ParseCommonHeader(mine->sent.back());
    Check(ended && ended->type == MsgType::TermClose && ended->sessionId == 4,
        "closing the shell we are in tells the host");
    Check(mine->client.State() == TerminalClientState::Closed && mine->client.TermId() == 0,
        "and takes the client down with it");
    const size_t quiet = mine->sent.size();
    mine->client.CloseSession(9);
    Check(mine->sent.size() == quiet, "a closed client closes nothing further");
}

void TestClientIgnoresJunk() {
    std::printf("[term] the client drops anything that is not its own protocol...\n");
    auto h = MakeClient();
    h->client.Open(TermSize{80, 24}, "");
    h->client.HandleMessage(AckMessage(2, TermReason::Accepted, false));

    uint8_t control[kMaxDatagram];
    const size_t n = BuildPing(control, 2, PingPong{1, 2});
    h->client.HandleMessage(std::span<const uint8_t>(control, n));
    Check(h->client.State() == TerminalClientState::Open,
        "a control-channel message on the terminal stream is ignored");

    for (int i = 0; i < 500; ++i) {
        std::vector<uint8_t> soup(Rnd() % 60);
        for (auto& b : soup) b = uint8_t(Rnd());
        h->client.HandleMessage(soup);
    }
    Check(true, "and 500 random messages leave it standing");

    std::vector<uint8_t> big(kMaxRecordSize);
    const std::vector<uint8_t> payload(kMaxTermDataBytes, 'x');
    big.resize(BuildTermData(big, 2, payload));
    h->client.HandleMessage(big);
    Check(h->output.size() == kMaxTermDataBytes, "a full-size chunk of output is accepted");

    const std::string typed(kMaxTermDataBytes * 2 + 5, 'y');
    const size_t before = h->sent.size();
    h->client.SendInput(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(typed.data()), typed.size()));
    Check(h->sent.size() == before + 3, "a paste larger than one chunk is split across messages");
}

}

void RunTerminalSessionTests() {
    TestSharingGate();
    TestSessionCap();
    TestIdentityIsRecorded();
    TestResizeAndDetach();
    TestReattach();
    TestAttachLocal();
    TestDetachedPersists();
    TestSessionList();
    TestRecordStream();
    TestClientLifecycle();
    TestClientReattachAndRefusal();
    TestClientListAndResume();
    TestClientClosesAShellItIsNotIn();
    TestClientIgnoresJunk();
}
