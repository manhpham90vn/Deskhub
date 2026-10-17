#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <sddl.h>
#include <wtsapi32.h>

#include "ServiceBroker.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "MachineFolder.h"
#include "WinPaths.h"
#include "deskhub/protocol/ByteOrder.h"
#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/OverlappedPipe.h"
#include "secure/SecureDesktopNames.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")

namespace {

constexpr wchar_t kControlPipeSddl[] = L"D:P(D;;GA;;;NU)(A;;GA;;;SY)(A;;GRGW;;;BA)S:(ML;;NW;;;HI)";
constexpr wchar_t kAgentPipeSddl[] = L"D:P(A;;GA;;;SY)";
constexpr DWORD kPipeBufferBytes = 1u << 20;
constexpr DWORD kStartHintMs = 3000;
constexpr DWORD kSupervisorPollMs = 2000;
constexpr DWORD kRespawnBackoffMs = 2000;
constexpr DWORD kHostExitWaitMs = 5000;
constexpr DWORD kNoConsoleSession = 0xFFFFFFFF;
constexpr int kMaxConcurrentHosts = 16;

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;
HANDLE g_stateChanged = nullptr;
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

struct UnattendedSupervisor {
    std::mutex mutex;
    int takeoverHolds = 0;
    OwnedHandle job;
    OwnedHandle process;
    uint64_t lastExitMs = 0;
};

UnattendedSupervisor g_unattended;

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

class InheritedHandleList {
public:
    explicit InheritedHandleList(HANDLE handle) : handle_(handle) {
        if (!handle_) return;
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        storage_.resize(bytes);
        auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(attrs, 1, 0, &bytes)) return;
        initialized_ = true;
        ok_ = UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &handle_,
            sizeof(handle_), nullptr, nullptr);
    }
    ~InheritedHandleList() {
        if (initialized_)
            DeleteProcThreadAttributeList(
                reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data()));
    }
    InheritedHandleList(const InheritedHandleList&) = delete;
    InheritedHandleList& operator=(const InheritedHandleList&) = delete;

    bool ok() const {
        return !handle_ || ok_;
    }
    LPPROC_THREAD_ATTRIBUTE_LIST attributes() {
        return handle_ ? reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data())
                       : nullptr;
    }

private:
    HANDLE handle_;
    std::vector<uint8_t> storage_;
    bool initialized_ = false;
    bool ok_ = false;
};

OwnedHandle SpawnInSession(DWORD sessionId, HANDLE job, const std::wstring& arguments,
    HANDLE inherited, const char* what) {
    OwnedHandle token = SystemTokenForSession(sessionId);
    if (!token) {
        LOGE("[Service] Could not build a SYSTEM token for session %lu: %lu", sessionId,
            GetLastError());
        return OwnedHandle();
    }
    InheritedHandleList handles(inherited);
    if (!handles.ok()) return OwnedHandle();

    std::wstring desktop = L"winsta0\\default";
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.lpDesktop = desktop.data();
    si.lpAttributeList = handles.attributes();

    const std::wstring exe = SelfExePath();
    std::wstring cmd = L"\"" + exe + L"\" " + arguments;
    const DWORD flags = CREATE_NO_WINDOW | CREATE_SUSPENDED |
                        (inherited ? EXTENDED_STARTUPINFO_PRESENT : 0);
    PROCESS_INFORMATION pi{};
    const BOOL created = CreateProcessAsUserW(token.get(), exe.c_str(), cmd.data(), nullptr,
        nullptr, inherited ? TRUE : FALSE, flags, nullptr, nullptr, &si.StartupInfo, &pi);
    if (!created) {
        LOGE("[Service] Could not start the %s in session %lu: %lu", what, sessionId,
            GetLastError());
        return OwnedHandle();
    }
    OwnedHandle thread(pi.hThread);
    OwnedHandle process(pi.hProcess);
    if (!AssignProcessToJobObject(job, process.get())) {
        LOGE("[Service] Could not tie the %s to its job: %lu", what, GetLastError());
        TerminateProcess(process.get(), 1);
        return OwnedHandle();
    }
    ResumeThread(thread.get());
    return OwnedHandle(process.release());
}

