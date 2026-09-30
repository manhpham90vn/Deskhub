#include "deskhub/cli/Command.h"

#include <optional>
#include <string_view>

#include "deskhub/media/SourceLabel.h"
#include "deskhub/net/Ipv4.h"
#include "deskhub/net/PairingInvite.h"
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

FlagResult ApplyGlobalFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name == "--config-dir") {
        std::string value;
        if (!ValueOf(flag, cursor, value, command.error)) return FlagResult::Failed;
        command.configDir = value;
        return FlagResult::Handled;
    }
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

FlagResult ApplyConnectionFlag(Command& command, const Flag& flag, Cursor& cursor) {
    if (flag.name == "--accept-new-host-key" && !flag.hasInlineValue) {
        command.acceptNewHostKey = true;
        return FlagResult::Handled;
    }
    return ApplyBoundedFlag(command, flag, cursor, "--approval-wait", 1, kMaxApprovalWaitSeconds,
        command.approvalWaitSeconds);
}

bool TakeAddress(Command& command, std::string_view token) {
    std::string host;
    uint16_t port = command.port;
    if (IsPairingInvite(token)) {
        if (!ParsePairingInvite(token)) {
            command.error = ui::kInviteInvalid;
            return false;
        }
        command.pairingInvite = std::string(ui::TrimAscii(token));
        return true;
    }
    if ((command.verb == Verb::Connect || command.verb == Verb::Sources ||
            command.verb == Verb::Shell || command.verb == Verb::Send) &&
        !token.empty() && token.find(':') == std::string_view::npos &&
        token.find('.') == std::string_view::npos &&
        token.find('/') == std::string_view::npos) {
        command.profileAlias = std::string(token);
        return true;
    }
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
    if (token == "devices") return Verb::Devices;
    if (token == "trust") return Verb::Trust;
    if (token == "host" || token == "host-key") return Verb::Host;
    if (token == "key" || token == "access") return Verb::Devices;
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;
        if (verb == Verb::Displays && !flag.hasInlineValue && flag.name == "--forget") {
            command.forget = true;
            continue;
        }
        command.error = UnknownOption(flag.name, command.verb);
        return;
    }
}

void ParseAddressVerb(Command& command, Cursor& cursor) {
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;

        const FlagResult connection = ApplyConnectionFlag(command, flag, cursor);
        if (connection == FlagResult::Failed) return;
        if (connection == FlagResult::Handled) continue;

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
    } else if (action == "public") {
        command.devices = DevicesAction::Public;
    } else if (action == "add") {
        command.devices = DevicesAction::Add;
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = NeedsAction(Verb::Devices, "add PUBLIC_KEY or add -");
            return;
        }
        command.target = std::string(Take(cursor));
    } else if (action == "forget") {
        bool forgetAll = false;
        if (!TakeForgetTarget(command, cursor, forgetAll)) return;
        command.devices = forgetAll ? DevicesAction::ForgetAll : DevicesAction::Forget;
    } else {
        command.error = NeedsAction(Verb::Devices,
            "list, public, add PUBLIC_KEY, forget FINGERPRINT, forget all");
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
    } else if (action == "public") {
        command.trust = TrustAction::Public;
    } else if (action == "add") {
        command.trust = TrustAction::Add;
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = NeedsAction(Verb::Trust, "add ADDRESS FINGERPRINT");
            return;
        }
        command.target = std::string(Take(cursor));
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = NeedsAction(Verb::Trust, "add ADDRESS FINGERPRINT");
            return;
        }
        command.value = std::string(Take(cursor));
        while (More(cursor)) {
            const std::string_view token = Take(cursor);
            if (!IsFlagToken(token)) {
                command.error = "trust add takes one address and one key";
                return;
            }
            const Flag flag = SplitFlag(token);
            if (WantsHelp(flag)) {
                command.helpFor = command.verb;
                command.verb = Verb::Help;
                return;
            }
            const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
            if (global == FlagResult::Failed) return;
            if (global == FlagResult::Handled) continue;
            const FlagResult name = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
            if (name == FlagResult::Failed) return;
            if (name == FlagResult::Handled) continue;
            command.error = UnknownOption(flag.name, command.verb);
            return;
        }
        return;
    } else if (action == "forget") {
        bool forgetAll = false;
        if (!TakeForgetTarget(command, cursor, forgetAll)) return;
        command.trust = forgetAll ? TrustAction::ForgetAll : TrustAction::Forget;
    } else {
        command.error = NeedsAction(Verb::Trust,
            "list, public, add ADDRESS FINGERPRINT, forget ADDRESS, forget all");
        return;
    }
    ParseNoArgVerb(command, cursor);
}

