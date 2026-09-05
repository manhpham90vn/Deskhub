#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=${1:-out/build/x64-debug}
DB="$BUILD_DIR/compile_commands.json"
CLANG_TIDY=${CLANG_TIDY:-clang-tidy}

command -v "$CLANG_TIDY" >/dev/null 2>&1 || {
    echo "clang-tidy.sh: '$CLANG_TIDY' not found." >&2
    exit 1
}

if [ ! -f "$DB" ]; then
    echo "clang-tidy.sh: $DB is missing - run 'cmake --preset x64-debug' first." >&2
    exit 1
fi

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
    echo "clang-tidy.sh: no working python on PATH. It reads $DB to pick which core and" >&2
    echo "clang-tidy.sh: platform sources to check. On Windows 'python3' is usually the" >&2
    echo "clang-tidy.sh: Microsoft Store alias, which exits 49 without running anything, so" >&2
    echo "clang-tidy.sh: install a real python and expose it as python3, python or py." >&2
    exit 1
fi

FILES=$("$PYTHON" - "$DB" <<'EOF'
import json, sys

with open(sys.argv[1]) as db:
    entries = json.load(db)

wanted = [e["file"] for e in entries if "/core/src/" in e["file"] or "/platform/src/" in e["file"]]
print("\n".join(sorted(set(wanted))))
EOF
)
FILES=$(printf '%s' "$FILES" | tr -d '\r')

if [ -z "$FILES" ]; then
    echo "clang-tidy.sh: no core/platform sources in $DB." >&2
    exit 1
fi

TIDY_ARGS=(-p "$BUILD_DIR" --quiet)
SYSROOT_NOTE=""

if [ "$(uname -s)" = "Darwin" ]; then
    SDK=$(xcrun --show-sdk-path 2>/dev/null || true)
    if [ -n "$SDK" ]; then
        TIDY_ARGS+=(--extra-arg=-isysroot "--extra-arg=$SDK")
        SYSROOT_NOTE=" [sysroot $SDK]"
    else
        echo "clang-tidy.sh: xcrun found no SDK - results on macOS will be unreliable." >&2
    fi
fi

echo "[clang-tidy] $(echo "$FILES" | grep -c .) files ($(command -v "$CLANG_TIDY"))$SYSROOT_NOTE"
echo "$FILES" | xargs "$CLANG_TIDY" "${TIDY_ARGS[@]}"
