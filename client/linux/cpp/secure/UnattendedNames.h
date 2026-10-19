#pragma once

inline constexpr const char* kUnattendedSocketDir = "/run/deskhub";
inline constexpr const char* kUnattendedSocketPath = "/run/deskhub/control.sock";
inline constexpr const char* kUnattendedMachineFolder = "/var/lib/deskhub";
inline constexpr const char* kUnattendedDaemonExe = "/usr/libexec/deskhub/deskhubd";
inline constexpr const char* kUnattendedDaemonUnit = "/usr/lib/systemd/system/deskhubd.service";
inline constexpr const char* kUnattendedAppExe = "/usr/bin/deskhub";
inline constexpr const char* kUnattendedCliExe = "/usr/bin/deskhub-cli";
inline constexpr const char* kUnattendedHostArg = "--unattended-host";