OwnedHandle SpawnAgent(DWORD sessionId, HANDLE agentPipe, HANDLE job) {
    const std::wstring arguments =
        std::wstring(kSecureDesktopAgentArg) + L" " + std::to_wstring(uintptr_t(agentPipe));
    return SpawnInSession(sessionId, job, arguments, agentPipe, "secure desktop agent");
}

bool SendByte(OverlappedPipe& control, uint8_t byte) {
    return control.WriteAll(std::span<const uint8_t>(&byte, 1));
}

bool SendHandleValue(OverlappedPipe& control, HANDLE remote) {
    uint8_t bytes[8];
    deskhub::PutU64(bytes, uint64_t(uintptr_t(remote)));
    return control.WriteAll(bytes);
}

bool ReadExactly(OverlappedPipe& control, std::span<uint8_t> into) {
    size_t filled = 0;
    while (filled < into.size()) {
        size_t got = 0;
        if (!control.ReadSome(into.subspan(filled), got)) return false;
        filled += got;
    }
    return true;
}

void DrainUntilClosed(OverlappedPipe& control, HANDLE alsoStopOn) {
    OwnedHandle done(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::thread watcher([&control, alsoStopOn, doneEvent = done.get()] {
        std::vector<HANDLE> waits = {doneEvent, g_stopEvent};
        if (alsoStopOn) waits.push_back(alsoStopOn);
        if (WaitForMultipleObjects(DWORD(waits.size()), waits.data(), FALSE, INFINITE) !=
            WAIT_OBJECT_0)
            control.Close();
    });
    uint8_t sink[64];
    size_t got = 0;
    while (control.ReadSome(sink, got)) {
    }
    SetEvent(done.get());
    watcher.join();
}

bool UnattendedHostAlive() {
    return g_unattended.process &&
           WaitForSingleObject(g_unattended.process.get(), 0) == WAIT_TIMEOUT;
}

void StopUnattendedHostLocked() {
    if (g_unattended.process) {
        TerminateProcess(g_unattended.process.get(), 0);
        if (WaitForSingleObject(g_unattended.process.get(), kHostExitWaitMs) != WAIT_OBJECT_0)
            LOGW("[Service] The unattended host did not exit within %lu ms.", kHostExitWaitMs);
        else
            LOGI("[Service] The unattended host stepped aside.");
    }
    g_unattended.process.reset();
    g_unattended.job.reset();
    g_unattended.lastExitMs = GetTickCount64();
}

void StopUnattendedHost() {
    const std::lock_guard<std::mutex> lock(g_unattended.mutex);
    StopUnattendedHostLocked();
}

void SpawnUnattendedHostLocked() {
    const DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == kNoConsoleSession || sessionId == 0) return;
    OwnedHandle job = KillOnCloseJob();
    if (!job) {
        LOGE("[Service] Could not create the unattended host job: %lu", GetLastError());
        return;
    }
    OwnedHandle process =
        SpawnInSession(sessionId, job.get(), kUnattendedHostArg, nullptr, "unattended host");
    if (!process) {
        g_unattended.lastExitMs = GetTickCount64();
        return;
    }
    LOGI("[Service] Unattended host started in session %lu (pid %lu).", sessionId,
        GetProcessId(process.get()));
    g_unattended.job.reset(job.release());
    g_unattended.process.reset(process.release());
}

OwnedHandle DuplicateForWait(HANDLE process) {
    HANDLE copy = nullptr;
    if (!process || !DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(), &copy,
                        SYNCHRONIZE, FALSE, 0))
        return OwnedHandle();
    return OwnedHandle(copy);
}

