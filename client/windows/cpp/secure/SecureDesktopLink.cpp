#include "secure/SecureDesktopLink.h"

#include <string>
#include <utility>
#include <vector>

#include "WinPaths.h"
#include "deskhub/protocol/ByteOrder.h"
#include "deskhubp/diag/Log.h"
#include "secure/SecureDesktopNames.h"

namespace {

constexpr size_t kReadChunk = 1u << 20;
constexpr size_t kHandleBytes = 8;

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

deskhub::SecureDesktopInput ToWireInput(const INPUT& in) {
    deskhub::SecureDesktopInput out;
    if (in.type == INPUT_KEYBOARD) {
        out.kind = deskhub::SecureDesktopInputKind::Keyboard;
        out.vk = in.ki.wVk;
        out.scan = in.ki.wScan;
        out.flags = in.ki.dwFlags;
        return out;
    }
    out.kind = deskhub::SecureDesktopInputKind::Mouse;
    out.dx = in.mi.dx;
    out.dy = in.mi.dy;
    out.data = in.mi.mouseData;
    out.flags = in.mi.dwFlags;
    return out;
}

}

SecureDesktopLink::~SecureDesktopLink() {
    Stop();
}

bool SecureDesktopLink::ConnectToService() {
    const HANDLE pipe = CreateFileW(kSecureDesktopPipeName, GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
            LOGW("[Secure] The DeskhubSecureDesktop service is not running: UAC prompts and the "
                 "lock screen will freeze for viewers. Reinstall DeskHub to add the service.");
        else
            LOGW("[Secure] Could not reach the DeskhubSecureDesktop service (error %lu): UAC "
                 "prompts and the lock screen will freeze for viewers.",
                error);
        return false;
    }
    control_.Reset(pipe);
    if (IsOurService(pipe)) return true;
    LOGW("[Secure] %ls is not served by the DeskhubService.exe installed beside Deskhub.exe; "
         "not using it.",
        kSecureDesktopPipeName);
    control_.Reset(INVALID_HANDLE_VALUE);
    return false;
}

bool SecureDesktopLink::ReceiveAgentPipe() {
    std::vector<uint8_t> bytes;
    uint8_t buf[kHandleBytes];
    size_t got = 0;
    while (bytes.size() < kHandleBytes) {
        if (!control_.ReadSome(std::span<uint8_t>(buf, kHandleBytes - bytes.size()), got)) {
            LOGW("[Secure] The service refused this Deskhub.exe: it only serves the copy "
                 "installed beside it.");
            return false;
        }
        bytes.insert(bytes.end(), buf, buf + got);
    }
    agent_.Reset(reinterpret_cast<HANDLE>(uintptr_t(deskhub::GetU64(bytes.data()))));
    return agent_.Valid();
}

bool SecureDesktopLink::Start(const deskhub::SecureDesktopWatch& watch, FrameSink onFrame,
    StateSink onState) {
    Stop();
    if (!ConnectToService() || !ReceiveAgentPipe()) {
        Stop();
        return false;
    }
    onFrame_ = std::move(onFrame);
    onState_ = std::move(onState);
    if (!agent_.WriteAll(deskhub::EncodeSecureDesktopWatch(watch))) {
        Stop();
        return false;
    }
    reader_ = std::thread([this] { ReadLoop(); });
    LOGI("[Secure] Secure desktop agent connected for source %u.", unsigned(watch.sourceId));
    return true;
}

void SecureDesktopLink::Stop() {
    agent_.Close();
    control_.Close();
    if (reader_.joinable()) reader_.join();
    agent_.Reset(INVALID_HANDLE_VALUE);
    control_.Reset(INVALID_HANDLE_VALUE);
    secure_.store(false, std::memory_order_release);
}

bool SecureDesktopLink::Forward(const INPUT& input) {
    if (!Secure()) return false;
    return agent_.WriteAll(deskhub::EncodeSecureDesktopInput(ToWireInput(input)));
}

void SecureDesktopLink::Dispatch(const deskhub::SecureDesktopMessage& msg) {
    if (msg.type == deskhub::SecureDesktopMsg::DesktopState) {
        const auto secure = deskhub::DecodeSecureDesktopState(msg.payload);
        if (!secure || *secure == Secure()) return;
        secure_.store(*secure, std::memory_order_release);
        if (onState_) onState_(*secure);
        return;
    }
    if (msg.type == deskhub::SecureDesktopMsg::Frame) {
        const auto frame = deskhub::DecodeSecureDesktopFrame(msg.payload);
        if (frame && onFrame_) onFrame_(frame->width, frame->height, frame->bgra);
    }
}

void SecureDesktopLink::ReadLoop() {
    deskhub::SecureDesktopStream stream;
    deskhub::SecureDesktopMessage msg;
    std::vector<uint8_t> buf(kReadChunk);
    size_t got = 0;
    while (agent_.ReadSome(buf, got)) {
        stream.Append(std::span<const uint8_t>(buf.data(), got));
        while (stream.Next(msg)) Dispatch(msg);
        if (stream.Failed()) {
            LOGE("[Secure] The agent sent a malformed message; secure desktop support is off.");
            break;
        }
    }
    if (Secure() && onState_) onState_(false);
    secure_.store(false, std::memory_order_release);
}
