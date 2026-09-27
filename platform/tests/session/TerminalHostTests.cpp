#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/RecordStream.h"
#include "deskhub/session/client/TerminalClient.h"
#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Screen.h"
#include "deskhubp/net/QuicEndpoint.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/client/TerminalViewer.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/ClientIdentity.h"

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kTestPort = 47793;
constexpr int kMaxRounds = 2000;
constexpr uint32_t kPollWaitMs = 2;

struct Viewer {
    deskhubp::QuicEndpoint endpoint{};
    deskhub::RecordStream framer{};
    deskhub::term::Screen screen{deskhub::TermSize{80, 24}};
    deskhubp::QuicConnId conn = 0;
    bool connected = false;
    std::vector<uint8_t> pending{};
    std::vector<deskhub::TermReason> refusals{};
    std::vector<int32_t> exits{};
    size_t opens = 0;
    deskhub::TermSessionList listed{};
    bool resumed = false;

    std::unique_ptr<deskhub::TerminalClient> client{};
    std::unique_ptr<deskhubp::ClientAuth> auth{};
    deskhub::AuthResultCode authCode = deskhub::AuthResultCode::NotPaired;
    bool authSettled = false;

    void Send(std::span<const uint8_t> message) {
        std::vector<uint8_t> record(deskhub::kRecordPrefixSize + message.size());
        record.resize(deskhub::BuildRecord(record, message));
        if (connected)
            endpoint.SendStream(conn, deskhubp::kQuicControlStream, record);
        else
            pending.insert(pending.end(), record.begin(), record.end());
    }

    void BeginAuth(const deskhubp::HostIdentity& own, const deskhub::Fingerprint& hostKey,
        std::string passcode) {
        deskhubp::ClientAuthConfig config;
        config.identity = own;
        config.passcode = std::move(passcode);
        config.hostFingerprint = hostKey;
        config.clientName = "test-client";
        auth = std::make_unique<deskhubp::ClientAuth>();
        auth->Configure(std::move(config));

        std::vector<uint8_t> out(deskhub::kMaxRecordSize);
        out.resize(deskhub::BuildAuthStart(out, auth->Begin()));
        Send(out);
    }

    bool HandleAuth(std::span<const uint8_t> message) {
        if (!auth) return false;
        const std::optional<deskhub::CommonHeader> header =
            deskhub::ParseCommonHeader(message);
        if (!header) return false;
        const std::span<const uint8_t> payload = deskhub::PayloadOf(message);

        if (header->type == deskhub::MsgType::AuthChallenge) {
            const std::optional<deskhub::AuthChallenge> challenge =
                deskhub::ParseAuthChallenge(payload);
            if (!challenge) return true;
            const std::optional<deskhub::AuthResponse> response = auth->Answer(*challenge);
            if (!response) {
                authSettled = true;
                authCode = deskhub::AuthResultCode::PairingDisabled;
                return true;
            }
            std::vector<uint8_t> out(deskhub::kMaxRecordSize);
            out.resize(deskhub::BuildAuthResponse(out, *response));
            Send(out);
            return true;
        }

        if (header->type != deskhub::MsgType::AuthResult) return false;
        if (const std::optional<deskhub::AuthResult> result =
                deskhub::ParseAuthResult(payload)) {
            authCode = result->code;
            authSettled = true;
        }
        return true;
    }

    bool Allowed() const {
        return authSettled && authCode == deskhub::AuthResultCode::Accepted;
    }

    void Start() {
        deskhub::TerminalClientCallbacks cb;
        cb.send = [this](std::span<const uint8_t> message) {
            std::vector<uint8_t> record(deskhub::kRecordPrefixSize + message.size());
            record.resize(deskhub::BuildRecord(record, message));
            if (connected)
                endpoint.SendStream(conn, deskhubp::kQuicControlStream, record);
            else
                pending.insert(pending.end(), record.begin(), record.end());
        };
        cb.onOutput = [this](std::span<const uint8_t> bytes) { screen.Write(bytes); };
        cb.onOpened = [this](const deskhub::TermOpenAck& ack) {
            ++opens;
            resumed = ack.resumed;
        };
        cb.onRefused = [this](deskhub::TermReason reason) { refusals.push_back(reason); };
        cb.onSessions = [this](const deskhub::TermSessionList& list) { listed = list; };
        cb.onExit = [this](int32_t code) { exits.push_back(code); };
        client = std::make_unique<deskhub::TerminalClient>(std::move(cb));
    }

