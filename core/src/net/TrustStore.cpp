#include "deskhub/net/TrustStore.h"

#include "RecordText.h"
#include "deskhub/net/Base64.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr size_t kMaxTrustStoreFileBytes = 131072;
constexpr size_t kMaxTrustLineBytes = 512;

using detail::ParseUnixTime;
using detail::Trim;

std::string SanitizeLabel(std::string_view label) {
    return detail::SanitizeText(label, kMaxTrustLabelBytes);
}

std::string SanitizeEndpoint(std::string_view endpoint) {
    std::string trimmed = Trim(endpoint);
    if (trimmed.size() > kMaxTrustEndpointBytes) return {};
    for (char c : trimmed)
        if (uint8_t(c) <= 0x20 || uint8_t(c) == 0x7F) return {};
    return trimmed;
}

}

bool IsZero(const Fingerprint& fp) {
    return std::all_of(fp.bytes.begin(), fp.bytes.end(), [](uint8_t b) { return b == 0; });
}

std::string FormatFingerprint(const Fingerprint& fp) {
    std::string out(kFingerprintPrefix);
    out += EncodeBase64(fp.bytes, Base64Alphabet::Standard, Base64Padding::None);
    return out;
}

std::string ShortFingerprint(const Fingerprint& fp) {
    if (IsZero(fp)) return {};
    std::string full = FormatFingerprint(fp);
    const size_t start = kFingerprintPrefix.size();
    if (full.size() <= start) return full;
    return full.substr(start, kShortFingerprintChars);
}

std::optional<Fingerprint> ParseFingerprint(std::string_view text) {
    const std::string trimmed = Trim(text);
    std::string_view body(trimmed);
    if (body.size() < kFingerprintPrefix.size()) return std::nullopt;
    if (body.substr(0, kFingerprintPrefix.size()) != kFingerprintPrefix) return std::nullopt;
    body.remove_prefix(kFingerprintPrefix.size());
    if (body.size() != kFingerprintTextBytes) return std::nullopt;
    const auto bytes = DecodeBase64(body, Base64Alphabet::Standard);
    if (!bytes || bytes->size() != kFingerprintBytes) return std::nullopt;
    Fingerprint fp;
    std::copy(bytes->begin(), bytes->end(), fp.bytes.begin());
    return fp;
}

TrustVerdict TrustStore::Check(const Fingerprint& fp) const {
    if (IsZero(fp)) return TrustVerdict::Unknown;
    return Find(fp) ? TrustVerdict::Trusted : TrustVerdict::Unknown;
}

void TrustStore::Remember(const Fingerprint& fp, std::string_view label,
    std::string_view endpoint, int64_t nowUnix) {
    if (IsZero(fp)) return;
    for (TrustedHost& h : hosts_) {
        if (h.fingerprint != fp) continue;
        const std::string cleanLabel = SanitizeLabel(label);
        if (!cleanLabel.empty()) h.label = cleanLabel;
        const std::string cleanEndpoint = SanitizeEndpoint(endpoint);
        if (!cleanEndpoint.empty()) h.endpoint = cleanEndpoint;
        h.lastSeenUnix = nowUnix;
        return;
    }
    Insert(TrustedHost{fp, SanitizeLabel(label), SanitizeEndpoint(endpoint), nowUnix, nowUnix});
}

bool TrustStore::Touch(const Fingerprint& fp, std::string_view endpoint, int64_t nowUnix) {
    for (TrustedHost& h : hosts_) {
        if (h.fingerprint != fp) continue;
        const std::string cleanEndpoint = SanitizeEndpoint(endpoint);
        if (!cleanEndpoint.empty()) h.endpoint = cleanEndpoint;
        h.lastSeenUnix = nowUnix;
        return true;
    }
    return false;
}

void TrustStore::Insert(const TrustedHost& host) {
    if (IsZero(host.fingerprint)) return;
    Forget(host.fingerprint);
    if (hosts_.size() >= kMaxTrustedHosts) {
        const auto oldest = std::min_element(hosts_.begin(), hosts_.end(),
            [](const TrustedHost& a, const TrustedHost& b) {
                return a.lastSeenUnix < b.lastSeenUnix;
            });
        hosts_.erase(oldest);
    }
    TrustedHost stored = host;
    stored.label = SanitizeLabel(host.label);
    stored.endpoint = SanitizeEndpoint(host.endpoint);
    hosts_.push_back(std::move(stored));
}

