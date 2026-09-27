#pragma once
#include "deskhub/cli/Command.h"

namespace deskhubcli {

using Command = deskhub::cli::Command;
using ExitCode = deskhub::cli::ExitCode;
using Verb = deskhub::cli::Verb;
using DevicesAction = deskhub::cli::DevicesAction;
using TrustAction = deskhub::cli::TrustAction;
using SettingsAction = deskhub::cli::SettingsAction;

ExitCode RunDisplays(const Command& command);
ExitCode RunSources(const Command& command);

ExitCode RunDevices(const Command& command);
ExitCode RunTrust(const Command& command);
ExitCode RunSettings(const Command& command);

ExitCode RunShare(const Command& command);
ExitCode RunShell(const Command& command);
ExitCode RunConnect(const Command& command);
ExitCode RunSend(const Command& command);

}
