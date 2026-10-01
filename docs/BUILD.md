**English** · [Tiếng Việt](BUILD.vi.md) · [中文](BUILD.zh.md) · [日本語](BUILD.ja.md)

# Deskhub — Building and developing

This guide covers building Deskhub, running its tests and preparing a release. If you
want to install the app without building it, start with [`INSTALL.md`](INSTALL.md).

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # once: toolchain + dependencies for this OS
make test             # build and run the core suite offline
make build-linux      # or build-windows / build-macos / build-ios / build-android
```

Run a platform target explicitly. A bare `make` only prints the target list from
`make/help.txt`; it does not build an app. The [`Makefile`](../Makefile) documents each
target.

---

## 1. What you need first

`make bootstrap` installs what it can and tells you what it cannot. Install these
yourself before running it:

| Host OS | Install first | What bootstrap then does |
| --- | --- | --- |
| **Ubuntu / Debian** | nothing beyond apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, CMake (`pipx install "cmake>=3.25,<4"` when apt's is older than 3.25), ninja, JDK 17, pipx, python3-venv, rpm, nasm, the GTK3 / PipeWire / VA-API / tray `-dev` packages, VA-API drivers, the GNOME portal, the static minimal FFmpeg, cargo-ndk, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode + command line tools, [Rust](https://rustup.rs) | cmake, ninja, pipx, Homebrew LLVM (Apple clang ships no libFuzzer runtime), Temurin JDK 17, cargo-ndk, quiche and opus for Apple |
| **Windows** | winget (App Installer), [Git for Windows](https://git-scm.com/download/win) (the build scripts run under its Git Bash), Python 3 | Visual Studio Build Tools with the C++ workload and *C++ Clang tools*, Rust, NASM, GNU make, Temurin JDK 17 and Android Studio via winget, driven by `scripts/bootstrap.ps1` |

The shared CMake tree needs CMake 3.25 or newer. On Windows the first run has no `make`
yet: run `powershell -ExecutionPolicy Bypass -File scripts\bootstrap.ps1` directly, then
open a new terminal.

On every OS it also pins the style and analysis tools CI uses — clang-format, clang-tidy,
ktlint, SwiftFormat, cppcheck and detekt, plus SwiftLint on macOS — each at a fixed
version. The downloaded ones (ktlint, detekt, SwiftFormat, SwiftLint and the cppcheck
source) are checked against a checksum; SwiftLint (0.65.0) goes to `tools/swiftlint`, not
Homebrew, and `make lint` installs it there by itself on macOS when it is missing; clang-format and clang-tidy are installed from PyPI
at their pinned versions into `tools/venv`, and on Windows cppcheck comes from winget at
its pinned version. Never install these by hand — CI compares against exactly these
versions. Periphery, the Swift dead-code tool, is fetched with a checksum check the first
time `make lint-dead-swift` runs.

Mobile targets need more: `build-android` needs the Android SDK with the NDK (bootstrap
installs the SDK packages — platform-tools, the platform, the NDK and CMake — once
`ANDROID_HOME` points at a cmdline-tools install, and then builds quiche and opus for
Android), and `build-ios` needs Xcode with a Simulator runtime. The NDK version comes from
`-PandroidNdkVersion=<v>` on the Gradle command line, else the `ANDROID_NDK_VERSION`
environment variable, else `26.1.10909125`; bootstrap installs the same one.

Only the `nvenc` headers are a git submodule; `--recurse-submodules` on clone, or
`git submodule update --init`, covers it. `make bootstrap` syncs them too.

## 2. How the tree is laid out

```
core/       platform-agnostic C++20 — protocol, packetization, FEC, session state,
            input mapping, bitrate control, the VT emulator. No OS headers. Unit-tested.
platform/   thin OS abstractions behind one shared API — sockets, clock, logging,
            randomness, source enumeration, and the media, audio and input code more
            than one app shares. Depends on core.
