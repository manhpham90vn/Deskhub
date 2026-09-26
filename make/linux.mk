ifeq ($(UNAME),Linux)
LINUX_APP_DEBUG   := out/build/x64-debug/client/linux/deskhub
LINUX_APP_RELEASE := out/build/x64-release/client/linux/deskhub

ffmpeg-min:
	@scripts/build-ffmpeg.sh

build-linux: ffmpeg-min quiche opus
	@cmake --preset x64-debug -DDESKHUB_LINUX_APP=ON -DDESKHUB_REQUIRE_LINUX_APP=ON && cmake --build --preset x64-debug --target deskhub_app

release-linux: ffmpeg-min quiche opus
	@cmake --preset x64-release -DDESKHUB_LINUX_APP=ON -DDESKHUB_REQUIRE_LINUX_APP=ON && cmake --build --preset x64-release --target deskhub_app

run-linux: build-linux
	$(LINUX_APP_DEBUG) $(ARGS)

dist-linux: release-linux release-cli
	@scripts/build-deb.sh
	@scripts/build-cli-deb.sh
	@scripts/build-rpm.sh
	@scripts/build-cli-rpm.sh

setup-linux-permissions:
	@sudo scripts/setup-uinput.sh
else
build-linux release-linux run-linux dist-linux setup-linux-permissions ffmpeg-min:
	@echo "make $@: needs Ubuntu/Linux"; exit 1
endif

.PHONY: build-linux release-linux run-linux dist-linux setup-linux-permissions ffmpeg-min
