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
    std::vector<uint8_t> pairingToken{};

    deskhub::AuthResult Settle(deskhub::AuthResultCode code) {
        state = HostAuthState::Settled;
        deskhub::AuthResult result;
        result.code = code;
        return result;
    }

    ClientKeyAuthorization AdmitWithToken() {
        if (!RedeemPairingToken(pairingToken)) {
            tokenRejected = true;
            return ClientKeyAuthorization::Denied;
        }
        const std::string keyText = LabelledPublicKeyText(peerPublicKey, peerName);
        if (keyText.empty() || !RememberAuthorizedKey(keyText)) {
            LOGE("[Auth] %s redeemed a pairing token but its key could not be saved",
                config.peerAddress.c_str());
            return CheckClientKeyAuthorization(peerPublicKey);
        }
        LOGI("[Auth] %s was allowed in by a pairing token", config.peerAddress.c_str());
        return CheckClientKeyAuthorization(peerPublicKey);
    }

    deskhub::AuthResult SettleProvenKey() {
        ClientKeyAuthorization authorization = CheckClientKeyAuthorization(peerPublicKey);
        if (authorization == ClientKeyAuthorization::Denied && !pairingToken.empty())
            authorization = AdmitWithToken();
        if (authorization == ClientKeyAuthorization::Authorized)
            return Settle(deskhub::AuthResultCode::Accepted);
        if (authorization == ClientKeyAuthorization::ConfigError)
            return Settle(deskhub::AuthResultCode::ConfigError);
        if (RememberAccessRequest(peerPublicKey, peerName, config.peerAddress))
            return Settle(deskhub::AuthResultCode::AwaitingApproval);
        return Settle(deskhub::AuthResultCode::NotPaired);
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
    impl_->pairingToken = start.pairingToken;

    deskhub::AuthChallenge challenge;
    challenge.mode = CheckClientKeyAuthorization(start.publicKey) == ClientKeyAuthorization::ConfigError
                         ? deskhub::AuthMode::ConfigError
                         : deskhub::AuthMode::Signature;
    impl_->state = challenge.mode == deskhub::AuthMode::Signature ? HostAuthState::AwaitingResponse
                                                                  : HostAuthState::Settled;
    return challenge;
}

deskhub::AuthResult HostAuth::Respond(const deskhub::AuthResponse& response) {
    if (impl_->state != HostAuthState::AwaitingResponse)
        return impl_->Settle(deskhub::AuthResultCode::NotPaired);

    const std::vector<uint8_t> transcript = deskhub::AuthTranscript(deskhub::AuthRole::Client,
        impl_->config.sessionId, impl_->peerPublicKey, impl_->config.identity.fingerprint);
    if (transcript.empty()) return impl_->Settle(deskhub::AuthResultCode::NotPaired);
    if (!VerifySignature(impl_->peerPublicKey, transcript, response.proof))
        return impl_->Settle(deskhub::AuthResultCode::BadSignature);
    return impl_->SettleProvenKey();
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
