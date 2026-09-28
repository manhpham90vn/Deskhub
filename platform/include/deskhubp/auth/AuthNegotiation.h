#pragma once
#include "deskhub/net/PairedDevices.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ClientIdentity.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

struct HostAuthConfig {
    HostIdentity identity{};
};

enum class HostAuthState : uint8_t {
    Idle = 0,
    AwaitingResponse = 1,
    Settled = 2,
};

class HostAuth {
public:
    HostAuth();
    ~HostAuth();
    HostAuth(const HostAuth&) = delete;
    HostAuth& operator=(const HostAuth&) = delete;

    void Configure(HostAuthConfig config);

    std::optional<deskhub::AuthChallenge> Begin(const deskhub::AuthStart& start);
    deskhub::AuthResult Respond(const deskhub::AuthResponse& response, int64_t nowUnix);

    HostAuthState State() const;
    const deskhub::Fingerprint& PeerFingerprint() const;
    const std::string& PeerName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct ClientAuthConfig {
    ClientIdentity identity{};
    deskhub::Fingerprint hostFingerprint{};
    std::string clientName{};
};

class ClientAuth {
public:
    ClientAuth();
    ~ClientAuth();
    ClientAuth(const ClientAuth&) = delete;
    ClientAuth& operator=(const ClientAuth&) = delete;

    void Configure(ClientAuthConfig config);

    deskhub::AuthStart Begin() const;
    std::optional<deskhub::AuthResponse> Answer(const deskhub::AuthChallenge& challenge);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
