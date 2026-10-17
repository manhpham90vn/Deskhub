#include "secure/SecureDesktopLink.h"

#include <string>
#include <utility>
#include <vector>

#include "deskhub/protocol/ByteOrder.h"
#include "deskhubp/diag/Log.h"
#include "secure/ServiceControlPipe.h"
#include "secure/UnattendedMode.h"

namespace {

constexpr size_t kReadChunk = 1u << 20;
constexpr size_t kHandleBytes = 8;

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

bool SecureDesktopLink::ReceiveAgentPipe() {
    std::vector<uint8_t> bytes;
    uint8_t buf[kHandleBytes];
    size_t got = 0;
    while (bytes.size() < kHandleBytes) {
        if (!control_->ReadSome(std::span<uint8_t>(buf, kHandleBytes - bytes.size()), got)) {
            LOGW(
                "[Secure] The service refused this process: it only serves the programs "
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
    control_ = ConnectSecureDesktopService(deskhub::SecureDesktopRequest::Agent);
    if (!control_ || !ReceiveAgentPipe()) {
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
    if (control_) control_->Close();
    if (reader_.joinable()) reader_.join();
    agent_.Reset(INVALID_HANDLE_VALUE);
    control_.reset();
    secure_.store(false, std::memory_order_release);
}

bool SecureDesktopLink::Forward(const INPUT& input) {
    if (!Secure() && !UnattendedMode()) return false;
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
