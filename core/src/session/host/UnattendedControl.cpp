#include "deskhub/session/host/UnattendedControl.h"

#include <algorithm>
#include <utility>

namespace deskhub {
namespace {

constexpr std::string_view kScreenKey = "screen=";
constexpr std::string_view kInputKey = "input=";
constexpr std::string_view kGranted = "granted";
constexpr std::string_view kDenied = "denied";
constexpr std::string_view kUnknown = "unknown";

std::string_view PermissionWord(UnattendedPermission permission) {
    switch (permission) {
        case UnattendedPermission::Granted: return kGranted;
        case UnattendedPermission::Denied: return kDenied;
        case UnattendedPermission::Unknown: break;
    }
    return kUnknown;
}

UnattendedPermission PermissionOf(std::string_view word) {
    if (word == kGranted) return UnattendedPermission::Granted;
    if (word == kDenied) return UnattendedPermission::Denied;
    return UnattendedPermission::Unknown;
}

std::optional<UnattendedPermission> PermissionOf(uint8_t byte) {
    if (byte > uint8_t(UnattendedPermission::Granted)) return std::nullopt;
    return UnattendedPermission(byte);
}

std::string_view TrimLine(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
    return line;
}

}

UnattendedAction UnattendedControlServer::Feed(std::span<const uint8_t> bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        const UnattendedAction action = Take(bytes[i]);
        if (action == UnattendedAction::NeedMore) continue;
        const bool trailing = i + 1 < bytes.size();
        return trailing ? Refuse() : action;
    }
    return UnattendedAction::NeedMore;
}

UnattendedAction UnattendedControlServer::Take(uint8_t byte) {
    switch (stage_) {
        case Stage::Request: return StartRequest(byte);
        case Stage::ConfigLength:
            pending_.push_back(byte);
            if (pending_.size() < kUnattendedConfigLengthSize) return UnattendedAction::NeedMore;
            return FinishConfigLength();
        case Stage::ConfigBody:
            pending_.push_back(byte);
            if (pending_.size() < bodyLength_) return UnattendedAction::NeedMore;
            return FinishConfigBody();
        case Stage::Served:
        case Stage::Refused: break;
    }
    return Refuse();
}

UnattendedAction UnattendedControlServer::StartRequest(uint8_t byte) {
    const std::optional<SecureDesktopRequest> request = DecodeSecureDesktopRequest(byte);
    if (!request) return Refuse();
    switch (*request) {
        case SecureDesktopRequest::Agent: stage_ = Stage::Served; return UnattendedAction::Agent;
        case SecureDesktopRequest::Takeover:
            stage_ = Stage::Served;
            return UnattendedAction::Takeover;
        case SecureDesktopRequest::Status:
            stage_ = Stage::Served;
            return UnattendedAction::Status;
        case SecureDesktopRequest::Configure:
            stage_ = Stage::ConfigLength;
            pending_.clear();
            return UnattendedAction::NeedMore;
    }
    return Refuse();
}

UnattendedAction UnattendedControlServer::FinishConfigLength() {
    const std::optional<uint32_t> length = DecodeUnattendedConfigLength(pending_);
    if (!length) return Refuse();
    bodyLength_ = *length;
    pending_.clear();
    pending_.reserve(bodyLength_);
    stage_ = Stage::ConfigBody;
    return UnattendedAction::NeedMore;
}

UnattendedAction UnattendedControlServer::FinishConfigBody() {
    std::optional<UnattendedConfig> config = DecodeUnattendedConfig(pending_);
    pending_.clear();
    if (!config) return Refuse();
    config_ = std::move(*config);
    stage_ = Stage::Served;
    return UnattendedAction::Configure;
}

UnattendedAction UnattendedControlServer::Refuse() {
    stage_ = Stage::Refused;
    pending_.clear();
    return UnattendedAction::Refuse;
}

std::array<uint8_t, kUnattendedStatusSize> EncodeUnattendedStatus(const UnattendedStatus& status) {
    return {uint8_t(status.screen), uint8_t(status.input)};
}

std::optional<UnattendedStatus> DecodeUnattendedStatus(std::span<const uint8_t> bytes) {
    if (bytes.size() != kUnattendedStatusSize) return std::nullopt;
    const std::optional<UnattendedPermission> screen = PermissionOf(bytes[0]);
    const std::optional<UnattendedPermission> input = PermissionOf(bytes[1]);
    if (!screen || !input) return std::nullopt;
    return UnattendedStatus{*screen, *input};
}

std::string SerializeUnattendedStatus(const UnattendedStatus& status) {
    std::string out;
    out += kScreenKey;
    out += PermissionWord(status.screen);
    out += '\n';
    out += kInputKey;
    out += PermissionWord(status.input);
    out += '\n';
    return out;
}

UnattendedStatus ParseUnattendedStatus(std::string_view text) {
    UnattendedStatus status;
    while (!text.empty()) {
        const size_t newline = text.find('\n');
        const std::string_view line = TrimLine(text.substr(0, newline));
        text = newline == std::string_view::npos ? std::string_view() : text.substr(newline + 1);
        if (line.starts_with(kScreenKey))
            status.screen = PermissionOf(line.substr(kScreenKey.size()));
        else if (line.starts_with(kInputKey))
            status.input = PermissionOf(line.substr(kInputKey.size()));
    }
    return status;
}

}
