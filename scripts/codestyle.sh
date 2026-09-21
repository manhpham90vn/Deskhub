#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CHECK=0
ONLY=all
while [ $# -gt 0 ]; do
    case "$1" in
    --check) CHECK=1 ;;
    --only)
        shift
        case "${1:-}" in
        cpp | kotlin | swift) ONLY=$1 ;;
        *) echo "usage: codestyle.sh [--check] [--only cpp|kotlin|swift]" >&2; exit 2 ;;
        esac
        ;;
    *) echo "usage: codestyle.sh [--check] [--only cpp|kotlin|swift]" >&2; exit 2 ;;
    esac
    shift
done

fail=0

if [ "$ONLY" = all ] || [ "$ONLY" = cpp ]; then
    command -v clang-format >/dev/null 2>&1 || {
        echo "clang-format not found - run 'make bootstrap' first." >&2
        exit 1
    }
    CLANG_FORMAT_VERSION=$(sed -n 's/^CLANG_FORMAT_VERSION=//p' scripts/bootstrap.sh)
    [ -n "$CLANG_FORMAT_VERSION" ] || {
        echo "no CLANG_FORMAT_VERSION in scripts/bootstrap.sh - codestyle reads the pin from there so what it enforces cannot drift from what bootstrap installs." >&2
        exit 1
    }
    clang-format --version | grep -qF "$CLANG_FORMAT_VERSION" || {
        echo "clang-format $CLANG_FORMAT_VERSION is what CI enforces, but $(command -v clang-format) is $(clang-format --version)." >&2
        echo "Reformatting with another version churns files CI then rejects - run 'make bootstrap'." >&2
        exit 1
    }
    CPP_LIST=$(git ls-files 'core/*' 'platform/*' 'client/*' 'tests/*' | grep -E '\.(h|hpp|cpp|cc|c)$' || true)
    if [ -z "$CPP_LIST" ]; then
        echo "codestyle.sh: found no C++ files - is this a full checkout?" >&2
        exit 1
    fi
    echo "[clang-format] $(echo "$CPP_LIST" | grep -c .) files ($(command -v clang-format))"
    if [ "$CHECK" = 1 ]; then
        if echo "$CPP_LIST" | xargs clang-format --dry-run --Werror; then
            echo "  OK"
        else
            fail=1
        fi
    else
        echo "$CPP_LIST" | xargs clang-format -i
        echo "  formatted"
    fi
fi

if [ "$ONLY" = all ] || [ "$ONLY" = kotlin ]; then
    if command -v java >/dev/null 2>&1; then
        KTLINT=tools/ktlint.jar
        if [ ! -f "$KTLINT" ]; then
            echo "tools/ktlint.jar not found - run 'make bootstrap' first." >&2
            exit 1
        fi
        echo "[ktlint] $(git ls-files 'client/android/*' | grep -c '\.kt$') files"
        KT_ARGS="--relative"
        if [ "$CHECK" = 0 ]; then KT_ARGS="$KT_ARGS -F"; fi
        if java -jar "$KTLINT" $KT_ARGS 'client/android/**/*.kt'; then
            echo "  OK"
        else
            fail=1
        fi
    else
        echo "[ktlint] skipped (java not found)"
    fi
fi

if [ "$ONLY" = all ] || [ "$ONLY" = swift ]; then
    SWIFTFORMAT=$(command -v swiftformat 2>/dev/null || true)
    if [ -z "$SWIFTFORMAT" ]; then
        SWIFTFORMAT=tools/swiftformat
        if [ ! -x "$SWIFTFORMAT" ]; then
            echo "tools/swiftformat not found - run 'make bootstrap' first." >&2
            exit 1
        fi
    fi
    echo "[swiftformat] $(git ls-files 'client/apple/*' 'client/ios/*' 'client/macos/*' | grep -c '\.swift$') files ($SWIFTFORMAT)"
    SF_ARGS="client/apple client/ios client/macos"
    if [ "$CHECK" = 1 ]; then SF_ARGS="--lint $SF_ARGS"; fi
    if "$SWIFTFORMAT" $SF_ARGS; then
        echo "  OK"
    else
        fail=1
    fi

    SWIFTLINT=$(command -v swiftlint 2>/dev/null || true)
    if [ -z "$SWIFTLINT" ]; then
        echo "[swiftlint] skipped (swiftlint not found)"
    else
        SL_DIRS="client/apple/swift client/ios/app/swift client/ios/broadcast/swift client/ios/shared client/macos/app/swift"
        echo "[swiftlint] $SL_DIRS ($SWIFTLINT)"
        if [ "$CHECK" = 0 ]; then "$SWIFTLINT" lint --fix --quiet $SL_DIRS >/dev/null || true; fi
        if "$SWIFTLINT" lint --strict --quiet $SL_DIRS; then
            echo "  OK"
        else
            if [ "$CHECK" = 1 ]; then fail=1; fi
        fi
    fi
fi

if [ "$fail" = 1 ]; then
    echo "codestyle: FAILED"
    exit 1
fi
echo "codestyle: OK"
