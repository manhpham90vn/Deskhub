#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/net/LocalSocket.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

constexpr int kAcceptWaitMs = 3000;
constexpr int kNoClientWaitMs = 50;

std::string FreshSocketPath(const char* prefix) {
    return UniqueTempDir(prefix).string() + ".sock";
}

#ifdef _WIN32

void TestLocalSocketsAreAbsentOnWindows() {
    std::printf("[local] Windows has no Unix socket helper; every call fails cleanly...\n");
    deskhubp::LocalSocket server = deskhubp::LocalSocket::Listen("unused", 0666);
    Check(!server.Valid(), "listening is refused");
    Check(!server.Accept(0).has_value(), "accepting yields nothing");
    Check(!deskhubp::LocalSocket::Connect("unused").Valid(), "connecting is refused");
}

#else

void TestEchoAndPeerIdentity() {
    std::printf("[local] a client reaches the listener and is identified by pid and uid...\n");
    const std::string path = FreshSocketPath("deskhub-local-");
    deskhubp::LocalSocket server = deskhubp::LocalSocket::Listen(path, 0600);
    Check(server.Valid(), "the listener binds a fresh socket path");

    std::optional<deskhubp::LocalPeer> seen;
    bool echoed = false;
    std::thread serverThread([&] {
        std::optional<deskhubp::LocalSocket> client = server.Accept(kAcceptWaitMs);
        if (!client) return;
        seen = client->Peer();
        std::array<uint8_t, 3> buf{};
        if (client->ReadExactly(buf)) echoed = client->WriteAll(buf);
    });

    deskhubp::LocalSocket client = deskhubp::LocalSocket::Connect(path);
    Check(client.Valid(), "a client connects to the path");
    const std::optional<deskhubp::LocalPeer> serverSide = client.Peer();
    Check(serverSide && serverSide->pid == int32_t(getpid()),
        "the client can identify the listener the same way");
    const std::array<uint8_t, 3> hello{1, 2, 3};
    Check(client.WriteAll(hello), "the client writes three bytes");
    std::array<uint8_t, 3> back{};
    Check(client.ReadExactly(back) && back == hello, "and reads the same three back");
    serverThread.join();
    Check(echoed, "the server saw the bytes and answered");
    Check(seen.has_value(), "the server learns who connected");
    Check(seen && seen->pid == int32_t(getpid()) && seen->uid == uint32_t(getuid()),
        "the peer is this very process");
    std::error_code ignored;
    Check(seen && !seen->exePath.empty() &&
              std::filesystem::is_regular_file(seen->exePath, ignored),
        "the peer's executable path points at a real file");

    server.Close();
    unlink(path.c_str());
}

void TestNobodyThereAndNobodyListening() {
    std::printf("[local] waiting for a client times out, and a missing path is refused...\n");
    const std::string path = FreshSocketPath("deskhub-local-idle-");
    deskhubp::LocalSocket server = deskhubp::LocalSocket::Listen(path, 0600);
    Check(server.Valid(), "the listener binds");
    Check(!server.Accept(kNoClientWaitMs).has_value(), "no client within the wait yields nothing");
    server.Close();
    unlink(path.c_str());
    Check(!deskhubp::LocalSocket::Connect(path).Valid(),
        "connecting where nobody listens fails");
    Check(!deskhubp::LocalSocket::Listen(std::string(200, 'x'), 0600).Valid(),
        "a path too long for a Unix socket is refused");
}

void TestShutdownUnblocksAReader() {
    std::printf("[local] shutting a connection wakes whoever is blocked reading it...\n");
    const std::string path = FreshSocketPath("deskhub-local-wake-");
    deskhubp::LocalSocket server = deskhubp::LocalSocket::Listen(path, 0600);
    std::optional<deskhubp::LocalSocket> accepted;
    std::thread serverThread([&] { accepted = server.Accept(kAcceptWaitMs); });
    deskhubp::LocalSocket client = deskhubp::LocalSocket::Connect(path);
    serverThread.join();
    Check(accepted.has_value(), "the connection is accepted");

    bool readEnded = false;
    std::thread reader([&] {
        std::array<uint8_t, 8> sink{};
        size_t got = 0;
        readEnded = !client.ReadSome(sink, got);
    });
    client.Shutdown();
    reader.join();
    Check(readEnded, "the blocked read returns false once the socket is shut");
    server.Close();
    unlink(path.c_str());
}

#endif

}

void RunLocalSocketTests() {
#ifdef _WIN32
    TestLocalSocketsAreAbsentOnWindows();
#else
    TestEchoAndPeerIdentity();
    TestNobodyThereAndNobodyListening();
    TestShutdownUnblocksAReader();
#endif
}
