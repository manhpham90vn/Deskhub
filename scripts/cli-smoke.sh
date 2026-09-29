#!/usr/bin/env bash
set -euo pipefail

CLI=${1:-out/build/x64-debug/client/cli/deskhub-cli}
PORT=${DESKHUB_SMOKE_PORT:-47989}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/deskhub-cli-smoke.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/home"
export HOME="$WORK/home"
export USERPROFILE="$WORK/home"

if [ ! -x "$CLI" ]; then
    echo "cli-smoke: $CLI is not there - run 'make build-cli' first" >&2
    exit 1
fi

fail() {
    echo "::error::cli-smoke: $1" >&2
    exit 1
}

expect_code() {
    local want=$1
    shift
    local got=0
    "$@" >"$WORK/out" 2>"$WORK/err" || got=$?
    if [ "$got" != "$want" ]; then
        echo "--- stdout"; cat "$WORK/out"
        echo "--- stderr"; cat "$WORK/err"
        fail "'$*' exited $got, expected $want"
    fi
}

echo "== version and help"
expect_code 0 "$CLI" version
expect_code 0 "$CLI" help
expect_code 0 "$CLI" help share
expect_code 2 "$CLI" nonsense
expect_code 2 "$CLI" sources
expect_code 2 "$CLI" share --fps 0

echo "== stores read without a host"
expect_code 0 "$CLI" devices --json
expect_code 0 "$CLI" trust --json
expect_code 0 "$CLI" settings --json

echo "== keys are exchanged as text before anything connects"
expect_code 0 "$CLI" devices public
cp "$WORK/out" "$WORK/client.pub"
expect_code 0 "$CLI" access add --stdin <"$WORK/client.pub"
expect_code 0 "$CLI" host-key public
cp "$WORK/out" "$WORK/host.pub"
expect_code 0 "$CLI" key generate --name stranger

echo "== nobody is listening"
expect_code 4 "$CLI" sources "127.0.0.1:$((PORT + 1))"
grep -q "Could not reach" "$WORK/err" || fail "an unreachable host was not reported as unreachable"

case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*)
        echo "== the rest needs POSIX signals - skipping on Windows"
        echo "cli-smoke: OK"
        exit 0
        ;;
esac

echo "== a shell-only host, and a viewer that talks to it"
"$CLI" share --no-screen --terminal --port "$PORT" --quiet \
    >"$WORK/share.out" 2>"$WORK/share.err" &
SHARE_PID=$!

ready=0
for _ in $(seq 1 200); do
    if grep -q "Listening on UDP" "$WORK/share.err" 2>/dev/null; then
        ready=1
        break
    fi
    if ! kill -0 "$SHARE_PID" 2>/dev/null; then
        break
    fi
    sleep 0.1
done
if [ "$ready" != 1 ]; then
    cat "$WORK/share.err"
    fail "the host never started listening"
fi

expect_code 4 "$CLI" sources "127.0.0.1:$PORT"
grep -q "not trusted yet" "$WORK/err" || fail "an unpinned host was not refused as unknown"
grep -q "Host key fingerprint: SHA256:" "$WORK/err" || fail "the refusal did not show the fingerprint"

expect_code 0 "$CLI" host add smoke --address "127.0.0.1:$PORT" --identity default \
    --host-key-stdin <"$WORK/host.pub"

offered=0
for _ in $(seq 1 200); do
    if "$CLI" sources smoke --json >"$WORK/out" 2>"$WORK/err" &&
        grep -q '"terminal":true' "$WORK/out"; then
        offered=1
        break
    fi
    sleep 0.1
done
if [ "$offered" != 1 ]; then
    echo "--- stdout"; cat "$WORK/out"
    echo "--- stderr"; cat "$WORK/err"
    fail "the host never offered its shell"
fi

expect_code 4 "$CLI" sources smoke --identity stranger
grep -q "not authorized" "$WORK/err" || fail "a key the host never allowed was not refused"

echo "== a host that takes no files says so"
printf 'deskhub-smoke-file\n' >"$WORK/notes.txt"
expect_code 4 "$CLI" send "127.0.0.1:$PORT" "$WORK/notes.txt" --quiet
expect_code 2 "$CLI" send "127.0.0.1:$PORT" "$WORK/absent.txt" --quiet

