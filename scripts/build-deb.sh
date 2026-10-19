#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

for tool in dpkg-deb dpkg-shlibdeps dpkg; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-deb.sh: '$tool' is required (sudo apt install dpkg-dev)." >&2
        exit 1
    }
done

VERSION="$(tr -d '[:space:]' < VERSION)"
ARCH="$(dpkg --print-architecture)"
DIST=out/dist/linux
STAGE="$DIST/deb"
DEB="$DIST/deskhub_${VERSION}_${ARCH}.deb"

rm -rf "$STAGE" "$DEB"
scripts/stage-linux-pkgroot.sh "$STAGE"

mkdir -p "$STAGE/debian"
touch "$STAGE/debian/control"
DEPENDS="$(cd "$STAGE" && dpkg-shlibdeps -O usr/bin/deskhub usr/bin/deskhub-cli usr/libexec/deskhub/deskhubd | sed 's/^shlibs:Depends=//')"
rm -rf "$STAGE/debian"

INSTALLED_SIZE="$(du -sk "$STAGE" | cut -f1)"

mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: deskhub
Version: $VERSION
Architecture: $ARCH
Maintainer: Manh Pham <manhpv151090@gmail.com>
Section: net
Priority: optional
Installed-Size: $INSTALLED_SIZE
Depends: $DEPENDS
Recommends: xdg-desktop-portal, va-driver-all
Provides: deskhub-cli
Replaces: deskhub-cli
Conflicts: deskhub-cli
Homepage: https://github.com/manhpham90vn/Deskhub
Description: LAN remote desktop - share and control screens
 Deskhub shares the screen of one machine and controls it from another
 over UDP on the local network.
 .
 This package contains the Ubuntu app (host + viewer role), the deskhub-cli
 command for scripts and SSH sessions, the deskhubd service that shares the
 sign-in screen once access before sign-in is switched on, and the udev rule
 that grants access to /dev/uinput, so remote mouse and keyboard input works
 without any manual permission setup.
EOF

cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
    if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload-rules || true
        modprobe uinput 2>/dev/null || true
        udevadm trigger --name-match=uinput || true
    fi
    if [ -d /run/systemd/system ]; then
        systemctl daemon-reload || true
        systemctl enable deskhubd.service || true
        systemctl restart deskhubd.service || true
    fi
fi
exit 0
EOF

cat > "$STAGE/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "remove" ] && [ -d /run/systemd/system ]; then
    systemctl disable --now deskhubd.service || true
fi
exit 0
EOF

cat > "$STAGE/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    rm -rf /var/lib/deskhub
    if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload-rules || true
    fi
    if [ -d /run/systemd/system ]; then
        systemctl daemon-reload || true
    fi
fi
exit 0
EOF

chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/prerm" "$STAGE/DEBIAN/postrm"

dpkg-deb --build --root-owner-group "$STAGE" "$DEB" >/dev/null
rm -rf "$STAGE"
echo "[ok]      $DEB"
