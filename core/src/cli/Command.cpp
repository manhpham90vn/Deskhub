#include "deskhub/cli/Command.h"

#include <optional>
#include <string_view>

#include "deskhub/media/EncoderBackend.h"
#include "deskhub/media/ShareTypes.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/net/Ipv4.h"
#include "deskhub/control/CongestionControl.h"
#include "deskhub/transport/AudioJitterBuffer.h"
#include "deskhub/transport/FecScheme.h"
#include "deskhub/ui/Strings.h"

namespace deskhub::cli {

namespace {

constexpr std::string_view kProgram = "deskhub-cli";

struct Cursor {
    int argc = 0;
    const char* const* argv = nullptr;
    int index = 1;
};

bool More(const Cursor& cursor) {
    return cursor.index < cursor.argc;
}

std::string_view Look(const Cursor& cursor) {
    return cursor.argv[cursor.index];
}

std::string_view Take(Cursor& cursor) {
    return cursor.argv[cursor.index++];
}

bool IsFlagToken(std::string_view token) {
    return token.size() > 1 && token.front() == '-';
}

std::optional<uint32_t> ParseUint(std::string_view text) {
    if (text.empty()) return std::nullopt;
    uint64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + uint64_t(c - '0');
        if (value > 0xFFFFFFFFull) return std::nullopt;
    }
    return uint32_t(value);
}

char FoldAscii(char c) {
    return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
}

bool ContainsFolded(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return false;
    if (needle.size() > haystack.size()) return false;
    for (size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
        size_t i = 0;
        while (i < needle.size() && FoldAscii(haystack[start + i]) == FoldAscii(needle[i])) ++i;
        if (i == needle.size()) return true;
    }
    return false;
}

std::string Quoted(std::string_view text) {
    return "\"" + std::string(text) + "\"";
}

std::string UnknownOption(std::string_view name, Verb verb) {
    std::string message = "unknown option " + Quoted(name);
    if (verb != Verb::None) message += " for '" + std::string(VerbName(verb)) + "'";
    return message;
}

std::string MissingValue(std::string_view name) {
    return std::string(name) + " needs a value";
}

std::string KnownFecSchemes() {
    std::string list;
    for (std::string_view name : deskhub::FecSchemeNames()) {
        if (!list.empty()) list += ", ";
        list += name;
    }
    return list;
}

std::string KnownCongestionControls() {
    std::string list;
    for (std::string_view name : deskhub::CongestionControlNames()) {
        if (!list.empty()) list += ", ";
        list += name;
    }
    return list;
}

std::string KnownEncoders() {
    std::string list;
    for (std::string_view name : media::EncoderBackendNames()) {
        if (!list.empty()) list += ", ";
        list += name;
    }
    return list;
}

std::string KnownVideoPaths() {
    return std::string(media::kVideoPathQuicDatagram) + ", " + std::string(media::kVideoPathRawUdp);
}

std::string BadValue(std::string_view name, std::string_view value) {
    return std::string(name) + " does not accept " + Quoted(value);
}

std::string NeedsAddress(Verb verb) {
    return std::string(VerbName(verb)) + " needs a host address, for example " + std::string(kProgram) + " " + VerbName(verb) + " 192.168.1.10";
}

std::string NeedsAction(Verb verb, std::string_view actions) {
    return std::string(VerbName(verb)) + " needs one of: " + std::string(actions);
}

enum class FlagResult { Handled,
    Unknown,
    Failed };

struct Flag {
    std::string_view name{};
    std::string_view inlineValue{};
    bool hasInlineValue = false;
};

Flag SplitFlag(std::string_view token) {
    Flag flag;
    const size_t equals = token.find('=');
    if (equals == std::string_view::npos) {
        flag.name = token;
        return flag;
    }
    flag.name = token.substr(0, equals);
    flag.inlineValue = token.substr(equals + 1);
    flag.hasInlineValue = true;
    return flag;
}

bool ValueOf(const Flag& flag, Cursor& cursor, std::string& out, std::string& error) {
    if (flag.hasInlineValue) {
        if (flag.inlineValue.empty()) {
            error = MissingValue(flag.name);
            return false;
        }
        out = std::string(flag.inlineValue);
        return true;
    }
    if (!More(cursor)) {
        error = MissingValue(flag.name);
        return false;
    }
    out = std::string(Take(cursor));
    return true;
}

FlagResult ApplyGlobalFlag(Command& command, const Flag& flag) {
    if (flag.hasInlineValue) return FlagResult::Unknown;
    if (flag.name == "--json") {
        command.json = true;
        return FlagResult::Handled;
    }
    if (flag.name == "--quiet" || flag.name == "-q") {
        command.quiet = true;
        return FlagResult::Handled;
    }
    if (flag.name == "--verbose" || flag.name == "-v") {
        command.verbose = true;
        return FlagResult::Handled;
    }
    return FlagResult::Unknown;
}

FlagResult ApplyTimeoutFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name != "--timeout") return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;
    const std::optional<uint32_t> value = ParseUint(text);
    if (!value || *value == 0 || *value > kMaxTimeoutMs) {
        command.error = BadValue(flag.name, text);
        return FlagResult::Failed;
    }
    command.timeoutMs = *value;
    return FlagResult::Handled;
}

