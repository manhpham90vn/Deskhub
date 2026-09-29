#include "deskhubp/client/HostProfiles.h"

#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

namespace deskhubp {

using deskhub::ui::HostProfileError;

std::optional<deskhub::Fingerprint> ParseHostKeyText(std::string_view text) {
    if (auto fingerprint = deskhub::ParseFingerprint(text)) return fingerprint;
    return FingerprintOfPublicKey(PublicKeySpkiFromText(text));
}

HostProfileError SaveHostProfile(deskhub::ui::HostProfileMode mode,
    const deskhub::ui::HostProfileRequest& request) {
    const auto store = TryLoadTrustStore();
    if (!store) return HostProfileError::StoreUnreadable;
    const deskhub::ui::HostProfilePlan plan = deskhub::ui::PlanHostProfile(*store, mode, request);
    if (plan.error != HostProfileError::None) return plan.error;
    if (!LoadClientIdentity(plan.profile.identityName).Valid())
        return HostProfileError::UnknownIdentity;

    const deskhub::TrustedHost& profile = plan.profile;
    const bool saved = plan.existing
                           ? UpdateTrustedHostProfile(*plan.existing, profile.endpoint,
                                 profile.label, profile.fingerprint, profile.identityName)
                           : CreateTrustedHostProfile(profile.endpoint, profile.label,
                                 profile.fingerprint, profile.identityName);
    return saved ? HostProfileError::None : HostProfileError::WriteFailed;
}

HostProfileError TrustNewHost(std::string_view address, const deskhub::Fingerprint& fingerprint) {
    const std::optional<std::string> endpoint = deskhub::ui::CanonicalHostEndpoint(address);
    if (!endpoint) return HostProfileError::InvalidAddress;
    if (deskhub::IsZero(fingerprint)) return HostProfileError::MissingHostKey;
    const auto store = TryLoadTrustStore();
    if (!store) return HostProfileError::StoreUnreadable;
    if (store->Find(*endpoint)) return HostProfileError::AddressInUse;
    if (store->Size() >= deskhub::kMaxTrustedHosts) return HostProfileError::StoreFull;
    const std::string alias = deskhub::ui::SuggestHostAlias(*store, *endpoint);
    return CreateTrustedHostProfile(*endpoint, alias, fingerprint, deskhub::ui::kDefaultIdentityName)
               ? HostProfileError::None
               : HostProfileError::WriteFailed;
}

HostProfileError RemoveHostProfile(std::string_view alias) {
    const auto store = TryLoadTrustStore();
    if (!store) return HostProfileError::StoreUnreadable;
    const deskhub::ui::HostProfileLookup lookup = deskhub::ui::FindHostProfile(*store, alias);
    if (!lookup.host) return lookup.error;
    return ForgetTrustedHost(lookup.host->endpoint) ? HostProfileError::None
                                                    : HostProfileError::WriteFailed;
}

}
