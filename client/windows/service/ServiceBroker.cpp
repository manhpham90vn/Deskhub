#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <sddl.h>

#include "ServiceBroker.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "WinPaths.h"
#include "deskhub/protocol/ByteOrder.h"
#include "deskhubp/diag/Log.h"
#include "secure/OverlappedPipe.h"
#include "secure/SecureDesktopNames.h"

#pragma comment(lib, "advapi32.lib")

namespace {

constexpr wchar_t kControlPipeSddl[] = L"D:P(D;;GA;;;NU)(A;;GA;;;SY)(A;;GRGW;;;BA)S:(ML;;NW;;;HI)";
constexpr wchar_t kAgentPipeSddl[] = L"D:P(A;;GA;;;SY)";
constexpr DWORD kPipeBufferBytes = 1u << 20;
constexpr DWORD kStartHintMs = 3000;
constexpr int kMaxConcurrentHosts = 16;

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;
std::atomic<int> g_activeHosts{0};
std::atomic<uint32_t> g_agentPipeCounter{0};

class OwnedHandle {
public:
    explicit OwnedHandle(HANDLE h = nullptr) : h_(h) {}
    ~OwnedHandle() {
        reset();
    }
    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
    HANDLE get() const {
        return h_;
    }
    HANDLE release() {
        HANDLE h = h_;
        h_ = nullptr;
        return h;
    }
    void reset(HANDLE h = nullptr) {
        if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
        h_ = h;
    }
    explicit operator bool() const {
        return h_ && h_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE h_;
};

class SecurityAttributes {
public:
    SecurityAttributes(const wchar_t* sddl, bool inheritable) {
        attributes_.nLength = sizeof(attributes_);
        attributes_.bInheritHandle = inheritable ? TRUE : FALSE;
        ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1,
            &attributes_.lpSecurityDescriptor, nullptr);
    }
    ~SecurityAttributes() {
        if (attributes_.lpSecurityDescriptor) LocalFree(attributes_.lpSecurityDescriptor);
    }
    SecurityAttributes(const SecurityAttributes&) = delete;
    SecurityAttributes& operator=(const SecurityAttributes&) = delete;
    SECURITY_ATTRIBUTES* get() {
        return attributes_.lpSecurityDescriptor ? &attributes_ : nullptr;
    }

private:
    SECURITY_ATTRIBUTES attributes_{};
};

void ReportStatus(DWORD state, DWORD exitCode = NO_ERROR) {
    if (!g_statusHandle) return;
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    status.dwWin32ExitCode = exitCode;
    status.dwWaitHint = state == SERVICE_START_PENDING ? kStartHintMs : 0;
    SetServiceStatus(g_statusHandle, &status);
}

DWORD WINAPI ControlHandler(DWORD control, DWORD, void*, void*) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        ReportStatus(SERVICE_STOP_PENDING);
        SetEvent(g_stopEvent);
    }
    return NO_ERROR;
}

std::wstring ExpectedHostImage() {
    return DirectoryOf(SelfExePath()) + L"\\" + kSecureDesktopHostExe;
}

bool WaitConnected(HANDLE pipe, HANDLE stopEvent) {
    OVERLAPPED ov{};
    OwnedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return false;
    ov.hEvent = event.get();
    if (ConnectNamedPipe(pipe, &ov)) return true;
    const DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) return true;
    if (error != ERROR_IO_PENDING) return false;
    const HANDLE waits[] = {event.get(), stopEvent};
    const DWORD which = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (which != WAIT_OBJECT_0) CancelIoEx(pipe, &ov);
    DWORD ignored = 0;
    return GetOverlappedResult(pipe, &ov, &ignored, TRUE) && which == WAIT_OBJECT_0;
}

struct AgentPipePair {
    OwnedHandle agentEnd;
    OwnedHandle hostEnd;
};

