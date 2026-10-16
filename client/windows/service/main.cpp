#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cwchar>

#include "Agent.h"
#include "ServiceBroker.h"
#include "ServiceControl.h"
#include "deskhubp/diag/LogFile.h"
#include "secure/SecureDesktopNames.h"

namespace {

bool IsArg(const wchar_t* arg, const wchar_t* expected) {
    return std::wcscmp(arg, expected) == 0;
}

}

int wmain(int argc, wchar_t** argv) {
    if (argc >= 2 && IsArg(argv[1], L"--install")) return InstallSecureDesktopService();
    if (argc >= 2 && IsArg(argv[1], L"--uninstall")) return UninstallSecureDesktopService();
    if (argc >= 3 && IsArg(argv[1], kSecureDesktopAgentArg)) {
        deskhubp::StartProcessLog();
        return RunSecureDesktopAgent(argv[2]);
    }
    deskhubp::StartProcessLog();
    return RunSecureDesktopService();
}
