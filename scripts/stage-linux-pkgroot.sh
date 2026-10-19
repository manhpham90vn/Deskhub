#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

DEST="${1:?usage: stage-linux-pkgroot.sh <dest-dir>}"
BIN=out/build/x64-release/client/linux/deskhub
CLI=out/build/x64-release/client/cli/deskhub-cli
DAEMON=out/build/x64-release/client/linux/daemon/deskhubd

[ -f "$BIN" ] || {
    echo "stage-linux-pkgroot.sh: $BIN not found - run 'make release-linux' first." >&2
    exit 1
}
[ -f "$DAEMON" ] || {
    echo "stage-linux-pkgroot.sh: $DAEMON not found - run 'make release-linux' first." >&2
    exit 1
}
[ -f "$CLI" ] || {
    echo "stage-linux-pkgroot.sh: $CLI not found - run 'make release-cli' first; the package ships the app and the CLI together." >&2
    exit 1
}

rm -rf "$DEST"

install -Dm755 "$BIN" "$DEST/usr/bin/deskhub"
strip "$DEST/usr/bin/deskhub"
install -Dm755 "$CLI" "$DEST/usr/bin/deskhub-cli"
strip "$DEST/usr/bin/deskhub-cli"
install -Dm755 "$DAEMON" "$DEST/usr/libexec/deskhub/deskhubd"
strip "$DEST/usr/libexec/deskhub/deskhubd"

mkdir -p "$DEST/usr/lib/systemd/system"
cat > "$DEST/usr/lib/systemd/system/deskhubd.service" <<'EOF'
[Unit]
Description=Deskhub access before sign-in
After=systemd-logind.service

[Service]
Type=simple
ExecStart=/usr/libexec/deskhub/deskhubd
Restart=on-failure
RestartSec=2
RuntimeDirectory=deskhub
RuntimeDirectoryMode=0755
StateDirectory=deskhub
StateDirectoryMode=0700

[Install]
WantedBy=multi-user.target
EOF

install -Dm644 client/linux/icons/deskhub-256.png \
    "$DEST/usr/share/icons/hicolor/256x256/apps/deskhub.png"
install -Dm644 client/linux/icons/deskhub-512.png \
    "$DEST/usr/share/icons/hicolor/512x512/apps/deskhub.png"

mkdir -p "$DEST/usr/share/applications"
cat > "$DEST/usr/share/applications/deskhub.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Deskhub
Comment=Share and control desktops over the local network
Exec=deskhub
Icon=deskhub
Terminal=false
Categories=Network;RemoteAccess;
EOF

mkdir -p "$DEST/usr/lib/udev/rules.d"
cat > "$DEST/usr/lib/udev/rules.d/60-deskhub-uinput.rules" <<'EOF'
KERNEL=="uinput", SUBSYSTEM=="misc", MODE="0660", GROUP="input", TAG+="uaccess", OPTIONS+="static_node=uinput"
EOF

mkdir -p "$DEST/usr/lib/modules-load.d"
echo uinput > "$DEST/usr/lib/modules-load.d/deskhub.conf"

mkdir -p "$DEST/usr/share/doc/deskhub"
{
    echo "Deskhub is distributed under the MIT License."
    echo "Third-party components and their licenses: see THIRD_PARTY_NOTICES.md."
    echo
    cat LICENSE
} > "$DEST/usr/share/doc/deskhub/copyright"
scripts/stage-licenses.sh linux "$DEST/usr/share/doc/deskhub"