FlagResult ApplyPortFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name != "--port") return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;
    const std::optional<uint32_t> value = ParseUint(text);
    if (!value || *value == 0 || *value > 65535) {
        command.error = BadValue(flag.name, text);
        return FlagResult::Failed;
    }
    command.port = uint16_t(*value);
    command.portGiven = true;
    return FlagResult::Handled;
}

FlagResult ApplyBoundedFlag(Command& command, const Flag& flag, Cursor& cursor,
    std::string_view name, uint32_t low, uint32_t high, std::optional<uint32_t>& out) {
    if (flag.name != name) return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;
    const std::optional<uint32_t> value = ParseUint(text);
    if (!value || *value < low || *value > high) {
        command.error = BadValue(flag.name, text);
        return FlagResult::Failed;
    }
    out = value;
    return FlagResult::Handled;
}

FlagResult ApplyTextFlag(Command& command, const Flag& flag, Cursor& cursor,
    std::string_view name, std::optional<std::string>& out) {
    if (flag.name != name) return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;
    out = text;
    return FlagResult::Handled;
}

FlagResult ApplyPasscodeFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name != "--passcode") return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;

    if (text == "-") {
        command.passcodeSource = PasscodeSource::Stdin;
        command.passcode.clear();
        return FlagResult::Handled;
    }
    if (text.front() == '@') {
        const std::string path = text.substr(1);
        if (path.empty()) {
            command.error = BadValue(flag.name, text);
            return FlagResult::Failed;
        }
        command.passcodeSource = PasscodeSource::File;
        command.passcode = path;
        return FlagResult::Handled;
    }
    if (!IsValidPasscode(text)) {
        command.error = std::string(ui::kPasscodeInvalid);
        return FlagResult::Failed;
    }
    command.passcodeSource = PasscodeSource::Literal;
    command.passcode = text;
    return FlagResult::Handled;
}

bool TakeAddress(Command& command, std::string_view token) {
    std::string host;
    uint16_t port = command.port;
    if (!ui::SplitHostPort(ui::TrimAscii(token), host, port)) {
        command.error = ui::InvalidAddressLine(token) + " " + ui::InvalidAddressHint();
        return false;
    }
    command.port = port;
    command.address = host + ":" + std::to_string(port);
    return true;
}

Verb VerbOf(std::string_view token) {
    if (token == "help") return Verb::Help;
    if (token == "version") return Verb::Version;
    if (token == "displays") return Verb::Displays;
    if (token == "sources") return Verb::Sources;
    if (token == "scan") return Verb::Scan;
    if (token == "probe") return Verb::Probe;
    if (token == "devices") return Verb::Devices;
    if (token == "trust") return Verb::Trust;
    if (token == "settings") return Verb::Settings;
    if (token == "share") return Verb::Share;
    if (token == "shell") return Verb::Shell;
    if (token == "connect") return Verb::Connect;
    if (token == "send") return Verb::Send;
    return Verb::None;
}

bool WantsHelp(const Flag& flag) {
    return !flag.hasInlineValue && (flag.name == "--help" || flag.name == "-h");
}

bool WantsVersion(const Flag& flag) {
    return !flag.hasInlineValue && (flag.name == "--version" || flag.name == "-V");
}

void ParseNoArgVerb(Command& command, Cursor& cursor) {
    const Verb verb = command.verb;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            command.error = std::string(VerbName(command.verb)) + " takes no arguments, but got " + Quoted(token);
            return;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = command.verb;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;
        if (verb == Verb::Displays && !flag.hasInlineValue && flag.name == "--forget") {
            command.forget = true;
            continue;
        }
        command.error = UnknownOption(flag.name, command.verb);
        return;
    }
}

void ParseScan(Command& command, Cursor& cursor) {
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            command.error = std::string("scan takes no arguments, but got ") + Quoted(token);
            return;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Scan;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;

        const FlagResult result = ApplyPortFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        command.error = UnknownOption(flag.name, Verb::Scan);
        return;
    }
}

