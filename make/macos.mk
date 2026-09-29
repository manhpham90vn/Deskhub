ifeq ($(UNAME),Darwin)
MACOS_PROJ := client/macos/Deskhub.xcodeproj
MACOS_OUT  := $(CURDIR)/out/build/macos
MACOS_APP  := $(MACOS_OUT)/Release/app.app
MACOS_TEAM ?= UPJRMYQ38F

MACOS_DIST    := $(CURDIR)/out/dist/macos
MACOS_ZIP     := $(MACOS_DIST)/Deskhub.zip
MACOS_DMG     := $(MACOS_DIST)/Deskhub.dmg
MACOS_DMG_SRC := $(MACOS_DIST)/dmg

MACOS_DEV_IDENTITIES := $(shell security find-identity -v -p codesigning 2>/dev/null | grep -E 'Apple Development|Mac Developer')
MACOS_SIGN ?= $(if $(MACOS_DEV_IDENTITIES),,adhoc)

ifeq ($(MACOS_SIGN),adhoc)
MACOS_SIGN_FLAGS := CODE_SIGN_IDENTITY=- CODE_SIGN_STYLE=Manual DEVELOPMENT_TEAM=
endif
ifeq ($(MACOS_SIGN),developerid)
MACOS_SIGN_FLAGS := CODE_SIGN_IDENTITY="Developer ID Application" CODE_SIGN_STYLE=Manual \
                    DEVELOPMENT_TEAM=$(MACOS_TEAM) \
                    CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO \
                    OTHER_CODE_SIGN_FLAGS=--timestamp
endif

MACOS_XCARGS ?=
MACOS_QUICHE_TARGETS := aarch64-apple-darwin x86_64-apple-darwin
MACOS_OPUS_TARGETS := aarch64-apple-darwin x86_64-apple-darwin

NOTARY_CREDS := --key "$(ASC_KEY_P8)" --key-id "$(ASC_KEY_ID)" --issuer "$(ASC_ISSUER_ID)"

define notarize
	xcrun notarytool submit $(1) --wait $(NOTARY_CREDS) | tee $(MACOS_DIST)/notary.txt; \
	grep -q 'status: Accepted' $(MACOS_DIST)/notary.txt || { \
	  sub_id=$$(awk '/^  id:/{print $$2; exit}' $(MACOS_DIST)/notary.txt); \
	  echo "notarize: rejected, fetching Apple log for $$sub_id"; \
	  xcrun notarytool log "$$sub_id" $(NOTARY_CREDS); exit 1; }
endef

quiche-macos:
	-@$(QUICHE_FOR) $(MACOS_QUICHE_TARGETS)

opus-macos:
	-@$(OPUS_FOR) $(MACOS_OPUS_TARGETS)

build-macos: quiche-macos opus-macos
	xcodebuild -project $(MACOS_PROJ) -target app -configuration Debug SYMROOT=$(MACOS_OUT) $(MACOS_SIGN_FLAGS) $(MACOS_XCARGS) build

release-macos: quiche-macos opus-macos
	xcodebuild -project $(MACOS_PROJ) -target app -configuration Release SYMROOT=$(MACOS_OUT) $(MACOS_SIGN_FLAGS) $(MACOS_XCARGS) build

run-macos: build-macos
	open out/build/macos/Debug/app.app

dist-macos:
	@test -n "$(ASC_KEY_P8)" || { echo "dist-macos: missing ASC_KEY_P8 (path to the .p8 file)"; exit 1; }
	@test -n "$(ASC_KEY_ID)" || { echo "dist-macos: missing ASC_KEY_ID"; exit 1; }
	@test -n "$(ASC_ISSUER_ID)" || { echo "dist-macos: missing ASC_ISSUER_ID"; exit 1; }
	$(MAKE) release-macos MACOS_SIGN=developerid MACOS_XCARGS="$(MACOS_XCARGS)"
	mkdir -p $(MACOS_DIST)
	ditto -c -k --keepParent $(MACOS_APP) $(MACOS_ZIP)
	$(call notarize,$(MACOS_ZIP))
	xcrun stapler staple $(MACOS_APP)
	rm -rf $(MACOS_DMG_SRC) $(MACOS_DMG)
	mkdir -p $(MACOS_DMG_SRC)
	cp -R $(MACOS_APP) $(MACOS_DMG_SRC)/Deskhub.app
	ln -s /Applications $(MACOS_DMG_SRC)/Applications
	hdiutil create -volname Deskhub -srcfolder $(MACOS_DMG_SRC) -ov -format UDZO $(MACOS_DMG)
	codesign --sign "Developer ID Application" --timestamp $(MACOS_DMG)
	$(call notarize,$(MACOS_DMG))
	xcrun stapler staple $(MACOS_DMG)
	@echo "dist-macos: done -> $(MACOS_DMG)"

verify-macos:
	xcrun stapler validate $(MACOS_APP)
	spctl -a -vvv -t install $(MACOS_APP)

reset-macos-permissions:
	@scripts/reset-macos-permissions.sh $(ARGS)
else
quiche-macos opus-macos build-macos release-macos run-macos dist-macos verify-macos reset-macos-permissions:
	@echo "make $@: needs macOS + Xcode"; exit 1
endif

.PHONY: quiche-macos opus-macos build-macos release-macos run-macos dist-macos verify-macos reset-macos-permissions
