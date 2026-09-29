#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

REF=${1:-${GITHUB_REF:-}}

case "$REF" in
refs/tags/*) TAG=${REF#refs/tags/} ;;
v*) TAG=$REF ;;
*)
    echo "not a tag ref (${REF:-none}) - no release notes to check"
    exit 0
    ;;
esac

NOTES=packaging/release-notes/$TAG.md
if [ ! -s "$NOTES" ]; then
    echo "::error::$NOTES is missing or empty - the GitHub Release body is that file, so write it, commit it and tag that commit again"
    exit 1
fi

echo "release notes for $TAG: $NOTES"
