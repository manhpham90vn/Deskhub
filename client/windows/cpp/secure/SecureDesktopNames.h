#pragma once

inline constexpr wchar_t kSecureDesktopServiceName[] = L"DeskhubSecureDesktop";
inline constexpr wchar_t kSecureDesktopServiceDisplayName[] = L"DeskHub Secure Desktop";
inline constexpr wchar_t kSecureDesktopServiceDescription[] =
    L"Lets a DeskHub viewer see and answer UAC prompts and the lock screen while this "
    L"computer is being shared.";
inline constexpr wchar_t kSecureDesktopPipeName[] = L"\\\\.\\pipe\\DeskhubSecureDesktop";
inline constexpr wchar_t kSecureDesktopServiceExe[] = L"DeskhubService.exe";
inline constexpr wchar_t kSecureDesktopHostExe[] = L"Deskhub.exe";
inline constexpr wchar_t kSecureDesktopCliExe[] = L"deskhub-cli.exe";
inline constexpr wchar_t kSecureDesktopAgentArg[] = L"--agent";
inline constexpr wchar_t kUnattendedHostArg[] = L"--unattended-host";