    deskhubp::QuicCallbacks Hooks() {
        deskhubp::QuicCallbacks hooks;
        hooks.onConnected = [this](deskhubp::QuicConnId id, const NetAddr&) {
            conn = id;
            connected = true;
            if (!pending.empty()) {
                endpoint.SendStream(id, deskhubp::kQuicControlStream, pending);
                pending.clear();
            }
        };
        hooks.onStream = [this](deskhubp::QuicConnId, uint64_t, std::span<const uint8_t> bytes,
                             bool) {
            framer.Append(bytes);
            std::vector<uint8_t> message;
            while (framer.Next(message)) {
                if (HandleAuth(message)) continue;
                client->HandleMessage(message);
            }
        };
        return hooks;
    }

    void Pump(int rounds) {
        for (int i = 0; i < rounds; ++i) endpoint.Poll(NowUs(), kPollWaitMs);
    }

    bool PumpUntil(const std::function<bool()>& done, int rounds) {
        for (int i = 0; i < rounds; ++i) {
            if (done()) return true;
            endpoint.Poll(NowUs(), kPollWaitMs);
        }
        return done();
    }

    void Type(std::string_view text) {
        const std::string bytes = deskhub::term::EncodeText(text, screen.Modes());
        client->SendInput(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
    }
};

struct HostRig {
    deskhubp::SessionTransport sock{};
    deskhubp::TerminalHost term{};
    std::thread pump{};
    std::atomic<bool> stopFlag{false};

    ~HostRig() {
        Stop();
    }

    bool Start(const deskhubp::HostIdentity& identity, uint16_t port,
        std::vector<std::string>* audit = nullptr) {
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPemPath = identity.certPath;
        settings.keyPemPath = identity.keyPath;
        if (!sock.Listen(settings, port, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = identity;
        deskhubp::TransportAuthCallbacks hooks;
        sock.SetHostAuth(std::move(auth), std::move(hooks));
        sock.SetOnPeerGone([this](const NetAddr& peer) { term.OnPeerGone(peer); });

        deskhubp::TerminalHostCallbacks termHooks;
        if (audit != nullptr)
            termHooks.onAudit = [audit](std::string_view line) { audit->emplace_back(line); };
        if (!term.Start(sock, std::string(), std::move(termHooks))) return false;

        stopFlag.store(false, std::memory_order_release);
        pump = std::thread([this] { PumpLoop(); });
        return true;
    }

    void PumpLoop() {
        uint8_t buf[deskhub::kMaxRecordSize];
        while (!stopFlag.load(std::memory_order_acquire)) {
            NetAddr from;
            const int n = sock.RecvFrom(buf, sizeof(buf), from);
            if (n <= 0) continue;
            const std::optional<deskhub::CommonHeader> header =
                deskhub::ParseCommonHeader(std::span<const uint8_t>(buf, size_t(n)));
            if (header && header->chan == deskhub::Chan::Terminal)
                term.HandleMessage(from, std::span<const uint8_t>(buf, size_t(n)));
        }
    }

    bool Running() const {
        return term.Running();
    }
    size_t SessionCount() const {
        return term.SessionCount();
    }
    std::vector<deskhub::TerminalRecord> Sessions() const {
        return term.Sessions();
    }
    void KickSession(uint32_t termId) {
        term.KickSession(termId);
    }

    void Stop() {
        if (pump.joinable()) {
            stopFlag.store(true, std::memory_order_release);
            pump.join();
        }
        term.Stop();
        sock.Close();
    }
};

void TestHostSharesAShell() {
    std::printf("[termhost] a client opens a real shell over QUIC and gets its output back...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid() && clientIdentity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    std::vector<std::string> audit;
    HostRig host;
    const bool started = host.Start(identity, kTestPort, &audit);
    Check(started, "the terminal host attaches to the shared listener");
    if (!started) return;
    Check(host.Running(), "and reports itself as sharing");

    Viewer viewer;
    viewer.Start();
    Check(viewer.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
              "deskhub-test", viewer.Hooks()),
        "a client can dial it");
    Check(viewer.PumpUntil([&viewer] { return viewer.connected; }, kMaxRounds),
        "and the handshake completes");
    if (!viewer.connected) {
        host.Stop();
        return;
    }

    {
        Viewer stranger;
        stranger.Start();
        stranger.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
            "deskhub-test", stranger.Hooks());
        stranger.PumpUntil([&stranger] { return stranger.connected; }, kMaxRounds);
        stranger.BeginAuth(clientIdentity, identity.fingerprint, "9999");
        Check(stranger.PumpUntil([&stranger] { return stranger.authSettled; }, kMaxRounds),
            "an unlisted client key is settled by the host");
        Check(stranger.authCode == deskhub::AuthResultCode::PairingDisabled,
            "and rejected without approval");
        stranger.client->Open(deskhub::TermSize{80, 24}, "test-client");
        stranger.Pump(200);
        Check(host.SessionCount() == 0, "asking for a shell anyway starts nothing");
    }

    {
        Viewer silent;
        silent.Start();
        silent.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
            "deskhub-test", silent.Hooks());
        silent.PumpUntil([&silent] { return silent.connected; }, kMaxRounds);
        silent.client->Open(deskhub::TermSize{80, 24}, "test-client");
        silent.Pump(200);
        Check(host.SessionCount() == 0,
            "a connection that never proved itself gets no shell, however it asks");
    }

