#include "Commands.h"

#include <optional>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Output.h"

#include "deskhub/cli/Json.h"
#include "deskhub/net/PairedDevices.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace deskhubcli {

namespace {

constexpr std::string_view kPasscodeKey = "passcode";
constexpr std::string_view kPasscodeMask = "****";

std::string DeviceName(const deskhub::PairedDevice& device) {
    return device.name.empty() ? std::string("(unnamed)") : device.name;
}

std::vector<std::string> SettingsKeys(const std::string& text) {
    std::vector<std::string> keys;
    size_t start = 0;
    while (start < text.size()) {
        const size_t lineEnd = text.find('\n', start);
        const std::string_view line(text.data() + start,
            (lineEnd == std::string::npos ? text.size() : lineEnd) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos) keys.emplace_back(line.substr(0, equals));
        if (lineEnd == std::string::npos) break;
        start = lineEnd + 1;
    }
    return keys;
}

bool ValueOfSetting(const std::string& text, std::string_view key, std::string& out) {
    size_t start = 0;
    while (start < text.size()) {
        const size_t lineEnd = text.find('\n', start);
        const std::string_view line(text.data() + start,
            (lineEnd == std::string::npos ? text.size() : lineEnd) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos && line.substr(0, equals) == key) {
            out = std::string(line.substr(equals + 1));
            return true;
        }
        if (lineEnd == std::string::npos) break;
        start = lineEnd + 1;
    }
    return false;
}

bool KnownSettingsKey(const std::vector<std::string>& keys, std::string_view key) {
    for (const std::string& known : keys)
        if (known == key) return true;
    return false;
}

std::string UnknownKeyMessage(std::string_view key, const std::vector<std::string>& keys) {
    std::string message = "unknown setting \"" + std::string(key) + "\". Known settings are:";
    for (const std::string& known : keys) message += "\n  " + known;
    return message;
}

}

