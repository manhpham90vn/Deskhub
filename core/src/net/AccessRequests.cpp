#include "deskhub/net/AccessRequests.h"

#include "RecordText.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr size_t kMaxAccessRequestLineBytes = 2048;

using detail::ParseUnixTime;
using detail::Trim;

std::string CleanAddress(std::string_view address) {
    std::string trimmed = Trim(address);
    if (trimmed.empty() || trimmed.size() > kMaxAccessRequestAddressBytes) return {};
    for (char c : trimmed)
        if (uint8_t(c) <= 0x20 || uint8_t(c) == 0x7F) return {};
    return trimmed;
}

bool ValidKey(const PublicKeyText& key) {
    return !FormatPublicKeyText(key).empty();
}

}

bool SameKey(const PublicKeyText& a, const PublicKeyText& b) {
    return a.algorithm == b.algorithm && a.blob == b.blob;
}

bool AccessRequests::Add(AccessRequest request, int64_t nowUnix) {
    request.address = CleanAddress(request.address);
    if (request.address.empty() || !ValidKey(request.key)) return false;
    request.requestedUnix = nowUnix;
    for (AccessRequest& existing : requests_) {
        if (!SameKey(existing.key, request.key)) continue;
        existing = std::move(request);
        return true;
    }
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

std::optional<AccessRequest> AccessRequests::Find(const PublicKeyText& key) const {
    for (const AccessRequest& request : requests_)
        if (SameKey(request.key, key)) return request;
    return std::nullopt;
}

size_t AccessRequests::Expire(int64_t nowUnix) {
    const size_t before = requests_.size();
    std::erase_if(requests_, [&](const AccessRequest& request) {
        return request.requestedUnix + kAccessRequestTtlSeconds <= nowUnix ||
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
        const std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty()) continue;
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
        if (requestedUnix + kAccessRequestTtlSeconds <= nowUnix) continue;
        AccessRequest request{*key, address, requestedUnix};
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
