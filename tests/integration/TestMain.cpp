#include "Tests.h"
#include "support/TestSupport.h"

#include <cstdio>
#include <string_view>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstdlib>

namespace {

constexpr const char* kTestHome = "/tmp/deskhub-integration-tests";

void KeepTestStateOutOfTheDeveloperHome() {
    if (mkdir(kTestHome, 0700) == 0 || errno == EEXIST) setenv("HOME", kTestHome, 1);
}

void ReportCrashesWithAStack() {}

}
#else
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <system_error>

#include "deskhubp/diag/LogFile.h"

namespace {

bool PointAppDataAt(const char* leaf) {
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / leaf;
    if (ec) return false;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;
    deskhubp::SetAppDataDir(dir.string());
    return true;
}

void KeepTestStateOutOfTheDeveloperHome() {
    if (PointAppDataAt("deskhub-integration-tests")) return;
    std::printf(
        "=== no private app data directory, so this run shares %%USERPROFILE%%\\.deskhub with "
        "every other Deskhub on this machine: the host key and the trusted-host list are one "
        "file each, and a running app or a second test binary can change what a session test "
        "expects halfway through ===\n");
}

#if defined(_M_X64)

void PrintCrashFrame(HANDLE process, int index, uint64_t address) {
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    uint64_t displacement = 0;
    if (SymFromAddr(process, address, &displacement, symbol) != 0) {
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineShift = 0;
        if (SymGetLineFromAddr64(process, address, &lineShift, &line) != 0) {
            std::printf("  #%02d %s+0x%llx (%s:%lu)\n", index, symbol->Name,
                static_cast<unsigned long long>(displacement), line.FileName, line.LineNumber);
            return;
        }
        std::printf("  #%02d %s+0x%llx\n", index, symbol->Name,
            static_cast<unsigned long long>(displacement));
        return;
    }
    HMODULE module = nullptr;
    char moduleName[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(address), &module) != 0)
        GetModuleFileNameA(module, moduleName, sizeof(moduleName));
    std::printf("  #%02d %s+0x%llx\n", index, moduleName,
        static_cast<unsigned long long>(address - reinterpret_cast<uint64_t>(module)));
}

LONG WINAPI ReportFatalException(EXCEPTION_POINTERS* info) {
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    std::printf(
        "=== FATAL: unhandled exception 0x%08lx at 0x%llx on thread %lu - the stack below "
        "is the crash CI cannot otherwise show ===\n",
        record->ExceptionCode,
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(record->ExceptionAddress)),
        GetCurrentThreadId());

    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int i = 0; i < 48; ++i) {
        if (StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame,
                &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                nullptr) == 0)
            break;
        if (frame.AddrPC.Offset == 0) break;
        PrintCrashFrame(process, i, frame.AddrPC.Offset);
    }
    std::fflush(stdout);
    return EXCEPTION_EXECUTE_HANDLER;
}

void PrintCrashStack(const char* headline) {
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    std::printf("=== FATAL: %s on thread %lu ===\n", headline, GetCurrentThreadId());
    void* frames[48] = {};
    const USHORT taken = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (USHORT i = 0; i < taken; ++i)
        PrintCrashFrame(process, int(i), reinterpret_cast<uint64_t>(frames[i]));
    std::fflush(stdout);
    TerminateProcess(process, 3);
}

void ReportTerminate() {
    PrintCrashStack(
        "std::terminate - usually a joinable std::thread destroyed or assigned over, or an "
        "exception nothing caught");
}

void ReportAbort(int) {
    PrintCrashStack(
        "abort() - std::terminate on a thread that never installed its own handler, since "
        "the CRT keeps that handler per thread");
}

void ReportPureCall() {
    PrintCrashStack("a pure virtual call - an object used while its base was being destroyed");
}

void ReportInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned,
    uintptr_t) {
    PrintCrashStack("a CRT call that rejected its arguments");
}

void ReportCrashesWithAStack() {
    SetUnhandledExceptionFilter(ReportFatalException);
    std::set_terminate(ReportTerminate);
    std::signal(SIGABRT, ReportAbort);
    _set_purecall_handler(ReportPureCall);
    _set_invalid_parameter_handler(ReportInvalidParameter);
}

#else

void ReportCrashesWithAStack() {}

#endif

}
#endif

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    ReportCrashesWithAStack();
    KeepTestStateOutOfTheDeveloperHome();

    const bool onlyUnderLoad = argc > 1 && std::string_view(argv[1]) == "--under-load";

    std::printf("=== integration self-test (host + viewer over loopback, fake codecs) ===\n");

    if (!onlyUnderLoad) {
        std::printf("--- wire: golden byte vectors (same on every OS) ---\n");
        RunWireVectorTests();

        std::printf("--- end to end: connect, stream, input, disconnect ---\n");
        RunSessionFlowTests();
    }

    std::printf("--- under load: a file transfer beside a live stream ---\n");
    RunTransferUnderLoadTests();

    std::printf("--- under load: a terminal beside video and bulk transfer ---\n");
    RunLagUnderCrossLoadTests();

    if (g_failures == 0) {
        std::printf("=== PASS: all checks passed ===\n");
        return 0;
    }
    std::printf("=== FAIL: %d checks failed ===\n", g_failures);
    return 1;
}