void SuperviseUnattendedHost() {
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        OwnedHandle process;
        {
            const std::lock_guard<std::mutex> lock(g_unattended.mutex);
            if (g_unattended.process && !UnattendedHostAlive()) {
                LOGW("[Service] The unattended host exited; it will be started again.");
                g_unattended.process.reset();
                g_unattended.job.reset();
                g_unattended.lastExitMs = GetTickCount64();
            }
            const bool wanted = g_unattended.takeoverHolds == 0 && UnattendedEnabled();
            const bool backedOff = GetTickCount64() - g_unattended.lastExitMs >= kRespawnBackoffMs;
            if (wanted && !g_unattended.process && backedOff) SpawnUnattendedHostLocked();
            if (!wanted && g_unattended.process) StopUnattendedHostLocked();
            process.reset(DuplicateForWait(g_unattended.process.get()).release());
        }
        std::vector<HANDLE> waits = {g_stopEvent, g_stateChanged};
        if (process) waits.push_back(process.get());
        WaitForMultipleObjects(DWORD(waits.size()), waits.data(), FALSE, kSupervisorPollMs);
    }
    StopUnattendedHost();
}

void ServeAgent(OverlappedPipe& control, HANDLE host, ULONG pid, DWORD sessionId) {
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
    if (!DuplicateHandle(GetCurrentProcess(), pair.hostEnd.release(), host, &remote, 0, FALSE,
            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        LOGE("[Service] Could not hand the agent pipe to process %lu: %lu", pid, GetLastError());
        return;
    }
    if (!SendHandleValue(control, remote)) return;
    LOGI("[Service] Secure desktop agent started for pid %lu, session %lu.", pid, sessionId);
    DrainUntilClosed(control, agent.get());
    LOGI("[Service] Process %lu left; stopping its agent.", pid);
}

void ServeTakeover(OverlappedPipe& control, ULONG pid) {
    {
        const std::lock_guard<std::mutex> lock(g_unattended.mutex);
        ++g_unattended.takeoverHolds;
        StopUnattendedHostLocked();
    }
    LOGI("[Service] Process %lu is taking over from the unattended host.", pid);
    if (SendByte(control, deskhub::kSecureDesktopAck)) DrainUntilClosed(control, nullptr);
    {
        const std::lock_guard<std::mutex> lock(g_unattended.mutex);
        --g_unattended.takeoverHolds;
    }
    SetEvent(g_stateChanged);
    LOGI("[Service] Process %lu stopped sharing; the unattended host may return.", pid);
}

std::optional<deskhub::UnattendedConfig> ReadConfig(OverlappedPipe& control) {
    uint8_t prefix[deskhub::kUnattendedConfigLengthSize];
    if (!ReadExactly(control, prefix)) return std::nullopt;
    const std::optional<uint32_t> length = deskhub::DecodeUnattendedConfigLength(prefix);
    if (!length) return std::nullopt;
    std::vector<uint8_t> body(*length);
    if (!ReadExactly(control, body)) return std::nullopt;
    return deskhub::DecodeUnattendedConfig(body);
}

void RemoveUnattendedFiles() {
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kAuthorizedKeysFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kUiSettingsFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kAccessRequestsFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kPairingTokensFileName);
}

bool ApplyConfig(const deskhub::UnattendedConfig& config) {
    const std::lock_guard<std::mutex> lock(g_unattended.mutex);
    if (!config.enabled) {
        StopUnattendedHostLocked();
        RemoveUnattendedFiles();
        LOGI("[Service] Access before sign-in switched off; the machine copies are removed.");
        return true;
    }
    const bool restart = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName) != config.hostKeyPem ||
                         deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName) != config.settings;
    const bool written =
        deskhubp::WriteAppDataFileAtomic(deskhubp::kHostKeyFileName, config.hostKeyPem) &&
        deskhubp::WriteAppDataFileAtomic(deskhubp::kAuthorizedKeysFileName, config.authorizedKeys) &&
        deskhubp::WriteAppDataFileAtomic(deskhubp::kUiSettingsFileName, config.settings);
    if (!written) {
        LOGE("[Service] Could not write the unattended configuration into %ls.",
            MachineFolderPath().c_str());
        return false;
    }
    if (restart) {
        StopUnattendedHostLocked();
        g_unattended.lastExitMs = 0;
    }
    LOGI("[Service] Access before sign-in is on; %s.",
        restart ? "the unattended host restarts with the new key or settings"
                : "the allowed list is updated in place");
    return true;
}

