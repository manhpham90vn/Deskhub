#include "secure/ServiceControlPipe.h"

#include <string>
#include <vector>

#include "WinPaths.h"
#include "deskhubp/diag/Log.h"
#include "secure/SecureDesktopNames.h"

namespace {

bool IsOurService(HANDLE pipe) {
    ULONG pid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &pid)) return false;
    DWORD sessionId = 1;
    if (!ProcessIdToSessionId(pid, &sessionId) || sessionId != 0) return false;
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    const std::wstring image = ProcessImagePath(process);
    if (process) CloseHandle(process);
    return SamePath(image, DirectoryOf(SelfExePath()) + L"\\" + kSecureDesktopServiceExe);
}

void LogUnreachable(DWORD error) {
    if (error == ERROR_FILE_NOT_FOUND)
        LOGW(
            "[Secure] The DeskhubSecureDesktop service is not running: UAC prompts and the "
            "lock screen will freeze for viewers. Reinstall DeskHub to add the service.");
    else
        LOGW(
            "[Secure] Could not reach the DeskhubSecureDesktop service (error %lu): UAC "
            "prompts and the lock screen will freeze for viewers.",
            error);
}

}

std::unique_ptr<OverlappedPipe> ConnectSecureDesktopService(
    deskhub::SecureDesktopRequest request) {
    const HANDLE pipe = CreateFileW(kSecureDesktopPipeName, GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        LogUnreachable(GetLastError());
        return nullptr;
    }
    auto control = std::make_unique<OverlappedPipe>(pipe);
    if (!IsOurService(pipe)) {
        LOGW(
            "[Secure] %ls is not served by the DeskhubService.exe installed beside this "
            "program; not using it.",
            kSecureDesktopPipeName);
        return nullptr;
    }
    const uint8_t byte = uint8_t(request);
    if (!control->WriteAll(std::span<const uint8_t>(&byte, 1))) return nullptr;
    return control;
}

bool AwaitSecureDesktopAck(OverlappedPipe& control) {
    uint8_t ack = 0;
    size_t got = 0;
    if (!control.ReadSome(std::span<uint8_t>(&ack, 1), got)) return false;
    return got == 1 && ack == deskhub::kSecureDesktopAck;
}

std::unique_ptr<OverlappedPipe> TakeoverUnattendedHost() {
    std::unique_ptr<OverlappedPipe> control =
        ConnectSecureDesktopService(deskhub::SecureDesktopRequest::Takeover);
    if (!control) return nullptr;
    if (!AwaitSecureDesktopAck(*control)) {
        LOGW("[Secure] The service did not confirm the takeover of the unattended host.");
        return nullptr;
    }
    LOGI("[Secure] The unattended host, if any, has stepped aside for this share.");
    return control;
}

bool SendUnattendedConfig(const deskhub::UnattendedConfig& config) {
    const std::vector<uint8_t> payload = deskhub::EncodeUnattendedConfig(config);
    if (payload.empty()) {
        LOGE("[Secure] The unattended configuration does not fit the wire limits.");
        return false;
    }
    std::unique_ptr<OverlappedPipe> control =
        ConnectSecureDesktopService(deskhub::SecureDesktopRequest::Configure);
    if (!control || !control->WriteAll(payload)) return false;
    const bool accepted = AwaitSecureDesktopAck(*control);
    if (accepted)
        LOGI("[Secure] Access before sign-in is now %s.", config.enabled ? "on" : "off");
    else
        LOGW("[Secure] The service did not accept the unattended configuration.");
    return accepted;
}

bool SecureDesktopServiceInstalled() {
    const std::wstring service = DirectoryOf(SelfExePath()) + L"\\" + kSecureDesktopServiceExe;
    const DWORD attributes = GetFileAttributesW(service.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