void ParseAddressVerb(Command& command, Cursor& cursor, bool wantsPasscode, bool wantsTimeout) {
    bool haveAddress = false;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            if (haveAddress) {
                command.error = std::string(VerbName(command.verb)) + " takes one address, but got " + Quoted(token) + " as well";
                return;
            }
            if (!TakeAddress(command, token)) return;
            haveAddress = true;
            continue;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = command.verb;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;

        if (wantsTimeout) {
            const FlagResult result = ApplyTimeoutFlag(command, flag, cursor);
            if (result == FlagResult::Failed) return;
            if (result == FlagResult::Handled) continue;
        }

        if (wantsPasscode) {
            FlagResult result = ApplyPasscodeFlag(command, flag, cursor);
            if (result == FlagResult::Failed) return;
            if (result == FlagResult::Handled) continue;

            result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
            if (result == FlagResult::Failed) return;
            if (result == FlagResult::Handled) continue;
        }

        command.error = UnknownOption(flag.name, command.verb);
        return;
    }
    if (!haveAddress) command.error = NeedsAddress(command.verb);
}

bool TakeForgetTarget(Command& command, Cursor& cursor, bool& forgetAll) {
    if (!More(cursor) || IsFlagToken(Look(cursor))) {
        command.error = std::string(VerbName(command.verb)) + " forget needs a target, or 'all'";
        return false;
    }
    const std::string_view token = Take(cursor);
    if (token == "all") {
        forgetAll = true;
        return true;
    }
    forgetAll = false;
    command.target = std::string(token);
    return true;
}

void ParseDevices(Command& command, Cursor& cursor) {
    if (!More(cursor)) {
        command.devices = DevicesAction::List;
        return;
    }
    if (IsFlagToken(Look(cursor))) {
        command.devices = DevicesAction::List;
        ParseNoArgVerb(command, cursor);
        return;
    }
    const std::string_view action = Take(cursor);
    if (action == "list") {
        command.devices = DevicesAction::List;
    } else if (action == "forget") {
        bool forgetAll = false;
        if (!TakeForgetTarget(command, cursor, forgetAll)) return;
        command.devices = forgetAll ? DevicesAction::ForgetAll : DevicesAction::Forget;
    } else {
        command.error = NeedsAction(Verb::Devices, "list, forget FINGERPRINT, forget all");
        return;
    }
    ParseNoArgVerb(command, cursor);
}

void ParseTrust(Command& command, Cursor& cursor) {
    if (!More(cursor)) {
        command.trust = TrustAction::List;
        return;
    }
    if (IsFlagToken(Look(cursor))) {
        command.trust = TrustAction::List;
        ParseNoArgVerb(command, cursor);
        return;
    }
    const std::string_view action = Take(cursor);
    if (action == "list") {
        command.trust = TrustAction::List;
    } else if (action == "forget") {
        bool forgetAll = false;
        if (!TakeForgetTarget(command, cursor, forgetAll)) return;
        command.trust = forgetAll ? TrustAction::ForgetAll : TrustAction::Forget;
    } else {
        command.error = NeedsAction(Verb::Trust, "list, forget ADDRESS, forget all");
        return;
    }
    ParseNoArgVerb(command, cursor);
}

void ParseSettings(Command& command, Cursor& cursor) {
    if (!More(cursor)) {
        command.settings = SettingsAction::List;
        return;
    }
    if (IsFlagToken(Look(cursor))) {
        command.settings = SettingsAction::List;
        ParseNoArgVerb(command, cursor);
        return;
    }
    const std::string_view action = Take(cursor);
    if (action == "list") {
        command.settings = SettingsAction::List;
    } else if (action == "get") {
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = "settings get needs a key, for example settings get fps";
            return;
        }
        command.settings = SettingsAction::Get;
        command.key = std::string(Take(cursor));
    } else if (action == "set") {
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = "settings set needs KEY=VALUE, for example settings set fps=90";
            return;
        }
        const std::string pair(Take(cursor));
        const size_t equals = pair.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 == pair.size()) {
            command.error = "settings set needs KEY=VALUE, but got " + Quoted(pair);
            return;
        }
        command.settings = SettingsAction::Set;
        command.key = pair.substr(0, equals);
        command.value = pair.substr(equals + 1);
    } else {
        command.error = NeedsAction(Verb::Settings, "list, get KEY, set KEY=VALUE");
        return;
    }
    ParseNoArgVerb(command, cursor);
}

