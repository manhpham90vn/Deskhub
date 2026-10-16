#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace deskhub {

enum class SecureDesktopMsg : uint8_t {
    Watch = 1,
    Input = 2,
    DesktopState = 3,
    Frame = 4,
};

inline constexpr size_t kSecureDesktopHeaderSize = 5;
inline constexpr uint32_t kSecureDesktopMaxFrameSide = 8192;
inline constexpr size_t kSecureDesktopFrameHeaderSize = 9;
inline constexpr size_t kSecureDesktopMaxPayload =
    kSecureDesktopFrameHeaderSize +
    size_t(kSecureDesktopMaxFrameSide) * kSecureDesktopMaxFrameSide * 4;

struct SecureDesktopWatch {
    uint8_t sourceId = 0;
    int32_t left = 0;
    int32_t top = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

enum class SecureDesktopInputKind : uint8_t {
    Keyboard = 1,
    Mouse = 2,
};

struct SecureDesktopInput {
    SecureDesktopInputKind kind = SecureDesktopInputKind::Keyboard;
    uint16_t vk = 0;
    uint16_t scan = 0;
    uint32_t flags = 0;
    int32_t dx = 0;
    int32_t dy = 0;
    uint32_t data = 0;
};

struct SecureDesktopFrame {
    uint8_t sourceId = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::span<const uint8_t> bgra;
};

struct SecureDesktopMessage {
    SecureDesktopMsg type = SecureDesktopMsg::Watch;
    std::vector<uint8_t> payload;
};

bool IsSecureDesktopName(std::string_view inputDesktopName);

std::vector<uint8_t> EncodeSecureDesktopWatch(const SecureDesktopWatch& watch);
std::vector<uint8_t> EncodeSecureDesktopInput(const SecureDesktopInput& input);
std::vector<uint8_t> EncodeSecureDesktopState(bool secure);
std::vector<uint8_t> EncodeSecureDesktopFrame(uint8_t sourceId, uint32_t width, uint32_t height,
    std::span<const uint8_t> bgra);

std::optional<SecureDesktopWatch> DecodeSecureDesktopWatch(std::span<const uint8_t> payload);
std::optional<SecureDesktopInput> DecodeSecureDesktopInput(std::span<const uint8_t> payload);
std::optional<bool> DecodeSecureDesktopState(std::span<const uint8_t> payload);
std::optional<SecureDesktopFrame> DecodeSecureDesktopFrame(std::span<const uint8_t> payload);

class SecureDesktopStream {
public:
    void Append(std::span<const uint8_t> bytes);
    bool Next(SecureDesktopMessage& out);

    bool Failed() const {
        return failed_;
    }

private:
    void Fail();
    void Compact();

    std::vector<uint8_t> buffer_{};
    size_t consumed_ = 0;
    bool failed_ = false;
};

}