    Check(deskhubp::RememberPairedDevice(clientIdentity.fingerprint, "test-client", 500),
        "the owner grants the client key locally");
    viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    Check(viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds),
        "the permitted key signs and is admitted");

    viewer.client->Open(deskhub::TermSize{80, 24}, "test-client");
    Check(viewer.PumpUntil([&viewer] { return viewer.opens == 1; }, kMaxRounds),
        "and the shell opens with no passcode in the request at all");
    if (viewer.opens != 1) {
        host.Stop();
        return;
    }
    Check(!viewer.resumed, "which is a fresh one, not a resumed session");
    Check(host.SessionCount() == 1, "and the host lists it");

    const std::vector<deskhub::TerminalRecord> sessions = host.Sessions();
    Check(sessions.size() == 1 && sessions[0].clientName == "test-client",
        "the host knows who opened it");
    Check(!sessions.empty() && !sessions[0].clientEndpoint.empty(),
        "and where it came from");
    Check(!audit.empty() && audit[0].find("terminal opened") == 0,
        "the session is written to the audit log");
    Check(!audit.empty() &&
              audit[0].find("key=" + deskhub::FormatFingerprint(clientIdentity.fingerprint)) !=
                  std::string::npos,
        "carrying the key the client proved during pairing");

    viewer.PumpUntil([] { return false; }, 400);
    viewer.Type("echo deskhub-remote-ok\n");
    const bool sawIt = viewer.PumpUntil(
        [&viewer] {
            return viewer.screen.Text().find("deskhub-remote-ok") != std::string::npos;
        },
        kMaxRounds * 3);
    Check(sawIt, "a command typed on the client runs on the host and comes back to the grid");

    viewer.client->Resize(deskhub::TermSize{120, 40});
    viewer.Pump(100);
    Check(host.Sessions().size() == 1 && host.Sessions()[0].size == deskhub::TermSize{120, 40},
        "a window resize reaches the host");

    viewer.client->RequestList();
    Check(viewer.PumpUntil([&viewer] { return viewer.listed.sessions.size() == 1; }, kMaxRounds),
        "the host answers a listing with the shell it is keeping");
    Check(viewer.listed.sessions[0].state == deskhub::TerminalState::Live &&
              viewer.listed.sessions[0].clientName == "test-client",
        "naming its state and who opened it");

    viewer.client->Close();
    Check(viewer.PumpUntil([&host] { return host.SessionCount() == 0; }, kMaxRounds),
        "closing the shell from the client ends the session on the host");

    viewer.client->Open(deskhub::TermSize{80, 24}, "test-client");
    if (viewer.PumpUntil([&viewer] { return viewer.opens == 2; }, kMaxRounds)) {
        const std::vector<deskhub::TerminalRecord> open = host.Sessions();
        Check(open.size() == 1, "a second shell opens after the first one ended");
        host.KickSession(open[0].termId);
        Check(viewer.PumpUntil([&host] { return host.SessionCount() == 0; }, kMaxRounds),
            "and the host can end a shell from its own table");
        Check(viewer.PumpUntil([&viewer] { return !viewer.exits.empty(); }, kMaxRounds),
            "telling the client the shell is gone");
    }

