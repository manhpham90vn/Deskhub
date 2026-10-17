#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

enum class SecureDesktopMsg : uint8_t {
    Watch = 1,
    Input = 2,
    DesktopState = 3,
    Frame = 4,
};

enum class SecureDesktopRequest : uint8_t {
    Agent = 1,
    Takeover = 2,
    Configure = 3,
    Status = 4,
};

inline constexpr uint8_t kSecureDesktopAck = 1;
inline constexpr uint32_t kSecureDesktopDefaultMaxFps = 10;
inline constexpr uint32_t kSecureDesktopMaxFps = 240;
inline constexpr size_t kUnattendedMaxHostKeyBytes = size_t{8} * 1024;
inline constexpr size_t kUnattendedMaxAuthorizedKeysBytes = size_t{256} * 1024;
inline constexpr size_t kUnattendedMaxSettingsBytes = size_t{16} * 1024;
inline constexpr size_t kUnattendedConfigLengthSize = 4;
inline constexpr size_t kUnattendedConfigMaxBytes = 1 + 3 * 4 + kUnattendedMaxHostKeyBytes +
                                                    kUnattendedMaxAuthorizedKeysBytes +
                                                    kUnattendedMaxSettingsBytes;

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
    bool alwaysStream = false;
    uint32_t maxFps = kSecureDesktopDefaultMaxFps;
};

struct UnattendedConfig {
    bool enabled = false;
    std::string hostKeyPem;
    std::string authorizedKeys;
    std::string settings;

    bool operator==(const UnattendedConfig&) const = default;
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
std::optional<SecureDesktopRequest> DecodeSecureDesktopRequest(uint8_t byte);

std::vector<uint8_t> EncodeUnattendedConfig(const UnattendedConfig& config);
std::optional<uint32_t> DecodeUnattendedConfigLength(std::span<const uint8_t> prefix);
std::optional<UnattendedConfig> DecodeUnattendedConfig(std::span<const uint8_t> body);

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
