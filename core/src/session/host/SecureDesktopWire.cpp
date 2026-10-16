#include "deskhub/session/host/SecureDesktopWire.h"

#include "deskhub/protocol/ByteOrder.h"

#include <algorithm>

namespace deskhub {
namespace {

constexpr size_t kWatchSize = 17;
constexpr size_t kInputSize = 21;
constexpr size_t kStateSize = 1;

std::vector<uint8_t> StartMessage(SecureDesktopMsg type, size_t payloadSize) {
    std::vector<uint8_t> out(kSecureDesktopHeaderSize + payloadSize);
    out[0] = uint8_t(type);
    PutU32(out.data() + 1, uint32_t(payloadSize));
    return out;
}

uint8_t* Payload(std::vector<uint8_t>& message) {
    return message.data() + kSecureDesktopHeaderSize;
}

bool KnownType(uint8_t type) {
    return type >= uint8_t(SecureDesktopMsg::Watch) && type <= uint8_t(SecureDesktopMsg::Frame);
}

bool ValidFrameSize(uint32_t width, uint32_t height) {
    return width > 0 && height > 0 && width <= kSecureDesktopMaxFrameSide &&
           height <= kSecureDesktopMaxFrameSide;
}

size_t FrameBytes(uint32_t width, uint32_t height) {
    return size_t(width) * height * 4;
}

char AsciiLower(char c) {
    return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

}

bool IsSecureDesktopName(std::string_view inputDesktopName) {
    if (inputDesktopName.empty()) return false;
    constexpr std::string_view kDefault = "default";
    if (inputDesktopName.size() != kDefault.size()) return true;
    return !std::equal(inputDesktopName.begin(), inputDesktopName.end(), kDefault.begin(),
        [](char a, char b) { return AsciiLower(a) == b; });
}

std::vector<uint8_t> EncodeSecureDesktopWatch(const SecureDesktopWatch& watch) {
    std::vector<uint8_t> out = StartMessage(SecureDesktopMsg::Watch, kWatchSize);
    uint8_t* p = Payload(out);
    p[0] = watch.sourceId;
    PutU32(p + 1, uint32_t(watch.left));
    PutU32(p + 5, uint32_t(watch.top));
    PutU32(p + 9, watch.width);
    PutU32(p + 13, watch.height);
    return out;
}

std::vector<uint8_t> EncodeSecureDesktopInput(const SecureDesktopInput& input) {
    std::vector<uint8_t> out = StartMessage(SecureDesktopMsg::Input, kInputSize);
    uint8_t* p = Payload(out);
    p[0] = uint8_t(input.kind);
    PutU16(p + 1, input.vk);
    PutU16(p + 3, input.scan);
    PutU32(p + 5, input.flags);
    PutU32(p + 9, uint32_t(input.dx));
    PutU32(p + 13, uint32_t(input.dy));
    PutU32(p + 17, input.data);
    return out;
}

std::vector<uint8_t> EncodeSecureDesktopState(bool secure) {
    std::vector<uint8_t> out = StartMessage(SecureDesktopMsg::DesktopState, kStateSize);
    Payload(out)[0] = secure ? 1 : 0;
    return out;
}

std::vector<uint8_t> EncodeSecureDesktopFrame(uint8_t sourceId, uint32_t width, uint32_t height,
    std::span<const uint8_t> bgra) {
    if (!ValidFrameSize(width, height) || bgra.size() != FrameBytes(width, height)) return {};
    std::vector<uint8_t> out =
        StartMessage(SecureDesktopMsg::Frame, kSecureDesktopFrameHeaderSize + bgra.size());
    uint8_t* p = Payload(out);
    p[0] = sourceId;
    PutU32(p + 1, width);
    PutU32(p + 5, height);
    std::copy(bgra.begin(), bgra.end(), p + kSecureDesktopFrameHeaderSize);
    return out;
}

std::optional<SecureDesktopWatch> DecodeSecureDesktopWatch(std::span<const uint8_t> payload) {
    if (payload.size() != kWatchSize) return std::nullopt;
    const uint8_t* p = payload.data();
    SecureDesktopWatch watch;
    watch.sourceId = p[0];
    watch.left = int32_t(GetU32(p + 1));
    watch.top = int32_t(GetU32(p + 5));
    watch.width = GetU32(p + 9);
    watch.height = GetU32(p + 13);
    if (!ValidFrameSize(watch.width, watch.height)) return std::nullopt;
    return watch;
}

std::optional<SecureDesktopInput> DecodeSecureDesktopInput(std::span<const uint8_t> payload) {
    if (payload.size() != kInputSize) return std::nullopt;
    const uint8_t* p = payload.data();
    if (p[0] != uint8_t(SecureDesktopInputKind::Keyboard) &&
        p[0] != uint8_t(SecureDesktopInputKind::Mouse))
        return std::nullopt;
    SecureDesktopInput input;
    input.kind = SecureDesktopInputKind(p[0]);
    input.vk = GetU16(p + 1);
    input.scan = GetU16(p + 3);
    input.flags = GetU32(p + 5);
    input.dx = int32_t(GetU32(p + 9));
    input.dy = int32_t(GetU32(p + 13));
    input.data = GetU32(p + 17);
    return input;
}

std::optional<bool> DecodeSecureDesktopState(std::span<const uint8_t> payload) {
    if (payload.size() != kStateSize || payload[0] > 1) return std::nullopt;
    return payload[0] == 1;
}

std::optional<SecureDesktopFrame> DecodeSecureDesktopFrame(std::span<const uint8_t> payload) {
    if (payload.size() < kSecureDesktopFrameHeaderSize) return std::nullopt;
    const uint8_t* p = payload.data();
    SecureDesktopFrame frame;
    frame.sourceId = p[0];
    frame.width = GetU32(p + 1);
    frame.height = GetU32(p + 5);
    if (!ValidFrameSize(frame.width, frame.height)) return std::nullopt;
    const size_t pixels = payload.size() - kSecureDesktopFrameHeaderSize;
    if (pixels != FrameBytes(frame.width, frame.height)) return std::nullopt;
    frame.bgra = payload.subspan(kSecureDesktopFrameHeaderSize);
    return frame;
}

void SecureDesktopStream::Append(std::span<const uint8_t> bytes) {
    if (failed_ || bytes.empty()) return;
    Compact();
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

bool SecureDesktopStream::Next(SecureDesktopMessage& out) {
    if (failed_) return false;
    const size_t pending = buffer_.size() - consumed_;
    if (pending < kSecureDesktopHeaderSize) return false;
    const uint8_t* head = buffer_.data() + consumed_;
    const uint32_t length = GetU32(head + 1);
    if (!KnownType(head[0]) || length > kSecureDesktopMaxPayload) {
        Fail();
        return false;
    }
    if (pending - kSecureDesktopHeaderSize < length) return false;
    out.type = SecureDesktopMsg(head[0]);
    out.payload.assign(head + kSecureDesktopHeaderSize, head + kSecureDesktopHeaderSize + length);
    consumed_ += kSecureDesktopHeaderSize + length;
    if (consumed_ == buffer_.size()) {
        buffer_.clear();
        consumed_ = 0;
    }
    return true;
}

void SecureDesktopStream::Fail() {
    failed_ = true;
    buffer_.clear();
    consumed_ = 0;
}

void SecureDesktopStream::Compact() {
    if (consumed_ == 0) return;
    buffer_.erase(buffer_.begin(), buffer_.begin() + std::ptrdiff_t(consumed_));
    consumed_ = 0;
}

}
