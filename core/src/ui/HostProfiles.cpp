#include "deskhub/ui/HostProfiles.h"

#include "deskhub/net/Ipv4.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/Strings.h"

namespace deskhub::ui {

namespace {

constexpr size_t kAliasSuffixRoom = 4;

bool IsAliasChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_';
}

HostProfilePlan Refuse(HostProfileError error) {
    HostProfilePlan plan;
    plan.error = error;
    return plan;
}

bool AliasTaken(const TrustStore& store, std::string_view alias) {
    for (const TrustedHost& host : store.Hosts())
        if (host.label == alias) return true;
    return false;
}

std::string AliasBase(std::string_view endpoint) {
    std::string host;
    uint16_t port = kDeskhubPort;
    SplitHostPort(endpoint, host, port);
    std::string base = port == kDeskhubPort ? host : host + "-" + std::to_string(port);
    for (char& c : base)
        if (!IsAliasChar(c)) c = '-';
    if (base.empty()) base = "host";
    return base.substr(0, kMaxTrustLabelBytes - kAliasSuffixRoom);
}

std::string ChosenIdentity(const HostProfileRequest& request,
    const std::optional<TrustedHost>& existing) {
    if (!request.identityName.empty()) return request.identityName;
    if (existing) return ProfileIdentityName(*existing);
    return std::string(kDefaultIdentityName);
}

}

bool IsValidHostAlias(std::string_view alias) {
    if (alias.empty() || alias.size() > kMaxTrustLabelBytes) return false;
    for (const char c : alias)
        if (!IsAliasChar(c)) return false;
    return true;
}

std::optional<std::string> CanonicalHostEndpoint(std::string_view address) {
    std::string host;
    uint16_t port = kDeskhubPort;
    if (!SplitHostPort(TrimAscii(address), host, port) || !ParseIPv4(host)) return std::nullopt;
    return host + ":" + std::to_string(port);
}

std::string ProfileIdentityName(const TrustedHost& host) {
    return host.identityName.empty() ? std::string(kDefaultIdentityName) : host.identityName;
}

std::string SuggestHostAlias(const TrustStore& store, std::string_view endpoint) {
    std::string base = AliasBase(endpoint);
    if (!AliasTaken(store, base)) return base;
    for (size_t n = 2; n <= kMaxTrustedHosts + 1; ++n) {
        const std::string candidate = base + "-" + std::to_string(n);
        if (!AliasTaken(store, candidate)) return candidate;
    }
    return base;
}

HostProfileLookup FindHostProfile(const TrustStore& store, std::string_view alias) {
    HostProfileLookup lookup;
    for (const TrustedHost& host : store.Hosts()) {
        if (host.label != alias) continue;
        if (lookup.host) return HostProfileLookup{HostProfileError::AliasAmbiguous, std::nullopt};
        lookup.host = host;
    }
    if (!lookup.host) lookup.error = HostProfileError::AliasMissing;
    return lookup;
}

HostProfilePlan PlanHostProfile(const TrustStore& store, HostProfileMode mode,
    const HostProfileRequest& request) {
    if (!IsValidHostAlias(request.alias)) return Refuse(HostProfileError::InvalidAlias);

    const HostProfileLookup lookup = FindHostProfile(store, request.alias);
    if (lookup.error == HostProfileError::AliasAmbiguous) return Refuse(lookup.error);
    const std::optional<TrustedHost>& existing = lookup.host;
    if (mode == HostProfileMode::Add && existing) return Refuse(HostProfileError::AliasExists);
    if (mode == HostProfileMode::Update && !existing)
        return Refuse(HostProfileError::AliasMissing);

    const std::string address =
        request.address.empty() && existing ? existing->endpoint : request.address;
    const std::optional<std::string> endpoint = CanonicalHostEndpoint(address);
    if (!endpoint) return Refuse(HostProfileError::InvalidAddress);

    const std::optional<Fingerprint> hostKey =
        request.hostKey ? request.hostKey
                        : (existing ? std::optional<Fingerprint>(existing->fingerprint)
                                    : std::nullopt);
    if (!hostKey || IsZero(*hostKey)) return Refuse(HostProfileError::MissingHostKey);

    const bool movesAddress = !existing || existing->endpoint != *endpoint;
    if (movesAddress && store.Find(*endpoint)) return Refuse(HostProfileError::AddressInUse);
    if (!existing && store.Size() >= kMaxTrustedHosts) return Refuse(HostProfileError::StoreFull);

    HostProfilePlan plan;
    plan.existing = existing;
    plan.profile = TrustedHost{*endpoint, request.alias, *hostKey,
        existing ? existing->firstSeenUnix : 0, existing ? existing->lastSeenUnix : 0,
        ChosenIdentity(request, existing)};
    return plan;
}

const char* HostProfileErrorText(HostProfileError error) {
    switch (error) {
        case HostProfileError::None: return "";
        case HostProfileError::InvalidAlias:
            return "The host name must be 1-64 ASCII letters, digits, '-' or '_'.";
        case HostProfileError::InvalidAddress:
            return "The address must be an IPv4 address with an optional port, for example "
                   "192.168.1.10:47777.";
        case HostProfileError::MissingHostKey: return "The host's public key is required.";
        case HostProfileError::InvalidHostKey:
            return "That is not a supported host public key or SHA256 fingerprint.";
        case HostProfileError::UnknownIdentity:
            return "That client key does not exist or cannot be read.";
        case HostProfileError::AliasExists: return "A saved host already has that name.";
        case HostProfileError::AliasMissing: return "No saved host has that name.";
        case HostProfileError::AliasAmbiguous:
            return "Several saved hosts share that name; fix known_hosts first.";
        case HostProfileError::AddressInUse: return "Another saved host already uses that address.";
        case HostProfileError::StoreUnreadable:
            return "The saved hosts file cannot be read; fix known_hosts before changing it.";
        case HostProfileError::StoreFull: return "The list of saved hosts is full.";
        case HostProfileError::WriteFailed: return "The saved hosts file could not be written.";
    }
    return "";
}

}
