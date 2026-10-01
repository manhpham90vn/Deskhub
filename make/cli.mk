ifeq ($(OS),Windows_NT)
CLI_DEBUG   := out\build\x64-debug\client\cli\deskhub-cli.exe
CLI_RELEASE := out\build\x64-release\client\cli\deskhub-cli.exe
else
CLI_DEBUG   := out/build/x64-debug/client/cli/deskhub-cli
CLI_RELEASE := out/build/x64-release/client/cli/deskhub-cli
endif

CLI_THIRD_PARTY := quiche opus $(if $(filter Linux,$(UNAME)),ffmpeg-min)

build-cli: $(CLI_THIRD_PARTY)
	@$(DEVCMD) cmake --preset x64-debug -DDESKHUB_CLI=ON >$(NULDEV) && cmake --build --preset x64-debug --target deskhub_cli

release-cli: $(CLI_THIRD_PARTY)
	@$(DEVCMD) cmake --preset x64-release -DDESKHUB_CLI=ON >$(NULDEV) && cmake --build --preset x64-release --target deskhub_cli

run-cli: build-cli
	$(CLI_DEBUG) $(ARGS)

cli-smoke: build-cli
	@$(RUNSH) scripts/cli-smoke.sh $(CLI_DEBUG)

.PHONY: build-cli release-cli run-cli cli-smoke
