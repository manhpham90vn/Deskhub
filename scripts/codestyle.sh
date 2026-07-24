#!/usr/bin/env bash
# =============================================================================
# codestyle.sh — format/lint cho macOS/Ubuntu, cùng hành vi với codestyle.ps1
# (Windows). C++ (clang-format) + Kotlin (ktlint) + Swift (swiftformat).
#
#   scripts/codestyle.sh                  # ÁP format tại chỗ cho cả 3 ngôn ngữ
#   scripts/codestyle.sh --check          # chỉ KIỂM TRA, exit != 0 nếu có file lệch (khớp CI)
#   scripts/codestyle.sh --only cpp       # giới hạn một ngôn ngữ: cpp | kotlin | swift
#   scripts/codestyle.sh --check --only swift
#
# Tool do `make bootstrap` cài (script này chỉ DÙNG, thiếu thì nhắc chạy bootstrap):
#   clang-format — lấy từ PATH (bootstrap ghim 22.1.3 qua pipx cho khớp CI).
#   ktlint       — tools/ktlint.jar (tools/ đã gitignore).
#   swiftformat  — ưu tiên PATH, không có thì tools/swiftformat.
#
# Tương thích bash 3.2 (macOS mặc định) — không dùng mapfile/assoc array.
# =============================================================================
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

# --- C++ (clang-format) ----------------------------------------------------
if [ "$ONLY" = all ] || [ "$ONLY" = cpp ]; then
    command -v clang-format >/dev/null 2>&1 || {
        echo "clang-format not found - run 'make bootstrap' first." >&2
        exit 1
    }
    # Repo không có tên file chứa khoảng trắng nên truyền qua xargs là an toàn.
    CPP_LIST=$(git ls-files 'core/*' 'platform/*' 'client/*' | grep -E '\.(h|hpp|cpp|cc|c)$')
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

# --- Kotlin (ktlint) -------------------------------------------------------
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
            if [ "$CHECK" = 1 ]; then fail=1; fi
        fi
    else
        echo "[ktlint] skipped (java not found)"
    fi
fi

# --- Swift (swiftformat) ---------------------------------------------------
if [ "$ONLY" = all ] || [ "$ONLY" = swift ]; then
    SWIFTFORMAT=$(command -v swiftformat 2>/dev/null || true)
    if [ -z "$SWIFTFORMAT" ]; then
        SWIFTFORMAT=tools/swiftformat
        if [ ! -x "$SWIFTFORMAT" ]; then
            echo "tools/swiftformat not found - run 'make bootstrap' first." >&2
            exit 1
        fi
    fi
    # swiftformat tự quét thư mục theo .swiftformat ở gốc repo; đếm file chỉ để in.
    echo "[swiftformat] $(git ls-files 'client/ios/*' | grep -c '\.swift$') files ($SWIFTFORMAT)"
    SF_ARGS="client/ios"
    if [ "$CHECK" = 1 ]; then SF_ARGS="--lint $SF_ARGS"; fi
    if "$SWIFTFORMAT" $SF_ARGS; then
        echo "  OK"
    else
        if [ "$CHECK" = 1 ]; then fail=1; fi
    fi
fi

if [ "$fail" = 1 ]; then
    echo "codestyle: FAILED"
    exit 1
fi
echo "codestyle: OK"
