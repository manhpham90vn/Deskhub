# Cần GNU make. Tự tìm Visual Studio qua vswhere và nạp VsDevCmd,
# nên chạy được từ cmd / PowerShell / Git Bash thường (không cần Developer prompt).
#   make            build debug
#   make release    build release
#   make run ARGS="notepad.exe --loopback"
#   make test       chạy core_tests (offline)
#   make format     áp clang-format (C++) + ktlint (Kotlin) tại chỗ
#   make lint       kiểm tra style, không sửa (dùng trước khi push cho khớp CI)
#   make clean

SHELL := cmd.exe
.SHELLFLAGS := /c

VSWHERE := C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe
VSDIR   := $(shell "$(VSWHERE)" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
VSDEV   := $(VSDIR)\Common7\Tools\VsDevCmd.bat
DEVCMD  := call "$(VSDEV)" -arch=x64 -host_arch=x64 -no_logo &&

all: debug

debug:
	@$(DEVCMD) cmake --preset x64-debug && cmake --build --preset x64-debug

release:
	@$(DEVCMD) cmake --preset x64-release && cmake --build --preset x64-release

run: debug
	out\build\x64-debug\client\windows\client.exe $(ARGS)

# Test của core: offline, không cần mạng/GPU. Exit code 0 = pass.
test: debug
	out\build\x64-debug\core\core_tests.exe

# Format/lint dùng chung một script PowerShell (tự dò clang-format trong VS + ktlint).
format:
	@powershell -NoProfile -ExecutionPolicy Bypass -File scripts\codestyle.ps1

lint:
	@powershell -NoProfile -ExecutionPolicy Bypass -File scripts\codestyle.ps1 -Check

clean:
	@$(DEVCMD) cmake -E rm -rf out

.PHONY: all debug release run test format lint clean
