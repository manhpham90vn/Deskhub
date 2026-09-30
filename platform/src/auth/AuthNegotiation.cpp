#include "deskhubp/auth/AuthNegotiation.h"

#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/PairingTokenFile.h"

namespace deskhubp {

namespace {

std::string LabelledPublicKeyText(std::span<const uint8_t> publicKeySpki, std::string_view name) {
    auto parsed = deskhub::ParsePublicKeyText(PublicKeyTextFromSpki(publicKeySpki));
    if (!parsed) return {};
    parsed->label = std::string(name);
    std::string text = deskhub::FormatPublicKeyText(*parsed);
    if (text.empty()) {
        parsed->label.clear();
        text = deskhub::FormatPublicKeyText(*parsed);
    }
    return text;
}

}

struct HostAuth::Impl {
    HostAuthConfig config{};
    HostAuthState state = HostAuthState::Idle;
    bool tokenRejected = false;
    deskhub::Fingerprint peer{};
    std::string peerName{};
    std::vector<uint8_t> peerPublicKey{};

    deskhub::AuthResult Settle(deskhub::AuthResultCode code) {
        state = HostAuthState::Settled;
        deskhub::AuthResult result;
        result.code = code;
        return result;
    }

    ClientKeyAuthorization AdmitWithToken(const deskhub::AuthStart& start) {
        if (!RedeemPairingToken(start.pairingToken)) {
            tokenRejected = true;
            return ClientKeyAuthorization::Denied;
        }
        const std::string keyText = LabelledPublicKeyText(start.publicKey, start.clientName);
        if (keyText.empty() || !RememberAuthorizedKey(keyText)) {
            LOGE("[Auth] %s redeemed a pairing token but its key could not be saved",
                config.peerAddress.c_str());
            return CheckClientKeyAuthorization(start.publicKey);
        }
        LOGI("[Auth] %s was allowed in by a pairing token", config.peerAddress.c_str());
        return CheckClientKeyAuthorization(start.publicKey);
    }

    deskhub::AuthMode ModeFor(ClientKeyAuthorization authorization, const deskhub::AuthStart& start) {
        if (authorization == ClientKeyAuthorization::Authorized) return deskhub::AuthMode::Signature;
        if (authorization == ClientKeyAuthorization::ConfigError) return deskhub::AuthMode::ConfigError;
        if (RememberAccessRequest(start.publicKey, start.clientName, config.peerAddress))
            return deskhub::AuthMode::AwaitingApproval;
        return deskhub::AuthMode::Denied;
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
    if (PublicKeyTextFromSpki(start.publicKey).empty()) return std::nullopt;
    const std::optional<deskhub::Fingerprint> peer = FingerprintOfPublicKey(start.publicKey);
    if (!peer) return std::nullopt;

    impl_->peer = *peer;
    impl_->peerName = start.clientName;
    impl_->peerPublicKey = start.publicKey;

    ClientKeyAuthorization authorization = CheckClientKeyAuthorization(start.publicKey);
    if (authorization == ClientKeyAuthorization::Denied && !start.pairingToken.empty())
        authorization = impl_->AdmitWithToken(start);

    deskhub::AuthChallenge challenge;
    challenge.mode = impl_->ModeFor(authorization, start);
    impl_->state = challenge.mode == deskhub::AuthMode::Signature ? HostAuthState::AwaitingResponse
                                                                  : HostAuthState::Settled;
    return challenge;
}

deskhub::AuthResult HostAuth::Respond(const deskhub::AuthResponse& response) {
    if (impl_->state != HostAuthState::AwaitingResponse)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);

    const ClientKeyAuthorization authorization = CheckClientKeyAuthorization(impl_->peerPublicKey);
    if (authorization != ClientKeyAuthorization::Authorized)
        return impl_->Settle(authorization == ClientKeyAuthorization::ConfigError
                                 ? deskhub::AuthResultCode::ConfigError
                                 : deskhub::AuthResultCode::NotPaired);
    const std::vector<uint8_t> transcript = deskhub::AuthTranscript(deskhub::AuthRole::Client,
        impl_->config.sessionId, impl_->peerPublicKey, impl_->config.identity.fingerprint);
    if (transcript.empty()) return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    if (!VerifySignature(impl_->peerPublicKey, transcript, response.proof))
        return impl_->Settle(deskhub::AuthResultCode::BadSignature);
    return impl_->Settle(deskhub::AuthResultCode::Accepted);
}

HostAuthState HostAuth::State() const {
    return impl_->state;
}

bool HostAuth::PairingTokenRejected() const {
    return impl_->tokenRejected;
}

const deskhub::Fingerprint& HostAuth::PeerFingerprint() const {
    return impl_->peer;
}

const std::vector<uint8_t>& HostAuth::PeerPublicKey() const {
    return impl_->peerPublicKey;
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
    start.pairingToken = impl_->config.pairingToken;
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
    response.proof = SignWithIdentity(impl_->config.identity, transcript);
    if (response.proof.empty()) return std::nullopt;
    return response;
}

}