ExitCode RunDevices(const Command& command) {
    if (command.devices == DevicesAction::Public) {
        const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub");
        const std::string publicKey = deskhubp::IdentityPublicKeyText(identity);
        if (publicKey.empty()) {
            PrintError("could not read this machine's public key");
            return ExitCode::Failed;
        }
        PrintLine(publicKey);
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Add) {
        std::string text = command.target;
        if (text == "-") {
            if (!std::getline(std::cin, text)) {
                PrintError("no public key arrived on stdin");
                return ExitCode::Usage;
            }
        }
        const std::vector<uint8_t> spki = deskhubp::PublicKeySpkiFromText(text);
        const auto fingerprint = deskhubp::FingerprintOfPublicKey(spki);
        if (!fingerprint) {
            PrintError("invalid or unsupported public key");
            return ExitCode::Usage;
        }
        const auto parsed = deskhub::ParsePublicKeyText(text);
        const std::string label = parsed ? parsed->label : "";
        if (!deskhubp::RememberPairedDevice(*fingerprint, label, std::time(nullptr))) {
            PrintError("could not save the authorized key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine(deskhub::FormatFingerprint(*fingerprint));
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::ForgetAll) {
        deskhubp::ForgetAllPairedDevices();
        if (!command.quiet) PrintLine("Every paired machine has to pair again.");
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Forget) {
        const std::optional<deskhub::Fingerprint> fingerprint =
            deskhub::ParseFingerprint(command.target);
        if (!fingerprint) {
            PrintError("not a key fingerprint: " + command.target);
            return ExitCode::Usage;
        }
        if (!deskhubp::ForgetPairedDevice(*fingerprint)) {
            PrintError("no paired machine has that key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("That machine has to pair again.");
        return ExitCode::Ok;
    }

    const deskhub::PairedDevices devices = deskhubp::LoadPairedDevices();

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ArrayBegin();
        for (const deskhub::PairedDevice& device : devices.Devices()) {
            json.ObjectBegin();
            json.Field("fingerprint", deskhub::FormatFingerprint(device.fingerprint));
            json.Field("name", device.name);
            json.Field("pairedUnix", device.pairedUnix);
            json.Field("lastSeenUnix", device.lastSeenUnix);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    if (devices.Devices().empty()) {
        if (!command.quiet) PrintLine("No machine has paired with this one yet.");
        return ExitCode::Ok;
    }

    Table table;
    table.Row({"KEY", "NAME", "PAIRED", "LAST SEEN"});
    for (const deskhub::PairedDevice& device : devices.Devices())
        table.Row({deskhub::ShortFingerprint(device.fingerprint), DeviceName(device),
            UnixDate(device.pairedUnix), UnixDate(device.lastSeenUnix)});
    table.Print();
    return ExitCode::Ok;
}

ExitCode RunTrust(const Command& command) {
    if (command.trust == TrustAction::Add) {
        const auto fingerprint = deskhub::ParseFingerprint(command.value);
        if (!fingerprint || command.target.empty()) {
            PrintError("expected an address and SHA256 fingerprint");
            return ExitCode::Usage;
        }
        if (!deskhubp::RememberTrustedHost(command.target, command.target, *fingerprint,
                std::time(nullptr))) {
            PrintError("could not save the trusted host key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("Trusted host key saved.");
        return ExitCode::Ok;
    }

    if (command.trust == TrustAction::ForgetAll) {
        deskhub::TrustStore store;
        deskhubp::SaveTrustStore(store);
        if (!command.quiet) PrintLine("Every host key is accepted afresh next time.");
        return ExitCode::Ok;
    }

    if (command.trust == TrustAction::Forget) {
        if (!deskhubp::ForgetTrustedHost(command.target)) {
            PrintError("no trusted host at " + command.target);
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("That host's key is accepted afresh next time.");
        return ExitCode::Ok;
    }

    const deskhub::TrustStore store = deskhubp::LoadTrustStore();

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ArrayBegin();
        for (const deskhub::TrustedHost& host : store.Hosts()) {
            json.ObjectBegin();
            json.Field("endpoint", host.endpoint);
            json.Field("label", host.label);
            json.Field("fingerprint", deskhub::FormatFingerprint(host.fingerprint));
            json.Field("firstSeenUnix", host.firstSeenUnix);
            json.Field("lastSeenUnix", host.lastSeenUnix);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    if (store.Hosts().empty()) {
        if (!command.quiet) PrintLine("This machine has not trusted any host yet.");
        return ExitCode::Ok;
    }

    Table table;
    table.Row({"ENDPOINT", "KEY", "LAST SEEN"});
    for (const deskhub::TrustedHost& host : store.Hosts())
        table.Row({host.endpoint, deskhub::ShortFingerprint(host.fingerprint),
            UnixDate(host.lastSeenUnix)});
    table.Print();
    return ExitCode::Ok;
}

ExitCode RunSettings(const Command& command) {
    const deskhub::ui::UiSettings current = deskhubp::LoadUiSettings();
    const std::string text = deskhub::ui::SerializeUiSettings(current);
    const std::vector<std::string> keys = SettingsKeys(text);

    if (command.settings == SettingsAction::Get) {
        if (!KnownSettingsKey(keys, command.key)) {
            PrintError(UnknownKeyMessage(command.key, keys));
            return ExitCode::Usage;
        }
        if (command.key == kPasscodeKey) {
            PrintLine(current.passcode);
            return ExitCode::Ok;
        }
        std::string value;
        ValueOfSetting(text, command.key, value);
        PrintLine(value);
        return ExitCode::Ok;
    }

    if (command.settings == SettingsAction::Set) {
        if (!KnownSettingsKey(keys, command.key)) {
            PrintError(UnknownKeyMessage(command.key, keys));
            return ExitCode::Usage;
        }
        const deskhub::ui::UiSettings updated =
            deskhub::ui::ParseUiSettings(text + command.key + "=" + command.value + "\n");
        if (updated == current) {
            std::string existing;
            ValueOfSetting(text, command.key, existing);
            if (existing == command.value) {
                if (!command.quiet) PrintLine(command.key + " is already " + command.value);
                return ExitCode::Ok;
            }
            PrintError(command.key + " does not accept \"" + command.value + "\"");
            return ExitCode::Usage;
        }
        deskhubp::SaveUiSettings(updated);
        if (!command.quiet) PrintLine(command.key + " = " + command.value);
        return ExitCode::Ok;
    }

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ObjectBegin();
        for (const std::string& key : keys) {
            std::string value;
            ValueOfSetting(text, key, value);
            json.Field(key, key == kPasscodeKey ? std::string(kPasscodeMask) : value);
        }
        json.ObjectEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    Table table;
    for (const std::string& key : keys) {
        std::string value;
        ValueOfSetting(text, key, value);
        table.Row({key, key == kPasscodeKey ? std::string(kPasscodeMask) : value});
    }
    table.Print();

    if (!command.quiet) {
        const deskhubp::HostIdentity identity = deskhubp::LoadHostIdentity();
        PrintLine("");
        PrintLine(identity.Valid()
                      ? "this machine's key: " + deskhub::FormatFingerprint(identity.fingerprint)
                      : std::string("this machine has no key yet - it is made on the first share"));
    }
    return ExitCode::Ok;
}

}
