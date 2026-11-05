ifeq ($(OS),Windows_NT)
APP_EXE_DEBUG   := out\build\x64-debug\client\windows\win32\Deskhub.exe
APP_EXE_RELEASE := out\build\x64-release\client\windows\win32\Deskhub.exe

build-windows: debug

release-windows: release

run-windows: export DESKHUB_RUN_ARGS = $(ARGS)
run-windows: build-windows
	@powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-windows.ps1 "$(APP_EXE_DEBUG)"
else
build-windows release-windows run-windows:
	@echo "make $@: run on a Windows machine (the Windows app needs MSVC)"; exit 1
endif

.PHONY: build-windows release-windows run-windows
