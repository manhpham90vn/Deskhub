#include "deskhub/net/AuthorizedKeys.h"
#include "deskhub/net/PublicKeyText.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/auth/PairingTokens.h"
#include "deskhub/net/AccessRequests.h"
#include "deskhub/net/PairingInvite.h"
#include "deskhub/ui/HostProfiles.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

void Require(bool condition) {
    if (!condition) __builtin_trap();
}

void CheckPublicKeyText(std::string_view text) {
    const auto key = deskhub::ParsePublicKeyText(text);
    if (!key) return;
    const std::string line = deskhub::FormatPublicKeyText(*key);
    const auto again = deskhub::ParsePublicKeyText(line);
    Require(again && again->algorithm == key->algorithm && again->blob == key->blob &&
            again->label == key->label);
}

void CheckAuthorizedKeys(std::string_view text) {
    const auto keys = deskhub::ParseAuthorizedKeys(text);
    if (!keys) return;
    Require(keys->Keys().size() <= deskhub::kMaxAuthorizedKeys);
    const std::string file = deskhub::SerializeAuthorizedKeys(*keys);
    const auto again = deskhub::ParseAuthorizedKeys(file);
    Require(again && deskhub::SerializeAuthorizedKeys(*again) == file);
}

void CheckTrustStore(std::string_view text) {
    const auto store = deskhub::ParseTrustStoreStrict(text);
    if (!store) return;
    Require(store->Size() <= deskhub::kMaxTrustedHosts);
    const std::string file = deskhub::SerializeTrustStore(*store);
    const auto again = deskhub::ParseTrustStoreStrict(file);
    Require(again && deskhub::SerializeTrustStore(*again) == file);
    for (const deskhub::TrustedHost& host : store->Hosts()) {
        deskhub::ui::FindHostProfile(*store, host.label);
        Require(deskhub::ui::IsValidHostAlias(deskhub::ui::SuggestHostAlias(*store, host.endpoint)));
    }
}

void CheckNames(std::string_view text) {
    const auto endpoint = deskhub::ui::CanonicalHostEndpoint(text);
    if (endpoint) Require(deskhub::ui::CanonicalHostEndpoint(*endpoint) == endpoint);
}

void CheckPairingInvite(std::string_view text) {
    const auto invite = deskhub::ParsePairingInvite(text);
    if (!invite) return;
    const std::string link = deskhub::FormatPairingInvite(*invite);
    Require(!link.empty() && link.size() <= deskhub::kMaxPairingInviteChars);
    const auto again = deskhub::ParsePairingInvite(link);
    Require(again && *again == *invite);
}

void CheckAccessRequests(std::string_view text) {
    const auto requests = deskhub::ParseAccessRequests(text, 0);
    if (!requests) return;
    Require(requests->Requests().size() <= deskhub::kMaxAccessRequests);
    const std::string file = deskhub::SerializeAccessRequests(*requests);
    const auto again = deskhub::ParseAccessRequests(file, 0);
    Require(again && deskhub::SerializeAccessRequests(*again) == file);
}

void CheckPairingTokens(std::string_view text) {
    const auto tokens = deskhub::ParsePairingTokens(text, 0);
    if (!tokens) return;
    Require(tokens->Tokens().size() <= deskhub::kMaxPairingTokens);
    const std::string file = deskhub::SerializePairingTokens(*tokens);
    const auto again = deskhub::ParsePairingTokens(file, 0);
    Require(again && again->Tokens() == tokens->Tokens());
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    CheckPublicKeyText(text);
    CheckAuthorizedKeys(text);
    CheckTrustStore(text);
    CheckNames(text);
    CheckPairingInvite(text);
    CheckAccessRequests(text);
    CheckPairingTokens(text);
    return 0;
}
