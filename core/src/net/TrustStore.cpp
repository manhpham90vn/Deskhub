#include "deskhub/net/TrustStore.h"

#include "RecordText.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr size_t kFingerprintBits = kFingerprintBytes * 8;

int Base64Value(char c) {
    const size_t at = kBase64Alphabet.find(c);
    return at == std::string_view::npos ? -1 : int(at);
}

using detail::ParseUnixTime;
using detail::Trim;

std::string SanitizeLabel(std::string_view label) {
    return detail::SanitizeText(label, kMaxTrustLabelBytes);
}

}

bool IsZero(const Fingerprint& fp) {
    return std::all_of(fp.bytes.begin(), fp.bytes.end(), [](uint8_t b) { return b == 0; });
}

std::string FormatFingerprint(const Fingerprint& fp) {
    std::string out(kFingerprintPrefix);
    out.reserve(kFingerprintPrefix.size() + kFingerprintTextBytes);
    uint32_t acc = 0;
    int bits = 0;
    for (uint8_t b : fp.bytes) {
        acc = (acc << 8) | b;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out.push_back(kBase64Alphabet[(acc >> bits) & 0x3F]);
        }
    }
    if (bits > 0) out.push_back(kBase64Alphabet[(acc << (6 - bits)) & 0x3F]);
    return out;
}

std::optional<Fingerprint> ParseFingerprint(std::string_view text) {
    const std::string trimmed = Trim(text);
    std::string_view body(trimmed);
    if (body.size() < kFingerprintPrefix.size()) return std::nullopt;
    if (body.substr(0, kFingerprintPrefix.size()) != kFingerprintPrefix) return std::nullopt;
    body.remove_prefix(kFingerprintPrefix.size());
    if (body.size() != kFingerprintTextBytes) return std::nullopt;

    Fingerprint fp;
    uint32_t acc = 0;
    int bits = 0;
    size_t written = 0;
    for (char c : body) {
        const int v = Base64Value(c);
        if (v < 0) return std::nullopt;
        acc = (acc << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            fp.bytes[written++] = uint8_t((acc >> bits) & 0xFF);
        }
    }
    static_assert(kFingerprintTextBytes * 6 - kFingerprintBits == 2);
    if ((acc & 0x3) != 0) return std::nullopt;
    return fp;
}

TrustVerdict TrustStore::Check(std::string_view endpoint, const Fingerprint& fp) const {
    if (IsZero(fp)) return TrustVerdict::Unknown;
    const std::string key = Trim(endpoint);
    for (const TrustedHost& h : hosts_) {
        if (h.endpoint != key) continue;
        return h.fingerprint == fp ? TrustVerdict::Trusted : TrustVerdict::Changed;
    }
    for (const TrustedHost& h : hosts_)
        if (h.fingerprint == fp) return TrustVerdict::Trusted;
    return TrustVerdict::Unknown;
}

void TrustStore::Remember(std::string_view endpoint, std::string_view label,
    const Fingerprint& fp, int64_t nowUnix) {
    const std::string key = Trim(endpoint);
    if (key.empty() || IsZero(fp)) return;

    for (TrustedHost& h : hosts_) {
        if (h.endpoint != key) continue;
        h.fingerprint = fp;
        h.label = SanitizeLabel(label);
        h.lastSeenUnix = nowUnix;
        return;
    }
    Insert(TrustedHost{key, SanitizeLabel(label), fp, nowUnix, nowUnix});
}

void TrustStore::Insert(const TrustedHost& host) {
    const std::string key = Trim(host.endpoint);
    if (key.empty() || IsZero(host.fingerprint)) return;
    Forget(key);
    if (hosts_.size() >= kMaxTrustedHosts) {
        const auto oldest = std::min_element(hosts_.begin(), hosts_.end(),
            [](const TrustedHost& a, const TrustedHost& b) {
                return a.lastSeenUnix < b.lastSeenUnix;
            });
        hosts_.erase(oldest);
    }
    TrustedHost stored = host;
    stored.endpoint = key;
    stored.label = SanitizeLabel(host.label);
    hosts_.push_back(std::move(stored));
}

bool TrustStore::Forget(std::string_view endpoint) {
    const std::string key = Trim(endpoint);
    const auto at = std::remove_if(hosts_.begin(), hosts_.end(),
        [&](const TrustedHost& h) { return h.endpoint == key; });
    if (at == hosts_.end()) return false;
    hosts_.erase(at, hosts_.end());
    return true;
}

void TrustStore::Clear() {
    hosts_.clear();
}

std::optional<TrustedHost> TrustStore::Find(std::string_view endpoint) const {
    const std::string key = Trim(endpoint);
    for (const TrustedHost& h : hosts_)
        if (h.endpoint == key) return h;
    return std::nullopt;
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

        const size_t s1 = line.find(' ');
        if (s1 == std::string::npos) continue;
        const size_t s2 = line.find(' ', s1 + 1);
        if (s2 == std::string::npos) continue;
        const size_t s3 = line.find(' ', s2 + 1);

        const std::optional<Fingerprint> fp = ParseFingerprint(line.substr(0, s1));
        if (!fp) continue;
        int64_t firstSeen = 0;
        int64_t lastSeen = 0;
        if (!ParseUnixTime(std::string_view(line).substr(s1 + 1, s2 - s1 - 1), firstSeen))
            continue;
        const size_t stampEnd = s3 == std::string::npos ? line.size() : s3;
        if (!ParseUnixTime(std::string_view(line).substr(s2 + 1, stampEnd - s2 - 1), lastSeen))
            continue;
        if (s3 == std::string::npos) continue;

        const size_t s4 = line.find(' ', s3 + 1);
        const size_t endpointEnd = s4 == std::string::npos ? line.size() : s4;
        const std::string endpoint = line.substr(s3 + 1, endpointEnd - s3 - 1);
        if (endpoint.empty()) continue;
        const std::string label =
            s4 == std::string::npos ? std::string() : SanitizeLabel(line.substr(s4 + 1));

        store.Insert(TrustedHost{endpoint, label, *fp, firstSeen, lastSeen});
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