    host.Stop();
    Check(!host.Running(), "and the host stops sharing cleanly");
    viewer.endpoint.Close();

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

bool WaitFor(const std::function<bool()>& done, int millis) {
    for (int i = 0; i < millis; ++i) {
        if (done()) return true;
        SleepUs(1000);
    }
    return done();
}

std::vector<std::string> LocalSnapshotRows(deskhubp::TerminalHost& term, uint32_t termId) {
    const deskhub::term::TerminalSnapshot shot = term.LocalSnapshot(termId, 0);
    std::vector<std::string> rows;
    for (uint16_t r = 0; r < shot.size.rows; ++r) {
        std::string line;
        for (uint16_t c = 0; c < shot.size.cols; ++c)
            line += deskhub::term::EncodeUtf8(shot.At(r, c).ch);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        rows.push_back(line);
    }
    return rows;
}

std::string LocalSnapshotReadingOrder(deskhubp::TerminalHost& term, uint32_t termId) {
    const deskhub::term::TerminalSnapshot shot = term.LocalSnapshot(termId, 0);
    std::string text;
    for (uint16_t r = 0; r < shot.size.rows; ++r)
        for (uint16_t c = 0; c < shot.size.cols; ++c)
            text += deskhub::term::EncodeUtf8(shot.At(r, c).ch);
    return text;
}

bool LocalSnapshotContains(deskhubp::TerminalHost& term, uint32_t termId,
    const std::string& needle) {
    return LocalSnapshotReadingOrder(term, termId).find(needle) != std::string::npos;
}

bool LocalSnapshotHasWholeRow(deskhubp::TerminalHost& term, uint32_t termId,
    const std::string& wanted) {
    for (const std::string& row : LocalSnapshotRows(term, termId))
        if (row == wanted) return true;
    return false;
}

void PrintLocalSnapshot(deskhubp::TerminalHost& term, uint32_t termId) {
    const std::vector<std::string> rows = LocalSnapshotRows(term, termId);
    for (size_t r = 0; r < rows.size(); ++r)
        if (!rows[r].empty()) std::printf("    row %02zu |%s|\n", r, rows[r].c_str());
}

size_t CountInLocalSnapshot(deskhubp::TerminalHost& term, uint32_t termId, char32_t ch) {
    const deskhub::term::TerminalSnapshot shot = term.LocalSnapshot(termId, 0);
    size_t found = 0;
    for (const deskhub::term::Cell& cell : shot.cells)
        if (cell.ch == ch) ++found;
    return found;
}

void TypeLocalChar(deskhubp::TerminalHost& term, uint32_t termId, char32_t ch) {
    deskhub::term::TermKeyEvent key;
    key.key = deskhub::term::TermKey::Char;
    key.codepoint = ch;
    term.SendLocalKey(termId, key);
}

void TypeLocalLine(deskhubp::TerminalHost& term, uint32_t termId, std::string_view line) {
    for (const char ch : line) TypeLocalChar(term, termId, char32_t(ch));
    deskhub::term::TermKeyEvent enter;
    enter.key = deskhub::term::TermKey::Enter;
    term.SendLocalKey(termId, enter);
}

void TestDroppedShellWaitsForItsClient() {
    std::printf("[termhost] a client that vanishes keeps its shell, and a second one picks it up...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid() && clientIdentity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    HostRig host;
    if (!host.Start(identity, kTestPort)) {
        Check(false, "the terminal host attaches to the shared listener");
        return;
    }

    Check(deskhubp::RememberPairedDevice(clientIdentity.fingerprint, "test-client", 500),
        "the first client key is authorized");
    Viewer first;
    first.Start();
    first.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
        "deskhub-test", first.Hooks());
    if (!first.PumpUntil([&first] { return first.connected; }, kMaxRounds)) {
        Check(false, "the first client dials in");
        host.Stop();
        return;
    }
    first.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    if (!first.PumpUntil([&first] { return first.Allowed(); }, kMaxRounds)) {
        Check(false, "and proves itself");
        host.Stop();
        return;
    }
    first.client->Open(deskhub::TermSize{80, 24}, "first-client");
    if (!first.PumpUntil([&first] { return first.opens == 1; }, kMaxRounds)) {
        Check(false, "and opens a shell");
        host.Stop();
        return;
    }
    const uint32_t id = host.Sessions()[0].termId;
    first.Type("echo kept-shell-marker\n");
    Check(first.PumpUntil(
              [&first] {
                  return first.screen.Text().find("kept-shell-marker") !=
                         std::string::npos;
              },
              kMaxRounds * 3),
        "which runs and shows its output");

    first.endpoint.Close();
    Check(WaitFor(
              [&host, id] {
                  const std::vector<deskhub::TerminalRecord> kept = host.Sessions();
                  return kept.size() == 1 && kept[0].termId == id &&
                         kept[0].state == deskhub::TerminalState::Detached;
              },
              10000),
        "losing the client detaches the shell instead of ending it");

    Viewer second;
    second.Start();
    second.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
        "deskhub-test", second.Hooks());
    if (!second.PumpUntil([&second] { return second.connected; }, kMaxRounds)) {
        Check(false, "a second client dials in");
        host.Stop();
        return;
    }
    second.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    if (!second.PumpUntil([&second] { return second.Allowed(); }, kMaxRounds)) {
        Check(false, "and proves itself too");
        host.Stop();
        return;
    }
    second.client->RequestList();
    Check(second.PumpUntil(
              [&second, id] {
                  for (const deskhub::TermSessionEntry& e : second.listed.sessions)
                      if (e.termId == id && e.state == deskhub::TerminalState::Detached)
                          return true;
                  return false;
              },
              kMaxRounds),
        "it sees the shell the first one left behind");
    second.client->Resume(id);
    Check(second.PumpUntil([&second] { return second.opens == 1 && second.resumed; }, kMaxRounds),
        "and picks it back up instead of starting over");
    Check(second.PumpUntil(
              [&second] {
                  return second.screen.Text().find("kept-shell-marker") !=
                         std::string::npos;
              },
              kMaxRounds * 3),
        "with the old output repainted on its grid");

