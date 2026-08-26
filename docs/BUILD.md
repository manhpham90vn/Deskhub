**English** · [Tiếng Việt](BUILD.vi.md)

# Deskhub — Building and developing

Everything needed to compile Deskhub yourself, run the test suites, and cut a release. If
you only want to *use* the app, take a prebuilt one from [`INSTALL.md`](INSTALL.md)
instead.

```bash
git clone --recurse-submodules https://github.com/manhpham90vn/Deskhub.git
cd Deskhub
make bootstrap        # once: toolchain + dependencies for this OS
make test             # build and run the core suite offline
make build-linux      # or build-windows / build-macos / build-ios / build-android
```

No platform is ever built implicitly: a bare `make` prints the target list and builds
nothing. Every target is documented in full at the top of the
[`Makefile`](../Makefile), and `make/help.txt` is what a bare `make` shows.

---

## 1. What you need first

`make bootstrap` installs what it can and tells you what it cannot. Install these
yourself before running it:

| Host OS | Install first | What bootstrap then does |
| --- | --- | --- |
| **Ubuntu / Debian** | nothing beyond apt; [Rust](https://rustup.rs) | build-essential, clang, llvm, cmake, ninja, JDK 17, the GTK3 / PipeWire / VA-API / tray `-dev` packages, VA-API drivers, the GNOME portal, the static minimal FFmpeg, quiche, opus |
| **macOS** | [Homebrew](https://brew.sh), Xcode + command line tools, [Rust](https://rustup.rs) | cmake, ninja, swiftlint, pipx, Homebrew LLVM (Apple clang ships no libFuzzer runtime), Temurin JDK 17, quiche and opus for Apple and Android |
| **Windows** | winget (App Installer), Visual Studio with the C++ toolchain and the *C++ Clang tools* component, [Rust](https://rustup.rs) | the rest via winget, driven by `scripts/bootstrap.ps1` |

On every OS it also pins the style tools: clang-format, clang-tidy, ktlint and
SwiftFormat, each at a fixed version with a checksum check — never install these by hand,
CI compares against exactly these versions.

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

Run both before considering a change done. `make format` applies the formatting instead
of just checking it — never hand-format, the tools are pinned for a reason.

New logic in `core/` needs a test in the matching `core/tests/` subdirectory.

## 4. Building and running an app

| Target | Builds | Needs |
| --- | --- | --- |
| `make build-windows` | one `Deskhub.exe` | Windows + MSVC |
| `make build-macos` | the macOS app | macOS + Xcode |
| `make build-linux` | one `deskhub` binary | Ubuntu + the `-dev` packages |
| `make build-ios` | the iOS app for the Simulator | macOS + Xcode + a Simulator runtime |
| `make build-android` | a debug APK | Android SDK + NDK, `adb` |

Each has a `release-<os>` (optimized) and a `run-<os>` (build, then launch) sibling.
The desktop apps parse no command-line flags at all — everything is chosen on their four
pages. `run-android` installs and opens on the connected device or emulator through adb,
and `run-ios` does the same on the Simulator.

### The command-line client

`client/cli/` builds one `deskhub-cli` binary that does the same job without a GUI
toolkit, driven by flags instead of pages. It is the way to run Deskhub over SSH, from a
script, or under systemd.

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
| `connect ADDRESS` | open a window on a host's screen and drive it |
| `shell ADDRESS` | open a shell on a host, in the terminal you are already in |
| `displays`, `scan`, `sources`, `probe` | what can be shared, and who is out there |
| `devices`, `trust`, `settings` | the same files the desktop app reads and writes |

`deskhub-cli help COMMAND` prints the flags. Every command answers `--json`, and the exit
code says what went wrong: `2` bad flags, `3` nobody answered, `4` refused, `5` the host
key changed, `9` this build cannot do it.

Per-OS state today: Linux does all of it. Windows shares and connects through the same
window code the desktop app already uses. macOS shares and opens shells, but `connect`
needs a window layer that is not written yet, and reports so.

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
| `make test-perf` | release build, offline | the hot paths of `core/` measured, not just exercised: packetize/reassemble/FEC, 1080p downscale, CRC and file batches, the VT parser and screen, wire encode/decode, the audio jitter buffer |

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

**Performance.** `make test-perf` builds `core_perf` with the release preset and measures
the hot paths — 27 workloads, a few seconds. Three things fail it, and none of them is a
millisecond figure picked out of the air:

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
| `make lint` | the same checks without writing — what CI enforces |
| `make lint-tidy` | clang-tidy over `core/src` + `platform/src` |

Single-language variants exist too: `format-cpp`, `lint-cpp`, `format-kotlin`,
`lint-kotlin`, `format-swift`, `lint-swift`.

House rules, in short — the full version is in `CLAUDE.md`:

- C++20, no compiler extensions. `deskhub` for core, `deskhubp` for platform.
- `PascalCase` functions and types, `camelCase` locals, trailing underscore on private
  members.
- **No comments anywhere.** Descriptive names, small functions, early returns and named
  constants instead. Knowledge that must survive goes into the error message of the path
  that fails without it, or into `ARCHITECTURE.md`.
- All identifiers and log messages in English; every prose document ships as an
  English/Vietnamese pair, English authoritative.

## 7. Packaging

| Command | Produces |
| --- | --- |
| `make dist-macos` | a dmg signed with Developer ID, notarized and stapled |
| `make verify-macos` | a Gatekeeper check on the build just produced |
| `make dist-linux` | `.deb` + `.rpm`, both installing the uinput udev rule |

The Windows and Linux apps are each a single file; there is no installer to build.

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

## 9. What CI gates

A green local `make test` + `make lint` is not the whole story. On every pull request:

- clang-tidy over `core/src` + `platform/src`, SwiftLint `--strict`, Android Lint
- actionlint + shellcheck over the workflows and `scripts/*.sh`
- all three suites under ASan/UBSan and TSan, and cross-built for arm64 Linux, an Android
  emulator and the iOS Simulator
- the whole integration suite three more times on Windows, hunting an intermittent stack
  corruption in `DrainStreams` that shows up in about one run in three and so slips through
  a single run
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

## 11. When the build fights back

- **CMake stops on a missing quiche library** — run the matching `make quiche*` target;
  each ABI needs its own. The same holds for opus and the `make opus*` targets.
- **`make fuzz` on macOS finds no libFuzzer** — it needs Homebrew LLVM; `make bootstrap`
  installs it, while the rest keeps building with the Xcode toolchain.
- **`make lint` disagrees with your editor** — the pinned tools win. Run `make bootstrap`
  again to pull the exact versions, then `make format`.
- **Android targets can't find the SDK** — set `ANDROID_HOME`, then re-run
  `make bootstrap`; `ANDROID_NDK_VERSION=<v>` selects a different NDK.
- **macOS permissions behave oddly after switching between a local and a downloaded
  build** — `make reset-macos-permissions`.

Bugs and questions: [issues](https://github.com/manhpham90vn/Deskhub/issues).
