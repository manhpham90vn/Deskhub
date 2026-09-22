#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

. scripts/tools.sh

BUILD_DIR=${1:-out/build/x64-debug}
DB="$BUILD_DIR/compile_commands.json"
if [ -z "${CLANG_TIDY:-}" ]; then
    CLANG_TIDY="$(resolve_local_clang_tidy || true)"
    if [ -z "$CLANG_TIDY" ]; then
        ensure_local_clang_tools
        CLANG_TIDY="$(resolve_local_clang_tidy || true)"
    fi
fi

[ -n "${CLANG_TIDY:-}" ] || {
    echo "clang-tidy.sh: no usable clang-tidy was found, even after downloading to tools/." >&2
    exit 1
}
command -v "$CLANG_TIDY" >/dev/null 2>&1 || {
    echo "clang-tidy.sh: '$CLANG_TIDY' not found." >&2
    exit 1
}

if [ ! -f "$DB" ]; then
    echo "clang-tidy.sh: $DB is missing - run 'cmake --preset x64-debug' first." >&2
    exit 1
fi

FILES=$(python3 - "$DB" <<'EOF'
import json, sys

with open(sys.argv[1]) as db:
    entries = json.load(db)

wanted = [e["file"] for e in entries if "/core/src/" in e["file"] or "/platform/src/" in e["file"]]
print("\n".join(sorted(set(wanted))))
EOF
)

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
