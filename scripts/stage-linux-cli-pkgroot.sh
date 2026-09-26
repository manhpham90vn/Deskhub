#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

DEST="${1:?usage: stage-linux-cli-pkgroot.sh <dest-dir>}"
BIN=out/build/x64-release/client/cli/deskhub-cli

[ -f "$BIN" ] || {
    echo "stage-linux-cli-pkgroot.sh: $BIN not found - run 'make release-cli' first." >&2
    exit 1
}

rm -rf "$DEST"
install -Dm755 "$BIN" "$DEST/usr/bin/deskhub-cli"
strip "$DEST/usr/bin/deskhub-cli"

mkdir -p "$DEST/usr/lib/udev/rules.d"
cat > "$DEST/usr/lib/udev/rules.d/60-deskhub-cli-uinput.rules" <<'EOF'
KERNEL=="uinput", SUBSYSTEM=="misc", MODE="0660", GROUP="input", TAG+="uaccess", OPTIONS+="static_node=uinput"
EOF

mkdir -p "$DEST/usr/lib/modules-load.d"
echo uinput > "$DEST/usr/lib/modules-load.d/deskhub-cli.conf"

mkdir -p "$DEST/usr/share/doc/deskhub-cli"
{
    echo "DeskHub CLI is distributed under the MIT License."
    echo "Third-party components and their licenses: see THIRD_PARTY_NOTICES.md."
    echo
    cat LICENSE
} > "$DEST/usr/share/doc/deskhub-cli/copyright"
install -Dm644 THIRD_PARTY_NOTICES.md "$DEST/usr/share/doc/deskhub-cli/THIRD_PARTY_NOTICES.md"
install -Dm644 licenses/LGPL-2.1.txt "$DEST/usr/share/doc/deskhub-cli/LGPL-2.1.txt"
