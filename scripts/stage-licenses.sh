#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

PLATFORM="${1:-}"
DEST="${2:-}"
QUICHE_AND_OPUS_LICENSES=(BSD-2-Clause-quiche BoringSSL Apache-2.0 rust-crates BSD-3-Clause-opus)

usage() {
    echo "usage: stage-licenses.sh <linux|apple> <dest-dir>" >&2
    exit 1
}

case "$PLATFORM" in
linux) LICENSES=("${QUICHE_AND_OPUS_LICENSES[@]}" LGPL-2.1) ;;
apple) LICENSES=("${QUICHE_AND_OPUS_LICENSES[@]}") ;;
*) usage ;;
esac
[ -n "$DEST" ] || usage

mkdir -p "$DEST/licenses"
cp THIRD_PARTY_NOTICES.md "$DEST/THIRD_PARTY_NOTICES.md"
chmod 644 "$DEST/THIRD_PARTY_NOTICES.md"
for name in "${LICENSES[@]}"; do
    cp "licenses/$name.txt" "$DEST/licenses/$name.txt"
    chmod 644 "$DEST/licenses/$name.txt"
done
