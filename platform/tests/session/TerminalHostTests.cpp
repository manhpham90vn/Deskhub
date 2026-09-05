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

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
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
                authCode = deskhub::AuthResultCode::WrongPasscode;
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
    std::mutex mutex{};
    std::vector<deskhubp::PairingRequest> asks{};
    std::vector<std::pair<uint64_t, bool>> answers{};

    ~HostRig() {
        Stop();
    }

    bool Start(const deskhubp::HostIdentity& identity, uint16_t port,
        const std::string& passcode, std::vector<std::string>* audit = nullptr) {
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPemPath = identity.certPath;
        settings.keyPemPath = identity.keyPath;
        if (!sock.Listen(settings, port, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = identity;
        auth.SetPasscode(deskhubp::LoadOrCreateAuthSalt(), passcode);
        auth.allowNewPairings = true;
        deskhubp::TransportAuthCallbacks hooks;
        hooks.onApprovalNeeded = [this](const NetAddr& peer, const deskhub::Fingerprint& fp,
                                     std::string_view name) {
            const std::lock_guard<std::mutex> lock(mutex);
            asks.push_back(deskhubp::PairingRequest{peer.Pack(),
                deskhub::ShortFingerprint(fp), std::string(name)});
        };
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
            std::vector<std::pair<uint64_t, bool>> pending;
            {
                const std::lock_guard<std::mutex> lock(mutex);
                pending.swap(answers);
            }
            for (const auto& [addrPacked, allowed] : pending)
                sock.ApproveConnection(NetAddr::Unpack(addrPacked), allowed);

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

    std::vector<deskhubp::PairingRequest> TakePairingRequests() {
        const std::lock_guard<std::mutex> lock(mutex);
        std::vector<deskhubp::PairingRequest> out;
        out.swap(asks);
        return out;
    }

    void AnswerPairing(uint64_t addrPacked, bool allowed) {
        const std::lock_guard<std::mutex> lock(mutex);
        answers.emplace_back(addrPacked, allowed);
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

    const SavedIdentity guard;
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid() && clientIdentity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    std::vector<std::string> audit;
    HostRig host;
    const bool started = host.Start(identity, kTestPort, kTestPasscode, &audit);
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
            "a wrong code is settled by the host");
        Check(stranger.authCode == deskhub::AuthResultCode::WrongPasscode,
            "and named as the reason");
        stranger.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");
        stranger.Pump(200);
        Check(host.SessionCount() == 0, "asking for a shell anyway starts nothing");
    }

    {
        Viewer silent;
        silent.Start();
        silent.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, kTestPort},
            "deskhub-test", silent.Hooks());
        silent.PumpUntil([&silent] { return silent.connected; }, kMaxRounds);
        silent.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");
        silent.Pump(200);
        Check(host.SessionCount() == 0,
            "a connection that never proved itself gets no shell, however it asks");
    }

    viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    Check(viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds),
        "the right code proves the machine, without the code ever being sent");

    viewer.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");
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

    viewer.client->Close();
    Check(viewer.PumpUntil([&host] { return host.SessionCount() == 0; }, kMaxRounds),
        "closing the shell from the client ends the session on the host");

    viewer.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");
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

