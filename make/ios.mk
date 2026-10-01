ifeq ($(UNAME),Darwin)
IOS_PROJ   := client/ios/Deskhub.xcodeproj
IOS_OUT    := $(CURDIR)/out/build/ios
IOS_BUNDLE := com.ios.deskhub
IOS_APP    := out/build/ios/Debug-iphonesimulator/app.app
IOS_DEVICE ?=
IOS_QUICHE_TARGETS := aarch64-apple-ios-sim aarch64-apple-ios
IOS_OPUS_TARGETS := aarch64-apple-ios-sim aarch64-apple-ios

quiche-ios:
	@$(QUICHE_FOR) $(IOS_QUICHE_TARGETS)

opus-ios:
	@$(OPUS_FOR) $(IOS_OPUS_TARGETS)

build-ios: quiche-ios opus-ios
	xcodebuild -project $(IOS_PROJ) -target app -configuration Debug -sdk iphonesimulator SYMROOT=$(IOS_OUT) build

release-ios: quiche-ios opus-ios
	xcodebuild -project $(IOS_PROJ) -target app -configuration Release -sdk iphonesimulator SYMROOT=$(IOS_OUT) build

run-ios: build-ios
	@udid="$(IOS_DEVICE)"; \
	if [ -z "$$udid" ]; then \
		udid=$$(xcrun simctl list devices booted | grep -oE '[0-9A-F-]{36}' | head -1); \
	fi; \
	if [ -z "$$udid" ]; then \
		udid=$$(xcrun simctl list devices available | grep -E '^ +iPhone' | grep -oE '[0-9A-F-]{36}' | head -1); \
	fi; \
	if [ -z "$$udid" ]; then \
		echo "make run-ios: no iPhone simulator available"; exit 1; \
	fi; \
	xcrun simctl boot "$$udid" 2>/dev/null || true; \
	open -a Simulator --args -CurrentDeviceUDID "$$udid"; \
	xcrun simctl bootstatus "$$udid" -b; \
	xcrun simctl install "$$udid" $(IOS_APP); \
	xcrun simctl launch "$$udid" $(IOS_BUNDLE)
else
quiche-ios opus-ios build-ios release-ios run-ios:
	@echo "make $@: needs macOS + Xcode"; exit 1
endif

.PHONY: quiche-ios opus-ios build-ios release-ios run-ios
