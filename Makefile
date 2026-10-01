# Requires GNU make. Runs on Windows, macOS and Ubuntu.
#
# This file is only the ENTRY POINT: it keeps the shared targets (bootstrap, clean)
# and includes the pieces under make/. One file per platform — adding a platform
# means adding make/<name>.mk plus one include line below, without touching the
# shared part.
#
#   make/toolchain.mk   HOST-dependent vars (include FIRST): SHELL, UNAME, DEVCMD (VsDevCmd),
#                       LLVM/LLVMPATH, BOOTSTRAP, GIT_BASH, RUNSH, QUICHE/QUICHE_FOR,
#                       OPUS/OPUS_FOR, PYTHON, NULDEV, RMRF, HELPCAT
#   make/core.mk        shared CMake tree: quiche, opus, debug, release, test, test-platform,
#                       test-integration, test-all, test-ctest, test-asan, test-tsan,
#                       perf-build, test-perf, perf-baseline, fuzz, fuzz-coverage, coverage
#   make/windows.mk     Windows app — CMake (Win32 app, ONE Deskhub.exe)
#   make/macos.mk       macOS app   — xcodebuild
#   make/linux.mk       Ubuntu app  — CMake (GTK3 + native, ONE `deskhub`)
#   make/ios.mk         iOS app     — xcodebuild (Simulator)
#   make/android.mk     Android APK — Gradle (builds both the .so and the APK)
#   make/cli.mk         command-line client — CMake (one deskhub-cli, no GUI toolkit)
#   make/codestyle.mk   format/lint for C++ + Kotlin + Swift
#   make/tools.mk       one-off developer tools: icons, quic-smoke, opus-smoke, screenshots
#
# Windows uses cmd + VsDevCmd (it locates Visual Studio through vswhere, so it can be
# called from a plain cmd / PowerShell / Git Bash), macOS/Linux use sh + the system
# toolchain.
#   make bootstrap      install every dev dependency for the current OS (Android SDK, coverage too)
#   make                print the target list — NO platform is built implicitly, every
#                       platform must be named explicitly (see below)
#
# Explicit per-platform build/release/run — no platform is the default:
#   make build-windows   / release-windows   / run-windows   Win32 app, one Deskhub.exe (needs Windows + MSVC)
#   make build-macos     / release-macos     / run-macos     macOS app — both roles (needs macOS + Xcode)
#   make build-linux     / release-linux     / run-linux     Ubuntu app — both roles (needs Ubuntu + the -dev packages)
#   make build-android   / release-android   / run-android   debug APK / release APK (unsigned)
#   make build-ios       / release-ios       / run-ios       iOS app for the Simulator (needs macOS + Xcode)
#
# run-windows and run-linux pass $(ARGS) to the binary. Every build-*/release-* target
# builds its own quiche and opus first (see below).
#
# macOS knobs: MACOS_SIGN=adhoc|developerid (default: adhoc when no Apple Development
# identity is in the keychain), MACOS_TEAM=<team id>, MACOS_XCARGS="<extra xcodebuild
# arguments>". iOS: IOS_DEVICE=<simulator udid> picks the Simulator run-ios boots.
#
# The desktop apps parse no command-line flags at all — everything is chosen on their
# four pages. To drive Deskhub from a script or over SSH, build the command-line client
# instead. It runs on Linux, Windows and macOS and shares one binary name:
#   make build-cli       / release-cli       / run-cli       one deskhub-cli, no GUI toolkit
#   make cli-smoke       host + viewer + a remote shell over loopback, headless, no GPU
#
# run-cli takes ARGS="host list" and the like. build-cli, release-cli, run-cli and
# cli-smoke build quiche and opus first, and ffmpeg-min too on Linux. run-android
# installs and opens on the connected device/emulator via adb; run-ios does the same
# on the Simulator.
#
# Ubuntu only — build the static minimal FFmpeg the app and the CLI link (build-linux,
# release-linux, build-cli, release-cli, run-cli and cli-smoke run it automatically, it is
# a no-op once built):
#   make ffmpeg-min
#
# Distribution:
#   make dist-macos     macOS dmg signed with Developer ID + notarized + stapled. Needs
#                       ASC_KEY_P8=<path to the .p8>, ASC_KEY_ID, ASC_ISSUER_ID and a
#                       "Developer ID Application" identity in the keychain
#   make verify-macos   check that Gatekeeper accepts the build that was just produced
#   make dist-linux     Separate app and CLI .deb (Ubuntu/Debian) + .rpm (Fedora/openSUSE).
#                       Each installs its own uinput udev rule from its post-install step,
#                       so installing either package replaces setup-linux-permissions
#
# Shared CMake tree (core + platform + whatever client the current OS builds):
#   make debug          configure + build the debug preset
#   make release        configure + build the release preset
#   make quiche         build the QUIC library into third_party/quiche (scripts/build-quiche.sh)
#                       for the host target. debug, release, every build-*/release-* and CLI
#                       target, test, every test-* and lint-tidy run it first — it is a no-op
#                       once built, and a failed library build stops make there. Windows drives
#                       the script through Git Bash: override with GIT_BASH=<path to bash.exe>
#                       if Git is installed elsewhere
#   make opus           build the Opus audio codec into third_party/opus (scripts/build-opus.sh)
#                       for the host target, the same way and before the same targets: a no-op
#                       once built, and a failed build stops make
#
# quiche and opus are per-ABI, so every cross-compiled app builds its own before the app
# itself — without them CMake stops with an error instead of producing a binary that
# cannot connect or cannot carry sound:
#   make quiche-android  arm64-v8a + x86_64 static libs (build/release/run-android run it first).
#                        Needs the NDK (plus cargo-ndk outside Windows, which drives the NDK
#                        toolchain itself); ANDROID_NDK_VERSION=<v> picks another NDK
#   make quiche-ios      iOS device + Simulator (build/release-ios run it first)
#   make quiche-macos    arm64 + x86_64, lipo'd into macos-universal (build/release-macos run it)
#   make opus-android    arm64-v8a + x86_64 (needs the NDK; CMake drives it, no cargo-ndk)
#   make opus-ios        iOS device + Simulator
#   make opus-macos      arm64 + x86_64, lipo'd into macos-universal
#
# Ubuntu, ONE-TIME permission grant for the host role (mouse/keyboard injection via /dev/uinput):
#   make setup-linux-permissions    udev rule + add the user to the `input` group
#
# macOS, when a locally built app and a downloaded/CI build fight over the same
# bundle id and the Screen Recording / Accessibility grants stop working:
#   make reset-macos-permissions    drop every TCC grant for com.deskhub.macos and
#                                   com.deskhub.macos.debug (the local Debug build), and
#                                   list the app copies with how each one is signed.
#                                   ARGS="--purge" also deletes out/build/macos +
#                                   out/dist/macos so only one copy is left
#
#   test and every test-* target build quiche and opus first, like debug and release —
#   the presets stop without them. Once built they are no-ops.
#   make test              build core_tests and run it (offline, no client/GPU needed)
#   make test-platform     build platform_tests and run it (local only: loopback sockets)
#   make test-integration  host + viewer over loopback, fake codecs + golden wire bytes
#   make test-all          all three suites, core first
#   make test-ctest        run through CTest (--output-on-failure) — matches how CI runs it
#   make test-asan         all three suites under ASan + UBSan (clang/gcc only, not MSVC)
#   make test-tsan         all three suites under ThreadSanitizer (clang/gcc only, not MSVC)
#   make perf-build        build core_perf + platform_perf with the release preset, without
#                          running them (test-perf and perf-baseline run it first)
#   make test-perf         build core_perf + platform_perf with the release preset and
#                          measure the hot paths - packetize/reassemble/FEC, 1080p
#                          downscale, CRC + file batches, the VT parser and screen, wire
#                          encode/decode, the audio jitter buffer and PCM ring, the input
#                          path, the record framer, and real QUIC over loopback. Fails on
#                          allocations per unit, on a path that stops scaling linearly,
#                          and on drift past DESKHUB_PERF_TOLERANCE (25% by default)
#                          against out/perf/baseline.txt and platform-baseline.txt. Not
#                          part of `make test`; CI runs only the allocation and scaling
#                          gates on Linux/macOS release jobs
#   make perf-baseline     record out/perf/baseline.txt + platform-baseline.txt from the
#                          current tree on an idle machine - they describe this machine,
#                          so they stay out of git and have to be re-recorded after a
#                          deliberate speed change
#   make fuzz              libFuzzer + ASan over the wire/media/ui parsers, the session
#                          state machines, the key / access-list / invite text formats
#                          (fuzz_keys) and the QR encoder (fuzz_qr) (clang only,
#                          Linux/macOS; FUZZ_SECONDS=N per target, corpus in
#                          out/fuzz/corpus). Each target first replays
#                          core/fuzz/regressions/<target> (inputs from fixed crashes, so
#                          they cannot come back), then fuzzes seeded by the committed
#                          corpus in core/fuzz/seeds/<target> and guided by the protocol
#                          tokens in core/fuzz/dict/<target>.dict. On macOS the libFuzzer
#                          runtime comes from Homebrew LLVM (Apple clang ships none) —
#                          `make bootstrap` installs it, the rest still builds with the
#                          Xcode toolchain
#   make fuzz-coverage     measure which core lines the fuzz corpus + seeds actually reach
#                          (clang + llvm-cov, Linux/macOS) — finds the fuzzers' blind spots
#   make coverage          measure core coverage (clang + llvm-cov — Windows/macOS/Ubuntu)
#
# Format/lint — all three languages, or one at a time:
#   make format         apply formatting in place for C++ + Kotlin + Swift
#   make lint           check style for all three without fixing, then run lint-dead
#                       (run before pushing to match CI)
#   make format-cpp     / lint-cpp      C++ only (clang-format: core/ platform/ client/)
#   make format-kotlin  / lint-kotlin   Kotlin only (ktlint: client/android)
#   make format-swift   / lint-swift    Swift only (swiftformat + swiftlint --strict:
#                                       client/apple + client/ios + client/macos)
#   make lint-dead      dead code as an error, like Rust's dead_code: cppcheck finds C++
#                       functions production never calls (test-only counts as dead unless
#                       scripts/dead-code-allow.txt says why), plus unused FFI functions,
#                       string ids and Kotlin constants, and detekt's unused Kotlin rules
#   make lint-dead-swift  Periphery over both Apple apps (macOS + Xcode; builds them first)
#   make lint-tidy      clang-tidy over core/src + platform/src, the same gate CI runs.
#                       Configures the debug preset first for the compile database, so it
#                       needs quiche and opus and builds them itself; `make bootstrap`
#                       installs the pinned clang-tidy
#
# One-off developer tools:
#   make icons          regenerate every client icon from assets/icon_1024.png
#   make quic-smoke     build and run a standalone QUIC client+server against the quiche
#                       static library, with a throwaway certificate — proves the library
#                       links and handshakes without involving the app
#   make opus-smoke     build and run a standalone encode/decode round-trip against the opus
#                       static library — proves it links, that a 20 ms frame fits one
#                       datagram, and reports the real bitrate and DTX behaviour
#   make screenshots    macOS only — build the iOS, Android and macOS apps, boot the
#                       iPhone/iPad simulators and the phone/tablet emulators, open every
#                       page and recapture the store screenshots straight off the device
#                       framebuffer (no desktop background at the edges) into the fastlane
#                       trees; the macOS window shots land in out/screenshots/macos and the
#                       readme step derives docs/imgs from the captures.
#                       ARGS="ios android macos readme" runs a subset
#
#   make clean

all: help

include make/toolchain.mk
include make/core.mk
include make/windows.mk
include make/macos.mk
include make/linux.mk
include make/ios.mk
include make/android.mk
include make/cli.mk
include make/codestyle.mk
include make/tools.mk

help:
	@$(HELPCAT)

bootstrap:
	@$(BOOTSTRAP)

clean:
	@$(RMRF) out

.PHONY: all help bootstrap clean
