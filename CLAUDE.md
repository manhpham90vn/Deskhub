# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Rules

### 1. Do not write comments — anywhere

Write no comments in any file — no explanatory comments, no section banners, no
docstrings, no `TODO`/`FIXME` notes. This covers C++, `make/*.mk`, `CMakeLists.txt`,
`CMakePresets.json`, `scripts/*`, `.github/workflows/*` and config files alike.

Make the content self-explanatory instead: descriptive names, small functions, early
returns, named constants instead of magic numbers. If a block seems to need a comment,
extract it into a well-named function. Knowledge that must not be lost goes into the
error message of the path that fails without it, or into `docs/ARCHITECTURE.md`
§"Decisions worth remembering" (mirrored in the `.vi.md`) — never into a comment.

The single exception is the root `Makefile`: its header block is the one place where
every target is documented. Keep it in sync when you add or rename a target, and
mirror the change in `make/help.txt` (what a bare `make` prints).

Not comments, so they stay: shebang lines, preprocessor directives and include
guards, license headers, and the `# vX.Y.Z` tag after a pinned action SHA in the
workflows. Markdown documents are prose, not code.

Comments still present in older files are legacy: when you edit next to one, move
anything load-bearing into the documentation above and delete it.

### 2. Prefer `core/` and `platform/` — reuse before you add

The whole point of this layout is that logic is written once and shared by all five
clients. Before writing anything in `client/*`, check whether it belongs in a lower
layer.

```
core/       platform-agnostic logic, pure C++20, no OS headers, unit-tested
platform/   thin OS abstractions with one shared API (depends on core)
client/     per-OS apps: android, ios, linux, macos, windows (depend on platform + core)
client/apple/  Swift shared by the macOS and iOS apps — not an app of its own
```

Decision order when adding code:

1. **Does `core/` or `platform/` already do this?** Search first — `core/include/deskhub/`
   and `platform/include/deskhubp/` are the public surfaces. Reuse it.
2. **Is it platform-agnostic?** Protocol, packetization, FEC, session state, input
   mapping, bitrate control, diagnostics → `core/`. Add tests in `core/tests/`.
3. **Does it need the OS, but with the same API everywhere?** Sockets, clock, logging,
   randomness, source enumeration → `platform/`, behind one header in
   `platform/include/deskhubp/`, with per-OS `.cpp` files selected in
   `platform/CMakeLists.txt` (see `UdpSocketPosix.cpp` / `UdpSocketWin.cpp`).
4. **Only genuinely OS-specific?** Capture, encode, decode, render, windowing, UI →
   `client/<os>/`. These conform to the contracts in
   `core/include/deskhub/media/VideoContract.h`.

Never duplicate logic across `client/*`. If you find yourself writing the same thing for
a second platform, stop and lift it into `core/` or `platform/`.

Swift that both Apple apps need is the one exception to "lift it into a lower layer": it
goes in `client/apple/swift/`, which both `client/macos` and `client/ios` reference as a
folder in their Xcode projects. Check it before adding Swift to either app.

Hard constraints:

- `core/` must not include any OS or third-party header, and must not depend on
  `platform/`. It stays unit-testable offline with no network and no GPU.
- `platform/` may include OS headers, but its public headers must expose one identical
  API on every OS.
- Use the shared helpers rather than raw OS calls: `LOGI`/`LOGW`/`LOGE` from
  `deskhubp/diag/Log.h`, plus `deskhubp/system/Clock.h`, `deskhubp/system/Random.h`,
  `deskhubp/net/UdpSocket.h`, `deskhubp/client/SourceQuery.h`.
- Platform code is split by role: `deskhubp/auth` (the one handshake),
  `deskhubp/client` (`HostLink`, `ScreenViewer`, `TerminalViewer`,
  `FileTransferClient`), `deskhubp/host` (`HostEngine`, `SharingHost`,
  `TerminalHost`, `FileHost`). Core session machines mirror it:
  `deskhub/session/client` and `deskhub/session/host`, with shared types beside
  them in `deskhub/session`. Put new code on the right side, or beside them if
  both sides genuinely share it.

## Commands

```sh
make                 # print the target list — builds nothing
make bootstrap       # install toolchain + deps (run once)
make test            # build and run core_tests offline — the fast feedback loop
make test-all        # core + platform + integration suites
make test-ctest      # same tests through CTest, as CI runs them
make coverage        # core coverage report (clang + llvm-cov)
make debug           # configure + build the debug preset of the shared CMake tree
make format          # format C++ / Kotlin / Swift
make lint            # check formatting without writing (what CI enforces)
make lint-tidy       # clang-tidy over core/src + platform/src, the same gate CI runs
```