    host.Stop();
    second.endpoint.Close();

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestAnotherClientClosesAShell() {
    std::printf("[termhost] any admitted client can end a shell, and the one in it is told...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid() && clientIdentity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    HostRig host;
    if (!host.Start(identity, kTestPort)) {
        Check(false, "the terminal host attaches to the shared listener");
        return;
    }

    Check(deskhubp::RememberPairedDevice(clientIdentity.fingerprint, "test-client", 500),
        "the owner client key is authorized");
    Viewer owner;
    owner.Start();
    owner.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
        "deskhub-test", owner.Hooks());
    if (!owner.PumpUntil([&owner] { return owner.connected; }, kMaxRounds)) {
        Check(false, "the client holding the shell dials in");
        host.Stop();
        return;
    }
    owner.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    if (!owner.PumpUntil([&owner] { return owner.Allowed(); }, kMaxRounds)) {
        Check(false, "and proves itself");
        host.Stop();
        return;
    }
    owner.client->Open(deskhub::TermSize{80, 24}, "owner-client");
    if (!owner.PumpUntil([&owner] { return owner.opens == 1; }, kMaxRounds)) {
        Check(false, "and opens a shell");
        host.Stop();
        return;
    }
    const uint32_t id = host.Sessions()[0].termId;

    Viewer other;
    other.Start();
    other.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
        "deskhub-test", other.Hooks());
    if (!other.PumpUntil([&other] { return other.connected; }, kMaxRounds)) {
        Check(false, "a second client dials in");
        host.Stop();
        return;
    }
    other.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    if (!other.PumpUntil([&other] { return other.Allowed(); }, kMaxRounds)) {
        Check(false, "and proves itself too");
        host.Stop();
        return;
    }

    other.client->RequestList();
    Check(other.PumpUntil(
              [&other, id] {
                  for (const deskhub::TermSessionEntry& e : other.listed.sessions)
                      if (e.termId == id && e.state == deskhub::TerminalState::Live) return true;
                  return false;
              },
              kMaxRounds),
        "it sees the shell the first one is sitting in");

    other.client->CloseSession(id);
    const auto bothPump = [&owner, &other](const std::function<bool()>& done, int rounds) {
        for (int i = 0; i < rounds; ++i) {
            if (done()) return true;
            owner.Pump(1);
            other.Pump(1);
        }
        return done();
    };
    Check(bothPump([&host] { return host.SessionCount() == 0; }, kMaxRounds),
        "closing it by id ends the shell even though another machine opened it");
    Check(bothPump([&owner] { return !owner.exits.empty(); }, kMaxRounds),
        "and the machine that was typing in it is told its shell ended");
    Check(other.client->State() != deskhub::TerminalClientState::Closed,
        "while the machine that closed it carries on");

