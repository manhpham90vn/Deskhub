#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "ServiceControl.h"

#include <cstdio>
#include <string>

#include "MachineFolder.h"
#include "WinPaths.h"
#include "secure/SecureDesktopNames.h"

#pragma comment(lib, "advapi32.lib")

namespace {

constexpr DWORD kRestartDelayMs = 5000;
constexpr DWORD kFailureResetSeconds = 86400;
constexpr DWORD kStopWaitMs = 15000;
constexpr DWORD kStopPollMs = 200;

class ScHandle {
public:
    explicit ScHandle(SC_HANDLE h) : h_(h) {}
    ~ScHandle() {
        if (h_) CloseServiceHandle(h_);
    }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    SC_HANDLE get() const {
        return h_;
    }
    explicit operator bool() const {
        return h_ != nullptr;
    }

private:
    SC_HANDLE h_;
};

int Fail(const char* what) {
    std::fprintf(stderr, "DeskhubService: %s failed (error %lu).\n", what, GetLastError());
    return 1;
}

void SetRestartOnFailure(SC_HANDLE service) {
    SC_ACTION actions[3] = {{SC_ACTION_RESTART, kRestartDelayMs},
        {SC_ACTION_RESTART, kRestartDelayMs}, {SC_ACTION_RESTART, kRestartDelayMs}};
    SERVICE_FAILURE_ACTIONSW failure{};
    failure.dwResetPeriod = kFailureResetSeconds;
    failure.cActions = 3;
    failure.lpsaActions = actions;
    ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);
}

void SetDescription(SC_HANDLE service) {
    std::wstring text = kSecureDesktopServiceDescription;
    SERVICE_DESCRIPTIONW description{text.data()};
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description);
}

bool WaitUntilStopped(SC_HANDLE service) {
    for (DWORD waited = 0; waited < kStopWaitMs; waited += kStopPollMs) {
        SERVICE_STATUS status{};
        if (!QueryServiceStatus(service, &status)) return false;
        if (status.dwCurrentState == SERVICE_STOPPED) return true;
        Sleep(kStopPollMs);
    }
    return false;
}

}

int InstallSecureDesktopService() {
    const std::wstring exe = SelfExePath();
    if (exe.empty()) return Fail("GetModuleFileName");
    const std::wstring binaryPath = L"\"" + exe + L"\"";

    const ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (!scm) return Fail("OpenSCManager (run the installer as an administrator)");

    ScHandle service(CreateServiceW(scm.get(), kSecureDesktopServiceName,
        kSecureDesktopServiceDisplayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, binaryPath.c_str(), nullptr, nullptr, nullptr,
        nullptr, nullptr));
    if (!service && GetLastError() == ERROR_SERVICE_EXISTS) {
        ScHandle existing(OpenServiceW(scm.get(), kSecureDesktopServiceName, SERVICE_ALL_ACCESS));
        if (!existing) return Fail("OpenService");
        if (!ChangeServiceConfigW(existing.get(), SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                SERVICE_ERROR_NORMAL, binaryPath.c_str(), nullptr, nullptr, nullptr, nullptr,
                nullptr, kSecureDesktopServiceDisplayName))
            return Fail("ChangeServiceConfig");
        SetDescription(existing.get());
        SetRestartOnFailure(existing.get());
        if (!StartServiceW(existing.get(), 0, nullptr) &&
            GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
            return Fail("StartService");
        return 0;
    }
    if (!service) return Fail("CreateService");
    SetDescription(service.get());
    SetRestartOnFailure(service.get());
    if (!StartServiceW(service.get(), 0, nullptr)) return Fail("StartService");
    return 0;
}

int UninstallSecureDesktopService() {
    const ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) return Fail("OpenSCManager (run the uninstaller as an administrator)");
    const ScHandle service(OpenServiceW(scm.get(), kSecureDesktopServiceName,
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (!service) return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : Fail("OpenService");
    SERVICE_STATUS status{};
    if (ControlService(service.get(), SERVICE_CONTROL_STOP, &status) &&
        !WaitUntilStopped(service.get()))
        std::fprintf(stderr, "DeskhubService: the service did not stop within %lu ms.\n",
            kStopWaitMs);
    if (!DeleteService(service.get()) && GetLastError() != ERROR_SERVICE_MARKED_FOR_DELETE)
        return Fail("DeleteService");
    if (!WipeMachineFolder())
        std::fprintf(stderr, "DeskhubService: could not remove %ls.\n", MachineFolderPath().c_str());
    return 0;
}