FlagResult ApplyPairingFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name != "--pairing") return FlagResult::Unknown;
    std::string text;
    if (!ValueOf(flag, cursor, text, command.error)) return FlagResult::Failed;
    if (text == "deny") {
        command.share.pairing = PairingPolicy::Deny;
    } else if (text == "allow") {
        command.share.pairing = PairingPolicy::Allow;
    } else if (text == "ask") {
        command.share.pairing = PairingPolicy::Ask;
    } else {
        command.error = BadValue(flag.name, text);
        return FlagResult::Failed;
    }
    return FlagResult::Handled;
}

void ParseSend(Command& command, Cursor& cursor) {
    bool haveAddress = false;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            if (!haveAddress) {
                if (!TakeAddress(command, token)) return;
                haveAddress = true;
                continue;
            }
            if (command.send.files.size() >= kMaxTransferFiles) {
                command.error = "send carries at most " + std::to_string(kMaxTransferFiles) +
                                " files at a time";
                return;
            }
            command.send.files.emplace_back(token);
            continue;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Send;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;

        FlagResult result = ApplyPasscodeFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        command.error = UnknownOption(flag.name, Verb::Send);
        return;
    }
    if (!haveAddress) {
        command.error = NeedsAddress(Verb::Send);
        return;
    }
    if (command.send.files.empty()) command.error = "send needs at least one file to send";
}

void ParseShare(Command& command, Cursor& cursor) {
    ShareOptions& share = command.share;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            command.error = std::string("share names its displays with --display, so it does not take ") + Quoted(token);
            return;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Share;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;

        if (!flag.hasInlineValue) {
            if (flag.name == "--terminal") {
                share.terminal = true;
                continue;
            }
            if (flag.name == "--files") {
                share.files = true;
                continue;
            }
            if (flag.name == "--no-screen") {
                share.screen = false;
                continue;
            }
            if (flag.name == "--no-input") {
                share.allowInput = false;
                continue;
            }
            if (flag.name == "--audio") {
                share.audio = true;
                continue;
            }
            if (flag.name == "--no-audio") {
                share.audio = false;
                continue;
            }
            if (flag.name == "--no-new-pairings") {
                share.allowNewPairings = false;
                continue;
            }
            if (flag.name == "--no-status") {
                share.status = false;
                continue;
            }
        }

        if (flag.name == "--display") {
            std::string text;
            if (!ValueOf(flag, cursor, text, command.error)) return;
            share.displays.push_back(text);
            continue;
        }

        FlagResult result = ApplyPortFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyPasscodeFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyPairingFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyBoundedFlag(command, flag, cursor, "--fps", 1, ui::kMaxSettingsFps, share.fps);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyBoundedFlag(command, flag, cursor, "--bitrate", 1, ui::kMaxSettingsBitrateMbps, share.bitrateMbps);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyBoundedFlag(command, flag, cursor, "--max-dim", 1, ui::kMaxSettingsDim, share.maxDim);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        std::optional<uint32_t> interval;
        result = ApplyBoundedFlag(command, flag, cursor, "--status-interval", kMinStatusIntervalMs, kMaxStatusIntervalMs, interval);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) {
            if (interval) share.statusIntervalMs = *interval;
            continue;
        }

        result = ApplyTextFlag(command, flag, cursor, "--bind", share.bindIp);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--files-dir", share.filesDir);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--fec", command.fecScheme);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--cc", share.congestionControl);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--encoder", share.encoder);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyBoundedFlag(command, flag, cursor, "--fec-parity", 1,
            uint32_t(kMaxFecRecoveryPerGroup), share.fecParity);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyBoundedFlag(command, flag, cursor, "--fec-depth", 1,
            uint32_t(kMaxSignalledFecGroups), share.fecDepth);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        if (flag.name == "--fec-arm") {
            std::string text;
            if (!ValueOf(flag, cursor, text, command.error)) return;
            if (text != "always" && text != "policy" && text != "never") {
                command.error = BadValue("--fec-arm", text) +
                                " - always holds parity on the wire for a measurement, policy lets loss "
                                "decide as a real session does, never turns FEC off so NACK is the only "
                                "repair left";
                return;
            }
            share.fecArm = text;
            continue;
        }

        result = ApplyTextFlag(command, flag, cursor, "--video-path", command.videoPath);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        command.error = UnknownOption(flag.name, Verb::Share);
        return;
    }

    if (!share.screen && !share.terminal && !share.files)
        command.error =
            "--no-screen leaves nothing to share - add --terminal or --files, or drop "
            "--no-screen";
    if (!share.screen && !share.displays.empty())
        command.error = "--display and --no-screen ask for opposite things";
    if (share.bindIp && !share.bindIp->empty() && !ParseIPv4(*share.bindIp))
        command.error = BadValue("--bind", *share.bindIp);
}

