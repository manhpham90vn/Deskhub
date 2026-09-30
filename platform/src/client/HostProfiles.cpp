#include "deskhubp/client/HostProfiles.h"

#include "deskhub/ui/Strings.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/Clock.h"
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

    const bool saved = plan.existing ? UpdateTrustedHostProfile(*plan.existing, plan.profile)
                                     : CreateTrustedHostProfile(plan.profile);
    return saved ? HostProfileError::None : HostProfileError::WriteFailed;
}

HostProfileError TrustNewHost(std::string_view address, const deskhub::Fingerprint& fingerprint) {
    const std::optional<std::string> endpoint = deskhub::ui::CanonicalHostEndpoint(address);
    if (!endpoint) return HostProfileError::InvalidAddress;
    if (deskhub::IsZero(fingerprint)) return HostProfileError::MissingHostKey;
    const auto store = TryLoadTrustStore();
    if (!store) return HostProfileError::StoreUnreadable;
    if (store->Find(fingerprint))
        return TouchTrustedHost(fingerprint, *endpoint, NowUnixSeconds())
                   ? HostProfileError::None
                   : HostProfileError::WriteFailed;
    if (store->Size() >= deskhub::kMaxTrustedHosts) return HostProfileError::StoreFull;
    const std::string alias = deskhub::ui::SuggestHostAlias(*store, *endpoint);
    const int64_t now = NowUnixSeconds();
    return CreateTrustedHostProfile(deskhub::TrustedHost{fingerprint, alias, *endpoint, now, now})
               ? HostProfileError::None
               : HostProfileError::WriteFailed;
}

HostProfileError RemoveHostProfile(std::string_view alias) {
    const auto store = TryLoadTrustStore();
    if (!store) return HostProfileError::StoreUnreadable;
    const deskhub::ui::HostProfileLookup lookup = deskhub::ui::FindHostProfile(*store, alias);
    if (!lookup.host) return lookup.error;
    return ForgetTrustedHost(lookup.host->fingerprint) ? HostProfileError::None
                                                       : HostProfileError::WriteFailed;
}

std::string PreviousOwnerWarningFor(std::string_view address,
    const deskhub::Fingerprint& fingerprint) {
    const std::optional<std::string> endpoint = deskhub::ui::CanonicalHostEndpoint(address);
    if (!endpoint) return {};
    const auto previous = LoadTrustStore().FindByEndpoint(*endpoint);
    if (!previous || previous->fingerprint == fingerprint) return {};
    return deskhub::ui::PreviousOwnerWarning(*endpoint, previous->label,
        deskhub::FormatFingerprint(previous->fingerprint));
}

}
