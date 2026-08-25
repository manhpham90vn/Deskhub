#include "deskhub/ui/RecentDevices.h"

#include <algorithm>

#include "deskhub/ui/SecretText.h"
#include "deskhub/ui/Strings.h"

namespace deskhub::ui {

namespace {

bool ParseUnixTime(std::string_view s, int64_t& out) {
    if (s.empty()) return false;
    int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        if (v > (INT64_MAX - 9) / 10) return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

bool ParseLine(std::string_view line, RecentDevice& out) {
    const std::string trimmed = TrimAscii(line);
    const size_t space = trimmed.find(' ');
    if (space == std::string::npos) return false;

    int64_t stamp = 0;
    if (!ParseUnixTime(std::string_view(trimmed).substr(0, space), stamp)) return false;

    const std::string rest = TrimAscii(std::string_view(trimmed).substr(space + 1));
    const size_t next = rest.find(' ');

    std::string addr = rest;
    std::string passcode;
    if (next != std::string::npos) {
        const std::string tail = DecodeSecret(TrimAscii(std::string_view(rest).substr(next + 1)));
        if (IsValidPasscode(tail)) passcode = tail;
        addr = TrimAscii(std::string_view(rest).substr(0, next));
    }
    if (addr.empty()) return false;

    out.addr = std::move(addr);
    out.lastConnectedUnix = stamp;
    out.passcode = std::move(passcode);
    return true;
}

bool ContainsAddr(const std::vector<RecentDevice>& devices, std::string_view addr) {
    return std::any_of(devices.begin(), devices.end(),
        [&](const RecentDevice& d) { return d.addr == addr; });
}

}

std::vector<RecentDevice> ParseRecentDevices(std::string_view text) {
    std::vector<RecentDevice> out;
    size_t pos = 0;
    while (pos < text.size() && out.size() < kMaxRecentDevices) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();

        RecentDevice device;
        if (ParseLine(text.substr(pos, end - pos), device) && !ContainsAddr(out, device.addr))
            out.push_back(std::move(device));

        pos = end + 1;
    }
    return out;
}

std::string SerializeRecentDevices(const std::vector<RecentDevice>& devices) {
    std::string out;
    size_t count = 0;
    for (const RecentDevice& d : devices) {
        if (count == kMaxRecentDevices) break;
        if (d.addr.empty()) continue;
        out += std::to_string(d.lastConnectedUnix);
        out += ' ';
        out += d.addr;
        if (IsValidPasscode(d.passcode)) {
            out += ' ';
            out += EncodeSecret(d.passcode);
        }
        out += '\n';
        ++count;
    }
    return out;
}

void TouchRecentDevice(std::vector<RecentDevice>& devices, std::string_view addr,
    int64_t nowUnix, std::string_view passcode) {
    const std::string trimmed = TrimAscii(addr);
    if (trimmed.empty()) return;

    std::string kept = IsValidPasscode(passcode) ? std::string(passcode)
                                                 : PasscodeForDevice(devices, trimmed);
    RemoveRecentDevice(devices, trimmed);
    devices.insert(devices.begin(), RecentDevice{trimmed, nowUnix, std::move(kept)});
    if (devices.size() > kMaxRecentDevices) devices.resize(kMaxRecentDevices);
}

std::string PasscodeForDevice(const std::vector<RecentDevice>& devices, std::string_view addr) {
    const std::string wanted = NormalizedDeviceAddr(addr);
    if (wanted.empty()) return {};
    for (const RecentDevice& d : devices)
        if (NormalizedDeviceAddr(d.addr) == wanted) return d.passcode;
    return {};
}

void RemoveRecentDevice(std::vector<RecentDevice>& devices, std::string_view addr) {
    const std::string wanted = NormalizedDeviceAddr(addr);
    if (wanted.empty()) return;
    devices.erase(std::remove_if(devices.begin(), devices.end(),
                      [&](const RecentDevice& d) { return NormalizedDeviceAddr(d.addr) == wanted; }),
        devices.end());
}

}
