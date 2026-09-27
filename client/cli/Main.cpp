#include <cstdio>
#include <string>

#include "Commands.h"
#include "Signals.h"
#include "Output.h"

#include "deskhub/cli/Command.h"
#include "deskhub/ui/Strings.h"

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

}

int main(int argc, char** argv) {
    const deskhub::cli::Command command = deskhub::cli::ParseCommand(argc, argv);
    if (!command.error.empty()) return RunUsageError(command);

    switch (command.verb) {
        case deskhub::cli::Verb::Help: return RunHelp(command);
        case deskhub::cli::Verb::Version:
            deskhubcli::PrintLine(deskhub::ui::VersionLine());
            return Report(deskhubcli::ExitCode::Ok);
        case deskhub::cli::Verb::Displays: return Report(deskhubcli::RunDisplays(command));
        case deskhub::cli::Verb::Sources: return Report(deskhubcli::RunSources(command));
        case deskhub::cli::Verb::Devices: return Report(deskhubcli::RunDevices(command));
        case deskhub::cli::Verb::Trust: return Report(deskhubcli::RunTrust(command));
        case deskhub::cli::Verb::Settings: return Report(deskhubcli::RunSettings(command));
        case deskhub::cli::Verb::Share: return Report(deskhubcli::RunShare(command));
        case deskhub::cli::Verb::Shell: return Report(deskhubcli::RunShell(command));
        case deskhub::cli::Verb::Connect: return Report(deskhubcli::RunConnect(command));
        case deskhub::cli::Verb::Send: return Report(deskhubcli::RunSend(command));
        case deskhub::cli::Verb::None: break;
    }
    return RunHelp(command);
}