void ParseConnect(Command& command, Cursor& cursor) {
    bool haveAddress = false;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            if (haveAddress) {
                command.error = std::string("connect takes one address, but got ") + Quoted(token) + " as well";
                return;
            }
            if (!TakeAddress(command, token)) return;
            haveAddress = true;
            continue;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Connect;
            command.verb = Verb::Help;
            return;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;

        if (!flag.hasInlineValue) {
            if (flag.name == "--view-only") {
                command.connect.control = false;
                continue;
            }
            if (flag.name == "--audio") {
                command.connect.audio = true;
                continue;
            }
            if (flag.name == "--no-audio") {
                command.connect.audio = false;
                continue;
            }
        }

        if (flag.name == "--source") {
            std::string text;
            if (!ValueOf(flag, cursor, text, command.error)) return;
            command.connect.sources.push_back(text);
            continue;
        }

        FlagResult result = ApplyPasscodeFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--fec", command.fecScheme);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        if (flag.name == "--nack" || flag.name == "--no-nack") {
            command.nack = flag.name == "--nack";
            continue;
        }

        result = ApplyBoundedFlag(command, flag, cursor, "--hold", 2,
            uint32_t(kMaxSignalledFecGroups), command.holdFrames);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        if (flag.name == "--audio-adaptive" || flag.name == "--no-audio-adaptive") {
            command.audioAdaptive = flag.name == "--audio-adaptive";
            continue;
        }

        result = ApplyBoundedFlag(command, flag, cursor, "--audio-delay", kAudioFrameMs,
            kMaxAudioDelayMs, command.audioDelayMs);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyTextFlag(command, flag, cursor, "--video-path", command.videoPath);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        command.error = UnknownOption(flag.name, Verb::Connect);
        return;
    }
    if (!haveAddress) command.error = NeedsAddress(Verb::Connect);
}

void ParseHelp(Command& command, Cursor& cursor) {
    command.verb = Verb::Help;
    if (!More(cursor) || IsFlagToken(Look(cursor))) return;
    const std::string_view token = Take(cursor);
    const Verb topic = VerbOf(token);
    if (topic == Verb::None) {
        command.error = "unknown command " + Quoted(token) + " - run '" + std::string(kProgram) + " help' for the command list";
        return;
    }
    command.helpFor = topic;
}

}

const char* VerbName(Verb verb) {
    switch (verb) {
        case Verb::Help: return "help";
        case Verb::Version: return "version";
        case Verb::Displays: return "displays";
        case Verb::Sources: return "sources";
        case Verb::Scan: return "scan";
        case Verb::Probe: return "probe";
        case Verb::Devices: return "devices";
        case Verb::Trust: return "trust";
        case Verb::Settings: return "settings";
        case Verb::Share: return "share";
        case Verb::Shell: return "shell";
        case Verb::Connect: return "connect";
        case Verb::Send: return "send";
        case Verb::None: break;
    }
    return "";
}

Command ParseCommand(int argc, const char* const* argv) {
    Command command;
    Cursor cursor{argc, argv, 1};

    while (More(cursor) && IsFlagToken(Look(cursor))) {
        const Flag flag = SplitFlag(Take(cursor));
        if (WantsHelp(flag)) {
            command.verb = Verb::Help;
            return command;
        }
        if (WantsVersion(flag)) {
            command.verb = Verb::Version;
            return command;
        }
        if (ApplyGlobalFlag(command, flag) == FlagResult::Handled) continue;
        command.error = UnknownOption(flag.name, Verb::None);
        return command;
    }

    if (!More(cursor)) {
        command.verb = Verb::Help;
        return command;
    }

    const std::string_view token = Take(cursor);
    command.verb = VerbOf(token);
    if (command.verb == Verb::None) {
        command.error = "unknown command " + Quoted(token) + " - run '" + std::string(kProgram) + " help' for the command list";
        return command;
    }

    switch (command.verb) {
        case Verb::Help: ParseHelp(command, cursor); break;
        case Verb::Version:
        case Verb::Displays: ParseNoArgVerb(command, cursor); break;
        case Verb::Scan: ParseScan(command, cursor); break;
        case Verb::Sources: ParseAddressVerb(command, cursor, true, false); break;
        case Verb::Probe: ParseAddressVerb(command, cursor, false, true); break;
        case Verb::Devices: ParseDevices(command, cursor); break;
        case Verb::Trust: ParseTrust(command, cursor); break;
        case Verb::Settings: ParseSettings(command, cursor); break;
        case Verb::Share: ParseShare(command, cursor); break;
        case Verb::Shell: ParseAddressVerb(command, cursor, true, false); break;
        case Verb::Connect: ParseConnect(command, cursor); break;
        case Verb::Send: ParseSend(command, cursor); break;
        case Verb::None: break;
    }
    if (command.error.empty() && command.videoPath && !media::IsVideoPathName(*command.videoPath))
        command.error = BadValue("--video-path", *command.videoPath) + " - it takes " +
                        KnownVideoPaths() +
                        ", and both ends of a session must be given the same one";
    if (command.error.empty() && command.fecScheme && !IsFecSchemeName(*command.fecScheme))
        command.error = BadValue("--fec", *command.fecScheme) + " - built in are " +
                        KnownFecSchemes() +
                        ", and both ends of a session must be given the same one";

    if (command.error.empty() && command.share.congestionControl &&
        !IsCongestionControlName(*command.share.congestionControl))
        command.error = BadValue("--cc", *command.share.congestionControl) + " - built in are " +
                        KnownCongestionControls() +
                        "; the host alone decides this, so the viewer is told nothing";

    if (command.error.empty() && command.share.encoder &&
        !media::IsEncoderBackendName(*command.share.encoder))
        command.error = BadValue("--encoder", *command.share.encoder) + " - the backends are " +
                        KnownEncoders() +
                        ", and a host that has no such backend refuses to share rather than "
                        "measuring a different one";
    return command;
}