printf 'echo deskhub-smoke-ok\nexit\n' |
    "$CLI" shell smoke >"$WORK/shell.out" 2>"$WORK/shell.err" || true
if ! tr -d '\r' <"$WORK/shell.out" | grep -aq "deskhub-smoke-ok"; then
    echo "--- shell stdout"; cat "$WORK/shell.out"
    echo "--- shell stderr"; cat "$WORK/shell.err"
    echo "--- host stderr"; cat "$WORK/share.err"
    fail "the remote shell did not run the command"
fi

kill -INT "$SHARE_PID"
wait "$SHARE_PID" || fail "the host did not stop cleanly on an interrupt"

echo "== files from a viewer to a host that takes them"
FILE_PORT=$((PORT + 2))
LANDING="$WORK/landing"
mkdir -p "$LANDING"
"$CLI" share --no-screen --terminal --files --files-dir "$LANDING" --port "$FILE_PORT" \
    --quiet >"$WORK/files.out" 2>"$WORK/files.err" &
FILES_PID=$!

ready=0
for _ in $(seq 1 200); do
    if grep -q "Listening on UDP" "$WORK/files.err" 2>/dev/null; then
        ready=1
        break
    fi
    if ! kill -0 "$FILES_PID" 2>/dev/null; then
        break
    fi
    sleep 0.1
done
if [ "$ready" != 1 ]; then
    cat "$WORK/files.err"
    fail "the file host never started listening"
fi

head -c 120000 /dev/urandom >"$WORK/payload.bin"
expect_code 0 "$CLI" send "127.0.0.1:$FILE_PORT" "$WORK/notes.txt" "$WORK/payload.bin" \
    --accept-new-host-key --quiet
cmp -s "$WORK/notes.txt" "$LANDING/notes.txt" || fail "the small file did not arrive intact"
cmp -s "$WORK/payload.bin" "$LANDING/payload.bin" || fail "the large file did not arrive intact"

expect_code 0 "$CLI" send "127.0.0.1:$FILE_PORT" "$WORK/notes.txt" --quiet
[ -f "$LANDING/notes (2).txt" ] || fail "a second copy overwrote the first instead of landing beside it"

expect_code 4 "$CLI" send "127.0.0.1:$FILE_PORT" "$WORK/notes.txt" --identity stranger --quiet

kill -INT "$FILES_PID"
wait "$FILES_PID" || fail "the file host did not stop cleanly on an interrupt"

echo "== a host sharing nothing but files stays up"
ONLY_PORT=$((PORT + 3))
ONLY_LANDING="$WORK/only"
mkdir -p "$ONLY_LANDING"
"$CLI" share --no-screen --files --files-dir "$ONLY_LANDING" --port "$ONLY_PORT" \
    --quiet >"$WORK/only.out" 2>"$WORK/only.err" &
ONLY_PID=$!

ready=0
for _ in $(seq 1 200); do
    if grep -q "Listening on UDP" "$WORK/only.err" 2>/dev/null; then
        ready=1
        break
    fi
    if ! kill -0 "$ONLY_PID" 2>/dev/null; then
        break
    fi
    sleep 0.1
done
if [ "$ready" != 1 ]; then
    cat "$WORK/only.err"
    fail "a host with only file transfer never started listening"
fi

expect_code 0 "$CLI" host add only --address "127.0.0.1:$ONLY_PORT" --identity default \
    --host-key-stdin <"$WORK/host.pub"

sleep 1
kill -0 "$ONLY_PID" 2>/dev/null || fail "a host with only file transfer stopped on its own"

expect_code 0 "$CLI" send "127.0.0.1:$ONLY_PORT" "$WORK/notes.txt" --quiet
cmp -s "$WORK/notes.txt" "$ONLY_LANDING/notes.txt" ||
    fail "the file never reached a host that shares nothing else"

kill -INT "$ONLY_PID"
wait "$ONLY_PID" || fail "the file-only host did not stop cleanly on an interrupt"

echo "cli-smoke: OK"