void ParseKey(Command& command, Cursor& cursor) {
    if (More(cursor) && WantsHelp(SplitFlag(Look(cursor)))) {
        command.helpFor = Verb::Devices;
        command.verb = Verb::Help;
        return;
    }
    if (!More(cursor) || IsFlagToken(Look(cursor)) || Take(cursor) != "public") {
        command.error = "key needs public";
        return;
    }
    command.devices = DevicesAction::Public;
    ParseNoArgVerb(command, cursor);
}

void ParseAccess(Command& command, Cursor& cursor) {
    command.accessSyntax = true;
    if (More(cursor) && WantsHelp(SplitFlag(Look(cursor)))) {
        command.helpFor = Verb::Devices;
        command.verb = Verb::Help;
        return;
    }
    constexpr const char* kActions =
        "access needs add, list, requests, approve, deny, remove, or clear";
    if (!More(cursor) || IsFlagToken(Look(cursor))) {
        command.error = kActions;
        return;
    }
    const std::string_view action = Take(cursor);
    if (action == "add")
        command.devices = DevicesAction::Add;
    else if (action == "list")
        command.devices = DevicesAction::List;
    else if (action == "requests")
        command.devices = DevicesAction::Requests;
    else if (action == "approve")
        command.devices = DevicesAction::Approve;
    else if (action == "deny")
        command.devices = DevicesAction::Deny;
    else if (action == "remove")
        command.devices = DevicesAction::Forget;
    else if (action == "clear")
        command.devices = DevicesAction::ForgetAll;
    else {
        command.error = kActions;
        return;
    }
    const bool wantsFingerprint = command.devices == DevicesAction::Forget ||
                                  command.devices == DevicesAction::Approve ||
                                  command.devices == DevicesAction::Deny;
    while (More(cursor)) {
        const Flag flag = SplitFlag(Take(cursor));
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Devices;
            command.verb = Verb::Help;
            return;
        }
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;
        if (command.devices == DevicesAction::Add && flag.name == "--stdin" &&
            !flag.hasInlineValue) {
            command.target = "-";
            continue;
        }
        if (wantsFingerprint && flag.name == "--fingerprint") {
            if (!ValueOf(flag, cursor, command.target, command.error)) return;
            continue;
        }
        command.error = UnknownOption(flag.name, Verb::Devices);
        return;
    }
    if ((command.devices == DevicesAction::Add || wantsFingerprint) && command.target.empty())
        command.error =
            "access add needs --stdin; access approve, deny and remove need --fingerprint";
}