DisplayPick PickDisplays(const std::vector<std::string>& wanted,
    const std::vector<media::ShareSource>& available) {
    std::vector<std::string> names;
    names.reserve(available.size());
    for (size_t i = 0; i < available.size(); ++i)
        names.push_back(media::SourceName(available[i].name, uint8_t(i)));
    return PickByName(wanted, names);
}

DisplayPick PickByName(const std::vector<std::string>& wanted,
    const std::vector<std::string>& names) {
    DisplayPick pick;
    const std::vector<std::string>& available = names;
    if (available.empty()) {
        pick.error = std::string(ui::kNoDisplayFound);
        return pick;
    }

    std::vector<bool> chosen(available.size(), false);
    bool everything = wanted.empty();
    for (const std::string& want : wanted)
        if (want == kEveryDisplay) everything = true;

    if (everything) {
        for (size_t i = 0; i < available.size(); ++i) pick.indices.push_back(i);
        return pick;
    }

    for (const std::string& want : wanted) {
        const std::optional<uint32_t> index = ParseUint(want);
        if (index) {
            if (*index >= available.size()) {
                pick.error = "there is no display " + want + " - run '" + std::string(kProgram) + " displays' to see them";
                return pick;
            }
            chosen[*index] = true;
            continue;
        }

        size_t match = 0;
        size_t matches = 0;
        for (size_t i = 0; i < available.size(); ++i) {
            if (!ContainsFolded(available[i], want)) continue;
            match = i;
            ++matches;
        }
        if (matches == 0) {
            pick.error = "no display is called " + Quoted(want) + " - run '" + std::string(kProgram) + " displays' to see them";
            return pick;
        }
        if (matches > 1) {
            pick.error = Quoted(want) + " matches more than one display - name it exactly, or use its id";
            return pick;
        }
        chosen[match] = true;
    }

    for (size_t i = 0; i < available.size(); ++i)
        if (chosen[i]) pick.indices.push_back(i);
    return pick;
}

ui::UiSettings ApplyShareOptions(const Command& command, ui::UiSettings settings) {
    const ShareOptions& share = command.share;
    if (command.portGiven) settings.port = command.port;
    if (share.fps) settings.fps = *share.fps;
    if (share.bitrateMbps) settings.bitrateMbps = *share.bitrateMbps;
    if (share.maxDim) settings.maxDim = *share.maxDim;
    if (share.allowInput) settings.allowInput = *share.allowInput;
    if (share.audio) settings.shareAudio = *share.audio;
    if (share.allowNewPairings) settings.allowNewPairings = *share.allowNewPairings;
    if (command.deviceName) settings.deviceName = ui::TruncateDeviceName(*command.deviceName);
    if (share.bindIp) settings.bindIp = *share.bindIp;
    if (share.filesDir) settings.transferDir = ui::TruncateSettingsPath(*share.filesDir);
    return settings;
}

