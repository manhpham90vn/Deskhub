#include "deskhubp/auth/AuthNegotiation.h"

#include <utility>

#include "deskhubp/system/PairedDevicesFile.h"

namespace deskhubp {

struct HostAuth::Impl {
    HostAuthConfig config{};
    HostAuthState state = HostAuthState::Idle;
    deskhub::Fingerprint peer{};
    std::string peerName{};
    std::vector<uint8_t> peerPublicKey{};

    deskhub::AuthResult Settle(deskhub::AuthResultCode code) {
        state = HostAuthState::Settled;
        deskhub::AuthResult result;
        result.code = code;
        return result;
    }
};

HostAuth::HostAuth() : impl_(std::make_unique<Impl>()) {
}

HostAuth::~HostAuth() = default;

void HostAuth::Configure(HostAuthConfig config) {
    impl_->config = std::move(config);
}

std::optional<deskhub::AuthChallenge> HostAuth::Begin(const deskhub::AuthStart& start) {
    if (impl_->state != HostAuthState::Idle) return std::nullopt;
    const std::optional<deskhub::Fingerprint> peer = FingerprintOfPublicKey(start.publicKey);
    if (!peer) return std::nullopt;

    const bool paired = CheckPairedDevice(*peer) == deskhub::PairVerdict::Paired;
    impl_->peer = *peer;
    impl_->peerName = start.clientName;
    impl_->peerPublicKey = start.publicKey;
    deskhub::AuthChallenge challenge;

    challenge.mode = paired ? deskhub::AuthMode::Signature : deskhub::AuthMode::Denied;
    impl_->state = paired ? HostAuthState::AwaitingResponse : HostAuthState::Settled;
    return challenge;
}

deskhub::AuthResult HostAuth::Respond(const deskhub::AuthResponse& response, int64_t nowUnix) {
    if (impl_->state != HostAuthState::AwaitingResponse)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);

    if (CheckPairedDevice(impl_->peer) != deskhub::PairVerdict::Paired)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    const std::vector<uint8_t> transcript = deskhub::AuthTranscript(deskhub::AuthRole::Client,
        impl_->config.sessionId, impl_->peerPublicKey, impl_->config.identity.fingerprint);
    if (transcript.empty()) return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    if (!VerifySignature(impl_->peerPublicKey, transcript, response.proof))
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    TouchPairedDevice(impl_->peer, impl_->peerName, nowUnix);
    return impl_->Settle(deskhub::AuthResultCode::Accepted);
}

HostAuthState HostAuth::State() const {
    return impl_->state;
}

const deskhub::Fingerprint& HostAuth::PeerFingerprint() const {
    return impl_->peer;
}

const std::string& HostAuth::PeerName() const {
    return impl_->peerName;
}

struct ClientAuth::Impl {
    ClientAuthConfig config{};
};

ClientAuth::ClientAuth() : impl_(std::make_unique<Impl>()) {
}

ClientAuth::~ClientAuth() = default;

void ClientAuth::Configure(ClientAuthConfig config) {
    impl_->config = std::move(config);
}

deskhub::AuthStart ClientAuth::Begin() const {
    deskhub::AuthStart start;
    start.publicKey = impl_->config.identity.publicKey;
    start.clientName = impl_->config.clientName;
    return start;
}

std::optional<deskhub::AuthResponse> ClientAuth::Answer(
    const deskhub::AuthChallenge& challenge) {
    if (challenge.mode != deskhub::AuthMode::Signature) return std::nullopt;
    deskhub::AuthResponse response;
    const std::vector<uint8_t> transcript = deskhub::AuthTranscript(deskhub::AuthRole::Client,
        impl_->config.sessionId, impl_->config.identity.publicKey,
        impl_->config.hostFingerprint);
    if (transcript.empty()) return std::nullopt;
    response.proof = SignWithClientIdentity(impl_->config.identity, transcript);
    if (response.proof.empty()) return std::nullopt;
    return response;
}

}
