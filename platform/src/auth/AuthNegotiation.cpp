#include "deskhubp/auth/AuthNegotiation.h"

#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/PairedDevicesFile.h"

namespace deskhubp {

namespace {

constexpr std::string_view kClientLabel = "client";

}

struct HostAuth::Impl {
    HostAuthConfig config{};
    HostAuthState state = HostAuthState::Idle;
    deskhub::AuthMode mode = deskhub::AuthMode::Denied;
    deskhub::Fingerprint peer{};
    std::string peerName{};
    std::vector<uint8_t> peerPublicKey{};
    AuthNonce nonce{};

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
    const std::optional<deskhub::Fingerprint> peer = FingerprintOfPublicKey(start.publicKey);
    if (!peer) return std::nullopt;

    impl_->peer = *peer;
    impl_->peerName = start.clientName;
    impl_->peerPublicKey = start.publicKey;
    impl_->nonce = NewAuthNonce();
    deskhub::AuthChallenge challenge;
    challenge.nonce = impl_->nonce;

    const bool paired = CheckPairedDevice(*peer) == deskhub::PairVerdict::Paired;
    impl_->mode = paired ? deskhub::AuthMode::Signature : deskhub::AuthMode::Denied;

    challenge.mode = impl_->mode;
    impl_->state = paired ? HostAuthState::AwaitingResponse : HostAuthState::Settled;
    return challenge;
}

deskhub::AuthResult HostAuth::Respond(const deskhub::AuthResponse& response, int64_t nowUnix) {
    if (impl_->state != HostAuthState::AwaitingResponse)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);

    if (impl_->mode != deskhub::AuthMode::Signature ||
        CheckPairedDevice(impl_->peer) != deskhub::PairVerdict::Paired)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    const std::vector<uint8_t> transcript =
        AuthTranscript(kClientLabel, impl_->nonce, impl_->config.identity.fingerprint);
    if (!VerifySignature(impl_->peerPublicKey, transcript, response.proof))
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    TouchPairedDevice(impl_->peer, impl_->peerName, nowUnix);
    return impl_->Settle(deskhub::AuthResultCode::Accepted);
}

HostAuthState HostAuth::State() const {
    return impl_->state;
}

deskhub::AuthMode HostAuth::Mode() const {
    return impl_->mode;
}

const deskhub::Fingerprint& HostAuth::PeerFingerprint() const {
    return impl_->peer;
}

const std::string& HostAuth::PeerName() const {
    return impl_->peerName;
}

struct ClientAuth::Impl {
    ClientAuthConfig config{};
    deskhub::AuthMode mode = deskhub::AuthMode::Denied;
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
    start.hasPasscode = false;
    return start;
}

std::optional<deskhub::AuthResponse> ClientAuth::Answer(
    const deskhub::AuthChallenge& challenge) {
    impl_->mode = challenge.mode;
    if (challenge.mode != deskhub::AuthMode::Signature) return std::nullopt;
    deskhub::AuthResponse response;
    response.proof = SignWithClientIdentity(impl_->config.identity,
        AuthTranscript(kClientLabel, challenge.nonce, impl_->config.hostFingerprint));
    if (response.proof.empty()) return std::nullopt;
    return response;
}

bool ClientAuth::HostProvedThePasscode(const deskhub::AuthResult& result) const {
    (void)result;
    return false;
}

deskhub::AuthMode ClientAuth::Mode() const {
    return impl_->mode;
}

}