std::string UsageText() {
    return std::string(kProgram) +
           " - Deskhub from the command line\n"
           "\n"
           "Usage:\n"
           "  " +
           std::string(kProgram) +
           " [--json] [--quiet] [--verbose] <command> [arguments]\n"
           "\n"
           "Commands:\n"
           "  share               share this machine on the network\n"
           "  connect ADDRESS     watch and drive a host, in a window of its own\n"
           "  shell ADDRESS       open a shell on a host, right here in this terminal\n"
           "  send ADDRESS FILE   send files to a host that takes them\n"
           "  displays            the displays this machine can share\n"
           "  scan                look for machines sharing on this network\n"
           "  sources ADDRESS     ask a host what it is sharing\n"
           "  probe ADDRESS       measure the round trip to a host\n"
           "  devices             machines allowed to connect to this one\n"
           "  trust               hosts this machine has decided to trust\n"
           "  settings            the settings the desktop app also uses\n"
           "  version             print the version\n"
           "  help [COMMAND]      print this list, or one command in detail\n"
           "\n"
           "Run '" +
           std::string(kProgram) + " help COMMAND' for the flags a command takes.\n";
}

std::string UsageText(Verb verb) {
    const std::string program(kProgram);
    switch (verb) {
        case Verb::Displays:
            return "Usage: " + program +
                   " displays [--forget]\n"
                   "\n"
                   "List the displays this machine can share, each with the id to pass to --display.\n"
                   "\n"
                   "  --forget    drop the screen choice this machine saved, so the next listing\n"
                   "              asks again. Use it when sharing reports that the compositor\n"
                   "              sent no frame - the saved choice has gone stale.\n";
        case Verb::Scan:
            return "Usage: " + program +
                   " scan [--port PORT]\n"
                   "\n"
                   "Look for machines sharing on this network. Every address on every real\n"
                   "adapter is tried, the same way the desktop app scans.\n"
                   "\n"
                   "  --port PORT     the UDP port to look on (default " +
                   std::to_string(kDeskhubPort) + ")\n";
        case Verb::Sources:
            return "Usage: " + program +
                   " sources ADDRESS[:PORT] [--passcode VALUE]\n"
                   "\n"
                   "Ask a host what it is sharing.\n"
                   "\n"
                   "  --passcode VALUE  the host's passcode. '-' reads one line from stdin, '@FILE'\n"
                   "                    reads it from a file, or set DESKHUB_PASSCODE instead\n";
        case Verb::Probe:
            return "Usage: " + program +
                   " probe ADDRESS[:PORT] [--timeout MS]\n"
                   "\n"
                   "Measure the round trip to a host, and report whether it answers at all.\n"
                   "\n"
                   "  --timeout MS    how long to wait for the reply (default " +
                   std::to_string(kDefaultTimeoutMs) + ")\n";
        case Verb::Devices:
            return "Usage: " + program +
                   " devices [list]\n"
                   "       " +
                   program +
                   " devices forget FINGERPRINT\n"
                   "       " +
                   program +
                   " devices forget all\n"
                   "\n"
                   "Machines that are allowed to connect to this one. Forgetting a machine means it\n"
                   "has to pair again.\n";
        case Verb::Trust:
            return "Usage: " + program +
                   " trust [list]\n"
                   "       " +
                   program +
                   " trust forget ADDRESS\n"
                   "       " +
                   program +
                   " trust forget all\n"
                   "\n"
                   "Hosts this machine has decided to trust, by key. Forgetting a host means its key\n"
                   "is accepted afresh on the next connection.\n";
        case Verb::Settings:
            return "Usage: " + program +
                   " settings [list]\n"
                   "       " +
                   program +
                   " settings get KEY\n"
                   "       " +
                   program +
                   " settings set KEY=VALUE\n"
                   "\n"
                   "The same settings the desktop app reads and writes.\n";
        case Verb::Connect:
            return "Usage: " + program +
                   " connect ADDRESS[:PORT] [--source ID|NAME|all] [flags]\n"
                   "\n"
                   "Open a window on a host's screen and drive it. Without --source the host's\n"
                   "first screen is shown; 'all' opens one window each.\n"
                   "\n"
                   "  --source ID|NAME|all  which of the host's screens to watch\n"
                   "  --view-only           watch without typing or clicking\n"
                   "  --audio / --no-audio  play the host's sound, or do not\n"
                   "  --passcode VALUE      the host's passcode, the same way sources takes it\n"
                   "  --name NAME           what the host sees this machine called\n"
                   "  --fec NAME            which FEC scheme the host is sending parity\n"
                   "                        with; for measurement, and it must match\n"
                   "  --video-path NAME     quic-datagram or raw-udp; for measurement, and\n"
                   "                        it must match what the host was started with\n"
                   "  --nack / --no-nack    ask the host to resend lost packets, or do not;\n"
                   "                        for measurement\n"
                   "  --hold N              how many newer complete frames may pass before an\n"
                   "                        incomplete one is dropped; for measurement\n"
                   "  --audio-delay MS      how much audio to buffer before playing; for\n"
                   "                        measurement\n"
                   "  --audio-adaptive      let the buffer chase jitter instead of holding a\n"
                   "                        fixed target; for measurement\n"
                   "\n"
                   "F9 locks the pointer to the window, Escape lets it go again.\n";
        case Verb::Shell:
            return "Usage: " + program +
                   " shell ADDRESS[:PORT] [--passcode VALUE] [--name NAME]\n"
                   "\n"
                   "Open a shell on a host and drive it from this terminal. Everything the\n"
                   "shell prints is written straight through, so your own terminal draws it.\n"
                   "\n"
                   "  --passcode VALUE  the host's passcode, the same way sources takes it\n"
                   "  --name NAME       what the host sees this machine called\n";
        case Verb::Send:
            return "Usage: " + program +
                   " send ADDRESS[:PORT] FILE [FILE...] [--passcode VALUE] [--name NAME]\n"
                   "\n"
                   "Send files to a host that was started with --files. The host stores them in\n"
                   "its transfer folder without asking, so it only takes files from machines it\n"
                   "has admitted. Names are reduced to a plain file name before they are stored,\n"
                   "and nothing already there is ever overwritten.\n"
                   "\n"
                   "  --passcode VALUE  the host's passcode, the same way sources takes it\n"
                   "  --name NAME       what the host sees this machine called\n"
                   "\n"
                   "At most " +
                   std::to_string(kMaxTransferFiles) +
                   " files travel in one batch. It exits 4 if the host refuses them.\n";
        case Verb::Share:
            return "Usage: " + program +
                   " share [--display ID|NAME|all]... [--terminal] [flags]\n"
                   "\n"
                   "Share this machine on the network and keep running until interrupted.\n"
                   "Without --display every display is shared.\n"
                   "\n"
                   "  --display ID|NAME|all  which display to share, once per display\n"
                   "  --no-screen            share no display at all; needs --terminal or\n"
                   "                         --files\n"
                   "  --terminal             share a shell as well\n"
                   "  --files                take files viewers send, into the transfer folder\n"
                   "  --files-dir PATH       where files viewers send are stored\n"
                   "  --no-input             viewers watch but cannot type or click\n"
                   "  --audio / --no-audio   send this machine's sound, or do not\n"
                   "  --fps N                frames per second\n"
                   "  --bitrate MBPS         how much bandwidth the video may use\n"
                   "  --max-dim PX           cap the longest side of the picture\n"
                   "  --port PORT            the UDP port to share on\n"
                   "  --bind IP              share on one network only\n"
                   "  --passcode VALUE       the passcode viewers must type\n"
                   "  --name NAME            what viewers see this machine called\n"
                   "  --no-new-pairings      only machines that already paired may connect\n"
                   "  --pairing deny|allow|ask  what to do when a new machine asks in\n"
                   "                         (deny by default, ask needs a terminal)\n"
                   "  --status-interval MS   how often to print the status line\n"
                   "  --no-status            print nothing until it stops\n"
                   "  --cc NAME              which congestion control decides the bitrate;\n"
                   "                         the host alone chooses it, for measurement\n"
                   "  --encoder NAME         which encoder backend to measure, instead of the\n"
                   "                         first one that starts; a host without it refuses\n"
                   "                         to share rather than measuring another\n"
                   "  --fec NAME             which FEC scheme to send parity with; for\n"
                   "                         measurement, and the viewer must be given the\n"
                   "                         same one\n"
                   "  --fec-parity N         parity packets per group, pinned so the loss-driven\n"
                   "                         policy cannot move it mid-measurement\n"
                   "  --fec-depth N          spread a frame over N groups instead of deriving\n"
                   "                         the count; buys rescue and costs overhead\n"
                   "  --fec-arm MODE         always holds parity on the wire, policy lets loss\n"
                   "                         decide, never turns FEC off so NACK is the only\n"
                   "                         repair left\n"
                   "  --video-path NAME      quic-datagram or raw-udp; for measuring what\n"
                   "                         quiche's own congestion control costs the video,\n"
                   "                         and the viewer must be given the same one\n"
                   "\n"
                   "Anything not named here comes from the settings the desktop app uses.\n";
        case Verb::Version:
            return "Usage: " + program +
                   " version\n"
                   "\n"
                   "Print the version this build came from.\n";
        case Verb::Help:
        case Verb::None: break;
    }
    return UsageText();
}

}
