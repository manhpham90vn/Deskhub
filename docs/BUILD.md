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
| **Ubuntu / Debian** | nothing beyond apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, cmake, ninja, JDK 17, the GTK3 / PipeWire / VA-API / tray `-dev` packages, VA-API drivers, the GNOME portal, the static minimal FFmpeg, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode + command line tools, [Rust](https://rustup.rs) | cmake, ninja, swiftlint, pipx, Homebrew LLVM (Apple clang ships no libFuzzer runtime), Temurin JDK 17, quiche and opus for Apple and Android |
| **Windows** | winget (App Installer), Visual Studio with the C++ toolchain and the *C++ Clang tools* component, [Rust](https://rustup.rs) | the rest via winget, driven by `scripts/bootstrap.ps1` |

On every OS it also pins the style and analysis tools: clang-format, clang-tidy, ktlint,
SwiftFormat, cppcheck and detekt, each at a fixed version with a checksum check — never
install these by hand, CI compares against exactly these versions. Periphery, the Swift
dead-code tool, is fetched the same way the first time `make lint-dead-swift` runs.

Mobile targets need more: `build-android` needs the Android SDK with the NDK (bootstrap
installs the SDK packages once `ANDROID_HOME` points at a cmdline-tools install), and
`build-ios` needs Xcode with a Simulator runtime.

Only the `nvenc` headers are a git submodule; `--recurse-submodules` on clone, or
`git submodule update --init`, covers it. `make bootstrap` syncs them too.

## 2. How the tree is laid out

```
core/       platform-agnostic C++20 — protocol, packetization, FEC, session state,
            input mapping, bitrate control, the VT emulator. No OS headers. Unit-tested.
platform/   thin OS abstractions behind one shared API — sockets, clock, logging,
            randomness, source enumeration. Depends on core.
client/     the five apps: android, ios, linux, macos, windows.
            client/apple/ is Swift shared by the macOS and iOS apps, not an app itself.
            client/cli/ is the command-line client, one binary for all three desktops.
third_party/  quiche (QUIC), opus (audio), the nvenc headers, the minimal FFmpeg build
make/       one .mk per platform, included by the root Makefile
scripts/    bootstrap, packaging, coverage, style and CI helpers
.github/    workflows, and actions/ — the composite steps they all reuse
```

Write logic once and share it: before adding anything under `client/*`, check whether it
belongs in `core/` (platform-agnostic) or `platform/` (needs the OS, same API
everywhere). [`ARCHITECTURE.md`](ARCHITECTURE.md) explains the layering, the threading
model and the wire protocol; `CLAUDE.md` states the rules this repository enforces.

## 3. The everyday loop

```bash
make test      # core suite, offline, no GPU and no network — a few seconds
make lint      # formatting check across C++, Kotlin and Swift, without writing
```

Run both before finishing a change. Use `make format` to apply formatting when needed;
`make lint` only checks it. The repository pins the formatter versions used in CI.

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
build and launch it. Choose desktop app settings on its four pages; the desktop apps do
not accept command-line flags. `run-android` installs and opens the app on a connected
device or emulator through adb. `run-ios` does the same on the Simulator.

### The command-line client

`client/cli/` builds `deskhub-cli`, which uses commands instead of the app's pages. Use
it over SSH, from a script or under systemd. The `connect` command opens a viewer window
on Windows and Linux; macOS does not support that command yet.

```bash
make build-cli                       # debug build for this OS
make release-cli                     # optimized
make run-cli ARGS="scan"             # build, then run with those arguments
```

It is behind `-DDESKHUB_CLI=ON` (off by default), so the app builds and the sanitizer,
coverage and fuzz presets are untouched by it. Turning it on makes the per-OS media
libraries required rather than optional, because a client that cannot capture or decode is
not a client.

| Command | Does |
| --- | --- |
| `share` | share this machine — any display, the shell, or both |
| `connect ADDRESS` | open a window on a host's screen and control it (Windows and Linux) |
| `shell ADDRESS` | open a shell on a host, in the terminal you are already in |
| `send ADDRESS FILE...` | send files to a host that accepts them |
| `displays`, `scan`, `sources`, `probe` | what can be shared, and who is out there |
| `devices`, `trust`, `settings` | the same files the desktop app reads and writes |

`deskhub-cli help COMMAND` prints the flags. Every command answers `--json`, and the exit
code says what went wrong: `2` bad flags, `3` nobody answered, `4` refused, `5` the host
key changed, `9` this build cannot do it.

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
`debug`, `release` and every `build-*` target build the ABI they need first, and both are a
no-op once built — `make quiche`, `quiche-android`, `quiche-ios`, `quiche-macos` and the
matching `opus`, `opus-android`, `opus-ios`, `opus-macos` run those steps on their own. If
CMake stops with a missing-quiche error, that is deliberate: it refuses to produce a binary
that could never connect.

## 5. Tests

| Command | Runs | Covers |
| --- | --- | --- |
| `make test` | offline, no sockets | all of `core/`: wire format, framing, FEC, sessions, VT emulator, settings, strings |
| `make test-platform` | loopback sockets | real QUIC handshakes, SPAKE2 end to end, terminal host + viewer over the wire, PTY against a real shell, lockout, approval |
| `make test-integration` | loopback, fake capture/encode | full host↔client sessions: negotiation, video across the wire, input, passcode and approval gating, junk resistance |
| `make test-all` | all three, core first | |
| `make test-ctest` | the same tests through CTest | exactly how CI invokes them |
| `make test-asan` | all three under ASan + UBSan | clang/gcc only, not MSVC |
| `make test-tsan` | all three under ThreadSanitizer | clang/gcc only, not MSVC |
| `make test-perf` | release build, offline + loopback | the hot paths measured, not just exercised: `core_perf` covers packetize/reassemble/FEC, 1080p downscale, CRC and file batches, the VT parser and screen, wire encode/decode, the audio jitter buffer; `platform_perf` covers real QUIC over loopback |

Nothing in the test suites needs a remote peer, a GPU or a network.

**Coverage.** `make coverage` produces the `core/` report through clang + llvm-cov;
`scripts/check-coverage.sh` enforces the gate CI applies — **≥ 90 % lines, ≥ 80 %
branches**.

**Fuzzing.** `make fuzz` runs the libFuzzer targets over the wire, H.264, reassembly,
terminal-byte and UI-text parsers plus the host and viewer session state machines (clang,
Linux/macOS; `FUZZ_SECONDS=N` per target). Each target first replays
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
  `out/perf/baseline.txt` on an idle machine, later runs report the change on every row
  and fail past 25 %. The file describes that one machine, so it stays out of git.

`DESKHUB_PERF_TOLERANCE`, `DESKHUB_PERF_REPEATS`, `DESKHUB_PERF_BASELINE` and
`DESKHUB_PERF_WRITE` tune the timing half. Neither `make test` nor CI runs any of it:
debug, ASan and coverage builds say nothing about production speed.

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
member functions, templates and exception parameters, which CI's `-Werror` turns into
errors.

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

The Windows app and CLI are also published as Inno Setup installers, built from
`packaging/windows/` by the release workflow. Their portable Windows builds and the Linux
app remain single files.

## 8. Releasing

1. Bump [`VERSION`](../VERSION) — `scripts/check-version.sh` fails the deploy if the tag
   and the file disagree.
2. Update the documents the change touches, in both languages, in the same commit.
3. Tag `vX.Y.Z` and push it. `.github/workflows/deploy.yml` builds every platform, creates
   the GitHub Release, ships iOS to TestFlight, macOS through notarization, and Android to
   the Play internal track.

**Release notes are generated from the commit subjects** between the previous tag and this
one, by `scripts/changelog.sh`. Run it locally to see what a tag would produce:

```bash
scripts/changelog.sh v5.0.0     # or with no argument, for the tag at HEAD
```

Which means commit subjects are user-facing, and the conventional-commit type in front of
one decides the section it lands in. The full mapping, the rules that override it and
worked examples live in [`.claude/skills/commit/SKILL.md`](../.claude/skills/commit/SKILL.md)
— read it before writing a subject. Empty sections are left out of the release, and
`INCLUDE_INTERNAL=1 scripts/changelog.sh` shows the commits that were omitted.

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
- all three suites under ASan/UBSan and TSan, and cross-built for arm64 Linux, an Android
  emulator and the iOS Simulator
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
| `make reset-macos-permissions` | clears the TCC grants when a local build and a downloaded build fight over the bundle id (`ARGS="--purge"` also deletes the built copies) |
| `make ffmpeg-min` | Ubuntu: the static minimal FFmpeg the app links (run automatically by `build-linux`) |
| `make opus` | the Opus audio codec for the host target (run automatically by `debug`, `release` and `build-linux`) |
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
- **macOS permissions behave oddly after switching between a local and a downloaded
  build** — `make reset-macos-permissions`.

Bugs and questions: [issues](https://github.com/manhpham90vn/Deskhub/issues).