void ParseHost(Command& command, Cursor& cursor) {
    if (More(cursor) && WantsHelp(SplitFlag(Look(cursor)))) {
        command.helpFor = Verb::Host;
        command.verb = Verb::Help;
        return;
    }
    if (!More(cursor) || IsFlagToken(Look(cursor))) {
        command.error = "host needs add, update, list, remove, or public";
        return;
    }
    const std::string_view action = Take(cursor);
    if (action == "public")
        command.trust = TrustAction::Public;
    else if (action == "list")
        command.trust = TrustAction::List;
    else if (action == "add")
        command.trust = TrustAction::Add;
    else if (action == "update")
        command.trust = TrustAction::Update;
    else if (action == "remove")
        command.trust = TrustAction::Forget;
    else {
        command.error = "host needs add, update, list, remove, or public";
        return;
    }
    if (command.trust == TrustAction::Add || command.trust == TrustAction::Update ||
        command.trust == TrustAction::Forget) {
        if (More(cursor) && WantsHelp(SplitFlag(Look(cursor)))) {
            command.helpFor = Verb::Host;
            command.verb = Verb::Help;
            return;
        }
        if (!More(cursor) || IsFlagToken(Look(cursor))) {
            command.error = "host action needs an alias";
            return;
        }
        command.profileAlias = std::string(Take(cursor));
    }
    while (More(cursor)) {
        const Flag flag = SplitFlag(Take(cursor));
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Host;
            command.verb = Verb::Help;
            return;
        }
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;
        if (flag.name == "--address" && command.trust != TrustAction::List &&
            command.trust != TrustAction::Public && command.trust != TrustAction::Forget) {
            if (!ValueOf(flag, cursor, command.target, command.error)) return;
            continue;
        }
        if (flag.name == "--host-key-stdin" && !flag.hasInlineValue &&
            (command.trust == TrustAction::Add || command.trust == TrustAction::Update)) {
            command.value = "-";
            continue;
        }
        command.error = UnknownOption(flag.name, Verb::Host);
        return;
    }
    if (command.trust == TrustAction::Add && (command.target.empty() || command.value.empty()))
        command.error = "host add needs --address and --host-key-stdin";
    if (command.trust == TrustAction::Update && command.target.empty() && command.value.empty())
        command.error = "host update needs a field to change";
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;

        FlagResult result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyConnectionFlag(command, flag, cursor);
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;

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
            if (flag.name == "--no-status") {
                share.status = false;
                continue;
            }
            if (flag.name == "--qr") {
                share.qr = true;
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

void ParseShell(Command& command, Cursor& cursor) {
    bool haveAddress = false;
    while (More(cursor)) {
        const std::string_view token = Take(cursor);
        if (!IsFlagToken(token)) {
            if (haveAddress) {
                command.error = "shell takes one address, but got " + Quoted(token) + " as well";
                return;
            }
            if (!TakeAddress(command, token)) return;
            haveAddress = true;
            continue;
        }
        const Flag flag = SplitFlag(token);
        if (WantsHelp(flag)) {
            command.helpFor = Verb::Shell;
            command.verb = Verb::Help;
            return;
        }
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;

        if (!flag.hasInlineValue && flag.name == "--list") {
            command.shell.list = true;
            continue;
        }
        if (flag.name == "--resume") {
            std::string text;
            if (!ValueOf(flag, cursor, text, command.error)) return;
            const std::optional<uint32_t> id = ParseUint(text);
            if (!id || *id == 0) {
                command.error = BadValue(flag.name, text);
                return;
            }
            command.shell.resumeId = *id;
            continue;
        }

        FlagResult result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyConnectionFlag(command, flag, cursor);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        command.error = UnknownOption(flag.name, Verb::Shell);
        return;
    }
    if (!haveAddress) command.error = NeedsAddress(Verb::Shell);
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return;
        if (global == FlagResult::Handled) continue;

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

        FlagResult result = ApplyTextFlag(command, flag, cursor, "--name", command.deviceName);
        if (result == FlagResult::Failed) return;
        if (result == FlagResult::Handled) continue;

        result = ApplyConnectionFlag(command, flag, cursor);
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
        case Verb::Devices: return "devices";
        case Verb::Trust: return "trust";
        case Verb::Host: return "host";
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
        const FlagResult global = ApplyGlobalFlag(command, flag, cursor);
        if (global == FlagResult::Failed) return command;
        if (global == FlagResult::Handled) continue;
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
        case Verb::Sources: ParseAddressVerb(command, cursor); break;
        case Verb::Devices:
            if (token == "key")
                ParseKey(command, cursor);
            else if (token == "access")
                ParseAccess(command, cursor);
            else
                ParseDevices(command, cursor);
            break;
        case Verb::Trust: ParseTrust(command, cursor); break;
        case Verb::Host:
            if (token == "host-key") {
                if (More(cursor) && Take(cursor) == "public")
                    command.trust = TrustAction::Public;
                else
                    command.error = "host-key needs public";
                if (command.error.empty()) ParseNoArgVerb(command, cursor);
            } else
                ParseHost(command, cursor);
            break;
        case Verb::Settings: ParseSettings(command, cursor); break;
        case Verb::Share: ParseShare(command, cursor); break;
        case Verb::Shell: ParseShell(command, cursor); break;
        case Verb::Connect: ParseConnect(command, cursor); break;
        case Verb::Send: ParseSend(command, cursor); break;
        case Verb::None: break;
    }
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
           " [--json] [--quiet] [--verbose] [--config-dir PATH] <command> [arguments]\n"
           "\n"
           "Commands:\n"
           "  share               share this machine on the network\n"
           "  connect ADDRESS     watch and drive a host, in a window of its own\n"
           "  shell ADDRESS       open a shell on a host, right here in this terminal\n"
           "  send ADDRESS FILE   send files to a host that takes them\n"
           "  displays            the displays this machine can share\n"
           "  sources ADDRESS     ask a host what it is sharing\n"
           "  devices             machines allowed to connect to this one\n"
           "  trust               hosts this machine has decided to trust\n"
           "  key public          print this machine's public key\n"
           "  access              allowed clients and pending connection requests\n"
           "  host                manage saved host profiles\n"
           "  host-key public     print this host's public key\n"
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
        case Verb::Sources:
            return "Usage: " + program +
                   " sources ADDRESS[:PORT]|INVITE [--accept-new-host-key] [--approval-wait SEC]\n"
                   "\n"
                   "Ask a host what it is sharing. A host whose key is not saved yet is refused\n"
                   "and its fingerprint is printed; compare it with the host, then rerun with\n"
                   "--accept-new-host-key to save it. An INVITE is the deskhub://pair/ link a\n"
                   "host shows as a QR code: it pins the host and lets this machine in at once.\n";
        case Verb::Devices:
            return "Usage: " + program +
                   " key public\n"
                   "       " +
                   program +
                   " access add --stdin\n"
                   "       " +
                   program +
                   " access list [--json]\n"
                   "       " +
                   program +
                   " access requests [--json]\n"
                   "       " +
                   program +
                   " access approve --fingerprint SHA256:...\n"
                   "       " +
                   program +
                   " access deny --fingerprint SHA256:...\n"
                   "       " +
                   program +
                   " access remove --fingerprint SHA256:...\n"
                   "       " +
                   program +
                   " access clear\n"
                   "       " +
                   program +
                   " devices [list]\n"
                   "       " +
                   program +
                   " devices public\n"
                   "       " +
                   program +
                   " devices add PUBLIC_KEY|-\n"
                   "       " +
                   program +
                   " devices forget FINGERPRINT\n"
                   "       " +
                   program +
                   " devices forget all\n"
                   "\n"
                   "Machines allowed to connect to this one (authorized_keys), and the machines\n"
                   "waiting for approval (access requests). A device that connects without\n"
                   "being allowed waits up to ten minutes for 'access approve'; 'access deny'\n"
                   "drops its request. 'key public' prints this machine's own public key line.\n";
        case Verb::Trust:
            return "Usage: " + program +
                   " trust [list]\n"
                   "       " +
                   program +
                   " trust public\n"
                   "       " +
                   program +
                   " trust add ADDRESS PUBLIC_KEY|-|FINGERPRINT [--name ALIAS]\n"
                   "       " +
                   program +
                   " trust forget ADDRESS\n"
                   "       " +
                   program +
                   " trust forget all\n"
                   "\n"
                   "Hosts this machine has decided to trust, by key. --name saves an alias. Trust\n"
                   "follows the key, so a host stays trusted when its address changes. Forgetting\n"
                   "a host asks for its fingerprint again on the next connection.\n";
        case Verb::Host:
            return "Usage: " + program +
                   " host add ALIAS --address IP[:PORT] --host-key-stdin\n"
                   "       " +
                   program +
                   " host update ALIAS [--address IP[:PORT]] [--host-key-stdin]\n"
                   "       " +
                   program +
                   " host remove ALIAS\n"
                   "       " +
                   program +
                   " host list [--json]\n"
                   "       " +
                   program +
                   " host public\n"
                   "       " +
                   program +
                   " host-key public\n"
                   "\n"
                   "The host key is an OpenSSH public key line read from stdin. Saved aliases work with connect, sources, shell, and send.\n";
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
                   " connect ADDRESS[:PORT]|INVITE [--source ID|NAME|all] [flags]\n"
                   "\n"
                   "Open a window on a host's screen and drive it. Without --source the host's\n"
                   "first screen is shown; 'all' opens one window each. A host that has not\n"
                   "allowed this machine yet is asked to; connect waits up to two minutes for\n"
                   "its owner to press Approve. An INVITE (deskhub://pair/...) skips both steps.\n"
                   "\n"
                   "  --source ID|NAME|all  which of the host's screens to watch\n"
                   "  --view-only           watch without typing or clicking\n"
                   "  --audio / --no-audio  play the host's sound, or do not\n"
                   "  --name NAME           what the host sees this machine called\n"
                   "  --accept-new-host-key save the key of a host seen for the first time\n"
                   "  --approval-wait SEC   how long to wait for the host owner to approve (1-600, default 120)\n"
                   "\n"
                   "F9 locks the pointer to the window, Escape lets it go again.\n";
        case Verb::Shell:
            return "Usage: " + program +
                   " shell ADDRESS[:PORT]|INVITE [--name NAME] [--resume ID] [--list]\n"
                   "\n"
                   "Open a shell on a host and drive it from this terminal. Everything the\n"
                   "shell prints is written straight through, so your own terminal draws it.\n"
                   "A shell outlives a dropped connection or a closed window: it stays on the\n"
                   "host until its shell exits, so --list shows the shells left behind and\n"
                   "--resume ID picks one back up instead of opening a new one.\n"
                   "\n"
                   "  --name NAME       what the host sees this machine called\n"
                   "  --accept-new-host-key  save the key of a host seen for the first time\n"
                   "  --resume ID       reattach a shell the host is keeping, by its id\n"
                   "  --list            list the shells open on the host, then quit\n";
        case Verb::Send:
            return "Usage: " + program +
                   " send ADDRESS[:PORT]|INVITE FILE [FILE...] [--name NAME]\n"
                   "\n"
                   "Send files to a host that was started with --files. The host stores them in\n"
                   "its transfer folder without asking, so it only takes files from machines it\n"
                   "has admitted. Names are reduced to a plain file name before they are stored,\n"
                   "and nothing already there is ever overwritten.\n"
                   "\n"
                   "  --name NAME       what the host sees this machine called\n"
                   "  --accept-new-host-key  save the key of a host seen for the first time\n"
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
                   "  --name NAME            what viewers see this machine called\n"
                   "  --status-interval MS   how often to print the status line\n"
                   "  --no-status            print nothing until it stops\n"
                   "  --qr                   print a QR code a phone scans to connect at once\n"
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
