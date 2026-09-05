#pragma once
#include "deskhub/media/ShareSource.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/UiSettings.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskhub::cli {

enum class Verb {
    None,
    Help,
    Version,
    Displays,
    Sources,
    Scan,
    Probe,
    Devices,
    Trust,
    Settings,
    Share,
    Shell,
    Connect,
    Send,
};

enum class PairingPolicy { Deny,
    Allow,
    Ask };

enum class DevicesAction { List,
    Forget,
    ForgetAll };

enum class TrustAction { List,
    Forget,
    ForgetAll };

enum class SettingsAction { List,
    Get,
    Set };

enum class PasscodeSource { Absent,
    Literal,
    Stdin,
    File };

enum class ExitCode {
    Ok = 0,
    Failed = 1,
    Usage = 2,
    Unreachable = 3,
    Refused = 4,
    KeyChanged = 5,
    NothingToShare = 6,
    PermissionMissing = 7,
    BindFailed = 8,
    Unsupported = 9,
    Interrupted = 130,
};

inline constexpr uint32_t kDefaultTimeoutMs = 400;
inline constexpr uint32_t kMaxTimeoutMs = 60000;
inline constexpr uint32_t kDefaultStatusIntervalMs = 5000;
inline constexpr uint32_t kMinStatusIntervalMs = 200;
inline constexpr uint32_t kMaxStatusIntervalMs = 3600000;

inline constexpr const char* kEveryDisplay = "all";

struct ConnectOptions {
    std::vector<std::string> sources{};
    bool control = true;
    std::optional<bool> audio{};
};

struct SendOptions {
    std::vector<std::string> files{};
};

struct ShareOptions {
    std::vector<std::string> displays{};
    bool screen = true;
    bool terminal = false;
    bool files = false;
    std::optional<std::string> filesDir{};
    PairingPolicy pairing = PairingPolicy::Deny;
    uint32_t statusIntervalMs = kDefaultStatusIntervalMs;
    bool status = true;

    std::optional<uint32_t> fps{};
    std::optional<uint32_t> bitrateMbps{};
    std::optional<uint32_t> maxDim{};
    std::optional<bool> allowInput{};
    std::optional<bool> audio{};
    std::optional<bool> allowNewPairings{};
    std::optional<std::string> bindIp{};
    std::optional<uint32_t> fecParity{};
    std::optional<uint32_t> fecDepth{};
    std::optional<std::string> fecArm{};
    std::optional<std::string> congestionControl{};
    std::optional<std::string> encoder{};
};

struct Command {
    Verb verb = Verb::None;
    Verb helpFor = Verb::None;
    std::string error{};

    bool json = false;
    bool quiet = false;
    bool verbose = false;

    std::string address{};
    uint16_t port = kDeskhubPort;
    bool portGiven = false;
    uint32_t timeoutMs = kDefaultTimeoutMs;

    std::optional<std::string> fecScheme{};
    std::optional<std::string> videoPath{};
    std::optional<bool> nack{};
    std::optional<uint32_t> holdFrames{};
    std::optional<uint32_t> audioDelayMs{};
    std::optional<bool> audioAdaptive{};

    PasscodeSource passcodeSource = PasscodeSource::Absent;
    std::string passcode{};
    std::optional<std::string> deviceName{};

    DevicesAction devices = DevicesAction::List;
    TrustAction trust = TrustAction::List;
    SettingsAction settings = SettingsAction::List;
    std::string target{};
    bool forget = false;
    std::string key{};
    std::string value{};

    ShareOptions share{};
    ConnectOptions connect{};
    SendOptions send{};
};

ui::UiSettings ApplyShareOptions(const Command& command, ui::UiSettings settings);

struct DisplayPick {
    std::vector<size_t> indices{};
    std::string error{};
};

DisplayPick PickByName(const std::vector<std::string>& wanted,
    const std::vector<std::string>& names);

DisplayPick PickDisplays(const std::vector<std::string>& wanted,
    const std::vector<media::ShareSource>& available);

Command ParseCommand(int argc, const char* const* argv);

const char* VerbName(Verb verb);

std::string UsageText();
std::string UsageText(Verb verb);

}