client/     the five apps: android, ios, linux, macos, windows.
            client/apple/ is Swift shared by the macOS and iOS apps, not an app itself.
            client/cli/ is the command-line client, one binary for all three desktops.
tests/integration/  host + viewer over loopback, with fake capture and encode
cmake/      the shared CMake modules: warnings, quiche, opus
third_party/  quiche (QUIC), opus (audio), the nvenc headers, the minimal FFmpeg build
patches/    the patch applied to quiche before it is built
licenses/   the third-party licence texts the packages ship
packaging/  Windows installers, Homebrew templates, release notes, the apt and Pages sites
assets/     the source icon every client icon is generated from
make/       one .mk per platform, included by the root Makefile
scripts/    bootstrap, packaging, coverage, style and CI helpers
tools/      the pinned style tools bootstrap installs (git-ignored)
.github/    workflows, and actions/ — the composite steps they all reuse
```

Write logic once and share it: before adding anything under `client/*`, check whether it
belongs in `core/` (platform-agnostic) or `platform/` (needs the OS, same API
everywhere). [`ARCHITECTURE.md`](ARCHITECTURE.md) explains the layering, the threading
model and the wire protocol; `CLAUDE.md` states the rules this repository enforces.

## 3. The everyday loop

```bash
make test      # core suite, offline, no GPU and no network — a few seconds
make lint      # formatting check across C++, Kotlin and Swift, then the dead-code check
```

Run both before finishing a change. Use `make format` to apply formatting when needed;
`make lint` only checks it and never writes. The repository pins the formatter versions
used in CI.

New logic in `core/` needs a test in the matching `core/tests/` subdirectory.

## 4. Building and running an app

| Target | Builds | Needs |
| --- | --- | --- |
| `make build-windows` | one `Deskhub.exe` | Windows + MSVC |
| `make build-macos` | the macOS app | macOS + Xcode |
| `make build-linux` | one `deskhub` binary | Ubuntu + the `-dev` packages |
| `make build-ios` | the iOS app for the Simulator | macOS + Xcode + a Simulator runtime |
| `make build-android` | a debug APK | Android SDK + NDK, `adb` |

Each platform also has `release-<os>` to build an optimized version and `run-<os>` to
build and launch it; `release-ios` still targets the Simulator, and `release-android`
produces an unsigned release APK. Choose desktop app settings on its four pages; the
desktop apps do not accept command-line flags. `run-android` installs and opens the app on
a connected device or emulator through adb. `run-ios` does the same on the booted
Simulator, or on the first available iPhone simulator; `IOS_DEVICE=<udid>` picks one.

The macOS targets sign ad hoc when the keychain holds no Apple Development identity.
`MACOS_SIGN=adhoc` or `MACOS_SIGN=developerid` forces a mode, `MACOS_TEAM` sets the team
used for Developer ID signing, and `MACOS_XCARGS` passes extra build settings to
`xcodebuild`.

Debug builds keep out of the way of an installed release. On every desktop OS a Debug
build of the app or the CLI keeps its keys, allowed clients, trusted hosts, settings and
logs in `~/.deskhub-dev` (Windows: `%USERPROFILE%\.deskhub-dev`) instead of `~/.deskhub`,
and on macOS `build-macos`/`run-macos` produce **Deskhub Dev** with the bundle id
`com.deskhub.macos.debug`. Running a local build therefore never quits the installed app,
never touches its data, and asks for its own macOS privacy permissions. `release-<os>`
builds use the normal folder and bundle id.

### The command-line client

`client/cli/` builds `deskhub-cli`, which uses commands instead of the app's pages. Use
it over SSH, from a script or under systemd. The `connect` command opens a viewer window
on Windows and Linux; macOS does not support that command yet.

```bash
make build-cli                       # debug build for this OS
make release-cli                     # optimized
make run-cli ARGS="host list"        # build, then run with those arguments
make cli-smoke                       # build, then run it against itself over loopback
```

It is behind `-DDESKHUB_CLI=ON` (off by default), so the app builds and the sanitizer,
coverage and fuzz presets are untouched by it. Turning it on makes the per-OS media
libraries required rather than optional, because a client that cannot capture or decode is
not a client. On Linux that includes the static minimal FFmpeg, so the CLI targets build
`ffmpeg-min` first, as `build-linux` does.

`make cli-smoke` runs the built CLI headless against itself on loopback: key exchange, a
stranger waiting for approval, a QR invite, a remote shell and files sent to a host. On
Windows it skips the steps that need POSIX signals. CI runs it on all three desktops.

| Command | Does |
| --- | --- |
| `share` | share this machine — any display, the shell, or both |
| `connect ADDRESS` | open a window on a host's screen and control it (Windows and Linux) |
| `shell ADDRESS` | open a shell on a host, in the terminal you are already in |
| `send ADDRESS FILE...` | send files to a host that accepts them |
| `displays`, `sources ADDRESS` | local displays, and what an authenticated host shares |
| `key public`, `access`, `host`, `host-key public` | this machine's public key, allowed clients and connection requests, saved hosts, and this host's key |
| `devices`, `trust`, `settings` | older commands for the same configuration files |
| `help [COMMAND]`, `version` | usage, and the version this build reports |

Every machine has one key. Show its public half with `key public`; on a host, pipe that
line to `access add --stdin` to allow it by hand. A client that connects without being
allowed leaves a **connection request** on the host: `access requests` lists them and
`access approve --fingerprint SHA256:...` or `access deny --fingerprint SHA256:...` settles
one; `share --qr` prints a QR code whose `deskhub://pair/...` link lets one device in on its
own, and that link works in place of an address for `connect`, `sources`, `shell` and `send`.
Copy the host's `host-key public` output to the client and pipe it to
`host add office --address 192.168.1.10:47777 --host-key-stdin` to pin a host in advance.
Then use `connect office`, `sources office`, `shell office`, or `send office FILE`.
`host update office` changes the address or pinned key explicitly; `host remove office`
removes the profile. `access remove --fingerprint SHA256:...` revokes a client key.
`sources`, `connect`, `shell` and `send` take an address, an invite link or a saved alias,
`--accept-new-host-key` to save the key of a host seen for the first time, and
`--approval-wait SEC` to change how long they wait for the host owner's approval (default
120). Without `--accept-new-host-key` an unknown host is refused and its fingerprint
printed. Trust follows the host's key, so a host that changes address stays trusted. There
is no network scan and no passcode flag. `--config-dir PATH` selects one configuration
directory for every command and can be placed before or after the command.

`deskhub-cli help COMMAND` prints the flags. Listing commands support `--json`, and the exit
code says what went wrong: `1` any other failure, `2` bad flags, `3` nobody answered, `4`
refused or not approved in time, `6` nothing to share or view, `8` the host could not start
listening, `9` this build cannot do it, `130` interrupted with Ctrl-C.

Linux supports all the listed commands. Windows uses the desktop app's window code for
`connect`. macOS supports sharing and remote shells; `connect` reports that screen
viewing is unavailable in this build.

To work on `core/` and `platform/` alone, the shared CMake tree is faster:

```bash
make debug        # configure + build the debug preset
make release      # …the release preset
```

**quiche and opus are per-ABI.** The QUIC transport is a Rust static library built into
`third_party/quiche`, and nothing can share or connect without it. The Opus audio codec is
a C static library built into `third_party/opus`, and without it a share carries no sound.
`debug`, `release`, every `build-*`, `release-*` and CLI target, and `test`, every
`test-*` target and `lint-tidy` build the ABI they need first, and both are a no-op once built
(`coverage`, `fuzz` and `fuzz-coverage` build neither: they cover `core/` alone) — so
`make bootstrap && make test` works on a fresh clone on every OS. `make quiche`,
`quiche-android`, `quiche-ios`, `quiche-macos` and the matching `opus`, `opus-android`,
`opus-ios`, `opus-macos` run those steps on their own; a failure in either library stops
`make` there. If CMake stops with a missing-quiche error, that is deliberate: it refuses to
produce a binary that could never connect.

**Hardening.** `cmake/DeskhubHardening.cmake` gives GCC and Clang builds
`-fstack-protector-strong` and, in optimised builds, `-D_FORTIFY_SOURCE=3` — except on
Android, which keeps the NDK's default of 2; full RELRO (`-Wl,-z,relro,-z,now`) is added
on Linux only. MSVC builds use `/sdl`.

## 5. Tests

| Command | Runs | Covers |
| --- | --- | --- |
| `make test` | offline, no sockets | all of `core/`: wire format, framing, FEC, sessions, VT emulator, settings, strings |
| `make test-platform` | loopback sockets | real QUIC handshakes, key-signature authentication end to end, host-key pinning, terminal host + viewer over the wire, PTY against a real shell, bad-signature lockout |
| `make test-integration` | loopback, fake capture/encode | full host↔client sessions: negotiation, video across the wire, input, authorized-key admission, junk resistance |
| `make test-all` | all three, core first | |
| `make test-ctest` | the same tests through CTest | exactly how CI invokes them |
| `make test-asan` | all three under ASan + UBSan | clang/gcc only, not MSVC |
| `make test-tsan` | all three under ThreadSanitizer | clang/gcc only, not MSVC |
| `make test-perf` | release build, offline + loopback | the hot paths measured, not just exercised: `core_perf` covers packetize/reassemble/FEC, 1080p downscale, CRC and file batches, the VT parser and screen, wire encode/decode, the audio jitter buffer and PCM ring, the input path, record-stream framing; `platform_perf` covers real QUIC over loopback |
| `make perf-build` | release build, nothing run | builds `core_perf` and `platform_perf` without running them |
| `make cli-smoke` | loopback, headless | the command-line client against itself (see above) |

Nothing in the test suites needs a remote peer, a GPU or a network.

**Coverage.** `make coverage` produces the `core/` report through clang + llvm-cov;
`scripts/check-coverage.sh` enforces the gate CI applies — **≥ 90 % lines, ≥ 80 %
branches**.

**Fuzzing.** `make fuzz` runs the libFuzzer targets over the wire, H.264, reassembly,
terminal-byte and UI-text parsers, the key, access-list and invite text formats and the QR
encoder, plus the host and viewer session state machines (clang, Linux/macOS;
`FUZZ_SECONDS=N` per target). Each target first replays
`core/fuzz/regressions/<target>` so fixed crashes cannot come back, then fuzzes from the
committed seeds and dictionary. `make fuzz-coverage` shows which core lines the corpus
actually reaches. Every crash found becomes a regression input.

**Performance.** `make test-perf` builds both perf binaries with the release preset and
runs them: `core_perf` measures 37 workloads across the pure-C++ hot paths, then
`platform_perf` measures 6 more over real QUIC on loopback. A few seconds in total. Three
things fail either one, and none of them is a millisecond figure picked out of the air:

- **Allocations per unit**, counted exactly by replacing the global `operator new`. A
  path that starts allocating per packet or per frame fails on every machine, in every
  run.
- **How the cost scales**: each `-scaling` row runs the same work at 4× the input and
  fails when the time grows far faster than the input — the shape an accidental O(n²)
  has.
- **Drift against the recorded baseline**: `make perf-baseline` writes
  `out/perf/baseline.txt` and `out/perf/platform-baseline.txt` on an idle machine, later
  runs report the change on every row and fail past 25 %. The files describe that one
  machine, so they stay out of git.

`DESKHUB_PERF_TOLERANCE`, `DESKHUB_PERF_REPEATS`, `DESKHUB_PERF_BASELINE` and
`DESKHUB_PERF_WRITE` tune the timing half. `make test` runs none of it: debug, ASan and
coverage builds say nothing about production speed. CI runs both binaries on its Linux and
macOS release jobs, where only the allocation and scaling gates can fail, since a timing
baseline describes one machine. On a pull request it also builds the base commit and the
change on the same runner and posts the drift between them as a comment — warnings only,
because a shared runner is too noisy to fail on time.

## 6. Style and static analysis

| Command | Checks |
| --- | --- |
| `make format` | applies formatting to C++, Kotlin and Swift |
| `make lint` | the same checks without writing, then `lint-dead` — what CI enforces |
| `make lint-dead` | dead code: C++ functions, FFI functions, string ids and Kotlin code nothing uses |
| `make lint-dead-swift` | dead Swift code in both Apple apps, via Periphery (macOS + Xcode) |
| `make lint-tidy` | clang-tidy over `core/src` + `platform/src` |

Single-language variants exist too: `format-cpp`, `lint-cpp`, `format-kotlin`,
`lint-kotlin`, `format-swift`, `lint-swift`.

Dead code is an error, the way Rust's `dead_code` lint makes it one. `make lint-dead`
runs cppcheck over every C++ file production builds and fails on any function nothing
there calls — tests, fuzzers and benchmarks do not count, so a function only a test calls
is dead too. Calls from Swift, Kotlin and Objective-C++ count as use. The same script
fails on an FFI function no app calls, a `DHStr*` string id neither app shows, and a Kotlin
constant nobody reads, and detekt fails on unused private Kotlin code, imports and
parameters. When a test genuinely cannot observe a behaviour any other way, keep the
accessor and add `name: which test needs it and what it proves` to
`scripts/dead-code-allow.txt`; a line without a reason, or one whose function production
has started calling, fails the check too. `make lint-dead-swift` builds both Apple apps to
index them and runs Periphery over the result. Beyond these, clang builds warn on unused
member functions, templates and exception parameters, which `-Werror` turns into errors:
every CMake preset except `asan-msvc` sets `DESKHUB_WERROR=ON`, so a local `make test`
fails on a warning just as CI does.

House rules, in short — the full version is in `CLAUDE.md`:

- C++20, no compiler extensions. `deskhub` for core, `deskhubp` for platform.
- `PascalCase` functions and types, `camelCase` locals, trailing underscore on private
  members.
- **No comments anywhere.** Descriptive names, small functions, early returns and named
  constants instead. Knowledge that must survive goes into the error message of the path
  that fails without it, or into `ARCHITECTURE.md`.
- All identifiers and log messages in English; every prose document ships in four
  languages — English, Vietnamese, Chinese, Japanese — English authoritative.

## 7. Packaging

| Command | Produces |
| --- | --- |
| `make dist-macos` | a dmg signed with Developer ID, notarized and stapled |
| `make verify-macos` | a Gatekeeper check on the build just produced |
| `make dist-linux` | separate app and CLI `.deb` + `.rpm` packages, each installing its uinput udev rule |

`dist-macos` needs a *Developer ID Application* identity in the keychain and an App Store
Connect API key for notarization: `ASC_KEY_P8` (the path to the `.p8` file), `ASC_KEY_ID`
and `ASC_ISSUER_ID`. It stops before building when any of the three is missing.

The Windows app and CLI are also published as Inno Setup installers, built from
`packaging/windows/` by the release workflow. Their portable Windows builds and the Linux
app remain single files.

Every package carries `THIRD_PARTY_NOTICES.md` and the licence texts from `licenses/`
that apply to it: the app and CLI `.deb` and `.rpm` (under `/usr/share/doc/<package>/`,
the texts — the LGPL included — in its `licenses/` folder), both Windows installers, the
macOS app's `Resources`, the iOS app bundle and the Android APK's assets.
`scripts/stage-licenses.sh` stages the Linux and Apple sets, `packaging/windows/*.iss`
list the Windows ones, and the Android build copies its own. The portable binaries carry
nothing, so the release adds `deskhub-<tag>-licenses.zip` with `LICENSE`, the notices
and the whole `licenses/` folder. `licenses/rust-crates.txt` is generated:
`scripts/rust-crate-notices.py <quiche source dir> > licenses/rust-crates.txt` rewrites
it when the quiche pin moves. The APK also carries `licenses/android-libraries.txt`, the
Java and Kotlin libraries Gradle packages into it with their notices, and that file is
generated too: `scripts/android-library-notices.py > licenses/android-libraries.txt` reads
the release runtime classpath of `client/android` (with `JAVA_HOME` set, after one Android
build so Gradle has the artifacts cached) and rewrites it whenever an Android dependency
changes.

## 8. Releasing

1. Bump [`VERSION`](../VERSION) — `scripts/check-version.sh` fails the deploy if the tag
   and the file disagree.
2. Update the documents the change touches, in all four languages, in the same commit.
3. Write the release notes (below) and commit them.
4. Tag `vX.Y.Z` and push it. `.github/workflows/deploy.yml` builds every platform, creates
   the GitHub Release, ships iOS to TestFlight, macOS through notarization, and Android to
   the Play internal track.

**Release notes are written by hand**, one file per tag: `packaging/release-notes/vX.Y.Z.md`,
the file name matching the tag exactly. That file is the GitHub Release body, followed by a
fixed footer that points at the install guide and the security model. Lead with what users
must do after updating — breaking changes, how to reconnect — then what is new and what is
fixed. `scripts/check-release-notes.sh` fails the deploy before anything is built when the
tag has no such file, so commit it before tagging.

### Package managers

Once the GitHub Release exists, `deploy.yml` hands the tag to `publish-packages.yml`, whose jobs
publish it. To publish a tag again without a new release — after a fix to one of these
scripts, say — run it by hand: `gh workflow run publish-packages.yml -f tag=vX.Y.Z`.

| Job | Publishes | Secret in the `stg` environment |
| --- | --- | --- |
| `winget` | pull requests to `microsoft/winget-pkgs` for `ManhPham.Deskhub` and `ManhPham.DeskhubCLI`, through [komac](https://github.com/russellbanks/Komac) (pinned in `scripts/pinned-versions.txt`) | `WINGET_TOKEN` — classic PAT with `public_repo` |
| `homebrew` | `Casks/deskhub.rb` and `Formula/deskhub-cli.rb` in `manhpham90vn/homebrew-tap`, rendered from `packaging/homebrew/` | `HOMEBREW_TAP_TOKEN` — fine-grained PAT, Contents: write on the tap |
| `build-apt-repo` → `deploy-apt-repo` | a signed apt repository holding the debs of the last three releases, on GitHub Pages under `/apt` (`scripts/build-apt-repo.sh`) | `APT_GPG_PRIVATE_KEY`, `APT_GPG_PASSPHRASE` |

Each needs a one-time setup before the first tag that runs it:

- **winget** — the job only updates packages that already exist, so submit the first
  version of each by hand. Use the Inno Setup installer for `ManhPham.Deskhub`, and
  choose `deskhub-cli` as the portable command alias for `ManhPham.DeskhubCLI`.
  Until Microsoft merges those pull requests the job warns and skips.

  ```bash
  komac new ManhPham.Deskhub --version X.Y.Z --urls https://github.com/manhpham90vn/Deskhub/releases/download/vX.Y.Z/deskhub-vX.Y.Z-windows-setup.exe
  ```

  Repeat with `ManhPham.DeskhubCLI` and the `deskhub-cli-vX.Y.Z-windows.exe` URL.
- **Homebrew** — create the public repository `manhpham90vn/homebrew-tap`; the job fills it.
- **apt** — generate the signing key once, store the file as `APT_GPG_PRIVATE_KEY` and keep
  an offline backup. Every user trusts this key: replacing it breaks their `apt update`.

  ```bash
  gpg --quick-gen-key "Deskhub APT <manhpv151090@gmail.com>" rsa4096 sign 5y
  gpg --armor --export-secret-keys "Deskhub APT" > deskhub-apt.key
  ```

  In *Settings → Pages* set the source to **GitHub Actions**, and in *Settings →
  Environments → github-pages* allow tags matching `v*` — otherwise Pages refuses a deploy
  from a tag.

## 9. What CI gates

A green local `make test` + `make lint` is not the whole story. On every pull request:

- clang-tidy over `core/src` + `platform/src`, SwiftLint `--strict`, Android Lint
- actionlint + shellcheck over the workflows and `scripts/*.sh`
- dead code: `scripts/dead-code.sh` (cppcheck, the FFI / string-id / Kotlin-constant checks
  and detekt) and Periphery over both Apple apps
- all three suites on Linux x64 and arm64 (both built natively), macOS and Windows; under
  ASan/UBSan and TSan on Linux and ASan/UBSan on macOS; the platform and integration suites
  under MSVC's ASan on Windows; and cross-built for Android — run on an x86_64 emulator,
  only built for arm64-v8a — and for the iOS Simulator
- `make cli-smoke`'s script against the CLI on all three desktops
- the whole integration suite three more times on Windows, hunting an intermittent memory
  corruption that shows up in about one run in three and so slips through a single run. The
  frame it crashes in is a victim of the corruption, never its cause, so every Windows job
  writes a full minidump beside the symbols, and the nightly run repeats the load tests
  twice over: once under the full page heap, where a write past an allocation faults on the
  instruction that makes it, and once against a quiche built with Rust debug assertions and
  overflow checks on, which is the only trap that can see inside quiche at all — ASan does
  not instrument Rust and the page heap guards only the heap
- core coverage ≥ 90 % lines / 80 % branches
- the libFuzzer targets for 30 s each (15 min each nightly)
- CodeQL over C++/Kotlin/Swift, a gitleaks sweep of the whole history, and a dependency
  review

## 10. Developer tools

| Command | Does |
| --- | --- |
| `make icons` | regenerates every client icon from `assets/icon_1024.png` |
| `make quic-smoke` | a standalone QUIC client + server against the quiche static library |
| `make opus-smoke` | a standalone encode/decode round-trip against the opus static library — reports the real bitrate, the largest packet and whether DTX engages |
| `make screenshots` | macOS: recaptures the store screenshots on the iPhone/iPad simulators, the Android emulators and the macOS app, then refreshes `docs/imgs` (`ARGS="ios android macos readme"` for a subset) |
| `make setup-linux-permissions` | the `/dev/uinput` udev rule + `input` group, for hosting from a source build |
| `make reset-macos-permissions` | clears the TCC grants of both the release (`com.deskhub.macos`) and the local Debug (`com.deskhub.macos.debug`) bundle ids, and lists every app copy with how it is signed — for when Screen Recording or Accessibility stop working after rebuilds (`ARGS="--purge"` also deletes the built copies) |
| `make ffmpeg-min` | Ubuntu: the static minimal FFmpeg the app and CLI link (run automatically by `build-linux`, `release-linux` and the CLI targets) |
| `make opus` | the Opus audio codec for the host target (run automatically by `debug`, `release`, the Linux, CLI and test targets) |
| `make clean` | removes `out/` |

## 11. Troubleshooting builds

- **CMake stops on a missing quiche library** — run the matching `make quiche*` target;
  each ABI needs its own. The same holds for opus and the `make opus*` targets.
- **`make fuzz` on macOS finds no libFuzzer** — it needs Homebrew LLVM; `make bootstrap`
  installs it, while the rest keeps building with the Xcode toolchain.
- **`make lint` disagrees with your editor** — run `make bootstrap` to get the tool
  versions used by CI, then run `make format`.
- **Android targets can't find the SDK** — set `ANDROID_HOME`, then re-run
  `make bootstrap`; `ANDROID_NDK_VERSION=<v>` selects a different NDK.
- **macOS permissions behave oddly after rebuilding, or after switching between a local
  and a downloaded build** — `make reset-macos-permissions`, then grant them again to the
  one copy you keep.

Bugs and questions: [issues](https://github.com/manhpham90vn/Deskhub/issues).
