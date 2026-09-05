#include "deskhubp/system/Pty.h"

#include <windows.h>

#include <cstdlib>
#include <vector>

#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

constexpr DWORD kShellReapWaitMs = 5000;

COORD ToCoord(deskhub::TermSize size) {
    const deskhub::TermSize clamped = deskhub::ClampTermSize(size);
    COORD out{};
    out.X = SHORT(clamped.cols);
    out.Y = SHORT(clamped.rows);
    return out;
}

std::vector<wchar_t> ToWide(const std::string& text) {
    const int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
    std::vector<wchar_t> out(size_t(need) + 1, L'\0');
    if (need > 0)
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), out.data(), need);
    return out;
}

bool StandardHandlesAreConsole() {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == nullptr || out == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    return GetConsoleMode(out, &mode) != 0;
}

std::string EnvOr(const char* name, const char* fallback) {
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) return fallback;
    std::string out(value);
    std::free(value);
    return out.empty() ? std::string(fallback) : out;
}

}

struct Pty::Impl {
    HPCON console = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    PROCESS_INFORMATION process{};
    std::vector<uint8_t> attributeList{};
    STARTUPINFOEXW startup{};
    bool exited = false;
    int exitCode = 0;

    void Poll() {
        if (exited || process.hProcess == nullptr) return;
        DWORD code = 0;
        if (!GetExitCodeProcess(process.hProcess, &code)) return;
        if (code == STILL_ACTIVE) return;
        exited = true;
        exitCode = int(code);
    }

    void Shutdown() {
        if (inputWrite != nullptr) {
            CloseHandle(inputWrite);
            inputWrite = nullptr;
        }
        if (outputRead != nullptr) {
            CloseHandle(outputRead);
            outputRead = nullptr;
        }
        if (console != nullptr) {
            ClosePseudoConsole(console);
            console = nullptr;
        }
        if (startup.lpAttributeList != nullptr) {
            DeleteProcThreadAttributeList(startup.lpAttributeList);
            startup.lpAttributeList = nullptr;
        }
        if (process.hProcess != nullptr) {
            DWORD code = 0;
            if (GetExitCodeProcess(process.hProcess, &code) && code == STILL_ACTIVE) {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, kShellReapWaitMs);
            }
        }
        if (process.hThread != nullptr) {
            CloseHandle(process.hThread);
            process.hThread = nullptr;
        }
        if (process.hProcess != nullptr) {
            CloseHandle(process.hProcess);
            process.hProcess = nullptr;
        }
    }
};

Pty::Pty() : impl_(std::make_unique<Impl>()) {
}

Pty::~Pty() {
    impl_->Shutdown();
}

std::string DefaultShell() {
    std::string comspec = EnvOr("COMSPEC", "cmd.exe");
    const std::string root = EnvOr("SystemRoot", "C:\\Windows");
    std::string powershell = root + "\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (GetFileAttributesA(powershell.c_str()) != INVALID_FILE_ATTRIBUTES) return powershell;
    return comspec;
}

bool Pty::Start(const std::string& shell, deskhub::TermSize size) {
    if (impl_->console != nullptr) return false;

    HANDLE inputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&inputRead, &impl_->inputWrite, nullptr, 0) ||
        !CreatePipe(&impl_->outputRead, &outputWrite, nullptr, 0)) {
        LOGE("pty: could not create the console pipes");
        impl_->Shutdown();
        return false;
    }

    const HRESULT created = CreatePseudoConsole(ToCoord(size), inputRead, outputWrite, 0,
        &impl_->console);
    CloseHandle(inputRead);
    CloseHandle(outputWrite);
    if (FAILED(created)) {
        LOGE("pty: CreatePseudoConsole failed (0x%08lx)", static_cast<unsigned long>(created));
        impl_->Shutdown();
        return false;
    }

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    impl_->attributeList.assign(attributeBytes, 0);
    impl_->startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    impl_->startup.lpAttributeList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(impl_->attributeList.data());
    if (!InitializeProcThreadAttributeList(impl_->startup.lpAttributeList, 1, 0,
            &attributeBytes)) {
        LOGE("pty: InitializeProcThreadAttributeList failed (%lu)", GetLastError());
        impl_->Shutdown();
        return false;
    }
    if (!UpdateProcThreadAttribute(impl_->startup.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, impl_->console, sizeof(impl_->console), nullptr,
            nullptr)) {
        LOGE("pty: UpdateProcThreadAttribute failed (%lu)", GetLastError());
        impl_->Shutdown();
        return false;
    }
    if (!StandardHandlesAreConsole()) {
        impl_->startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        impl_->startup.StartupInfo.hStdInput = nullptr;
        impl_->startup.StartupInfo.hStdOutput = nullptr;
        impl_->startup.StartupInfo.hStdError = nullptr;
    }

    const std::string command = shell.empty() ? DefaultShell() : shell;
    std::vector<wchar_t> mutableCommand = ToWide(command);

    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
            &impl_->startup.StartupInfo, &impl_->process)) {
        LOGE("pty: could not start %s (%lu)", command.c_str(), GetLastError());
        impl_->Shutdown();
        return false;
    }

    impl_->exited = false;
    impl_->exitCode = 0;
    return true;
}

bool Pty::Running() const {
    return impl_->console != nullptr && !impl_->exited;
}

int Pty::Read(uint8_t* buf, size_t cap, uint32_t waitMs) {
    if (impl_->outputRead == nullptr) return -1;
    DWORD available = 0;
    const ULONGLONG deadline = GetTickCount64() + waitMs;
    for (;;) {
        if (!PeekNamedPipe(impl_->outputRead, nullptr, 0, nullptr, &available, nullptr)) {
            impl_->Poll();
            impl_->exited = true;
            return -1;
        }
        if (available > 0) break;
        impl_->Poll();
        if (impl_->exited) return -1;
        if (GetTickCount64() >= deadline) return 0;
        Sleep(1);
    }

    DWORD read = 0;
    const DWORD want = DWORD(cap < available ? cap : available);
    if (!ReadFile(impl_->outputRead, buf, want, &read, nullptr)) {
        impl_->Poll();
        impl_->exited = true;
        return -1;
    }
    return int(read);
}

bool Pty::Write(std::span<const uint8_t> bytes) {
    if (impl_->inputWrite == nullptr) return false;
    size_t at = 0;
    while (at < bytes.size()) {
        DWORD written = 0;
        if (!WriteFile(impl_->inputWrite, bytes.data() + at, DWORD(bytes.size() - at), &written,
                nullptr))
            return false;
        if (written == 0) return false;
        at += written;
    }
    return true;
}

bool Pty::Resize(deskhub::TermSize size) {
    if (impl_->console == nullptr) return false;
    return SUCCEEDED(ResizePseudoConsole(impl_->console, ToCoord(size)));
}

bool Pty::Exited() const {
    impl_->Poll();
    return impl_->exited;
}

int Pty::ExitCode() const {
    return impl_->exitCode;
}

void Pty::Close() {
    impl_->Shutdown();
}

}