bool TrustStore::SetLabel(const Fingerprint& fp, std::string_view label) {
    if (label != SanitizeLabel(label)) return false;
    for (TrustedHost& host : hosts_) {
        if (host.fingerprint != fp) continue;
        host.label = std::string(label);
        return true;
    }
    return false;
}

bool TrustStore::Forget(const Fingerprint& fp) {
    const auto at = std::remove_if(hosts_.begin(), hosts_.end(),
        [&](const TrustedHost& h) { return h.fingerprint == fp; });
    if (at == hosts_.end()) return false;
    hosts_.erase(at, hosts_.end());
    return true;
}

void TrustStore::Clear() {
    hosts_.clear();
}

std::optional<TrustedHost> TrustStore::Find(const Fingerprint& fp) const {
    if (IsZero(fp)) return std::nullopt;
    for (const TrustedHost& h : hosts_)
        if (h.fingerprint == fp) return h;
    return std::nullopt;
}

std::optional<TrustedHost> TrustStore::FindByEndpoint(std::string_view endpoint) const {
    const std::string key = SanitizeEndpoint(endpoint);
    if (key.empty()) return std::nullopt;
    const TrustedHost* latest = nullptr;
    for (const TrustedHost& h : hosts_) {
        if (h.endpoint != key) continue;
        if (latest == nullptr || h.lastSeenUnix > latest->lastSeenUnix) latest = &h;
    }
    if (latest == nullptr) return std::nullopt;
    return *latest;
}

TrustStore ParseTrustStore(std::string_view text) {
    TrustStore store;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        const size_t tab = line.find('\t');
        const std::string_view base =
            tab == std::string::npos ? std::string_view(line) : std::string_view(line).substr(0, tab);

        const size_t s1 = base.find(' ');
        if (s1 == std::string::npos) continue;
        const size_t s2 = base.find(' ', s1 + 1);
        if (s2 == std::string::npos) continue;
        const size_t s3 = base.find(' ', s2 + 1);
        if (s3 == std::string::npos) continue;

        const std::optional<Fingerprint> fp = ParseFingerprint(base.substr(0, s1));
        if (!fp) continue;
        int64_t firstSeen = 0;
        int64_t lastSeen = 0;
        if (!ParseUnixTime(base.substr(s1 + 1, s2 - s1 - 1), firstSeen)) continue;
        if (!ParseUnixTime(base.substr(s2 + 1, s3 - s2 - 1), lastSeen)) continue;

        const size_t s4 = base.find(' ', s3 + 1);
        const size_t endpointEnd = s4 == std::string::npos ? base.size() : s4;
        const std::string endpoint = SanitizeEndpoint(base.substr(s3 + 1, endpointEnd - s3 - 1));
        if (endpoint.empty()) continue;
        const std::string label =
            s4 == std::string::npos ? std::string() : SanitizeLabel(base.substr(s4 + 1));

        store.Insert(TrustedHost{*fp, label, endpoint, firstSeen, lastSeen});
    }
    return store;
}

std::optional<TrustStore> ParseTrustStoreStrict(std::string_view text) {
    if (text.size() > kMaxTrustStoreFileBytes) return std::nullopt;
    TrustStore store;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        if (line.size() > kMaxTrustLineBytes || store.Size() >= kMaxTrustedHosts)
            return std::nullopt;
        for (char c : line)
            if (uint8_t(c) < 32 && c != '\t') return std::nullopt;
        const TrustStore parsed = ParseTrustStore(line);
        if (parsed.Size() != 1) return std::nullopt;
        const TrustedHost& host = parsed.Hosts().front();
        if (store.Find(host.fingerprint)) return std::nullopt;
        store.Insert(host);
    }
    return store;
}

std::string SerializeTrustStore(const TrustStore& store) {
    std::string out;
    for (const TrustedHost& h : store.Hosts()) {
        if (h.endpoint.empty() || IsZero(h.fingerprint)) continue;
        out += FormatFingerprint(h.fingerprint);
        out += ' ';
        out += std::to_string(h.firstSeenUnix);
        out += ' ';
        out += std::to_string(h.lastSeenUnix);
        out += ' ';
        out += h.endpoint;
        if (!h.label.empty()) {
            out += ' ';
            out += h.label;
        }
        out += '\n';
    }
    return out;
}

}
