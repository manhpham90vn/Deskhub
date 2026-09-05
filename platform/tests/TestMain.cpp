#include "Tests.h"
#include "support/TestSupport.h"

#include <cstdio>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdlib>

namespace {

constexpr const char* kTestHome = "/tmp/deskhub-platform-tests";

void KeepTestStateOutOfTheDeveloperHome() {
    if (mkdir(kTestHome, 0700) == 0 || errno == EEXIST) setenv("HOME", kTestHome, 1);
}

}
#else
#include <filesystem>
#include <system_error>

#include "deskhubp/diag/LogFile.h"

namespace {

bool PointAppDataAt(const char* leaf) {
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / leaf;
    if (ec) return false;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;
    deskhubp::SetAppDataDir(dir.string());
    return true;
}

void KeepTestStateOutOfTheDeveloperHome() {
    if (PointAppDataAt("deskhub-platform-tests")) return;
    std::printf(
        "=== no private app data directory, so this run shares %%USERPROFILE%%\\.deskhub with "
        "every other Deskhub on this machine: the host key and the trusted-host list are one "
        "file each, and a running app or a second test binary can change what a link test "
        "expects halfway through ===\n");
}

}
#endif

int main() {
    KeepTestStateOutOfTheDeveloperHome();

    std::printf("=== platform self-test (local only: loopback sockets, no remote peer) ===\n");

    std::printf("--- system: monotonic clock + sleep ---\n");
    RunClockTests();

    std::printf("--- system: OS randomness ---\n");
    RunRandomTests();

    std::printf("--- system: process memory footprint ---\n");
    RunMemoryFootprintTests();

    std::printf("--- system: default device name ---\n");
    RunDeviceNameTests();

    std::printf("--- diag: log file naming + timestamps ---\n");
    RunLogFileTests();

    std::printf("--- net: address parsing, printing and packing ---\n");
    RunNetAddrTests();

    std::printf("--- net: per-viewer client id ---\n");
    RunClientIdTests();

    std::printf("--- net: UDP socket over loopback + local adapters ---\n");
    RunUdpSocketTests();

    std::printf("--- net: pre-session source query over loopback ---\n");
    RunSourceQueryTests();

    std::printf("--- net: host probe + device status poller over loopback ---\n");
    RunHostProbeTests();

    std::printf("--- net: which neighbours a LAN scan would knock on ---\n");
    RunLanScannerTests();

    std::printf("--- system: app data files next to the logs ---\n");
    RunAppDataFileTests();

    std::printf("--- system: the host's own key pair and the machines it trusts ---\n");
    RunHostIdentityTests();

    std::printf("--- system: proving which machine, and that it knows the code ---\n");
    RunAuthProofTests();

    std::printf("--- session: pairing a machine, and letting it back in later ---\n");
    RunAuthNegotiationTests();

    std::printf("--- net: QUIC over loopback (handshake, stream, datagram) ---\n");
    RunQuicEndpointTests();

    std::printf("--- net: the session transport the host and viewer loops speak through ---\n");
    RunSessionTransportTests();

    std::printf("--- session: the one link every client surface shares ---\n");
    RunHostLinkTests();

    std::printf("--- system: pseudo terminal running a real shell ---\n");
    RunPtyTests();

    std::printf("--- session: sharing a shell over QUIC end to end ---\n");
    RunTerminalHostTests();
    RunTerminalFfiTests();

    std::printf("--- session: files from a viewer to the host, over QUIC ---\n");
    RunFileTransferPlatformTests();

    RunSharingHostTests();
    RunDiscoveryFfiTests();

    std::printf("--- ffi: string handover to the managed clients ---\n");
    RunFfiTextTests();

    std::printf("--- input: local-input gate shared by every injector ---\n");
    RunLocalInputGateTests();
    RunOpusCodecTests();
    RunAudioBroadcasterTests();

    std::printf("--- session: host callbacks wired to a source pipeline ---\n");
    RunScreenHostCallbackTests();
    RunEncoderRecoveryTests();

    if (g_failures == 0) {
        std::printf("=== PASS: all checks passed ===\n");
        return 0;
    }
    std::printf("=== FAIL: %d checks failed ===\n", g_failures);
    return 1;
}
