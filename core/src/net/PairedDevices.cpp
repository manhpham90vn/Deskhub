#include "deskhub/net/PairedDevices.h"

#include "RecordText.h"

#include <algorithm>

namespace deskhub {

namespace {

using detail::ParseUnixTime;
using detail::Trim;

std::string SanitizeName(std::string_view name) {
    return detail::SanitizeText(name, kMaxPairedNameBytes);
}

std::optional<PairedDevice> ParsePairedDeviceLine(std::string_view line) {
    const size_t s1 = line.find(' ');
    if (s1 == std::string_view::npos) return std::nullopt;
    const size_t s2 = line.find(' ', s1 + 1);
    if (s2 == std::string_view::npos) return std::nullopt;
    const size_t s3 = line.find(' ', s2 + 1);

    const std::optional<Fingerprint> fp = ParseFingerprint(line.substr(0, s1));
    if (!fp || IsZero(*fp)) return std::nullopt;
    int64_t paired = 0;
    int64_t lastSeen = 0;
    if (!ParseUnixTime(line.substr(s1 + 1, s2 - s1 - 1), paired)) return std::nullopt;
    const size_t stampEnd = s3 == std::string_view::npos ? line.size() : s3;
    if (!ParseUnixTime(line.substr(s2 + 1, stampEnd - s2 - 1), lastSeen))
        return std::nullopt;

    const std::string_view rawName =
        s3 == std::string_view::npos ? std::string_view() : line.substr(s3 + 1);
    const std::string name = SanitizeName(rawName);
    if (name != rawName) return std::nullopt;
    return PairedDevice{*fp, name, paired, lastSeen};
}

}

PairVerdict PairedDevices::Check(const Fingerprint& fp) const {
    if (IsZero(fp)) return PairVerdict::Unknown;
    for (const PairedDevice& d : devices_)
        if (d.fingerprint == fp) return PairVerdict::Paired;
    return PairVerdict::Unknown;
}

void PairedDevices::Remember(const Fingerprint& fp, std::string_view name, int64_t nowUnix) {
    if (IsZero(fp)) return;
    if (Touch(fp, name, nowUnix)) return;
    Insert(PairedDevice{fp, SanitizeName(name), nowUnix, nowUnix});
}

void PairedDevices::Insert(const PairedDevice& device) {
    if (IsZero(device.fingerprint)) return;
    Forget(device.fingerprint);
    if (devices_.size() >= kMaxPairedDevices) {
        const auto oldest = std::min_element(devices_.begin(), devices_.end(),
            [](const PairedDevice& a, const PairedDevice& b) {
                return a.lastSeenUnix < b.lastSeenUnix;
            });
        devices_.erase(oldest);
    }
    PairedDevice stored = device;
    stored.name = SanitizeName(device.name);
    devices_.push_back(std::move(stored));
}

bool PairedDevices::Touch(const Fingerprint& fp, std::string_view name, int64_t nowUnix) {
    if (IsZero(fp)) return false;
    for (PairedDevice& d : devices_) {
        if (!(d.fingerprint == fp)) continue;
        const std::string clean = SanitizeName(name);
        if (!clean.empty()) d.name = clean;
        d.lastSeenUnix = nowUnix;
        return true;
    }
    return false;
}

bool PairedDevices::Forget(const Fingerprint& fp) {
    const auto at = std::remove_if(devices_.begin(), devices_.end(),
        [&](const PairedDevice& d) { return d.fingerprint == fp; });
    if (at == devices_.end()) return false;
    devices_.erase(at, devices_.end());
    return true;
}

void PairedDevices::Clear() {
    devices_.clear();
}

std::optional<PairedDevice> PairedDevices::Find(const Fingerprint& fp) const {
    if (IsZero(fp)) return std::nullopt;
    for (const PairedDevice& d : devices_)
        if (d.fingerprint == fp) return d;
    return std::nullopt;
}

std::string ShortFingerprint(const Fingerprint& fp) {
    if (IsZero(fp)) return {};
    std::string full = FormatFingerprint(fp);
    const size_t start = kFingerprintPrefix.size();
    if (full.size() <= start) return full;
    return full.substr(start, kShortFingerprintChars);
}

std::optional<PairedDevices> ParsePairedDevicesStrict(std::string_view text) {
    if (text.size() > 32768) return std::nullopt;
    PairedDevices out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string line = Trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty() || line[0] == '#') continue;
        const auto device = ParsePairedDeviceLine(line);
        if (!device || out.Size() >= kMaxPairedDevices ||
            out.Find(device->fingerprint).has_value() || line.size() > 512)
            return std::nullopt;
        out.Insert(*device);
    }
    return out;
}

std::string SerializePairedDevices(const PairedDevices& devices) {
    std::string out;
    for (const PairedDevice& d : devices.Devices()) {
        if (IsZero(d.fingerprint)) continue;
        out += FormatFingerprint(d.fingerprint);
        out += ' ';
        out += std::to_string(d.pairedUnix);
        out += ' ';
        out += std::to_string(d.lastSeenUnix);
        if (!d.name.empty()) {
            out += ' ';
            out += d.name;
        }
        out += '\n';
    }
    return out;
}

}