bool CreateAgentPipePair(AgentPipePair& pair) {
    const std::wstring name = std::wstring(kSecureDesktopPipeName) + L"-" +
                              std::to_wstring(GetCurrentProcessId()) + L"-" +
                              std::to_wstring(g_agentPipeCounter.fetch_add(1));
    SecurityAttributes inheritable(kAgentPipeSddl, true);
    if (!inheritable.get()) return false;
    pair.agentEnd.reset(CreateNamedPipeW(name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
        kPipeBufferBytes, kPipeBufferBytes, 0, inheritable.get()));
    if (!pair.agentEnd) return false;
    pair.hostEnd.reset(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
        nullptr));
    if (!pair.hostEnd) return false;
    OVERLAPPED ov{};
    OwnedHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    ov.hEvent = event.get();
    if (!ConnectNamedPipe(pair.agentEnd.get(), &ov) && GetLastError() != ERROR_PIPE_CONNECTED)
        return false;
    return true;
}

OwnedHandle SystemTokenForSession(DWORD sessionId) {
    HANDLE self = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
            TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT |
                TOKEN_ADJUST_SESSIONID,
            &self))
        return OwnedHandle();
    OwnedHandle selfToken(self);
    HANDLE primary = nullptr;
    if (!DuplicateTokenEx(selfToken.get(), MAXIMUM_ALLOWED, nullptr, SecurityImpersonation,
            TokenPrimary, &primary))
        return OwnedHandle();
    OwnedHandle token(primary);
    if (!SetTokenInformation(token.get(), TokenSessionId, &sessionId, sizeof(sessionId)))
        return OwnedHandle();
    return OwnedHandle(token.release());
}

OwnedHandle KillOnCloseJob() {
    OwnedHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) return OwnedHandle();
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits,
            sizeof(limits)))
        return OwnedHandle();
    return OwnedHandle(job.release());
}

OwnedHandle SpawnAgent(DWORD sessionId, HANDLE agentPipe, HANDLE job) {
    OwnedHandle token = SystemTokenForSession(sessionId);
    if (!token) {
        LOGE("[Service] Could not build a SYSTEM token for session %lu: %lu", sessionId,
            GetLastError());
        return OwnedHandle();
    }

    SIZE_T attrBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrBytes);
    std::vector<uint8_t> attrStorage(attrBytes);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrStorage.data());
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrBytes)) return OwnedHandle();
    HANDLE inherit[] = {agentPipe};
    const bool listed = UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherit, sizeof(inherit), nullptr, nullptr);
    if (!listed) {
        DeleteProcThreadAttributeList(attrs);
        return OwnedHandle();
    }

    std::wstring desktop = L"winsta0\\default";
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.lpDesktop = desktop.data();
    si.lpAttributeList = attrs;

    const std::wstring exe = SelfExePath();
    std::wstring cmd = L"\"" + exe + L"\" " + kSecureDesktopAgentArg + L" " +
                       std::to_wstring(uintptr_t(agentPipe));
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessAsUserW(token.get(), exe.c_str(), cmd.data(), nullptr,
        nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr, nullptr, &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(attrs);
    if (!created) {
        LOGE("[Service] Could not start the secure desktop agent in session %lu: %lu", sessionId,
            GetLastError());
        return OwnedHandle();
    }
    OwnedHandle thread(pi.hThread);
    OwnedHandle process(pi.hProcess);
    if (!AssignProcessToJobObject(job, process.get())) {
        LOGE("[Service] Could not tie the agent to the host's lifetime: %lu", GetLastError());
        TerminateProcess(process.get(), 1);
        return OwnedHandle();
    }
    ResumeThread(thread.get());
    return OwnedHandle(process.release());
}

bool SendHandleValue(OverlappedPipe& control, HANDLE remote) {
    uint8_t bytes[8];
    deskhub::PutU64(bytes, uint64_t(uintptr_t(remote)));
    return control.WriteAll(bytes);
}

