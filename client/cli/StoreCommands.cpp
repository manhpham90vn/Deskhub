#include "Commands.h"

#include <optional>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Output.h"

#include "deskhub/cli/Json.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/ClientKeys.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace deskhubcli {

namespace {

ExitCode ReportHostProfile(deskhub::ui::HostProfileError error, const Command& command,
    std::string_view done) {
    if (error == deskhub::ui::HostProfileError::None) {
        if (!command.quiet) PrintLine(done);
        return ExitCode::Ok;
    }
    PrintError(deskhub::ui::HostProfileErrorText(error));
    const bool fileProblem = error == deskhub::ui::HostProfileError::StoreUnreadable ||
                             error == deskhub::ui::HostProfileError::WriteFailed ||
                             error == deskhub::ui::HostProfileError::AliasAmbiguous;
    return fileProblem ? ExitCode::Failed : ExitCode::Usage;
}

ExitCode RunHostProfile(const Command& command) {
    if (command.trust == TrustAction::Forget)
        return ReportHostProfile(
            deskhubp::RemoveHostProfile(command.profileAlias), command, "Saved host removed.");

    deskhub::ui::HostProfileRequest request;
    request.alias = command.profileAlias;
    request.address = command.target;
    request.identityName = command.identityName.value_or(std::string());
    if (command.value == "-") {
        std::string text;
        if (!std::getline(std::cin, text)) {
            PrintError("no host public key arrived on stdin");
            return ExitCode::Usage;
        }
        request.hostKey = deskhubp::ParseHostKeyText(text);
        if (!request.hostKey)
            return ReportHostProfile(deskhub::ui::HostProfileError::InvalidHostKey, command, "");
    }
    const auto mode = command.trust == TrustAction::Add ? deskhub::ui::HostProfileMode::Add
                                                        : deskhub::ui::HostProfileMode::Update;
    return ReportHostProfile(deskhubp::SaveHostProfile(mode, request), command,
        "Host profile saved.");
}

std::string ClientName(const deskhubp::AuthorizedClient& client) {
    return client.label.empty() ? std::string("(unnamed)") : client.label;
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
    if (command.devices == DevicesAction::Import) {
        std::ifstream file(command.target, std::ios::binary);
        if (!file) {
            PrintError("could not read the private key file");
            return ExitCode::Failed;
        }
        std::string pem(65537, '\0');
        file.read(pem.data(), std::streamsize(pem.size()));
        const std::streamsize length = file.gcount();
        if (length <= 0 || length > 65536 || file.bad()) {
            PrintError("invalid private key file size");
            return ExitCode::Usage;
        }
        pem.resize(size_t(length));
        if (pem.find("-----BEGIN RSA PRIVATE KEY-----") != std::string::npos) {
            PrintError("RSA private keys are not supported; use Ed25519 or P-256");
            return ExitCode::Usage;
        }
        if (!command.keyPassphraseStdin &&
            pem.find("-----BEGIN ENCRYPTED PRIVATE KEY-----") != std::string::npos) {
            PrintError("this private key is encrypted; pass its unlock phrase with --passphrase-stdin");
            return ExitCode::Usage;
        }
        std::string passphrase;
        if (command.keyPassphraseStdin && !std::getline(std::cin, passphrase)) {
            PrintError("no key passphrase arrived on stdin");
            return ExitCode::Usage;
        }
        const bool imported = command.keyName.empty()
                                  ? deskhubp::ImportClientIdentity(pem, passphrase)
                                  : deskhubp::ImportClientIdentity(command.keyName, pem, passphrase);
        if (!imported) {
            PrintError("could not import identity: use an Ed25519 or P-256 OpenSSH/PKCS#8 private key; check its passphrase and whether the name already exists");
            return ExitCode::Usage;
        }
        if (!command.quiet) {
            PrintLine("Client identity imported. Update the public key on each host that allows it.");
        }
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Public) {
        const bool defaultKey =
            command.keyName.empty() || command.keyName == deskhub::ui::kDefaultIdentityName;
        const deskhubp::ClientIdentity identity = defaultKey
                                                      ? deskhubp::LoadOrCreateClientIdentity()
                                                      : deskhubp::LoadClientIdentity(command.keyName);
        const std::string publicKey = deskhubp::ClientPublicKeyLine(identity, command.keyName);
        if (publicKey.empty()) {
            PrintError("could not read this machine's public key");
            return ExitCode::Failed;
        }
        PrintLine(publicKey);
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Generate) {
        const auto identity = deskhubp::GenerateClientIdentity(command.keyName);
        if (!identity.Valid()) {
            PrintError("could not create identity: check the name and whether it already exists");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine(deskhubp::ClientPublicKeyLine(identity, command.keyName));
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Identities) {
        const auto identities = deskhubp::ListClientIdentities();
        if (command.json) {
            deskhub::cli::JsonWriter json;
            json.ArrayBegin();
            for (const auto& identity : identities) {
                json.ObjectBegin();
                json.Field("name", identity.name);
                json.Field("publicKey", identity.publicKeyText);
                json.Field("fingerprint", identity.valid
                                              ? deskhub::FormatFingerprint(identity.fingerprint)
                                              : std::string());
                json.Field("valid", identity.valid);
                json.ObjectEnd();
            }
            json.ArrayEnd();
            PrintLine(json.Text());
        } else {
            for (const auto& identity : identities)
                PrintLine(identity.name + " " + (identity.valid ? deskhub::FormatFingerprint(identity.fingerprint) : "unusable"));
        }
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
        if (!deskhubp::RememberAuthorizedKey(text)) {
            PrintError("could not save the authorized key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine(deskhub::FormatFingerprint(*fingerprint));
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::DeleteKey) {
        const deskhub::ui::ClientKeyError error = deskhubp::DeleteClientKey(command.keyName);
        if (error != deskhub::ui::ClientKeyError::None) {
            PrintError(deskhub::ui::ClientKeyErrorText(error));
            return error == deskhub::ui::ClientKeyError::WriteFailed ? ExitCode::Failed
                                                                     : ExitCode::Usage;
        }
        if (!command.quiet) PrintLine("Key deleted.");
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::ForgetAll) {
        if (!deskhubp::ClearAuthorizedKeys()) {
            PrintError("could not revoke the authorized keys");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("Every client key has been removed; nobody can connect until a key is added.");
        return ExitCode::Ok;
    }

    if (command.devices == DevicesAction::Forget) {
        const std::optional<deskhub::Fingerprint> fingerprint =
            deskhub::ParseFingerprint(command.target);
        if (!fingerprint) {
            PrintError("not a key fingerprint: " + command.target);
            return ExitCode::Usage;
        }
        if (!deskhubp::ForgetAuthorizedClient(*fingerprint)) {
            PrintError("no allowed client has that key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("That client key has been removed.");
        return ExitCode::Ok;
    }

    if (command.accessSyntax) {
        const auto snapshot = deskhubp::LoadAuthorizedKeys();
        if (!snapshot) {
            PrintError("could not read authorized_keys");
            return ExitCode::Failed;
        }
        if (command.json) {
            deskhub::cli::JsonWriter json;
            json.ArrayBegin();
            for (const auto& key : snapshot->Keys()) {
                const std::string publicKey = deskhub::FormatPublicKeyText(key);
                const auto fingerprint = deskhubp::FingerprintOfPublicKey(
                    deskhubp::PublicKeySpkiFromText(publicKey));
                json.ObjectBegin();
                json.Field("publicKey", publicKey);
                json.Field("fingerprint", fingerprint
                                              ? deskhub::FormatFingerprint(*fingerprint)
                                              : std::string());
                json.Field("name", key.label);
                json.ObjectEnd();
            }
            json.ArrayEnd();
            PrintLine(json.Text());
        } else {
            Table table;
            table.Row({"FINGERPRINT", "NAME", "PUBLIC KEY"});
            for (const auto& key : snapshot->Keys()) {
                const std::string publicKey = deskhub::FormatPublicKeyText(key);
                const auto fingerprint = deskhubp::FingerprintOfPublicKey(
                    deskhubp::PublicKeySpkiFromText(publicKey));
                table.Row({fingerprint ? deskhub::FormatFingerprint(*fingerprint) : "",
                    key.label, publicKey});
            }
            table.Print();
        }
        return ExitCode::Ok;
    }

    const auto clients = deskhubp::ListAuthorizedClients();
    if (!clients) {
        PrintError("could not read authorized_keys");
        return ExitCode::Failed;
    }

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ArrayBegin();
        for (const deskhubp::AuthorizedClient& client : *clients) {
            json.ObjectBegin();
            json.Field("fingerprint", deskhub::FormatFingerprint(client.fingerprint));
            json.Field("name", client.label);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    if (clients->empty()) {
        if (!command.quiet) PrintLine("No client key is allowed to connect to this machine yet.");
        return ExitCode::Ok;
    }

    Table table;
    table.Row({"KEY", "NAME"});
    for (const deskhubp::AuthorizedClient& client : *clients)
        table.Row({deskhub::ShortFingerprint(client.fingerprint), ClientName(client)});
    table.Print();
    return ExitCode::Ok;
}

ExitCode RunTrust(const Command& command) {
    const bool profileCommand = command.verb == deskhub::cli::Verb::Host;
    if (profileCommand && command.trust != TrustAction::Public &&
        command.trust != TrustAction::List)
        return RunHostProfile(command);
    if (command.trust == TrustAction::Public) {
        const auto identity = deskhubp::LoadOrCreateHostIdentity("deskhub");
        const std::string publicKey = deskhubp::IdentityPublicKeyText(identity);
        if (publicKey.empty()) {
            PrintError("could not read this host's public key");
            return ExitCode::Failed;
        }
        PrintLine(publicKey);
        return ExitCode::Ok;
    }

    if (command.trust == TrustAction::Add) {
        std::string supplied = command.value;
        if (supplied == "-") {
            if (!std::getline(std::cin, supplied)) {
                PrintError("no host public key arrived on stdin");
                return ExitCode::Usage;
            }
        }
        const auto fingerprint = deskhubp::ParseHostKeyText(supplied);
        if (!fingerprint || command.target.empty()) {
            PrintError("expected an address and host public key or SHA256 fingerprint");
            return ExitCode::Usage;
        }
        if (!deskhubp::RememberTrustedHostProfile(command.target,
                command.deviceName.value_or(command.target), *fingerprint,
                command.identityName.value_or("default"), NowUnixSeconds())) {
            PrintError("could not save the trusted host key");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("Trusted host key saved.");
        return ExitCode::Ok;
    }

    if (command.trust == TrustAction::ForgetAll) {
        if (!deskhubp::ClearTrustedHosts()) {
            PrintError("could not clear trusted host keys; check known_hosts and its permissions");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("Every saved host key has been removed. Add a host key before connecting again.");
        return ExitCode::Ok;
    }

    if (command.trust == TrustAction::Forget) {
        if (!deskhubp::ForgetTrustedHost(command.target)) {
            PrintError("could not remove trusted host at " + command.target +
                       "; check the address, known_hosts, and its permissions");
            return ExitCode::Failed;
        }
        if (!command.quiet) PrintLine("That saved host key has been removed. Add its key before connecting again.");
        return ExitCode::Ok;
    }

    const auto store = deskhubp::TryLoadTrustStore();
    if (!store) {
        PrintError("could not read known_hosts; fix the file before listing or changing trusted hosts");
        return ExitCode::Failed;
    }

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ArrayBegin();
        for (const deskhub::TrustedHost& host : store->Hosts()) {
            json.ObjectBegin();
            json.Field("endpoint", host.endpoint);
            json.Field("label", host.label);
            json.Field("identity", host.identityName.empty() ? "default" : host.identityName);
            json.Field("fingerprint", deskhub::FormatFingerprint(host.fingerprint));
            json.Field("firstSeenUnix", host.firstSeenUnix);
            json.Field("lastSeenUnix", host.lastSeenUnix);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    if (store->Hosts().empty()) {
        if (!command.quiet) PrintLine("This machine has not trusted any host yet.");
        return ExitCode::Ok;
    }

    Table table;
    table.Row({"ALIAS", "ENDPOINT", "CLIENT KEY", "HOST KEY"});
    for (const deskhub::TrustedHost& host : store->Hosts())
        table.Row({host.label, host.endpoint,
            host.identityName.empty() ? "default" : host.identityName,
            deskhub::ShortFingerprint(host.fingerprint)});
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
            json.Field(key, value);
        }
        json.ObjectEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    Table table;
    for (const std::string& key : keys) {
        std::string value;
        ValueOfSetting(text, key, value);
        table.Row({key, value});
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
