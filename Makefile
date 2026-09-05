# Requires GNU make. Runs on Windows, macOS and Ubuntu.
#
# This file is only the ENTRY POINT: it keeps the shared targets (bootstrap, clean)
# and includes the pieces under make/. One file per platform — adding a platform
# means adding make/<name>.mk plus one include line below, without touching the
# shared part.
#
#   make/toolchain.mk   HOST-dependent vars: SHELL, DEVCMD (VsDevCmd), LLVM/LLVMPATH,
#                       BOOTSTRAP, RMRF, HELPCAT, NULDEV (include FIRST)
#   make/core.mk        shared CMake tree: debug/release/test*/test-ctest/coverage
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
# The desktop apps parse no command-line flags at all — everything is chosen on their
# four pages. To drive Deskhub from a script or over SSH, build the command-line client
# instead. It runs on Linux, Windows and macOS and shares one binary name:
#   make build-cli       / release-cli       / run-cli       one deskhub-cli, no GUI toolkit
#   make cli-smoke       host + viewer + a remote shell over loopback, headless, no GPU
#
# run-cli takes ARGS="scan" and the like. run-android installs and opens on the
# connected device/emulator via adb; run-ios does the same on the Simulator.
#
# Ubuntu only — build the static minimal FFmpeg the app links (build-linux and
# release-linux run it automatically, it is a no-op once built):
#   make ffmpeg-min
#
# Distribution:
#   make dist-macos     macOS dmg signed with Developer ID + notarized + stapled
#   make verify-macos   check that Gatekeeper accepts the build that was just produced
#   make dist-linux     Linux packages: .deb (Ubuntu/Debian) + .rpm (Fedora/openSUSE).
#                       Both install the uinput udev rule from their post-install step,
#                       so installing either package replaces setup-linux-permissions
#
# Shared CMake tree (core + platform + whatever client the current OS builds):
#   make debug          configure + build the debug preset
#   make release        configure + build the release preset
#   make quiche         build the QUIC library into third_party/quiche (scripts/build-quiche.sh)
#                       for the host target. debug and release run it first — it is a no-op once
#                       built. Windows drives the script through Git Bash: override with
#                       GIT_BASH=<path to bash.exe> if Git is installed elsewhere
#   make opus           build the Opus audio codec into third_party/opus (scripts/build-opus.sh)
#                       for the host target, the same way: debug and release run it first and it
#                       is a no-op once built
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
#                                   list the app copies with how each one is signed.
#                                   ARGS="--purge" also deletes out/build/macos +
#                                   out/dist/macos so only one copy is left
#
#   make test              build core_tests and run it (offline, no client/GPU needed)
#   make test-platform     build platform_tests and run it (local only: loopback sockets)
#   make test-integration  host + viewer over loopback, fake codecs + golden wire bytes
#   make test-all          all three suites, core first
#   make test-ctest        run through CTest (--output-on-failure) — matches how CI runs it
#   make test-asan         all three suites under ASan + UBSan (clang/gcc only, not MSVC)
#   make test-tsan         all three suites under ThreadSanitizer (clang/gcc only, not MSVC)
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
#   make fuzz              libFuzzer + ASan over the wire/media/ui parsers and the session
#                          state machines (clang only, Linux/macOS; FUZZ_SECONDS=N per
#                          target, corpus in out/fuzz/corpus). Each target first replays
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
#   make lint           check style for all three without fixing (run before pushing to match CI)
#   make format-cpp     / lint-cpp      C++ only (clang-format: core/ platform/ client/)
#   make format-kotlin  / lint-kotlin   Kotlin only (ktlint: client/android)
#   make format-swift   / lint-swift    Swift only (swiftformat + swiftlint --strict:
#                                       client/apple + client/ios + client/macos)
#   make lint-tidy      clang-tidy over core/src + platform/src, the same gate CI runs.
#                       Configures the debug preset first for the compile database, so it
#                       needs quiche; `make bootstrap` installs the pinned clang-tidy
#
# One-off developer tools:
#   make icons          regenerate every client icon from assets/icon_1024.png
#   make quic-smoke     build and run a standalone QUIC client+server against the quiche
#                       static library, with a throwaway certificate — proves the library
#                       links and handshakes without involving the app
#   make opus-smoke     build and run a standalone encode/decode round-trip against the opus
#                       static library — proves it links, that a 20 ms frame fits one
#                       datagram, and reports the real bitrate and DTX behaviour
#   make encoder-bake-off  run every encoder backend on this machine over one fixed clip
#                       and print VMAF, enc_us_p50/p99, CPU% and GPU% side by side. Needs
#                       an ffmpeg built with libvmaf and the bench binary from
#                       `make build-windows`. ARGS="--clip FILE --width W --height H"
#                       measures a real clip instead of the generated one
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