Per-platform: `make build-<os>`, `run-<os>`, `release-<os>` where `<os>` is one of
`linux`, `windows`, `macos`, `ios`, `android`. No platform is the default — a bare
`make` prints `make/help.txt` instead of building anything, so always name the platform
explicitly.

Run `make test` and `make lint` before considering a change done.

CI gates a good deal more than those two:

- clang-tidy over `core/src` + `platform/src` (`scripts/clang-tidy.sh`) — run it
  locally with `make lint-tidy`
- SwiftLint `--strict` (runs in `make lint` only where swiftlint is installed) and
  Android Lint
- actionlint + shellcheck on the workflows and `scripts/*.sh`
- core coverage ≥ 90% lines / 80% branches (`scripts/check-coverage.sh`, checked
  after `make coverage`)
- all three suites under ASan/UBSan and TSan, and cross-built for arm64 Linux, an
  Android emulator and the iOS Simulator
- the whole integration suite again on Windows under MSVC ASan
- the libFuzzer targets for 30 s each on every PR, 15 min each nightly
- CodeQL over C++/Kotlin/Swift, a gitleaks sweep of the whole history, and a
  dependency review on pull requests

A green local `make test` + `make lint` does not cover those.

## Conventions

- C++20, no compiler extensions. Warnings are errors-adjacent on MSVC (`/W4 /permissive-`).
- Namespaces: `deskhub` for core, `deskhubp` for platform.
- `PascalCase` functions and types, `camelCase` locals, trailing underscore on private
  members (`cur_`, `min_`).
- Formatting is enforced by pinned tools — never hand-format; run `make format`.
- All identifiers, log messages, and code comments are in **English**. Documentation is
  bilingual — see below.
- New logic in `core/` needs a test in the matching `core/tests/` subdirectory.

## Documentation

Every prose document is published as an English/Vietnamese pair: `NAME.md` and
`NAME.vi.md` beside it. **English is the authoritative version** — write the change there
first, then mirror it into the `.vi.md`, in the same commit. A `.vi.md` that lags behind
its English original is a bug.

| English | Vietnamese | Covers |
| --- | --- | --- |
| `README.md` | `README.vi.md` | What it is, why, platforms, what's inside — short, and links out |
| `docs/INSTALL.md` | `docs/INSTALL.vi.md` | Getting a prebuilt Deskhub onto each platform |
| `docs/BUILD.md` | `docs/BUILD.vi.md` | Building from source, tests, packaging, releasing |
| `docs/SPECIFICATION.md` | `docs/SPECIFICATION.vi.md` | Feature spec — behaviour only, no implementation detail |
| `docs/ARCHITECTURE.md` | `docs/ARCHITECTURE.vi.md` | How it is built — layers, threads, wire protocol, design decisions |
| `SECURITY.md` | `SECURITY.vi.md` | Threat model, hardening, vulnerability reports |
| `PRIVACY.md` | `PRIVACY.vi.md` | Privacy policy — versioned, with a changelog table |
| `THIRD_PARTY_NOTICES.md` | `THIRD_PARTY_NOTICES.vi.md` | Third-party components and licences |

Rules for these files:

- Every one starts with a language switcher line: the English file shows **English** in
  bold followed by a link labelled *Tiếng Việt*, and the `.vi.md` shows the mirror of
  that. Each `.vi.md` states that English governs.
- Links inside a `.vi.md` point at the `.vi.md` counterpart wherever one exists.
- Cross-language link integrity is not enforced by CI — check relative links resolve
  before you finish.
- `PRIVACY.md` is a published legal document: any behaviour change that touches what is
  stored or transmitted needs a new version number, a new effective date, and a changelog
  row — in both languages.
- `CLAUDE.md` and store listings under `fastlane/metadata/*/vi/` are outside this scheme.

When you change behaviour, check whether these documents still describe it. Passcode
handling, what is persisted on disk, and per-platform capability tables go stale fastest.

## License

MIT (`LICENSE`). The Linux app statically links LGPL-2.1 FFmpeg — if you change how
FFmpeg is built or linked, update `THIRD_PARTY_NOTICES.md` (and its `.vi.md`)
accordingly.
