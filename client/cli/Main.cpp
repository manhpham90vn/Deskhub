#include <cstdio>
#include <string>

#include "Commands.h"
#include "Signals.h"
#include "Output.h"

#include "deskhub/cli/Command.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/TrustStoreFile.h"

namespace {

int Report(deskhubcli::ExitCode code) {
    return int(code);
}

int RunHelp(const deskhub::cli::Command& command) {
    std::fputs(deskhub::cli::UsageText(command.helpFor).c_str(), stdout);
    return Report(deskhubcli::ExitCode::Ok);
}

int RunUsageError(const deskhub::cli::Command& command) {
    deskhubcli::PrintError(command.error);
    std::fputs(deskhub::cli::UsageText(command.verb).c_str(), stderr);
    return Report(deskhubcli::ExitCode::Usage);
}

bool ResolveProfile(deskhub::cli::Command& command) {
    if (command.profileAlias.empty()) return true;
    if (command.verb != deskhub::cli::Verb::Connect &&
        command.verb != deskhub::cli::Verb::Sources &&
        command.verb != deskhub::cli::Verb::Shell &&
        command.verb != deskhub::cli::Verb::Send) return true;
    const auto store = deskhubp::TryLoadTrustStore();
    if (!store) {
        deskhubcli::PrintError("could not read known_hosts");
        return false;
    }
    const deskhub::TrustedHost* match = nullptr;
    for (const auto& host : store->Hosts()) {
        if (host.label != command.profileAlias) continue;
        if (match) {
            deskhubcli::PrintError("host alias is ambiguous");
            return false;
        }
        match = &host;
    }
    if (!match) {
        deskhubcli::PrintError("no saved host has that alias");
        return false;
    }
    command.address = match->endpoint;
    return true;
}

bool ResolveInvite(deskhub::cli::Command& command) {
    if (command.pairingInvite.empty()) return true;
    return deskhubcli::AdmitByInvite(command);
}

}

int main(int argc, char** argv) {
    deskhub::cli::Command command = deskhub::cli::ParseCommand(argc, argv);
    if (!command.error.empty()) return RunUsageError(command);
    if (command.configDir) {
        deskhubp::SetConfigDir(*command.configDir);
        if (deskhubp::ConfigDir().empty()) {
            deskhubcli::PrintError("cannot access --config-dir; check its path and permissions");
            return Report(deskhubcli::ExitCode::Failed);
        }
    }
    if (!ResolveProfile(command)) return Report(deskhubcli::ExitCode::Failed);
    if (!ResolveInvite(command)) return Report(deskhubcli::ExitCode::Refused);

    switch (command.verb) {
        case deskhub::cli::Verb::Help: return RunHelp(command);
        case deskhub::cli::Verb::Version:
            deskhubcli::PrintLine(deskhub::ui::VersionLine());
            return Report(deskhubcli::ExitCode::Ok);
        case deskhub::cli::Verb::Displays: return Report(deskhubcli::RunDisplays(command));
        case deskhub::cli::Verb::Sources: return Report(deskhubcli::RunSources(command));
        case deskhub::cli::Verb::Devices: return Report(deskhubcli::RunDevices(command));
        case deskhub::cli::Verb::Trust: return Report(deskhubcli::RunTrust(command));
        case deskhub::cli::Verb::Host: return Report(deskhubcli::RunTrust(command));
        case deskhub::cli::Verb::Settings: return Report(deskhubcli::RunSettings(command));
        case deskhub::cli::Verb::Share: return Report(deskhubcli::RunShare(command));
        case deskhub::cli::Verb::Shell: return Report(deskhubcli::RunShell(command));
        case deskhub::cli::Verb::Connect: return Report(deskhubcli::RunConnect(command));
        case deskhub::cli::Verb::Send: return Report(deskhubcli::RunSend(command));
        case deskhub::cli::Verb::None: break;
    }
    return RunHelp(command);
}
