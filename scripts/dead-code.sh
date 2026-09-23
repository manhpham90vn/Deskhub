#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

. scripts/tools.sh

fail=0

CPPCHECK=$(resolve_local_cppcheck || true)
if [ -z "$CPPCHECK" ]; then
    ensure_local_cppcheck
    CPPCHECK=$(resolve_local_cppcheck || true)
fi
[ -n "$CPPCHECK" ] || {
    echo "cppcheck $(deskhub_pinned_value CPPCHECK_VERSION) is what CI enforces but none was found, even after installing it." >&2
    exit 1
}
PYTHON=$(deskhub_host_python || true)
[ -n "$PYTHON" ] || {
    echo "dead-code.sh: Python 3 not found - install it, then re-run." >&2
    exit 1
}

echo "[dead-code] C++ functions, FFI entry points, string ids, Kotlin constants ($CPPCHECK)"
"$PYTHON" scripts/dead-code.py "$CPPCHECK" || fail=1

if command -v java >/dev/null 2>&1; then
    ensure_local_detekt >/dev/null
    echo "[detekt] unused Kotlin code (client/android)"
    if report=$(java -jar tools/detekt-cli.jar --input client/android/app/src/main/java \
        --config client/android/detekt.yml 2>&1); then
        echo "  OK"
    else
        echo "$report"
        fail=1
    fi
else
    echo "[detekt] skipped (java not found)"
fi

if [ "$fail" = 1 ]; then
    echo "dead-code: FAILED"
    exit 1
fi
echo "dead-code: OK"
