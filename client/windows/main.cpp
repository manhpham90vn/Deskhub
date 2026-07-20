// client.exe - MỘT exe kiểu AnyDesk chứa cả hai vai trò (host + client).
//
// GD5: toàn bộ điều khiển qua GUI Win32 (MainMenuWindow.h) - không còn CLI.
// Build: CMake + Ninja (CMakePresets.json, preset x64-debug/x64-release).
// Chạy:  client.exe (không tham số) -> mở màn hình chính: nút "Chia sẻ ứng
// dụng" (mở WindowPickerDialog.h rồi RunAgent) hoặc ô nhập IP + nút "Kết nối"
// (RunClient). Bitrate/FPS/port chỉnh trực tiếp trong cửa sổ đó.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <clocale>
#include <cstdio>

#include "MainMenuWindow.h"
#include "NetTest.h"
#include "WindowCapture.h"

#include <cstring>

int main(int argc, char** argv) {
    // GD5 bỏ CLI, nhưng self-test offline (M1) vẫn phải chạy được - nó không cần
    // GUI, không cần mạng, và là thứ duy nhất kiểm chứng được core trên một máy.
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--nettest") == 0) return RunNetTest();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    // UTF-8 cho console để wprintf in đúng tiêu đề cửa sổ có dấu (tiếng Việt...).
    std::setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    // Log ra ngay cả khi stdout bị redirect (CRT full-buffer khi không phải console).
    setvbuf(stdout, nullptr, _IONBF, 0);
    capture::InitRuntime();

    return RunMainMenuWindow();
}
