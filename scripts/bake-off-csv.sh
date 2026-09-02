#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

OUT_DIR=${1:-out/bake-off}
TESTS=${DESKHUB_CORE_TESTS:-out/build/x64-debug/core/core_tests}

if [ ! -x "$TESTS" ]; then
    TESTS="${TESTS}.exe"
fi

if [ ! -x "$TESTS" ]; then
    echo "bake-off-csv: no core_tests binary at ${TESTS%.exe}[.exe]" >&2
    echo "bake-off-csv: build it with 'make test' first, or point DESKHUB_CORE_TESTS at it" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
RAW="$OUT_DIR/run.log"
"$TESTS" >"$RAW" 2>&1 || {
    echo "bake-off-csv: core_tests failed; the numbers below would describe a broken build" >&2
    tail -20 "$RAW" >&2
    exit 1
}

split_table() {
    local name=$1 header_match=$2
    awk -v want="$header_match" '
        /^\[csv\] / {
            row = substr($0, 7)
            if (index(row, want) == 1) { printing = 1; print row; next }
            if (printing) {
                if (row ~ /^[a-z_]+,[a-z_]+/) { printing = 0; next }
                print row
            }
        }
    ' "$RAW" >"$OUT_DIR/$name.csv"
    local rows
    rows=$(($(wc -l <"$OUT_DIR/$name.csv") - 1))
    echo "  $OUT_DIR/$name.csv   $rows rows"
}

echo "bake-off CSV regenerated from $TESTS"
split_table fec-sweep "scheme,fec,armed_from_feedback"
split_table audio-delay "target_ms,adaptive,jitter_ms"
split_table pacer-judder "lead_us,vsync_us,wobble_ms"
split_table nack-hybrid "repair,rtt_ms,hold_frames"

echo
echo "Every row is produced by a seeded, deterministic simulation in core/tests —"
echo "no network and no GPU — so a second run reproduces these files byte for byte."
echo "Read docs/ARCHITECTURE.md section 9 for what each table concluded, including"
echo "the points where a Deskhub default lost."
