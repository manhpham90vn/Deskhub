#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/ffi/TerminalFfi.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/Pty.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint16_t kFfiTestPort = 47821;

struct FfiHostRig {
    deskhubp::SessionTransport sock{};
    deskhubp::TerminalHost term{};
    std::thread pump{};
    std::atomic<bool> stopFlag{false};

    ~FfiHostRig() {
        Stop();
    }

    bool Start(const deskhubp::HostIdentity& identity, const std::string& passcode) {
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPemPath = identity.certPath;
        settings.keyPemPath = identity.keyPath;
        if (!sock.Listen(settings, kFfiTestPort, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = identity;
        auth.SetPasscode(deskhubp::LoadOrCreateAuthSalt(), passcode);
        auth.allowNewPairings = true;
        sock.SetHostAuth(std::move(auth), deskhubp::TransportAuthCallbacks{});
        sock.SetOnPeerGone([this](const NetAddr& peer) { term.OnPeerGone(peer); });

        if (!term.Start(sock, std::string(), deskhubp::TerminalHostCallbacks{})) return false;

        pump = std::thread([this] {
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
        });
        return true;
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

bool WaitForMs(const std::function<bool()>& done, int millis) {
    for (int i = 0; i < millis; ++i) {
        if (done()) return true;
        SleepUs(1000);
    }
    return done();
}

size_t GridCount(DHTermSession* session, char wanted) {
    DHTermGrid grid{};
    std::vector<DHTermCell> cells(size_t(200) * 100);
    if (!dh_term_grid(session, 0, cells.data(), uint32_t(cells.size()), &grid)) return 0;
    size_t found = 0;
    for (uint32_t i = 0; i < uint32_t(grid.rows) * grid.cols; ++i)
        if (cells[i].codepoint == uint32_t(wanted)) ++found;
    return found;
}

std::vector<std::string> GridRows(DHTermSession* session) {
    DHTermGrid grid{};
    std::vector<DHTermCell> cells(size_t(200) * 100);
    if (!dh_term_grid(session, 0, cells.data(), uint32_t(cells.size()), &grid)) return {};
    std::vector<std::string> rows;
    for (uint32_t r = 0; r < grid.rows; ++r) {
        std::string line;
        for (uint32_t c = 0; c < grid.cols; ++c) {
            const uint32_t cp = cells[size_t(r) * grid.cols + c].codepoint;
            line += cp >= 0x20 && cp < 0x7F ? char(cp) : ' ';
        }
        rows.push_back(line);
    }
    return rows;
}

bool GridContains(DHTermSession* session, const std::string& needle) {
    std::string readingOrder;
    for (const std::string& row : GridRows(session)) readingOrder += row;
    return readingOrder.find(needle) != std::string::npos;
}

bool GridHasWholeRow(DHTermSession* session, const std::string& wanted) {
    for (std::string row : GridRows(session)) {
        while (!row.empty() && row.back() == ' ') row.pop_back();
        if (row == wanted) return true;
    }
    return false;
}

void PrintGrid(DHTermSession* session) {
    const std::vector<std::string> rows = GridRows(session);
    for (size_t r = 0; r < rows.size(); ++r) {
        std::string row = rows[r];
        while (!row.empty() && row.back() == ' ') row.pop_back();
        if (!row.empty()) std::printf("    row %02zu |%s|\n", r, row.c_str());
    }
}

void TestNullHandlesAndBadAddressesAreHarmless() {
    std::printf("[termffi] a null handle or a bad address never turns into a crash...\n");
    DHTermCallbacks callbacks{};
    Check(dh_term_open(nullptr, "", 80, 24, &callbacks) == nullptr, "no address opens nothing");
    Check(dh_term_open("not-an-address", "", 80, 24, &callbacks) == nullptr,
        "an address that does not parse opens nothing");

    Check(dh_term_state(nullptr) == DHTermIdle, "a null session is idle, not undefined");
    Check(dh_term_verdict(nullptr) == int32_t(deskhub::TrustVerdict::Unknown),
        "and has no verdict to report");

    char text[32] = {'x'};
    Check(dh_term_message(nullptr, text, sizeof(text)) == 0 && text[0] == '\0',
        "a null session has no message, and the buffer is still terminated");
    Check(dh_term_fingerprint(nullptr, text, sizeof(text)) == 0, "and no fingerprint");

    DHTermGrid grid{};
    Check(!dh_term_grid(nullptr, 0, nullptr, 0, &grid), "a null session draws no grid");

    dh_term_send_text(nullptr, "echo hi\n");
    dh_term_paste(nullptr, "clip");
    dh_term_send_key(nullptr, DHTermKeyEnter, 0, false, false, false);
    dh_term_resize(nullptr, 80, 24);
    dh_term_accept_key(nullptr);
    dh_term_reject_key(nullptr);
    dh_term_stop(nullptr);
    Check(true, "every other call on a null session is a quiet no-op");
}

}

void RunTerminalFfiTests() {
    std::printf("--- ffi: the terminal surface every mobile client drives ---\n");
    TestNullHandlesAndBadAddressesAreHarmless();
    std::printf("[termffi] a shell opens, echoes and resizes through the C ABI...\n");
    if (!deskhubp::QuicAvailable() || deskhubp::DefaultShell().empty()) {
        std::printf("[termffi] skipped: this build has no QUIC library or no shell to host\n");
        return;
    }

    const SavedIdentity guard;
    const std::string savedTrust = deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName);
    const std::string savedPaired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
    deskhubp::ForgetHostIdentity();
    deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    deskhubp::ForgetAllPairedDevices();

    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    FfiHostRig rig;
    if (!rig.Start(identity, kTestPasscode)) {
        Check(false, "the host rig starts");
        return;
    }

    struct CallbackSeen {
        std::atomic<int32_t> lastState{DHTermIdle};
        std::atomic<int> redraws{0};
    };
    CallbackSeen seen;
    DHTermCallbacks callbacks{};
    callbacks.onState = [](int32_t state, const char*, void* user) {
        static_cast<CallbackSeen*>(user)->lastState.store(state);
    };
    callbacks.onRedraw = [](void* user) {
        static_cast<CallbackSeen*>(user)->redraws.fetch_add(1);
    };
    callbacks.user = &seen;

    const std::string address = "127.0.0.1:" + std::to_string(kFfiTestPort);
    DHTermSession* session = dh_term_open(address.c_str(), kTestPasscode, 80, 24, &callbacks);
    Check(session != nullptr, "the C ABI opens a shell session");
    if (session == nullptr) {
        rig.Stop();
        return;
    }

    Check(WaitForMs([session] { return dh_term_state(session) == DHTermLive; }, 20000),
        "and it reaches Live with the passcode proved, never sent");
    Check(seen.lastState.load() == DHTermLive, "the state callback saw the same journey");
    Check(rig.term.SessionCount() == 1, "the host lists the shell");

    char fingerprint[64] = {};
    dh_term_fingerprint(session, fingerprint, sizeof(fingerprint));
    Check(std::string(fingerprint).rfind("SHA256:", 0) == 0,
        "the host key is available for a warning dialog to show");

    dh_term_send_text(session, "echo dh-ffi-ok\n");
    Check(WaitForMs([session] { return GridHasWholeRow(session, "dh-ffi-ok"); }, 30000),
        "typed text runs on the host and the grid comes back through the C ABI");
    Check(seen.redraws.load() > 0, "the redraw callback fired as the grid changed");
    SleepUs(400'000);

    const std::string typed = "zqx";
    std::vector<size_t> already;
    for (const char marker : typed) already.push_back(GridCount(session, marker));
    bool everyKeyLandedOnce = true;
    for (size_t i = 0; i < typed.size(); ++i) {
        dh_term_send_key(session, DHTermKeyChar, uint32_t(typed[i]), false, false, false);
        const char marker = typed[i];
        const size_t want = already[i] + 1;
        const bool landed =
            WaitForMs([session, marker, want] { return GridCount(session, marker) == want; },
                10000);
        if (!landed)
            std::printf("    key '%c' wanted count %zu, got %zu\n", marker, want,
                GridCount(session, marker));
        everyKeyLandedOnce &= landed;
        SleepUs(150'000);
    }
    const bool typedRunTogether = GridContains(session, typed);
    if (!typedRunTogether)
        std::printf("    typed '%s' does not read back in order\n", typed.c_str());
    if (!everyKeyLandedOnce || !typedRunTogether) PrintGrid(session);
    Check(everyKeyLandedOnce && typedRunTogether,
        "keys sent one at a time, at a human's pace, come back in order");
    bool eachKeyOnce = true;
    for (size_t i = 0; i < typed.size(); ++i)
        eachKeyOnce &= GridCount(session, typed[i]) == already[i] + 1;
    Check(eachKeyOnce, "and one key press puts exactly one character on the viewer's grid");

    DHTermGrid grid{};
    Check(!dh_term_grid(session, 0, nullptr, 0, &grid),
        "asking with no buffer reports the size instead of writing anywhere");
    Check(grid.rows == 24 && grid.cols == 80, "and the size is the one we opened with");

    dh_term_resize(session, 100, 30);
    Check(WaitForMs(
              [&rig] {
                  const std::vector<deskhub::TerminalRecord> open = rig.term.Sessions();
                  return open.size() == 1 && open[0].size == deskhub::TermSize{100, 30};
              },
              10000),
        "a resize through the C ABI reaches the host");

    dh_term_stop(session);
    Check(WaitForMs([&rig] { return rig.term.SessionCount() == 0; }, 10000),
        "closing the session ends the shell on the host");

    rig.Stop();
    if (savedTrust.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kTrustStoreFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kTrustStoreFileName, savedTrust);
    if (savedPaired.empty())
        deskhubp::RemoveAppDataFile(deskhubp::kPairedDevicesFileName);
    else
        deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, savedPaired);
}
