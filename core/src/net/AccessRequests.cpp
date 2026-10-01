#include "deskhub/net/AccessRequests.h"

#include "RecordText.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr size_t kMaxAccessRequestLineBytes = 2048;
constexpr std::string_view kDeniedPrefix = "denied ";

using detail::ParseUnixTime;
using detail::Trim;

std::string CleanAddress(std::string_view address) {
    std::string trimmed = Trim(address);
    if (trimmed.empty() || trimmed.size() > kMaxAccessRequestAddressBytes) return {};
    for (char c : trimmed)
        if (uint8_t(c) <= 0x20 || uint8_t(c) == 0x7F) return {};
    return trimmed;
}

bool OutlivedItsTtl(int64_t requestedUnix, int64_t nowUnix) {
    return requestedUnix <= nowUnix && nowUnix - requestedUnix >= kAccessRequestTtlSeconds;
}

bool ValidKey(const PublicKeyText& key) {
    return !FormatPublicKeyText(key).empty();
}

std::string_view HostOf(std::string_view address) {
    if (address.starts_with('[')) {
        const size_t close = address.find(']');
        return close == std::string_view::npos ? address : address.substr(0, close + 1);
    }
    const size_t colon = address.find(':');
    if (colon == std::string_view::npos || address.find(':', colon + 1) != std::string_view::npos)
        return address;
    return address.substr(0, colon);
}

bool Replaces(const AccessRequest& incoming, const AccessRequest& existing) {
    if (SameKey(existing.key, incoming.key)) return true;
    if (existing.denied || incoming.denied) return false;
    return HostOf(existing.address) == HostOf(incoming.address);
}

bool HoldsNewerFromSameSource(const AccessRequests& requests, const AccessRequest& request) {
    return std::ranges::any_of(requests.Requests(), [&](const AccessRequest& existing) {
        return Replaces(request, existing) && existing.requestedUnix > request.requestedUnix;
    });
}

}

bool SameKey(const PublicKeyText& a, const PublicKeyText& b) {
    return a.algorithm == b.algorithm && a.blob == b.blob;
}

bool AccessRequests::Add(AccessRequest request, int64_t nowUnix) {
    request.address = CleanAddress(request.address);
    if (request.address.empty() || !ValidKey(request.key)) return false;
    if (!request.denied && IsDenied(request.key)) return false;
    request.requestedUnix = nowUnix;
    std::erase_if(requests_,
        [&](const AccessRequest& existing) { return Replaces(request, existing); });
    if (requests_.size() >= kMaxAccessRequests) {
        const auto oldest = std::min_element(requests_.begin(), requests_.end(),
            [](const AccessRequest& a, const AccessRequest& b) {
                return a.requestedUnix < b.requestedUnix;
            });
        requests_.erase(oldest);
    }
    requests_.push_back(std::move(request));
    return true;
}

bool AccessRequests::Remove(const PublicKeyText& key) {
    const auto at = std::remove_if(requests_.begin(), requests_.end(),
        [&](const AccessRequest& request) { return SameKey(request.key, key); });
    if (at == requests_.end()) return false;
    requests_.erase(at, requests_.end());
    return true;
}

bool AccessRequests::Deny(const PublicKeyText& key, int64_t nowUnix) {
    for (AccessRequest& request : requests_) {
        if (!SameKey(request.key, key)) continue;
        request.denied = true;
        request.requestedUnix = nowUnix;
        return true;
    }
    return false;
}

bool AccessRequests::IsDenied(const PublicKeyText& key) const {
    const std::optional<AccessRequest> found = Find(key);
    return found && found->denied;
}

std::optional<AccessRequest> AccessRequests::Find(const PublicKeyText& key) const {
    for (const AccessRequest& request : requests_)
        if (SameKey(request.key, key)) return request;
    return std::nullopt;
}

size_t AccessRequests::Expire(int64_t nowUnix) {
    const size_t before = requests_.size();
    std::erase_if(requests_, [&](const AccessRequest& request) {
        return OutlivedItsTtl(request.requestedUnix, nowUnix) ||
               request.requestedUnix > nowUnix;
    });
    return before - requests_.size();
}

std::optional<AccessRequests> ParseAccessRequests(std::string_view text, int64_t nowUnix) {
    if (text.size() > kMaxAccessRequestsFileBytes) return std::nullopt;
    AccessRequests requests;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty()) continue;
        const bool denied = line.starts_with(kDeniedPrefix);
        if (denied) line.erase(0, kDeniedPrefix.size());
        if (line.size() > kMaxAccessRequestLineBytes) return std::nullopt;
        const size_t s1 = line.find(' ');
        if (s1 == std::string::npos) return std::nullopt;
        const size_t s2 = line.find(' ', s1 + 1);
        if (s2 == std::string::npos) return std::nullopt;
        int64_t requestedUnix = 0;
        if (!ParseUnixTime(std::string_view(line).substr(0, s1), requestedUnix))
            return std::nullopt;
        const std::string address = CleanAddress(std::string_view(line).substr(s1 + 1, s2 - s1 - 1));
        const std::optional<PublicKeyText> key =
            ParsePublicKeyText(std::string_view(line).substr(s2 + 1));
        if (address.empty() || !key) return std::nullopt;
        if (requests.Find(*key)) return std::nullopt;
        if (OutlivedItsTtl(requestedUnix, nowUnix)) continue;
        AccessRequest request{*key, address, requestedUnix, denied};
        if (HoldsNewerFromSameSource(requests, request)) continue;
        if (!requests.Add(std::move(request), requestedUnix)) return std::nullopt;
    }
    requests.Expire(nowUnix);
    return requests;
}

std::string SerializeAccessRequests(const AccessRequests& requests) {
    std::string out;
    for (const AccessRequest& request : requests.Requests()) {
        const std::string key = FormatPublicKeyText(request.key);
        if (key.empty() || request.address.empty()) continue;
        if (request.denied) out += kDeniedPrefix;
        out += std::to_string(request.requestedUnix);
        out += ' ';
        out += request.address;
        out += ' ';
        out += key;
        out += '\n';
    }
    return out;
}

}