void WaitForHostToLeave(OverlappedPipe& control, HANDLE agentProcess) {
    OwnedHandle done(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::thread watcher([&control, agentProcess, doneEvent = done.get()] {
        const HANDLE waits[] = {doneEvent, g_stopEvent, agentProcess};
        if (WaitForMultipleObjects(3, waits, FALSE, INFINITE) != WAIT_OBJECT_0) control.Close();
    });
    uint8_t sink[64];
    size_t got = 0;
    while (control.ReadSome(sink, got)) {
    }
    SetEvent(done.get());
    watcher.join();
}

void ServeHost(HANDLE pipeHandle) {
    OverlappedPipe control(pipeHandle);
    ULONG pid = 0;
    if (!GetNamedPipeClientProcessId(control.Handle(), &pid)) return;

    OwnedHandle host(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_DUP_HANDLE, FALSE, pid));
    if (!host) {
        LOGW("[Service] Could not open the connecting process %lu: %lu", pid, GetLastError());
        return;
    }
    const std::wstring image = ProcessImagePath(host.get());
    if (!SamePath(image, ExpectedHostImage())) {
        LOGW("[Service] Refused process %lu: it is not the Deskhub.exe installed beside this "
             "service (%ls).",
            pid, image.c_str());
        return;
    }
    DWORD sessionId = 0;
    if (!ProcessIdToSessionId(pid, &sessionId) || sessionId == 0) {
        LOGW("[Service] Refused process %lu: it is not in an interactive session.", pid);
        return;
    }

    AgentPipePair pair;
    if (!CreateAgentPipePair(pair)) {
        LOGE("[Service] Could not create the agent pipe: %lu", GetLastError());
        return;
    }
    OwnedHandle job = KillOnCloseJob();
    if (!job) {
        LOGE("[Service] Could not create the agent job: %lu", GetLastError());
        return;
    }
    OwnedHandle agent = SpawnAgent(sessionId, pair.agentEnd.get(), job.get());
    if (!agent) return;
    pair.agentEnd.reset();

    HANDLE remote = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), pair.hostEnd.release(), host.get(), &remote, 0,
            FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        LOGE("[Service] Could not hand the agent pipe to process %lu: %lu", pid, GetLastError());
        return;
    }
    if (!SendHandleValue(control, remote)) return;
    LOGI("[Service] Secure desktop agent started for Deskhub.exe (pid %lu, session %lu).", pid,
        sessionId);
    WaitForHostToLeave(control, agent.get());
    LOGI("[Service] Deskhub.exe (pid %lu) left; stopping its agent.", pid);
}

void StartHostThread(HANDLE pipe) {
    if (g_activeHosts.load() >= kMaxConcurrentHosts) {
        LOGW("[Service] Refused a connection: %d hosts are already served.", kMaxConcurrentHosts);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return;
    }
    g_activeHosts.fetch_add(1);
    std::thread([pipe] {
        ServeHost(pipe);
        g_activeHosts.fetch_sub(1);
    }).detach();
}

void ServePipe() {
    SecurityAttributes security(kControlPipeSddl, false);
    if (!security.get()) {
        LOGE("[Service] Could not build the pipe security descriptor: %lu", GetLastError());
        return;
    }
    bool first = true;
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        const DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                               (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        HANDLE pipe = CreateNamedPipeW(kSecureDesktopPipeName, openMode,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, security.get());
        if (pipe == INVALID_HANDLE_VALUE) {
            LOGE("[Service] Could not create %ls: %lu", kSecureDesktopPipeName, GetLastError());
            return;
        }
        first = false;
        if (!WaitConnected(pipe, g_stopEvent)) {
            CloseHandle(pipe);
            continue;
        }
        StartHostThread(pipe);
    }
}

void WINAPI ServiceMain(DWORD, wchar_t**) {
    g_statusHandle = RegisterServiceCtrlHandlerExW(kSecureDesktopServiceName, ControlHandler, nullptr);
    if (!g_statusHandle) return;
    ReportStatus(SERVICE_START_PENDING);
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        ReportStatus(SERVICE_STOPPED, GetLastError());
        return;
    }
    ReportStatus(SERVICE_RUNNING);
    LOGI("[Service] Running; waiting for Deskhub.exe on %ls.", kSecureDesktopPipeName);
    ServePipe();
    LOGI("[Service] Stopped.");
    ReportStatus(SERVICE_STOPPED);
}

}

int RunSecureDesktopService() {
    std::wstring name = kSecureDesktopServiceName;
    SERVICE_TABLE_ENTRYW table[] = {{name.data(), ServiceMain}, {nullptr, nullptr}};
    if (StartServiceCtrlDispatcherW(table)) return 0;
    LOGE("DeskhubService.exe is a Windows service: install it with --install from an "
         "elevated prompt, or let the DeskHub installer do it (error %lu).",
        GetLastError());
    return 1;
}
