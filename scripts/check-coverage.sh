#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

find_python() {
    local candidate
    for candidate in python3 python py; do
        command -v "$candidate" >/dev/null 2>&1 || continue
        "$candidate" -c "" >/dev/null 2>&1 || continue
        echo "$candidate"
        return 0
    done
    return 1
}

if ! PYTHON=$(find_python); then
    echo "check-coverage.sh: no working python on PATH. It parses the llvm-cov report to" >&2
    echo "check-coverage.sh: compare it against the thresholds. On Windows 'python3' is" >&2
    echo "check-coverage.sh: usually the Microsoft Store alias, which exits 49 without" >&2
    echo "check-coverage.sh: running anything, so expose a real python as python3, python or py." >&2
    exit 1
fi

MIN_LINES=${1:-90}
MIN_BRANCHES=${2:-80}

BIN=out/build/coverage/core/core_tests
PROFDATA=out/build/coverage/core_tests.profdata
LLVM_COV=${LLVM_COV:-llvm-cov}

if [ ! -f "$PROFDATA" ]; then
    echo "check-coverage.sh: $PROFDATA is missing - run 'make coverage' first." >&2
    exit 1
fi

command -v "$LLVM_COV" >/dev/null 2>&1 || {
    echo "check-coverage.sh: '$LLVM_COV' not found - set LLVM_COV to the llvm-cov to use." >&2
    exit 1
}

"$LLVM_COV" export "$BIN" -instr-profile="$PROFDATA" -summary-only core/src core/include |
    MIN_LINES="$MIN_LINES" MIN_BRANCHES="$MIN_BRANCHES" "$PYTHON" -c '
import json, os, sys

totals = json.load(sys.stdin)["data"][0]["totals"]
lines = totals["lines"]["percent"]
branches = totals["branches"]["percent"]
min_lines = float(os.environ["MIN_LINES"])
min_branches = float(os.environ["MIN_BRANCHES"])

print(f"core lines {lines:.2f}% (min {min_lines:.2f}%)   branches {branches:.2f}% (min {min_branches:.2f}%)")

failed = []
if lines < min_lines:
    failed.append(f"line coverage {lines:.2f}% is below {min_lines:.2f}%")
if branches < min_branches:
    failed.append(f"branch coverage {branches:.2f}% is below {min_branches:.2f}%")
for f in failed:
    print(f"::error::{f}")
sys.exit(1 if failed else 0)
'
