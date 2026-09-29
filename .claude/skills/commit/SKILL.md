---
name: commit
description: Write a Deskhub commit message. Use when staging or committing changes in this repository, when asked to "commit", "write a commit message", or when a commit subject needs fixing before a tag is pushed. Enforces conventional-commit types and keeps release notes in step with the change.
---

# Deskhub commit messages

Release notes are written by hand in `packaging/release-notes/vX.Y.Z.md` (see
`docs/BUILD.md` §Release); commits no longer generate them. The conventional-commit type
still tells a reader of `git log` what kind of change a commit is, so keep it accurate.

## Steps

1. **Get off `main` first.** Commits land on a branch so they can go up as a pull
   request: if HEAD is on `main`, create and switch to a new branch before committing —
   short, kebab-case, named for the change (`perf-lag-tests`, `terminal-repaint-fix`).
   Only commit directly on `main` when the user explicitly says to.
2. **Look at what is actually being committed** — `git status --short` and
   `git diff --staged` (or `git diff` if nothing is staged yet). Never write a subject
   from the conversation alone.
3. **Split if needed.** One change per commit; two unrelated things can only land in one
   section. Say so and stage them separately.
4. **Pick the type** from the table below.
5. **Write the subject**: English, imperative, no trailing period, ~72 characters, about
   what changed for someone using Deskhub. Add a body only when the *why* is not obvious.
   Never mention Claude: no `Co-Authored-By: Claude` trailer and no "Generated with
   Claude Code" line, in the subject, the body or a pull request description.
6. **Run the repo checks that apply** — see *Before committing* below.
7. **Note it for the release** when the change is something users will notice — above
   all a breaking change: add it to the next `packaging/release-notes/vX.Y.Z.md`, creating
   the file if it does not exist yet.

## Types

| Type | For |
| --- | --- |
| `feat:` | something new a user can see or use |
| `fix:` | a bug that reached a user |
| `perf:` `refactor:` `style:` `revert:` | same behaviour, reshaped |
| `security:` | hardening, a fixed vulnerability, a CVE bump |
| `docs:` `chore:` `ci:` `build:` `test:` `deps:` | internal, invisible to users |

`type(scope)!: …` — scope is optional and free-form (`core`, `platform`, `linux`, `macos`,
`windows`, `android`, `ios`, `quic`, `terminal`, `ci`); `!` marks a breaking change,
which also belongs in the release notes.

## Before committing

- **Documentation ships in pairs.** Touching any `NAME.md` means updating `NAME.vi.md` in
  the same commit, and the other way round. English is authoritative.
- **`VERSION` must match the tag** that will be pushed — `scripts/check-version.sh`
  fails the deploy otherwise — and the tag needs its `packaging/release-notes/` file, or
  `scripts/check-release-notes.sh` fails it.
- **No comments in code** — that rule is enforced by review, not by CI; a diff that adds
  comments is not ready to commit.
- **`make test` and `make lint`** for anything touching `core/`, `platform/` or a client;
  `make lint-tidy` when C++ under `core/src` or `platform/src` changed. Docs-only commits
  need neither.

## Rewrites

| Instead of | Write |
| --- | --- |
| `Add Vietnamese documentation for installation and building instructions` | `docs: add Vietnamese install and build guides` |
| `Enhance security and authentication features` | `security: reject a second passcode guess on the same connection` |
| `Implement terminal sharing functionality` | `feat(terminal): share a shell from desktop hosts` |
| `Update CMake configurations and enhance QUIC build options` | `build: pin the quiche static runtime on MSVC` |
| `fix stuff` | `fix(android): keep the session alive when the screen locks` |

If a subject cannot be written without listing files, the commit is doing too much —
split it.