void TestHostStopsAndAttachesShell() {
    std::printf("[termhost] the host takes a shell over and carries on where it stood...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const SavedIdentity guard;
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid() || !clientIdentity.Valid()) return;

    HostRig host;
    if (!host.Start(identity, uint16_t(kTestPort + 4), kTestPasscode)) {
        Check(false, "the terminal host starts");
        return;
    }

    Viewer viewer;
    viewer.Start();
    viewer.endpoint.Connect(deskhubp::QuicSettings{},
        NetAddr{0x7F000001u, uint16_t(kTestPort + 4)}, "deskhub-test", viewer.Hooks());
    viewer.PumpUntil([&viewer] { return viewer.connected; }, kMaxRounds);
    viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds);
    viewer.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");
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

    host.term.SendLocalText(termId, "echo deskhub-after-take\n");
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

    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestViewerTrustsThenRunsAShell() {
    std::printf("[termhost] the passcode settles a new key, and the shell runs...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termhost] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const SavedIdentity guard;
    const std::string savedTrust = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    deskhubp::ForgetHostIdentity();
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);

    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    HostRig host;
    if (!host.Start(identity, uint16_t(kTestPort + 1), kTestPasscode)) {
        Check(false, "the terminal host starts");
        return;
    }

    deskhubp::TerminalViewerConfig viewerConfig;
    viewerConfig.host = NetAddr{0x7F000001u, uint16_t(kTestPort + 1)};
    viewerConfig.hostLabel = "deskhub-test";
    viewerConfig.passcode = kTestPasscode;
    viewerConfig.clientName = "shared-viewer";
    viewerConfig.size = deskhub::TermSize{80, 24};

    std::atomic<int> trustAsks{0};
    std::atomic<int> redraws{0};
    deskhubp::TerminalViewerCallbacks hooks;
    hooks.onTrustAsked = [&trustAsks](deskhub::TrustVerdict, std::string_view) { ++trustAsks; };
    hooks.onRedraw = [&redraws] { ++redraws; };

    deskhubp::TerminalViewer viewer;
    Check(viewer.Start(viewerConfig, std::move(hooks)), "the viewer starts");
    Check(WaitFor([&viewer] { return viewer.State() == deskhubp::TerminalViewerState::Live; },
              20000),
        "a stranger guarded by a passcode opens without stopping to ask about its key");
    Check(trustAsks == 0, "so the user is never shown a fingerprint they would only click past");
    Check(viewer.Verdict() == deskhub::TrustVerdict::Trusted,
        "the accepted passcode is what settled it");
    Check(viewer.Fingerprint() == deskhub::FormatFingerprint(identity.fingerprint),
        "and the key it settled on is the one the host actually holds");
    Check(deskhubp::CheckTrustedHost(viewerConfig.host.ToString(), identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "which is written down, so the next visit needs no passcode to recognise it");

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

void TestTheTwoCasesAPasscodeCannotSettle() {
    std::printf("[termhost] no passcode, or a key that changed, still goes to the user...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) return;

    const SavedIdentity guard;
    const std::string savedTrust = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    const std::string savedPaired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetHostIdentity();
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    deskhubp::ForgetAllPairedDevices();

    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    HostRig host;
    if (host.Start(identity, uint16_t(kTestPort + 2), std::string())) {
        deskhubp::TerminalViewerConfig open;
        open.host = NetAddr{0x7F000001u, uint16_t(kTestPort + 2)};
        open.hostLabel = "deskhub-test";
        open.clientName = "shared-viewer";
        open.size = deskhub::TermSize{80, 24};

        std::atomic<int> asks{0};
        deskhubp::TerminalViewerCallbacks hooks;
        hooks.onTrustAsked = [&asks](deskhub::TrustVerdict, std::string_view) { ++asks; };

        deskhubp::TerminalViewer viewer;
        Check(viewer.Start(open, std::move(hooks)), "a viewer with no passcode starts");
        Check(!WaitFor([&viewer] {
            return viewer.State() == deskhubp::TerminalViewerState::Live;
        },
                  3000),
            "and never reaches a shell while nobody at the host has said yes");
        Check(asks == 0, "its own user is not asked to vouch for a machine they cannot check");
        Check(host.SessionCount() == 0, "and no shell was started for it");

        const std::vector<deskhubp::PairingRequest> question = host.TakePairingRequests();
        Check(question.size() == 1, "the question is queued for the person at the host instead");
        Check(!question.empty() && question[0].name == "shared-viewer",
            "named after the machine that is asking");
        if (!question.empty()) {
            host.AnswerPairing(question[0].addrPacked, false);
            Check(WaitFor([&viewer] {
                return viewer.State() == deskhubp::TerminalViewerState::Refused;
            },
                      15000),
                "saying no turns that machine away");
            Check(deskhubp::LoadPairedDevices().Size() == 0, "and pairs nothing");
        }
        viewer.Stop();

        deskhubp::TerminalViewer approved;
        Check(approved.Start(open, deskhubp::TerminalViewerCallbacks{}),
            "the machine can ask again");
        std::vector<deskhubp::PairingRequest> retry;
        Check(WaitFor([&host, &retry] {
            const std::vector<deskhubp::PairingRequest> got = host.TakePairingRequests();
            retry.insert(retry.end(), got.begin(), got.end());
            return !retry.empty();
        },
                  15000),
            "and the host queues a fresh question");
        if (!retry.empty()) {
            host.AnswerPairing(retry[0].addrPacked, true);
            Check(WaitFor([&approved] {
                return approved.State() == deskhubp::TerminalViewerState::Live;
            },
                      20000),
                "saying yes opens the shell");
            Check(host.SessionCount() == 1, "which the host lists");
            Check(deskhubp::LoadPairedDevices().Size() == 1,
                "and the machine is written down as paired");
        }
        approved.Stop();
        host.Stop();
    }

    deskhub::Fingerprint impostor = identity.fingerprint;
    impostor.bytes[0] = uint8_t(impostor.bytes[0] ^ 0xFF);
    const std::string endpoint = NetAddr{0x7F000001u, uint16_t(kTestPort + 2)}.ToString();
    Check(deskhubp::RememberTrustedHost(endpoint, "deskhub-test", impostor, 1),
        "the client has met this address before, holding a different key");
    Check(deskhubp::CheckTrustedHost(endpoint, identity.fingerprint) ==
              deskhub::TrustVerdict::Changed,
        "so the key the host now presents reads as changed, not as a first meeting");

    HostRig second;
    if (second.Start(identity, uint16_t(kTestPort + 2), kTestPasscode)) {
        deskhubp::TerminalViewerConfig withCode;
        withCode.host = NetAddr{0x7F000001u, uint16_t(kTestPort + 2)};
        withCode.hostLabel = "deskhub-test";
        withCode.passcode = kTestPasscode;
        withCode.clientName = "shared-viewer";
        withCode.size = deskhub::TermSize{80, 24};

        std::atomic<int> asks{0};
        deskhubp::TerminalViewerCallbacks hooks;
        hooks.onTrustAsked = [&asks](deskhub::TrustVerdict, std::string_view) { ++asks; };

        deskhubp::TerminalViewer viewer;
        Check(viewer.Start(withCode, std::move(hooks)), "a viewer that knows the passcode starts");
        Check(WaitFor([&viewer] {
            return viewer.State() == deskhubp::TerminalViewerState::Deciding;
        },
                  15000),
            "and is still stopped, because a passcode cannot answer a changed key");
        Check(WaitFor([&asks] { return asks.load() == 1; }, 5000), "the warning is raised");
        Check(viewer.Verdict() == deskhub::TrustVerdict::Changed, "as a change, not a stranger");
        viewer.RejectFingerprint();
        viewer.Stop();
        second.Stop();
    }

    if (savedTrust.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, savedTrust);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

void TestWrongGuessesLockTheHost() {
    std::printf("[termhost] three wrong passcodes lock the host for a while...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[termhost] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    const std::string savedPaired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid() || !clientIdentity.Valid()) return;

    HostRig host;
    if (!host.Start(identity, uint16_t(kTestPort + 3), kTestPasscode)) {
        Check(false, "the terminal host starts");
        return;
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        Viewer stranger;
        stranger.Start();
        stranger.endpoint.Connect(deskhubp::QuicSettings{},
            NetAddr{0x7F000001u, uint16_t(kTestPort + 3)}, "deskhub-test", stranger.Hooks());
        stranger.PumpUntil([&stranger] { return stranger.connected; }, kMaxRounds);
        stranger.BeginAuth(clientIdentity, identity.fingerprint, "0000");
        stranger.PumpUntil([&stranger] { return stranger.authSettled; }, kMaxRounds);
        Check(stranger.authCode == deskhub::AuthResultCode::WrongPasscode,
            "a wrong code is named as wrong while the door is still open");
        stranger.endpoint.Close();
    }

    Viewer lockedOut;
    lockedOut.Start();
    lockedOut.endpoint.Connect(deskhubp::QuicSettings{},
        NetAddr{0x7F000001u, uint16_t(kTestPort + 3)}, "deskhub-test", lockedOut.Hooks());
    lockedOut.PumpUntil([&lockedOut] { return lockedOut.connected; }, kMaxRounds);
    lockedOut.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
    lockedOut.PumpUntil([&lockedOut] { return lockedOut.authSettled; }, kMaxRounds);
    Check(lockedOut.authCode == deskhub::AuthResultCode::Locked,
        "after three wrong guesses even the right code is told to wait");
    Check(host.SessionCount() == 0, "and nothing opened");
    lockedOut.endpoint.Close();

    host.Stop();
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

    const SavedIdentity guard;
    const std::string savedPaired =
        deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetAllPairedDevices();
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity clientIdentity =
        deskhubp::LoadOrCreateHostIdentity("deskhub-client");
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");

    const uint16_t port = uint16_t(kTestPort + 5);
    HostRig host;
    if (identity.Valid() && clientIdentity.Valid() &&
        host.Start(identity, port, kTestPasscode)) {
        Viewer viewer;
        viewer.Start();
        viewer.endpoint.Connect(deskhubp::QuicSettings{}, NetAddr{0x7F000001u, port},
            "deskhub-test", viewer.Hooks());
        viewer.PumpUntil([&viewer] { return viewer.connected; }, kMaxRounds);
        viewer.BeginAuth(clientIdentity, identity.fingerprint, kTestPasscode);
        viewer.PumpUntil([&viewer] { return viewer.Allowed(); }, kMaxRounds);
        viewer.client->Open(std::string(), deskhub::TermSize{80, 24}, "test-client");

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
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}

}

void RunTerminalHostTests() {
    TestHostRefusesWithoutListener();
    TestHostSharesAShell();
    TestHostStopsAndAttachesShell();
    TestViewerTrustsThenRunsAShell();
    TestTheTwoCasesAPasscodeCannotSettle();
    TestWrongGuessesLockTheHost();
    TestAFloodOfOutputNeverTearsTheStream();
}
