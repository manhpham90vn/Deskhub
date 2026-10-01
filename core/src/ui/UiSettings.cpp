#include "deskhub/ui/UiSettings.h"

#include <optional>

#include "deskhub/net/Ipv4.h"
#include "deskhub/ui/Strings.h"

namespace deskhub::ui {

namespace {

std::optional<uint32_t> ParseUint(std::string_view s) {
    if (s.empty()) return std::nullopt;
    uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        v = v * 10 + uint64_t(c - '0');
        if (v > 0xFFFFFFFFull) return std::nullopt;
    }
    return uint32_t(v);
}

constexpr std::string_view kRetiredSettingsKeys[] = {"passcode"};

bool IsRetiredSettingsLine(std::string_view line) {
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) return false;
    const std::string key = TrimAscii(line.substr(0, eq));
    for (std::string_view retired : kRetiredSettingsKeys)
        if (key == retired) return true;
    return false;
}

void ApplyKeyValue(UiSettings& out, std::string_view key, std::string_view value) {
    if (key == "name") {
        out.deviceName = TruncateDeviceName(value);
        return;
    }
    if (key == "bind_ip") {
        if (value.empty() || ParseIPv4(value)) out.bindIp = value;
        return;
    }
    if (key == "transfer_dir") {
        out.transferDir = TruncateSettingsPath(value);
        return;
    }
    const std::optional<uint32_t> v = ParseUint(value);
    if (!v) return;

    if (key == "fps" && *v >= 1 && *v <= kMaxSettingsFps) out.fps = *v;
    if (key == "bitrate_mbps" && *v >= 1 && *v <= kMaxSettingsBitrateMbps) out.bitrateMbps = *v;
    if (key == "max_dim" && *v <= kMaxSettingsDim) out.maxDim = *v;
    if (key == "port" && *v >= 1 && *v <= kMaxSettingsPort) out.port = *v;
    if (key == "allow_input") out.allowInput = *v != 0;
    if (key == "client_control") out.clientControl = *v != 0;
    if (key == "autostart") out.autostart = *v != 0;
    if (key == "auto_share") out.autoShare = *v != 0;
    if (key == "clipboard_sync") out.clipboardSync = *v != 0;
    if (key == "share_audio") out.shareAudio = *v != 0;
    if (key == "play_audio") out.playAudio = *v != 0;
    if (key == "start_hidden") out.startHidden = *v != 0;
    if (key == "keep_awake") out.keepAwake = *v != 0;
}

}

std::string TruncateSettingsPath(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path)
        if (uint8_t(c) >= 0x20 && uint8_t(c) != 0x7F) out.push_back(c);
    if (out.size() > kMaxSettingsPathBytes) out.resize(kMaxSettingsPathBytes);
    return out;
}

std::string TruncateDeviceName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name)
        if (uint8_t(c) >= 0x20 && uint8_t(c) != 0x7F) out.push_back(c);
    if (out.size() > kMaxClientNameBytes) {
        size_t n = kMaxClientNameBytes;
        while (n > 0 && (uint8_t(out[n]) & 0xC0) == 0x80) --n;
        out.resize(n);
    }
    return out;
}

UiSettings ParseUiSettings(std::string_view text) {
    UiSettings out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();

        const std::string line = TrimAscii(text.substr(pos, end - pos));
        const size_t eq = line.find('=');
        if (eq != std::string::npos) {
            const std::string key = TrimAscii(std::string_view(line).substr(0, eq));
            const std::string value = TrimAscii(std::string_view(line).substr(eq + 1));
            ApplyKeyValue(out, key, value);
        }

        pos = end + 1;
    }
    return out;
}

std::string StripRetiredUiSettings(std::string_view text) {
    std::string kept;
    kept.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        end = end == std::string_view::npos ? text.size() : end + 1;
        const std::string_view line = text.substr(pos, end - pos);
        if (!IsRetiredSettingsLine(line)) kept += line;
        pos = end;
    }
    return kept;
}

std::string SerializeUiSettings(const UiSettings& settings) {
    std::string out;
    out += "fps=" + std::to_string(settings.fps) + '\n';
    out += "bitrate_mbps=" + std::to_string(settings.bitrateMbps) + '\n';
    out += "max_dim=" + std::to_string(settings.maxDim) + '\n';
    out += "port=" + std::to_string(settings.port) + '\n';
    out += std::string("allow_input=") + (settings.allowInput ? "1" : "0") + '\n';
    out += std::string("client_control=") + (settings.clientControl ? "1" : "0") + '\n';
    out += "name=" + TruncateDeviceName(settings.deviceName) + '\n';
    out += "bind_ip=";
    if (ParseIPv4(settings.bindIp)) out += settings.bindIp;
    out += '\n';
    out += std::string("autostart=") + (settings.autostart ? "1" : "0") + '\n';
    out += std::string("auto_share=") + (settings.autoShare ? "1" : "0") + '\n';
    out += std::string("clipboard_sync=") + (settings.clipboardSync ? "1" : "0") + '\n';
    out += std::string("share_audio=") + (settings.shareAudio ? "1" : "0") + '\n';
    out += std::string("play_audio=") + (settings.playAudio ? "1" : "0") + '\n';
    out += std::string("start_hidden=") + (settings.startHidden ? "1" : "0") + '\n';
    out += std::string("keep_awake=") + (settings.keepAwake ? "1" : "0") + '\n';
    out += "transfer_dir=" + TruncateSettingsPath(settings.transferDir) + '\n';
    return out;
}

}
