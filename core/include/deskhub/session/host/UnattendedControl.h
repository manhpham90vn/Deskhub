#pragma once
#include "deskhub/session/host/SecureDesktopWire.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

enum class UnattendedAction : uint8_t {
    NeedMore,
    Agent,
    Takeover,
    Configure,
    Status,
    Refuse,
};

class UnattendedControlServer {
public:
    UnattendedAction Feed(std::span<const uint8_t> bytes);

    const UnattendedConfig& config() const {
        return config_;
    }

private:
    enum class Stage : uint8_t {
        Request,
        ConfigLength,
        ConfigBody,
        Served,
        Refused,
    };

    UnattendedAction Take(uint8_t byte);
    UnattendedAction StartRequest(uint8_t byte);
    UnattendedAction FinishConfigLength();
    UnattendedAction FinishConfigBody();
    UnattendedAction Refuse();

    Stage stage_ = Stage::Request;
    std::vector<uint8_t> pending_;
    uint32_t bodyLength_ = 0;
    UnattendedConfig config_;
};

enum class UnattendedPermission : uint8_t {
    Unknown = 0,
    Denied = 1,
    Granted = 2,
};

struct UnattendedStatus {
    UnattendedPermission screen = UnattendedPermission::Unknown;
    UnattendedPermission input = UnattendedPermission::Unknown;

    bool operator==(const UnattendedStatus&) const = default;
};

inline constexpr size_t kUnattendedStatusSize = 2;

std::array<uint8_t, kUnattendedStatusSize> EncodeUnattendedStatus(const UnattendedStatus& status);
std::optional<UnattendedStatus> DecodeUnattendedStatus(std::span<const uint8_t> bytes);
std::string SerializeUnattendedStatus(const UnattendedStatus& status);
UnattendedStatus ParseUnattendedStatus(std::string_view text);

}