void ServeConfigure(OverlappedPipe& control, ULONG pid) {
    const std::optional<deskhub::UnattendedConfig> config = ReadConfig(control);
    if (!config) {
        LOGW("[Service] Process %lu sent an unusable unattended configuration.", pid);
        return;
    }
    if (!ApplyConfig(*config)) return;
    SetEvent(g_stateChanged);
    SendByte(control, deskhub::kSecureDesktopAck);
}

bool TrustedClient(HANDLE host, ULONG pid, DWORD& sessionId, bool& mayControl) {
    const std::wstring image = ProcessImagePath(host);
    const std::wstring folder = DirectoryOf(SelfExePath()) + L"\\";
    mayControl = SamePath(image, folder + kSecureDesktopHostExe) ||
                 SamePath(image, folder + kSecureDesktopCliExe);
    const bool isService = SamePath(image, SelfExePath());
    if (!mayControl && !isService) {
        LOGW(
            "[Service] Refused process %lu: it is not the Deskhub.exe, deskhub-cli.exe or "
            "DeskhubService.exe installed beside this service (%ls).",
            pid, image.c_str());
        return false;
    }
    if (!ProcessIdToSessionId(pid, &sessionId) || sessionId == 0) {
        LOGW("[Service] Refused process %lu: it is not in an interactive session.", pid);
        return false;
    }
    return true;
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
    DWORD sessionId = 0;
    bool mayControl = false;
    if (!TrustedClient(host.get(), pid, sessionId, mayControl)) return;

    uint8_t requestByte = 0;
    if (!ReadExactly(control, std::span<uint8_t>(&requestByte, 1))) return;
    const std::optional<deskhub::SecureDesktopRequest> request =
        deskhub::DecodeSecureDesktopRequest(requestByte);
    if (!request) {
        LOGW("[Service] Process %lu sent an unknown request %u.", pid, unsigned(requestByte));
        return;
    }
    if (*request == deskhub::SecureDesktopRequest::Agent) {
        ServeAgent(control, host.get(), pid, sessionId);
        return;
    }
    if (!mayControl) {
        LOGW(
            "[Service] Refused request %u from pid %lu: only Deskhub.exe or deskhub-cli.exe "
            "may send it.",
            unsigned(requestByte), pid);
        return;
    }
    if (*request == deskhub::SecureDesktopRequest::Takeover)
        ServeTakeover(control, pid);
    else
        ServeConfigure(control, pid);
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
    g_stateChanged = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_stopEvent || !g_stateChanged) {
        ReportStatus(SERVICE_STOPPED, GetLastError());
        return;
    }
    ReportStatus(SERVICE_RUNNING);
    LOGI("[Service] Running; waiting for hosts on %ls.", kSecureDesktopPipeName);
    std::thread supervisor(SuperviseUnattendedHost);
    ServePipe();
    SetEvent(g_stopEvent);
    supervisor.join();
    LOGI("[Service] Stopped.");
    ReportStatus(SERVICE_STOPPED);
}

}

int RunSecureDesktopService() {
    std::wstring name = kSecureDesktopServiceName;
    SERVICE_TABLE_ENTRYW table[] = {{name.data(), ServiceMain}, {nullptr, nullptr}};
    if (StartServiceCtrlDispatcherW(table)) return 0;
    LOGE(
        "DeskhubService.exe is a Windows service: install it with --install from an "
        "elevated prompt, or let the DeskHub installer do it (error %lu).",
        GetLastError());
    return 1;
}
