#pragma once
#include <string>

#ifdef DESKHUB_DEV_BUILD
inline constexpr const char* kUnattendedBundleId = "com.deskhub.macos.debug";
inline constexpr const char* kUnattendedSocketPath = "/var/run/deskhub/control-debug.sock";
inline constexpr const char* kUnattendedMachineFolder =
    "/Library/Application Support/Deskhub Debug";
#else
inline constexpr const char* kUnattendedBundleId = "com.deskhub.macos";
inline constexpr const char* kUnattendedSocketPath = "/var/run/deskhub/control.sock";
inline constexpr const char* kUnattendedMachineFolder = "/Library/Application Support/Deskhub";
#endif

inline constexpr const char* kUnattendedSocketDir = "/var/run/deskhub";
inline constexpr const char* kUnattendedDaemonExe = "DeskhubDaemon";
inline constexpr const char* kBundledCliExe = "deskhub-cli";
inline constexpr const char* kUnattendedHostArg = "--unattended-host";
inline constexpr const char* kUnattendedTeamId = "UPJRMYQ38F";
inline constexpr const char* kLaunchAgentsDir = "/Library/LaunchAgents";
inline constexpr const char* kLaunchDaemonsInBundle = "Contents/Library/LaunchDaemons";

inline std::string UnattendedDaemonLabel() {
    return std::string(kUnattendedBundleId) + ".daemon";
}

inline std::string UnattendedDaemonPlistName() {
    return UnattendedDaemonLabel() + ".plist";
}

inline std::string UnattendedAgentLabel() {
    return std::string(kUnattendedBundleId) + ".unattended";
}

inline std::string UnattendedAgentPlistPath() {
    return std::string(kLaunchAgentsDir) + "/" + UnattendedAgentLabel() + ".plist";
}
