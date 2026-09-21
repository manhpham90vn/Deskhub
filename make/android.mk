ANDROID_NDK_VERSION ?= 26.1.10909125
ANDROID_QUICHE_TARGETS := aarch64-linux-android x86_64-linux-android
ANDROID_OPUS_TARGETS := aarch64-linux-android x86_64-linux-android

ifeq ($(OS),Windows_NT)
ANDROID_SDK := $(if $(ANDROID_HOME),$(ANDROID_HOME),$(LOCALAPPDATA)\Android\Sdk)
GRADLEW := set "ANDROID_HOME=$(ANDROID_SDK)" && cd client\android && .\gradlew.bat
ADB     := $(ANDROID_SDK)\platform-tools\adb.exe
ANDROID_NDK_ENV := set "ANDROID_NDK_HOME=$(ANDROID_SDK)\ndk\$(ANDROID_NDK_VERSION)" &&
else
ANDROID_SDK := $(if $(ANDROID_HOME),$(ANDROID_HOME),$(firstword $(wildcard $(HOME)/Android/Sdk $(HOME)/Library/Android/sdk)))
GRADLEW := cd client/android && $(if $(ANDROID_SDK),ANDROID_HOME=$(ANDROID_SDK) ,)./gradlew
ADB     := $(if $(ANDROID_SDK),$(ANDROID_SDK)/platform-tools/adb,adb)
ANDROID_NDK_ENV := ANDROID_NDK_HOME=$(ANDROID_SDK)/ndk/$(ANDROID_NDK_VERSION)
endif

ANDROID_ACTIVITY := com.manhpham.deskhub/com.deskhub.app.MainActivity

quiche-android:
	-@$(ANDROID_NDK_ENV) $(QUICHE_FOR) $(ANDROID_QUICHE_TARGETS)

opus-android:
	-@$(ANDROID_NDK_ENV) $(OPUS_FOR) $(ANDROID_OPUS_TARGETS)

build-android: quiche-android opus-android
	$(GRADLEW) assembleDebug

release-android: quiche-android opus-android
	$(GRADLEW) assembleRelease

run-android: quiche-android opus-android
	$(GRADLEW) installDebug
	"$(ADB)" shell am start -n $(ANDROID_ACTIVITY)

.PHONY: quiche-android opus-android build-android release-android run-android