    host.Stop();
    owner.endpoint.Close();
    other.endpoint.Close();

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestHostStopsAndAttachesShell() {
    std::printf("[termhost] the host takes a shell over and carries on where it stood...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid() || !clientIdentity.Valid()) return;

    HostRig host;
    if (!host.Start(identity, uint16_t(kTestPort + 4))) {
        Check(false, "the terminal host starts");
        return;
    }

    Check(deskhubp::RememberPairedDevice(clientIdentity.fingerprint, "test-client", 500),
        "the client key is authorized");
    Viewer viewer;
    viewer.Start();
    viewer.endpoint.Connect(deskhubp::QuicSettings{},
        NetAddr{0x7F000001u, uint16_t(kTestPort + 4)}, "deskhub-test", viewer.Hooks());
    viewer.PumpUntil([&viewer] { return viewer.connected; }, kMaxRounds);
    viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds);
    viewer.client->Open(deskhub::TermSize{80, 24}, "test-client");
    if (!viewer.PumpUntil([&viewer] { return viewer.opens == 1; }, kMaxRounds)) {
        Check(false, "the shell opens for the remote client");
        host.Stop();
        return;
    }
    const uint32_t termId = host.Sessions()[0].termId;

    viewer.PumpUntil([] { return false; }, 400);
    viewer.Type("echo deskhub-before-take\n");
    viewer.PumpUntil(
        [&viewer] {
            return viewer.screen.Text().find("deskhub-before-take") != std::string::npos;
        },
        kMaxRounds * 3);

    Check(host.term.AttachLocal(termId), "the host takes the shell for itself");
    Check(host.term.LocalAlive(termId), "and now holds it locally");
    Check(viewer.PumpUntil([&viewer] { return !viewer.exits.empty(); }, kMaxRounds),
        "the remote client is told its shell ended");
    Check(host.SessionCount() == 1 &&
              host.Sessions()[0].state == deskhub::TerminalState::Local,
        "the session stays in the table, marked as the host's own");
    Check(WaitFor(
              [&host, termId] {
                  return LocalSnapshotContains(host.term, termId, "deskhub-before-take");
              },
              5000),
        "what the shell printed before the takeover is already on the host's grid");

    viewer.Type("echo deskhub-intruder\n");
    viewer.Pump(400);

    TypeLocalLine(host.term, termId, "echo deskhub-after-take");
    Check(WaitFor(
              [&host, termId] {
                  return LocalSnapshotHasWholeRow(host.term, termId, "deskhub-after-take");
              },
              30000),
        "a command typed at the host runs in the very same shell");
    SleepUs(400'000);
    Check(!LocalSnapshotContains(host.term, termId, "deskhub-intruder"),
        "while the kicked client can no longer type into it");

    const std::string typed = "zqx";
    std::vector<size_t> already;
    for (const char marker : typed)
        already.push_back(CountInLocalSnapshot(host.term, termId, char32_t(marker)));
    bool everyKeyLandedOnce = true;
    for (size_t i = 0; i < typed.size(); ++i) {
        TypeLocalChar(host.term, termId, char32_t(typed[i]));
        const char32_t marker = char32_t(typed[i]);
        const size_t want = already[i] + 1;
        const bool landed = WaitFor(
            [&host, termId, marker, want] {
                return CountInLocalSnapshot(host.term, termId, marker) == want;
            },
            10000);
        if (!landed)
            std::printf("    key '%c' wanted count %zu, got %zu\n", typed[i], want,
                CountInLocalSnapshot(host.term, termId, marker));
        everyKeyLandedOnce &= landed;
        SleepUs(150'000);
    }
    const bool typedRunTogether = LocalSnapshotContains(host.term, termId, typed);
    if (!typedRunTogether)
        std::printf("    typed '%s' does not read back in order\n", typed.c_str());
    if (!everyKeyLandedOnce || !typedRunTogether) PrintLocalSnapshot(host.term, termId);
    Check(everyKeyLandedOnce && typedRunTogether,
        "keys typed one at a time, with a human's pause between them, arrive in order");
    bool eachKeyOnce = true;
    for (size_t i = 0; i < typed.size(); ++i)
        eachKeyOnce &=
            CountInLocalSnapshot(host.term, termId, char32_t(typed[i])) == already[i] + 1;
    Check(eachKeyOnce, "and one key press puts exactly one character on the grid");

    host.term.ResizeLocal(termId, deskhub::TermSize{100, 30});
    Check(host.Sessions()[0].size == deskhub::TermSize{100, 30},
        "the host window now owns the shell's size");

    host.term.CloseLocal(termId);
    Check(host.SessionCount() == 0, "closing the host window ends the shell");
    Check(!host.term.LocalAlive(termId), "which is gone for good");
    Check(host.term.LocalSnapshot(termId, 0).cells.empty(),
        "and has nothing left to draw");

    host.Stop();
    viewer.endpoint.Close();

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestViewerTrustsThenRunsAShell() {
    std::printf("[termhost] a pinned host key and client auth open the shell...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedTrust = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    ForgetHostIdentity();
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);

    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    HostRig host;
    if (!host.Start(identity, uint16_t(kTestPort + 1))) {
        Check(false, "the terminal host starts");
        return;
    }

    deskhubp::TerminalViewerConfig viewerConfig;
    viewerConfig.host = NetAddr{0x7F000001u, uint16_t(kTestPort + 1)};
    viewerConfig.hostLabel = "deskhub-test";
    viewerConfig.passcode = kTestPasscode;
    viewerConfig.clientName = "shared-viewer";
    viewerConfig.size = deskhub::TermSize{80, 24};
    const auto clientKey = deskhubp::LoadOrCreateClientIdentity();
    Check(clientKey.Valid() &&
              deskhubp::RememberPairedDevice(clientKey.fingerprint, "shared-viewer", 500),
        "the viewer key is authorized before connecting");
    Check(deskhubp::RememberTrustedHost(viewerConfig.host.ToString(), viewerConfig.hostLabel,
              identity.fingerprint, NowUnixSeconds()),
        "the host public key is pinned before connecting");

    std::atomic<int> trustAsks{0};
    std::atomic<int> redraws{0};
    deskhubp::TerminalViewerCallbacks hooks;
    hooks.onTrustAsked = [&trustAsks](deskhub::TrustVerdict, std::string_view) { ++trustAsks; };
    hooks.onRedraw = [&redraws] { ++redraws; };

    deskhubp::TerminalViewer viewer;
    Check(viewer.Start(viewerConfig, std::move(hooks)), "the viewer starts");
    Check(WaitFor([&viewer] { return viewer.State() == deskhubp::TerminalViewerState::Live; },
              20000),
        "a host with a configured key opens a terminal session");
    Check(trustAsks == 0, "no trust popup is shown while connecting");
    Check(viewer.Verdict() == deskhub::TrustVerdict::Trusted,
        "the configured host key is trusted");
    Check(viewer.Fingerprint() == deskhub::FormatFingerprint(identity.fingerprint),
        "and the key it settled on is the one the host actually holds");
    Check(deskhubp::CheckTrustedHost(viewerConfig.host.ToString(), identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "the saved key is still present for the next visit");

    viewer.SendText("echo deskhub-viewer-ok\n");
    Check(WaitFor(
              [&viewer] {
                  const deskhubp::TerminalSnapshot shot = viewer.Snapshot();
                  for (uint16_t r = 0; r < shot.size.rows; ++r) {
                      std::string line;
                      for (uint16_t c = 0; c < shot.size.cols; ++c)
                          line += deskhub::term::EncodeUtf8(shot.At(r, c).ch);
                      if (line.find("deskhub-viewer-ok") != std::string::npos) return true;
                  }
                  return false;
              },
              30000),
        "and what the shell prints reaches the grid the client draws");
    Check(redraws > 0, "the client was told to repaint");

    const deskhubp::TerminalSnapshot shot = viewer.Snapshot();
    Check(shot.size == deskhub::TermSize{80, 24} &&
              shot.cells.size() == size_t(shot.size.rows) * shot.size.cols,
        "the snapshot holds one cell per position");
    Check(shot.At(999, 999) == deskhub::term::Cell{}, "and reading outside it is blank, not a crash");
    Check(shot.scrollOffset == 0 && shot.cursor.visible,
        "the live view is at the bottom, with the cursor on it");

    const std::string marker = "deskhub-scrollback-marker";
    viewer.SendText("echo " + marker + "\n");
    for (int i = 0; i < 60; ++i) viewer.SendText("echo filler-" + std::to_string(i) + "\n");
    Check(WaitFor([&viewer] { return viewer.Snapshot().scrollbackRows > 24; }, 30000),
        "enough output scrolls the first lines off the top and into the scrollback");

    const deskhubp::TerminalSnapshot back = viewer.Snapshot(viewer.Snapshot().scrollbackRows);
    Check(back.scrollOffset > 0, "the view can be walked back into it");
    Check(back.size == shot.size && back.cells.size() == shot.cells.size(),
        "a scrolled view is the same shape as the live one");
    Check(!back.cursor.visible, "with no cursor drawn, because it is not on this screen");
    const deskhubp::TerminalSnapshot clamped = viewer.Snapshot(999999);
    Check(clamped.scrollOffset == clamped.scrollbackRows,
        "scrolling past the oldest line stops there instead of running off the end");

    viewer.Stop();
    host.Stop();

    const deskhub::TrustStore trusted = deskhubp::LoadTrustStore();
    Check(trusted.Size() == 1, "the machine we trusted was written down");
    Check(deskhubp::CheckTrustedHost(viewerConfig.host.ToString(), identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "so the next connection will not ask again");

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedTrust.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, savedTrust);
}

void TestHostRefusesWithoutListener() {
    std::printf("[termhost] a terminal never offers a shell without the shared listener...\n");
    deskhubp::SessionTransport closed;
    deskhubp::TerminalHost host;
    Check(!host.Start(closed, std::string(), deskhubp::TerminalHostCallbacks{}),
        "starting on a transport that is not listening is refused");
    Check(!host.Running() && host.SessionCount() == 0, "and nothing is left running");
    host.Stop();
    Check(true, "stopping one that never started is safe");
}

void TestUnknownClientAndChangedHostKeyAreDenied() {
    std::printf("[termhost] unlisted clients and changed host keys are denied...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) return;

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedTrust = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    const std::string savedPaired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    ForgetHostIdentity();
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    deskhubp::ForgetAllPairedDevices();

    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    HostRig host;
    if (identity.Valid() && host.Start(identity, uint16_t(kTestPort + 2))) {
        deskhubp::TerminalViewerConfig config;
        config.host = NetAddr{0x7F000001u, uint16_t(kTestPort + 2)};
        config.hostLabel = "deskhub-test";
        config.clientName = "unlisted-viewer";
        config.size = deskhub::TermSize{80, 24};
        Check(deskhubp::RememberTrustedHost(config.host.ToString(), config.hostLabel,
                  identity.fingerprint, NowUnixSeconds()),
            "the host key is pinned before testing client access");

        deskhubp::TerminalViewer unlisted;
        Check(unlisted.Start(config, deskhubp::TerminalViewerCallbacks{}),
            "an unlisted viewer can begin a connection");
        Check(WaitFor([&unlisted] {
            return unlisted.State() == deskhubp::TerminalViewerState::Refused;
        },
                  15000),
            "an unlisted client key is refused without a popup");
        Check(host.SessionCount() == 0, "no shell opens for the unlisted key");
        unlisted.Stop();

        deskhub::Fingerprint stale = identity.fingerprint;
        stale.bytes[0] ^= 0xff;
        Check(deskhubp::RememberTrustedHost(config.host.ToString(), config.hostLabel,
                  stale, NowUnixSeconds()),
            "the host pin is deliberately changed");
        deskhubp::TerminalViewer changed;
        Check(changed.Start(config, deskhubp::TerminalViewerCallbacks{}),
            "the changed-key viewer begins a connection");
        Check(WaitFor([&changed] {
            return changed.State() == deskhubp::TerminalViewerState::Failed;
        },
                  15000),
            "the changed host key is rejected before client auth");
        Check(changed.Verdict() == deskhub::TrustVerdict::Changed,
            "the failure is identified as a changed host key");
        changed.Stop();
        host.Stop();
    }

    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedTrust.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, savedTrust);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestAFloodOfOutputNeverTearsTheStream() {
    std::printf("[termhost] a command that floods the screen keeps the session alive...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const std::string savedCert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");

    const uint16_t port = uint16_t(kTestPort + 5);
    HostRig host;
    if (identity.Valid() && clientIdentity.Valid() &&
        host.Start(identity, port)) {
        Check(deskhubp::RememberPairedDevice(clientIdentity.fingerprint, "test-client", 500),
            "the high-output client key is authorized");
        Viewer viewer;
        viewer.Start();
        viewer.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, port},
            "deskhub-test", viewer.Hooks());
        viewer.PumpUntil([&viewer] { return viewer.connected; }, kMaxRounds);
        viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
        viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds);
        viewer.client->Open(deskhub::TermSize{80, 24}, "test-client");

        if (viewer.PumpUntil([&viewer] { return viewer.opens == 1; }, kMaxRounds)) {
            viewer.PumpUntil([] { return false; }, 400);
            viewer.Type("seq 1 400000 2>/dev/null; echo BURST-DONE\n");
            WaitFor([] { return false; }, 800);

            const bool caughtUp = viewer.PumpUntil(
                [&viewer] {
                    return viewer.screen.Text().find("BURST-DONE") != std::string::npos;
                },
                kMaxRounds * 5);

            Check(!viewer.framer.Failed(),
                "the record stream never tears, however far behind the client falls");
            Check(host.SessionCount() == 1, "so the shell is still open on the host");
            Check(viewer.exits.empty(), "and the client was never told the shell had gone");
            Check(caughtUp, "the client catches up on what the command last printed");
        }
        viewer.endpoint.Close();
    }

    host.Stop();
    if (!savedCert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, savedCert);
    if (!savedKey.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

}

void RunTerminalHostTests() {
    TestHostRefusesWithoutListener();
    TestHostSharesAShell();
    TestDroppedShellWaitsForItsClient();
    TestAnotherClientClosesAShell();
    TestHostStopsAndAttachesShell();
    TestViewerTrustsThenRunsAShell();
    TestUnknownClientAndChangedHostKeyAreDenied();
    TestAFloodOfOutputNeverTearsTheStream();
}
